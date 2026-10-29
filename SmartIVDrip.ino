/*
 * 智慧點滴注射預警系統 (Smart IV Drip Alert System)
 * 平台：ESP32 (Arduino core 3.x)
 *
 * 功能：
 *   - 上、下兩組非接觸式紅外線液位感測器，雙階段判斷
 *   - 第一階段（液位低於上位）：警示燈恆亮 + 提示音 + 推播
 *   - 第二階段（液位低於下位）：警示燈快閃 + 每秒警報 + 推播
 *   - 感測器狀態矛盾（上有液、下無液）判為感測器異常
 *   - 手機網頁即時監控（每秒更新）、遠端靜音、預估剩餘時間、更換紀錄
 *   - 網頁設定頁：床位名稱、Wi-Fi、網址、配對碼、點滴參數（免重新燒錄）
 *   - 固定網址：http://<裝置名稱>.local（mDNS）
 *   - 網頁推播：ESP32 直接加密並送到 Apple / Google 推播服務，不經第三方通知平台
 *
 * 接線（對應「點滴注射預警系統電路原理圖」）：
 *   上位感測器（光電接收器集極，10kΩ 上拉）-> GPIO32
 *   下位感測器（光電接收器集極，10kΩ 上拉）-> GPIO33
 *   蜂鳴器：GPIO25 -> 1kΩ -> 2N2222 基極，3.3V 主動式蜂鳴器接集極
 *   Wi-Fi / 系統運作 LED：GPIO26 -> 330Ω -> LED -> GND
 *   Alert 警示 LED：GPIO27 -> 330Ω -> LED -> GND（原理圖未畫，對應外觀圖紅燈）
 *   Power LED：直接接 3.3V 經 330Ω 至 GND，不佔腳位
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <vector>
#include "webpush.h"
#include "index_html.h"
#include "pair_html.h"
#include "settings_html.h"

// ===== 使用者設定 =====
// Wi-Fi 密碼與推播金鑰放在 secrets.h（不會上傳 GitHub）
// 第一次使用：把 secrets.example.h 複製一份改名為 secrets.h 再填入，並執行 make_vapid.ps1
// 之後床位名稱、Wi-Fi 等都可以在 http://<裝置>/settings 修改
#include "secrets.h"

const char* AP_SSID       = "IV-Monitor";  // 連不上 Wi-Fi 時自建熱點
const char* VAPID_SUBJECT = "https://github.com/youenc120-ux/smart-iv-drip-alert";  // 推播服務出問題時的聯絡方式

const unsigned long REPEAT_EMPTY_MS = 2UL * 60 * 1000;  // 第二階段未處理時每 2 分鐘再推播
const size_t MAX_SUBS    = 10;                           // 最多綁定幾支手機
const size_t MAX_HISTORY = 20;                           // 保留幾筆更換紀錄

// 感測器偵測到液體時的輸出電位。
// 反射式：光電晶體收到較強反射光時導通 -> 輸出 LOW。
// 有液 / 無液哪一種反射較強要看點滴袋與安裝角度，請實測後設定。
const int LIQUID_LEVEL = LOW;

const int PIN_SENSOR_UPPER = 32;
const int PIN_SENSOR_LOWER = 33;
const int PIN_BUZZER       = 25;
const int PIN_LED_WIFI     = 26;
const int PIN_LED_ALERT    = 27;

const unsigned long DEBOUNCE_MS   = 1500;            // 液面晃動 / 氣泡濾除
const unsigned long MUTE_STAGE1_MS = 5UL * 60 * 1000; // 第一階段靜音 5 分鐘
const unsigned long MUTE_STAGE2_MS = 60UL * 1000;     // 第二階段最多靜音 1 分鐘

// ===== 狀態 =====
enum DripState { NORMAL, LOW_LEVEL, EMPTY, SENSOR_FAULT };
const char* STATE_KEY[]   = { "normal", "low", "empty", "fault" };
const char* STATE_TEXT[]  = { "液量充足", "第一階段：液量偏低", "第二階段：請立即更換", "感測器異常" };
const char* STATE_EMOJI[] = { "✅", "⚠️", "🚨", "❗" };

WebServer server(80);

DripState state = NORMAL;
unsigned long stateSince = 0;
unsigned long mutedUntil = 0;
bool apMode = false;
unsigned long restartAt = 0;  // 設定變更後延遲重新開機

// 型別需在第一個函式前定義，Arduino 自動產生的函式宣告才找得到

// 可在網頁上修改的設定（存在快閃記憶體）
struct Config {
  String bed = "301 病房 A 床";
  String host = "iv-monitor";  // 固定網址 http://iv-monitor.local
  String ssid, pass;           // 空白 = 使用 secrets.h
  String pin;                  // 配對碼，空白 = 不需要
  int mlBetween = 100;         // 上位到下位之間的液量（ml）
  int mlBelow = 50;            // 下位以下的剩餘液量（ml）
  int rateMlH = 0;             // 醫囑滴速（ml/小時），0 = 未設定
} cfg;

// 去彈跳
struct Debounced {
  int pin;
  bool stable;        // true = 偵測到液體
  bool lastRaw;
  unsigned long changedAt;
};
Debounced upper = { PIN_SENSOR_UPPER, true, true, 0 };
Debounced lower = { PIN_SENSOR_LOWER, true, true, 0 };

// 推播訊息；welcome = 只送給剛綁定的手機（welcomeEndpoint 空白 = 全部手機）的測試通知
struct Notice { uint8_t state; char body[200]; bool welcome; };

// 一袋點滴的紀錄（Unix 秒，0 = 沒有發生或時間未知）
struct BagRecord { uint32_t startAt, lowAt, emptyAt, ackAt, doneAt; };

// 已綁定的手機（網頁伺服器與推播工作都會用到，以互斥鎖保護）
std::vector<webpush::Subscription> subs;
SemaphoreHandle_t subsLock;
String welcomeEndpoint;
Preferences prefs;
uint8_t vapidPriv[32];
bool vapidReady = false;

// 更換紀錄與滴速
BagRecord history[MAX_HISTORY];
size_t historyCount = 0;
BagRecord cur = { 0, 0, 0, 0, 0 };
uint32_t lastDoneAt = 0;          // 上一袋更換完成的時間 = 這一袋開始
unsigned long lowEnteredMs = 0;   // 進入第一階段的 millis，用來實測滴速
float bagRate = 0;                // 這一袋實測滴速（ml/小時）
float lastRate = 0;               // 上一袋實測滴速

// 配對碼嘗試次數限制
int pinFails = 0;
unsigned long pinLockUntil = 0;

bool readLiquid(int pin) { return digitalRead(pin) == LIQUID_LEVEL; }

void updateDebounce(Debounced& s, unsigned long now) {
  bool raw = readLiquid(s.pin);
  if (raw != s.lastRaw) { s.lastRaw = raw; s.changedAt = now; }
  if (raw != s.stable && now - s.changedAt >= DEBOUNCE_MS) s.stable = raw;
}

DripState evaluate() {
  if (upper.stable && lower.stable)   return NORMAL;
  if (!upper.stable && lower.stable)  return LOW_LEVEL;
  if (!upper.stable && !lower.stable) return EMPTY;
  return SENSOR_FAULT;  // 上有液、下無液：物理上不合理
}

uint32_t nowEpoch() {
  time_t t = time(nullptr);
  return t >= 1700000000 ? (uint32_t)t : 0;
}

// ===== 輸出（LED / 蜂鳴器），非阻塞 =====
void updateOutputs(unsigned long now) {
  bool muted = now < mutedUntil;
  unsigned long t = now - stateSince;
  bool buzz = false;

  // Wi-Fi 燈：已連線（或熱點模式）恆亮，斷線時閃爍
  bool wifiOk = apMode || WiFi.status() == WL_CONNECTED;
  digitalWrite(PIN_LED_WIFI, wifiOk || (now / 500) % 2);

  // 警示燈：第一階段恆亮、第二階段快閃、感測器異常慢閃
  bool alertLed = false;
  if (state == LOW_LEVEL)    alertLed = true;
  if (state == EMPTY)        alertLed = (now / 250) % 2;
  if (state == SENSOR_FAULT) alertLed = (now / 1000) % 2;
  digitalWrite(PIN_LED_ALERT, alertLed);

  switch (state) {
    case LOW_LEVEL:
      // 進入時響「一聲」(0.5 秒)，之後每 30 秒短響提醒一次
      buzz = (t < 500) || (t % 30000) < 150;
      break;
    case EMPTY:
      buzz = (t % 1000) < 300;         // 每秒一聲，連續警報
      break;
    case SENSOR_FAULT:
      buzz = (t % 3000) < 80;          // 輕短提示
      break;
    default: break;
  }
  digitalWrite(PIN_BUZZER, (buzz && !muted) ? HIGH : LOW);
}

String jsonEscape(const char* s) {
  String out;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { out += '\\'; out += *s; }
    else if (*s == '\n') out += "\\n";
    else if ((uint8_t)*s >= 0x20) out += *s;
  }
  return out;
}
String jsonEscape(const String& s) { return jsonEscape(s.c_str()); }

// ===== 設定 =====
void loadConfig() {
  prefs.begin("cfg", true);
  cfg.bed       = prefs.getString("bed", cfg.bed);
  cfg.host      = prefs.getString("host", cfg.host);
  cfg.ssid      = prefs.getString("ssid", "");
  cfg.pass      = prefs.getString("pass", "");
  cfg.pin       = prefs.getString("pin", "");
  cfg.mlBetween = prefs.getInt("mlBetween", cfg.mlBetween);
  cfg.mlBelow   = prefs.getInt("mlBelow", cfg.mlBelow);
  cfg.rateMlH   = prefs.getInt("rate", cfg.rateMlH);
  prefs.end();
}

void saveConfig() {
  prefs.begin("cfg", false);
  prefs.putString("bed", cfg.bed);
  prefs.putString("host", cfg.host);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putString("pin", cfg.pin);
  prefs.putInt("mlBetween", cfg.mlBetween);
  prefs.putInt("mlBelow", cfg.mlBelow);
  prefs.putInt("rate", cfg.rateMlH);
  prefs.end();
}

const char* wifiSsid() { return cfg.ssid.length() ? cfg.ssid.c_str() : WIFI_SSID; }
const char* wifiPass() { return cfg.ssid.length() ? cfg.pass.c_str() : WIFI_PASS; }

// ===== 更換紀錄 =====
void loadHistory() {
  prefs.begin("hist", true);
  historyCount = prefs.getBytes("h", history, sizeof(history)) / sizeof(BagRecord);
  lastDoneAt = prefs.getUInt("last", 0);
  lastRate = prefs.getFloat("rate", 0);
  prefs.end();
}

void saveHistory() {
  prefs.begin("hist", false);
  prefs.putBytes("h", history, historyCount * sizeof(BagRecord));
  prefs.putUInt("last", lastDoneAt);
  prefs.putFloat("rate", lastRate);
  prefs.end();
}

// 狀態改變時更新這一袋的紀錄與實測滴速
void trackBag(DripState next, DripState prev, unsigned long now) {
  if (next == LOW_LEVEL && prev == NORMAL) {
    cur = { lastDoneAt, nowEpoch(), 0, 0, 0 };
    lowEnteredMs = now;
    bagRate = 0;
  } else if (next == EMPTY && prev != SENSOR_FAULT) {
    if (!cur.lowAt && !cur.emptyAt) cur.startAt = lastDoneAt;
    cur.emptyAt = nowEpoch();
    // 上位到下位的液量 ÷ 花費時間 = 實測滴速；過快或過慢視為誤判
    if (prev == LOW_LEVEL && lowEnteredMs) {
      float hours = (now - lowEnteredMs) / 3600000.0f;
      float r = cfg.mlBetween / hours;
      if (r >= 5 && r <= 2000) { bagRate = r; lastRate = r; }
    }
  } else if (next == NORMAL && (prev == LOW_LEVEL || prev == EMPTY)) {
    cur.doneAt = nowEpoch();
    if (historyCount == MAX_HISTORY) {
      memmove(history, history + 1, sizeof(BagRecord) * (MAX_HISTORY - 1));
      historyCount--;
    }
    history[historyCount++] = cur;
    lastDoneAt = cur.doneAt;
    cur = { 0, 0, 0, 0, 0 };
    lowEnteredMs = 0;
    bagRate = 0;
    saveHistory();
  }
}

// 預估剩餘時間（秒），-1 = 無法預估。src 回傳滴速來源
float currentRate(const char** src) {
  if (state == EMPTY && bagRate > 0) { *src = "measured"; return bagRate; }
  if (cfg.rateMlH > 0) { *src = "set"; return cfg.rateMlH; }
  if (lastRate > 0) { *src = "last"; return lastRate; }
  *src = "";
  return 0;
}

long etaSeconds(unsigned long now, const char** src) {
  float r = currentRate(src);
  if (r <= 0) return -1;
  float hours = (now - stateSince) / 3600000.0f;
  float remaining;
  if (state == LOW_LEVEL)  remaining = max(0.0f, cfg.mlBetween - r * hours) + cfg.mlBelow;
  else if (state == EMPTY) remaining = max(0.0f, cfg.mlBelow - r * hours);
  else return -1;
  return (long)(remaining / r * 3600);
}

String durationText(long s) {
  if (s < 60) return "不到 1 分鐘";
  long m = (s + 30) / 60;
  if (m < 60) return "約 " + String(m) + " 分鐘";
  return "約 " + String(m / 60) + " 小時 " + String(m % 60) + " 分";
}

// ===== 已綁定手機的儲存（存在快閃記憶體，斷電不會消失）=====
void loadSubs() {
  prefs.begin("push", true);
  for (size_t i = 0; i < MAX_SUBS; i++) {
    String v = prefs.getString(("s" + String(i)).c_str(), "");
    int a = v.indexOf('\n'), b = v.indexOf('\n', a + 1);
    if (a > 0 && b > a) subs.push_back({ v.substring(0, a), v.substring(a + 1, b), v.substring(b + 1) });
  }
  prefs.end();
  Serial.printf("[推播] 已綁定 %u 支手機\n", (unsigned)subs.size());
}

// 呼叫前須持有 subsLock
void saveSubs() {
  prefs.begin("push", false);
  prefs.clear();
  for (size_t i = 0; i < subs.size(); i++)
    prefs.putString(("s" + String(i)).c_str(), subs[i].endpoint + "\n" + subs[i].p256dh + "\n" + subs[i].auth);
  prefs.end();
}

void removeSub(const String& endpoint) {
  xSemaphoreTake(subsLock, portMAX_DELAY);
  for (size_t i = 0; i < subs.size(); i++) {
    if (subs[i].endpoint == endpoint) { subs.erase(subs.begin() + i); saveSubs(); break; }
  }
  xSemaphoreGive(subsLock);
}

// 配對碼檢查：設定了配對碼才需要；連續錯 5 次鎖 60 秒
// 回傳 nullptr = 通過，否則為錯誤訊息
const char* checkPin(const String& given) {
  if (!cfg.pin.length()) return nullptr;
  if (millis() < pinLockUntil) return "錯誤次數太多，請 1 分鐘後再試";
  if (given == cfg.pin) { pinFails = 0; return nullptr; }
  if (++pinFails >= 5) { pinFails = 0; pinLockUntil = millis() + 60000; }
  return given.length() ? "配對碼錯誤" : "需要配對碼";
}

// ===== 網頁 =====
// 監控頁在 index_html.h（由 phone.html 產生），配對頁在 pair_html.h，設定頁在 settings_html.h

void handleRoot()     { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }
void handlePair()     { server.send_P(200, "text/html; charset=utf-8", PAIR_HTML); }
void handleSettings() { server.send_P(200, "text/html; charset=utf-8", SETTINGS_HTML); }

void sendJson(const String& json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json; charset=utf-8", json);
}

void handleStatus() {
  unsigned long now = millis();
  bool muted = now < mutedUntil;
  const char* src;
  long eta = etaSeconds(now, &src);
  float rate = currentRate(&src);
  String json = "{";
  json += "\"bed\":\"" + jsonEscape(cfg.bed) + "\",";
  json += "\"state\":\"" + String(STATE_KEY[state]) + "\",";
  json += "\"text\":\"" + String(STATE_TEXT[state]) + "\",";
  json += "\"upper\":" + String(upper.stable ? "true" : "false") + ",";
  json += "\"lower\":" + String(lower.stable ? "true" : "false") + ",";
  json += "\"muted\":" + String(muted ? "true" : "false") + ",";
  json += "\"muteLeft\":" + String(muted ? (mutedUntil - now) / 1000 : 0) + ",";
  json += "\"since\":" + String((now - stateSince) / 1000) + ",";
  json += "\"eta\":" + String(eta) + ",";
  json += "\"rate\":" + String((int)(rate + 0.5f)) + ",";
  json += "\"rateSrc\":\"" + String(src) + "\",";
  json += "\"push\":" + String((unsigned)subs.size()) + ",";
  json += "\"host\":\"" + jsonEscape(cfg.host) + "\",";
  json += "\"ap\":" + String(apMode ? "true" : "false") + ",";
  json += "\"rssi\":" + String(apMode ? 0 : WiFi.RSSI());
  json += "}";
  sendJson(json);
}

void handleHistory() {
  String json = "{\"now\":" + String(nowEpoch()) + ",\"items\":[";
  for (size_t i = historyCount; i-- > 0;) {
    const BagRecord& r = history[i];
    json += "{\"s\":" + String(r.startAt) + ",\"l\":" + String(r.lowAt) + ",\"e\":" + String(r.emptyAt) +
            ",\"a\":" + String(r.ackAt) + ",\"d\":" + String(r.doneAt) + "}";
    if (i) json += ",";
  }
  sendJson(json + "]}");
}

void handleMute() {
  // 第二階段只允許短暫靜音，避免警報被長時間關閉
  unsigned long dur = (state == EMPTY) ? MUTE_STAGE2_MS : MUTE_STAGE1_MS;
  if (state != NORMAL) {
    mutedUntil = millis() + dur;
    if ((state == LOW_LEVEL || state == EMPTY) && !cur.ackAt) cur.ackAt = nowEpoch();  // 記錄確認時間
  }
  server.send(200, "text/plain", "ok");
}

void replyJson(bool ok, const char* error, bool needPin = false) {
  String j = String("{\"ok\":") + (ok ? "true" : "false") + ",\"bed\":\"" + jsonEscape(cfg.bed) + "\"";
  if (error) j += String(",\"error\":\"") + error + "\"";
  if (needPin) j += ",\"needPin\":true";
  sendJson(j + "}");
}

void queueNotice(uint8_t st, const char* body, bool welcome = false);

void handleSubscribe() {
  String e = server.arg("endpoint"), k = server.arg("p256dh"), a = server.arg("auth");
  uint8_t buf[70];
  if (!vapidReady) return replyJson(false, "裝置尚未設定推播金鑰（請執行 make_vapid.ps1 後重新燒錄）");
  if (const char* err = checkPin(server.arg("pin"))) return replyJson(false, err, true);
  if (e.length() > 700 || !webpush::endpointAllowed(e)) return replyJson(false, "不支援這個瀏覽器的推播服務");
  if (webpush::b64urlDecode(k, buf, sizeof(buf)) != 65 || webpush::b64urlDecode(a, buf, sizeof(buf)) != 16)
    return replyJson(false, "訂閱資料格式錯誤");

  xSemaphoreTake(subsLock, portMAX_DELAY);
  for (size_t i = 0; i < subs.size(); i++) {
    if (subs[i].endpoint == e) { subs.erase(subs.begin() + i); break; }
  }
  if (subs.size() >= MAX_SUBS) subs.erase(subs.begin());  // 滿了就移除最舊的
  subs.push_back({ e, k, a });
  saveSubs();
  welcomeEndpoint = e;
  xSemaphoreGive(subsLock);

  Serial.printf("[推播] 新增手機，目前 %u 支\n", (unsigned)subs.size());
  queueNotice(state, "已連結這支手機，之後點滴警報會推播到這裡。", true);
  replyJson(true, nullptr);
}

void handleUnsubscribe() {
  removeSub(server.arg("endpoint"));
  replyJson(true, nullptr);
}

// 傳一則測試推播給所有已綁定的手機
void handlePushTest() {
  if (subs.empty()) return replyJson(false, "還沒有綁定任何手機");
  xSemaphoreTake(subsLock, portMAX_DELAY);
  welcomeEndpoint = "";  // 空字串 = 所有手機
  xSemaphoreGive(subsLock);
  queueNotice(state, "這是測試推播，收到代表推播功能正常。", true);
  replyJson(true, nullptr);
}

// 目前設定（不含密碼與配對碼本身）
void handleConfigGet() {
  String json = "{";
  json += "\"bed\":\"" + jsonEscape(cfg.bed) + "\",";
  json += "\"host\":\"" + jsonEscape(cfg.host) + "\",";
  json += "\"ssid\":\"" + jsonEscape(wifiSsid()) + "\",";
  json += "\"pinSet\":" + String(cfg.pin.length() ? "true" : "false") + ",";
  json += "\"mlBetween\":" + String(cfg.mlBetween) + ",";
  json += "\"mlBelow\":" + String(cfg.mlBelow) + ",";
  json += "\"rate\":" + String(cfg.rateMlH) + ",";
  json += "\"lastRate\":" + String((int)(lastRate + 0.5f)) + ",";
  json += "\"ip\":\"" + (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString() + "\",";
  json += "\"ap\":" + String(apMode ? "true" : "false");
  sendJson(json + "}");
}

bool validHost(const String& h) {
  if (h.length() < 1 || h.length() > 30 || h[0] == '-' || h[h.length() - 1] == '-') return false;
  for (char c : h) if (!(isdigit(c) || (c >= 'a' && c <= 'z') || c == '-')) return false;
  return true;
}

void handleConfigPost() {
  if (const char* err = checkPin(server.arg("auth"))) return replyJson(false, err, true);

  String bed = server.arg("bed"), host = server.arg("host"), ssid = server.arg("ssid"), pin = server.arg("pin");
  bed.trim(); host.trim(); host.toLowerCase(); ssid.trim(); pin.trim();
  int mlBetween = server.arg("mlBetween").toInt(), mlBelow = server.arg("mlBelow").toInt();
  int rate = server.arg("rate").toInt();

  if (!bed.length() || bed.length() > 60) return replyJson(false, "床位名稱請輸入 1–20 個字");
  if (!validHost(host)) return replyJson(false, "裝置網址只能用小寫英文、數字和 -");
  if (ssid.length() > 32) return replyJson(false, "Wi-Fi 名稱太長");
  if (pin.length() && (pin.length() < 4 || pin.length() > 8)) return replyJson(false, "配對碼請設 4–8 位數字");
  for (char c : pin) if (!isdigit(c)) return replyJson(false, "配對碼只能用數字");
  if (mlBetween < 1 || mlBetween > 2000 || mlBelow < 1 || mlBelow > 2000) return replyJson(false, "液量請輸入 1–2000 ml");
  if (rate < 0 || rate > 2000) return replyJson(false, "滴速請輸入 0–2000 ml/小時");

  bool netChanged = host != cfg.host || ssid != String(wifiSsid()) || server.arg("pass").length() > 0;
  cfg.bed = bed;
  cfg.host = host;
  if (ssid != String(wifiSsid()) || server.arg("pass").length()) {
    cfg.ssid = ssid;
    if (server.arg("pass").length()) cfg.pass = server.arg("pass");
  }
  if (server.arg("clearPin") == "1") cfg.pin = "";
  else if (pin.length()) cfg.pin = pin;
  cfg.mlBetween = mlBetween;
  cfg.mlBelow = mlBelow;
  cfg.rateMlH = rate;
  saveConfig();
  Serial.println("[設定] 已儲存");

  if (netChanged) restartAt = millis() + 1500;  // 回應送出後再重新開機
  sendJson(String("{\"ok\":true,\"restart\":") + (netChanged ? "true" : "false") + "}");
}

// 非同步掃描附近 Wi-Fi，避免卡住警報
void handleScan() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); return sendJson("{\"scanning\":true}"); }
  if (n == WIFI_SCAN_RUNNING) return sendJson("{\"scanning\":true}");
  String json = "{\"scanning\":false,\"list\":[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (int j = 0; j < i; j++) if (WiFi.SSID(j) == s) { dup = true; break; }
    if (dup) continue;
    if (!first) json += ",";
    first = false;
    json += "{\"ssid\":\"" + jsonEscape(s) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"lock\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  WiFi.scanDelete();
  sendJson(json + "]}");
}

// ===== 推播通知 =====
// HTTPS 連線要 1–3 秒，放在獨立工作執行，蜂鳴器與網頁才不會卡住
QueueHandle_t noticeQueue;
unsigned long lastEmptyNotice = 0;

bool postJson(const char* service, const String& url, const String& json) {
  WiFiClientSecure client;
  client.setInsecure();  // 原型簡化：不驗證憑證
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  http.end();
  Serial.printf("[通知] %s 回應 %d\n", service, code);  // 不印網址，避免 Token 外洩
  return code >= 200 && code < 300;
}

void sendWebPush(const Notice& n) {
  if (!vapidReady) return;
  std::vector<webpush::Subscription> targets;
  xSemaphoreTake(subsLock, portMAX_DELAY);
  for (auto& s : subs)
    if (!n.welcome || welcomeEndpoint.isEmpty() || s.endpoint == welcomeEndpoint) targets.push_back(s);
  xSemaphoreGive(subsLock);
  if (targets.empty()) return;

  // 手機上的推播網頁依照這些欄位顯示通知與紀錄
  String payload = "{\"bed\":\"" + jsonEscape(cfg.bed) + "\",\"state\":\"" +
                   (n.welcome ? "test" : STATE_KEY[n.state]) + "\",\"body\":\"" + jsonEscape(n.body) +
                   "\",\"url\":\"http://" + WiFi.localIP().toString() + "/\",\"t\":" +
                   String((unsigned long)time(nullptr)) + "}";
  for (auto& s : targets) {
    int code = webpush::send(s, payload, vapidPriv, VAPID_PUBLIC, VAPID_SUBJECT);
    Serial.printf("[推播] 回應 %d\n", code);
    if (code == 404 || code == 410) removeSub(s.endpoint);  // 手機已取消訂閱
  }
}

void sendTelegram(const Notice& n) {
  if (n.welcome || !strlen(TG_TOKEN) || !strlen(TG_CHAT_ID)) return;
  String text = String(STATE_EMOJI[n.state]) + " " + cfg.bed + "\n" + n.body;
  String j = "{\"chat_id\":\"" + jsonEscape(TG_CHAT_ID) + "\",\"text\":\"" + jsonEscape(text) + "\"}";
  String url = String("https://api.telegram.org/bot") + TG_TOKEN + "/sendMessage";
  for (int i = 0; i < 2 && !postJson("Telegram", url, j); i++) delay(3000);
}

// 公曆日期換算成 1970-01-01 起算的天數，用來把 HTTP Date 標頭轉成 Unix 時間
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// 推播簽章需要正確時間。先等 NTP；學校或公司網路常擋 NTP，就改用 HTTPS 回應的 Date 標頭校時
bool ensureTime() {
  for (int i = 0; i < 20 && time(nullptr) < 1700000000; i++) delay(500);
  if (time(nullptr) >= 1700000000) return true;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  const char* keys[] = { "Date" };
  if (!http.begin(client, "https://www.google.com/generate_204")) return false;
  http.collectHeaders(keys, 1);
  http.GET();
  String date = http.header("Date");  // 例如 "Wed, 29 Oct 2026 08:12:33 GMT"
  http.end();

  int day, year, hh, mm, ss;
  char mon[4] = {0};
  if (sscanf(date.c_str(), "%*3s, %d %3s %d %d:%d:%d", &day, mon, &year, &hh, &mm, &ss) != 6) {
    Serial.println("[時間] 校時失敗，無法發送推播");
    return false;
  }
  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* p = strstr(months, mon);
  if (!p) return false;
  int month = (p - months) / 3 + 1;
  struct timeval tv = { (time_t)(daysFromCivil(year, month, day) * 86400L + hh * 3600L + mm * 60L + ss), 0 };
  settimeofday(&tv, nullptr);
  Serial.println("[時間] NTP 無回應，已改用網站時間校時");
  return true;
}

void notifyTask(void*) {
  if (!apMode) ensureTime();  // 開機先校時，更換紀錄才有正確時間
  Notice n;
  for (;;) {
    if (xQueueReceive(noticeQueue, &n, portMAX_DELAY) != pdTRUE) continue;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[通知] 未連上網際網路（熱點模式或斷線），略過推播");
      continue;
    }
    if (!ensureTime()) continue;
    sendWebPush(n);
    sendTelegram(n);
  }
}

void queueNotice(uint8_t st, const char* body, bool welcome) {
  if (subs.empty() && !strlen(TG_TOKEN)) return;
  Notice n;
  n.state = st;
  n.welcome = welcome;
  snprintf(n.body, sizeof(n.body), "%s", body);
  xQueueSend(noticeQueue, &n, 0);  // 佇列滿就丟棄，不阻塞主迴圈
}

// 狀態已更新後呼叫，訊息會附上預估剩餘時間
void announce(DripState next, DripState prev, unsigned long now) {
  const char* src;
  long eta = etaSeconds(now, &src);
  String msg;
  switch (next) {
    case LOW_LEVEL:
      if (prev == EMPTY) return;  // 液位不會自己變多，多半是晃動，不重複通知
      msg = "第一階段：液量偏低，請準備更換點滴。";
      if (eta >= 0) msg += "預計" + durationText(eta) + "後耗盡。";
      break;
    case EMPTY:
      msg = "第二階段：點滴即將耗盡，請立即更換！";
      if (eta >= 0) msg += "預計" + durationText(eta) + "後滴完。";
      lastEmptyNotice = now;
      break;
    case SENSOR_FAULT:
      msg = "感測器讀值異常，請檢查安裝位置。";
      break;
    case NORMAL:
      if (prev == NORMAL) return;
      msg = "液位已恢復正常（已更換或已排除）。";
      break;
  }
  queueNotice(next, msg.c_str());
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.host.c_str());
  WiFi.begin(wifiSsid(), wifiPass());
  Serial.printf("連線 Wi-Fi「%s」", wifiSsid());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");  // 推播簽章與紀錄需要正確時間
    if (MDNS.begin(cfg.host.c_str())) MDNS.addService("http", "tcp", 80);
    Serial.printf("\n已連線，手機開啟 http://%s.local 或 http://%s\n", cfg.host.c_str(),
                  WiFi.localIP().toString().c_str());
  } else {
    // 熱點模式同時保留 STA，設定頁才能掃描附近的 Wi-Fi
    apMode = true;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("\n連不上 Wi-Fi，改用熱點 %s，手機開啟 http://%s/settings 設定網路\n", AP_SSID,
                  WiFi.softAPIP().toString().c_str());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SENSOR_UPPER, INPUT);
  pinMode(PIN_SENSOR_LOWER, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_WIFI, OUTPUT);
  pinMode(PIN_LED_ALERT, OUTPUT);

  // 開機自檢：雙燈 + 蜂鳴器短響
  digitalWrite(PIN_LED_WIFI, HIGH); digitalWrite(PIN_LED_ALERT, HIGH);
  digitalWrite(PIN_BUZZER, HIGH); delay(150); digitalWrite(PIN_BUZZER, LOW);
  delay(350);

  loadConfig();
  loadHistory();

  upper.stable = upper.lastRaw = readLiquid(PIN_SENSOR_UPPER);
  lower.stable = lower.lastRaw = readLiquid(PIN_SENSOR_LOWER);
  state = evaluate();
  stateSince = millis();
  if (state == LOW_LEVEL || state == EMPTY) cur = { lastDoneAt, 0, 0, 0, 0 };

  uint8_t keyBuf[40];
  vapidReady = webpush::b64urlDecode(VAPID_PRIVATE, keyBuf, sizeof(keyBuf)) == 32;
  if (vapidReady) memcpy(vapidPriv, keyBuf, 32);
  else Serial.println("[推播] 未設定 VAPID 金鑰，請執行 make_vapid.ps1 後重新燒錄");
  subsLock = xSemaphoreCreateMutex();
  loadSubs();

  setupWiFi();
  noticeQueue = xQueueCreate(8, sizeof(Notice));
  xTaskCreate(notifyTask, "notify", 16384, nullptr, 1, nullptr);  // 加密 + TLS 需要較大堆疊
  if (state != NORMAL) announce(state, NORMAL, millis());  // 開機時就已偏低也要通知

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/history", handleHistory);
  server.on("/mute", HTTP_POST, handleMute);
  server.on("/pair", handlePair);
  server.on("/subscribe", HTTP_POST, handleSubscribe);
  server.on("/unsubscribe", HTTP_POST, handleUnsubscribe);
  server.on("/pushtest", HTTP_POST, handlePushTest);
  server.on("/settings", handleSettings);
  server.on("/config", HTTP_GET, handleConfigGet);
  server.on("/config", HTTP_POST, handleConfigPost);
  server.on("/scan", handleScan);
  server.begin();
}

void loop() {
  unsigned long now = millis();
  server.handleClient();

  updateDebounce(upper, now);
  updateDebounce(lower, now);

  DripState next = evaluate();
  if (next != state) {
    DripState prev = state;
    // 狀態升級（更嚴重）時取消靜音，確保新警報一定響
    if (next > prev) mutedUntil = 0;
    Serial.printf("[狀態] %s -> %s\n", STATE_TEXT[prev], STATE_TEXT[next]);
    state = next;
    stateSince = now;
    trackBag(next, prev, now);
    announce(next, prev, now);
  }

  // 第二階段一直沒人處理（也沒按靜音）就重複推播
  if (state == EMPTY && now >= mutedUntil && now - lastEmptyNotice >= REPEAT_EMPTY_MS) {
    String msg = "點滴耗盡已持續" + durationText((now - stateSince) / 1000) + "，仍未處理！";
    queueNotice(EMPTY, msg.c_str());
    lastEmptyNotice = now;
  }

  updateOutputs(now);

  // 校正用：每秒印出感測器原始電位，裝好後對照「有液 / 無液」決定 LIQUID_LEVEL
  static unsigned long lastPrint = 0;
  if (now - lastPrint > 1000) {
    lastPrint = now;
    Serial.printf("上位=%s 下位=%s 狀態=%s\n",
                  digitalRead(PIN_SENSOR_UPPER) ? "HIGH" : "LOW",
                  digitalRead(PIN_SENSOR_LOWER) ? "HIGH" : "LOW",
                  STATE_TEXT[state]);
  }

  // Wi-Fi 斷線自動重連
  static unsigned long lastCheck = 0;
  if (!apMode && now - lastCheck > 10000) {
    lastCheck = now;
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }

  if (restartAt && now >= restartAt) ESP.restart();
}
