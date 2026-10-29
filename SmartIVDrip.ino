/*
 * 智慧點滴注射預警系統 (Smart IV Drip Alert System)
 * 平台：ESP32 (Arduino core 3.x)
 *
 * 功能：
 *   - 上、下兩組非接觸式紅外線液位感測器，雙階段判斷
 *   - 第一階段（液位低於上位）：警示燈恆亮 + 提示音 + 推播
 *   - 第二階段（液位低於下位）：警示燈快閃 + 每秒警報 + 推播
 *   - 感測器狀態矛盾（上有液、下無液）判為感測器異常
 *   - 手機網頁即時監控（每秒更新）、遠端靜音
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
#include <Preferences.h>
#include <time.h>
#include <vector>
#include "webpush.h"
#include "index_html.h"
#include "pair_html.h"

// ===== 使用者設定 =====
// Wi-Fi 密碼與推播金鑰放在 secrets.h（不會上傳 GitHub）
// 第一次使用：把 secrets.example.h 複製一份改名為 secrets.h 再填入，並執行 make_vapid.ps1
#include "secrets.h"

const char* AP_SSID   = "IV-Monitor";      // 連不上 Wi-Fi 時自建熱點
const char* BED_NAME  = "301 病房 A 床";
const char* VAPID_SUBJECT = "https://github.com/youenc120-ux/smart-iv-drip-alert";  // 推播服務出問題時的聯絡方式

const unsigned long REPEAT_EMPTY_MS = 2UL * 60 * 1000;  // 第二階段未處理時每 2 分鐘再推播
const size_t MAX_SUBS = 10;                              // 最多綁定幾支手機

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

// 去彈跳
struct Debounced {
  int pin;
  bool stable;        // true = 偵測到液體
  bool lastRaw;
  unsigned long changedAt;
};
Debounced upper = { PIN_SENSOR_UPPER, true, true, 0 };
Debounced lower = { PIN_SENSOR_LOWER, true, true, 0 };

// 推播訊息（需在第一個函式前定義，Arduino 自動產生的宣告才找得到）
// welcome = 剛綁定的手機才收到的測試通知
struct Notice { uint8_t state; char body[160]; bool welcome; };

// 已綁定的手機（網頁伺服器與推播工作都會用到，以互斥鎖保護）
std::vector<webpush::Subscription> subs;
SemaphoreHandle_t subsLock;
String welcomeEndpoint;
Preferences prefs;
uint8_t vapidPriv[32];
bool vapidReady = false;

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

// ===== 輸出（LED / 蜂鳴器），非阻塞 =====
void updateOutputs(unsigned long now) {
  bool muted = now < mutedUntil;
  unsigned long t = now - stateSince;
  bool buzz = false;

  // Wi-Fi 燈：已連線（或熱點模式）恆亮，STA 斷線時閃爍
  bool wifiOk = WiFi.getMode() == WIFI_AP || WiFi.status() == WL_CONNECTED;
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
    else out += *s;
  }
  return out;
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

// ===== 網頁 =====
// 手機網頁內容在 index_html.h（由 phone.html 產生），配對頁在 pair_html.h

void handleRoot() { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }
void handlePair() { server.send_P(200, "text/html; charset=utf-8", PAIR_HTML); }

void handleStatus() {
  unsigned long now = millis();
  bool muted = now < mutedUntil;
  String json = "{";
  json += "\"bed\":\"" + jsonEscape(BED_NAME) + "\",";
  json += "\"state\":\"" + String(STATE_KEY[state]) + "\",";
  json += "\"text\":\"" + String(STATE_TEXT[state]) + "\",";
  json += "\"upper\":" + String(upper.stable ? "true" : "false") + ",";
  json += "\"lower\":" + String(lower.stable ? "true" : "false") + ",";
  json += "\"muted\":" + String(muted ? "true" : "false") + ",";
  json += "\"muteLeft\":" + String(muted ? (mutedUntil - now) / 1000 : 0) + ",";
  json += "\"since\":" + String((now - stateSince) / 1000) + ",";
  json += "\"push\":" + String((unsigned)subs.size()) + ",";
  json += "\"ap\":" + String(WiFi.getMode() == WIFI_AP ? "true" : "false") + ",";
  json += "\"rssi\":" + String(WiFi.getMode() == WIFI_AP ? 0 : WiFi.RSSI());
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleMute() {
  // 第二階段只允許短暫靜音，避免警報被長時間關閉
  unsigned long dur = (state == EMPTY) ? MUTE_STAGE2_MS : MUTE_STAGE1_MS;
  if (state != NORMAL) mutedUntil = millis() + dur;
  server.send(200, "text/plain", "ok");
}

void replyJson(bool ok, const char* error) {
  String j = String("{\"ok\":") + (ok ? "true" : "false") + ",\"bed\":\"" + jsonEscape(BED_NAME) + "\"";
  if (error) j += String(",\"error\":\"") + error + "\"";
  server.send(200, "application/json", j + "}");
}

void queueNotice(uint8_t st, const char* body, bool welcome = false);

void handleSubscribe() {
  String e = server.arg("endpoint"), k = server.arg("p256dh"), a = server.arg("auth");
  uint8_t buf[70];
  if (!vapidReady) return replyJson(false, "裝置尚未設定推播金鑰（請執行 make_vapid.ps1 後重新燒錄）");
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
  String payload = "{\"bed\":\"" + jsonEscape(BED_NAME) + "\",\"state\":\"" +
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
  String text = String(STATE_EMOJI[n.state]) + " " + BED_NAME + "\n" + n.body;
  String j = "{\"chat_id\":\"" + jsonEscape(TG_CHAT_ID) + "\",\"text\":\"" + jsonEscape(text.c_str()) + "\"}";
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

void announce(DripState next, DripState prev) {
  switch (next) {
    case LOW_LEVEL:
      if (prev == EMPTY) break;  // 液位不會自己變多，多半是晃動，不重複通知
      queueNotice(next, "第一階段：液量偏低，請準備更換點滴。");
      break;
    case EMPTY:
      queueNotice(next, "第二階段：點滴即將耗盡，請立即更換！");
      lastEmptyNotice = millis();
      break;
    case SENSOR_FAULT:
      queueNotice(next, "感測器讀值異常，請檢查安裝位置。");
      break;
    case NORMAL:
      if (prev == NORMAL) break;
      queueNotice(next, "液位已恢復正常（已更換或已排除）。");
      break;
  }
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("連線 Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n已連線，手機開啟 http://%s\n", WiFi.localIP().toString().c_str());
    configTime(0, 0, "pool.ntp.org", "time.google.com");  // 推播簽章需要正確時間
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("\n改用熱點 %s，手機開啟 http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
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

  upper.stable = upper.lastRaw = readLiquid(PIN_SENSOR_UPPER);
  lower.stable = lower.lastRaw = readLiquid(PIN_SENSOR_LOWER);
  state = evaluate();
  stateSince = millis();

  uint8_t keyBuf[40];
  vapidReady = webpush::b64urlDecode(VAPID_PRIVATE, keyBuf, sizeof(keyBuf)) == 32;
  if (vapidReady) memcpy(vapidPriv, keyBuf, 32);
  if (!vapidReady) Serial.println("[推播] 未設定 VAPID 金鑰，請執行 make_vapid.ps1 後重新燒錄");
  subsLock = xSemaphoreCreateMutex();
  loadSubs();

  setupWiFi();
  noticeQueue = xQueueCreate(8, sizeof(Notice));
  xTaskCreate(notifyTask, "notify", 16384, nullptr, 1, nullptr);  // 加密 + TLS 需要較大堆疊
  if (state != NORMAL) announce(state, NORMAL);  // 開機時就已偏低也要通知

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/mute", HTTP_POST, handleMute);
  server.on("/pair", handlePair);
  server.on("/subscribe", HTTP_POST, handleSubscribe);
  server.on("/unsubscribe", HTTP_POST, handleUnsubscribe);
  server.on("/pushtest", HTTP_POST, handlePushTest);
  server.begin();
}

void loop() {
  unsigned long now = millis();
  server.handleClient();

  updateDebounce(upper, now);
  updateDebounce(lower, now);

  DripState next = evaluate();
  if (next != state) {
    // 狀態升級（更嚴重）時取消靜音，確保新警報一定響
    if (next > state) mutedUntil = 0;
    Serial.printf("[狀態] %s -> %s\n", STATE_TEXT[state], STATE_TEXT[next]);
    announce(next, state);
    state = next;
    stateSince = now;
  }

  // 第二階段一直沒人處理（也沒按靜音）就重複推播
  if (state == EMPTY && now >= mutedUntil && now - lastEmptyNotice >= REPEAT_EMPTY_MS) {
    char body[160];
    snprintf(body, sizeof(body), "點滴耗盡已持續 %lu 分鐘，仍未處理！", (now - stateSince) / 60000);
    queueNotice(EMPTY, body);
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

  // Wi-Fi 斷線自動重連（STA 模式）
  static unsigned long lastCheck = 0;
  if (WiFi.getMode() == WIFI_STA && now - lastCheck > 10000) {
    lastCheck = now;
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }
}
