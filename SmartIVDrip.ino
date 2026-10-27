/*
 * 智慧點滴注射預警系統 (Smart IV Drip Alert System)
 * 平台：ESP32 (Arduino core 2.x / 3.x)
 *
 * 功能：
 *   - 上、下兩組非接觸式紅外線液位感測器，雙階段判斷
 *   - 第一階段（液位低於上位）：黃燈 + 間歇提示音 + Wi-Fi 通報
 *   - 第二階段（液位低於下位）：紅燈 + 連續急促警報 + Wi-Fi 通報
 *   - 感測器狀態矛盾（上有液、下無液）判為感測器異常
 *   - 手機網頁即時監控（每秒更新）、遠端靜音
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
#include "index_html.h"

// ===== 使用者設定 =====
// Wi-Fi 密碼與推播金鑰放在 secrets.h（不會上傳 GitHub）
// 第一次使用：把 secrets.example.h 複製一份改名為 secrets.h 再填入
#include "secrets.h"

const char* AP_SSID   = "IV-Monitor";      // 連不上 Wi-Fi 時自建熱點
const char* BED_NAME  = "301 病房 A 床";

const unsigned long REPEAT_EMPTY_MS = 2UL * 60 * 1000;  // 第二階段未處理時每 2 分鐘再推播

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
const char* STATE_KEY[]  = { "normal", "low", "empty", "fault" };
const char* STATE_TEXT[] = { "液量充足", "第一階段：液量偏低", "第二階段：請立即更換", "感測器異常" };

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
struct Notice { char title[64]; char body[192]; uint8_t priority; };

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

// ===== 網頁 =====
// 手機網頁內容在 index_html.h（由 phone.html 產生）

void handleRoot() { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }

void handleStatus() {
  unsigned long now = millis();
  bool muted = now < mutedUntil;
  String json = "{";
  json += "\"bed\":\"" + String(BED_NAME) + "\",";
  json += "\"state\":\"" + String(STATE_KEY[state]) + "\",";
  json += "\"text\":\"" + String(STATE_TEXT[state]) + "\",";
  json += "\"upper\":" + String(upper.stable ? "true" : "false") + ",";
  json += "\"lower\":" + String(lower.stable ? "true" : "false") + ",";
  json += "\"muted\":" + String(muted ? "true" : "false") + ",";
  json += "\"muteLeft\":" + String(muted ? (mutedUntil - now) / 1000 : 0) + ",";
  json += "\"since\":" + String((now - stateSince) / 1000) + ",";
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

// ===== 推播通知 =====
// HTTPS 連線要 1–3 秒，放在獨立工作執行，蜂鳴器與網頁才不會卡住
QueueHandle_t noticeQueue;
unsigned long lastEmptyNotice = 0;

String jsonEscape(const char* s) {
  String out;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { out += '\\'; out += *s; }
    else if (*s == '\n') out += "\\n";
    else out += *s;
  }
  return out;
}

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

void sendNotice(const Notice& n) {
  if (strlen(NTFY_TOPIC)) {
    const char* tag = n.priority >= 5 ? "rotating_light" : n.priority >= 4 ? "warning" : "white_check_mark";
    String j = "{\"topic\":\"" + jsonEscape(NTFY_TOPIC) + "\",\"title\":\"" + jsonEscape(n.title) +
               "\",\"message\":\"" + jsonEscape(n.body) + "\",\"priority\":" + String(n.priority) +
               ",\"tags\":[\"" + tag + "\"]}";
    for (int i = 0; i < 2 && !postJson("ntfy", "https://ntfy.sh/", j); i++) delay(3000);
  }
  if (strlen(TG_TOKEN) && strlen(TG_CHAT_ID)) {
    String j = "{\"chat_id\":\"" + jsonEscape(TG_CHAT_ID) + "\",\"text\":\"" + jsonEscape(n.title) +
               "\\n" + jsonEscape(n.body) + "\"}";
    String url = String("https://api.telegram.org/bot") + TG_TOKEN + "/sendMessage";
    for (int i = 0; i < 2 && !postJson("Telegram", url, j); i++) delay(3000);
  }
}

void notifyTask(void*) {
  Notice n;
  for (;;) {
    if (xQueueReceive(noticeQueue, &n, portMAX_DELAY) != pdTRUE) continue;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[通知] 未連上網際網路（熱點模式或斷線），略過推播");
      continue;
    }
    sendNotice(n);
  }
}

void queueNotice(const char* title, const char* body, uint8_t priority) {
  if (!strlen(NTFY_TOPIC) && !strlen(TG_TOKEN)) return;
  Notice n;
  snprintf(n.title, sizeof(n.title), "%s", title);
  snprintf(n.body, sizeof(n.body), "%s", body);
  n.priority = priority;
  xQueueSend(noticeQueue, &n, 0);  // 佇列滿就丟棄，不阻塞主迴圈
}

void announce(DripState next, DripState prev) {
  char body[192];
  switch (next) {
    case LOW_LEVEL:
      if (prev == EMPTY) break;  // 液位不會自己變多，多半是晃動，不重複通知
      snprintf(body, sizeof(body), "%s：點滴液量偏低，請準備更換。", BED_NAME);
      queueNotice("⚠️ 點滴液量偏低", body, 4);
      break;
    case EMPTY:
      snprintf(body, sizeof(body), "%s：點滴已低於下位，請立即更換！", BED_NAME);
      queueNotice("🚨 點滴即將耗盡", body, 5);
      lastEmptyNotice = millis();
      break;
    case SENSOR_FAULT:
      snprintf(body, sizeof(body), "%s：感測器讀值異常，請檢查安裝位置。", BED_NAME);
      queueNotice("感測器異常", body, 3);
      break;
    case NORMAL:
      if (prev == NORMAL) break;
      snprintf(body, sizeof(body), "%s：液位已恢復正常（已更換或已排除）。", BED_NAME);
      queueNotice("✅ 已恢復正常", body, 2);
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

  setupWiFi();
  noticeQueue = xQueueCreate(8, sizeof(Notice));
  xTaskCreate(notifyTask, "notify", 10240, nullptr, 1, nullptr);
  if (state != NORMAL) announce(state, NORMAL);  // 開機時就已偏低也要通知

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/mute", HTTP_POST, handleMute);
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
    char body[192];
    snprintf(body, sizeof(body), "%s：點滴耗盡已持續 %lu 分鐘，仍未處理！", BED_NAME, (now - stateSince) / 60000);
    queueNotice("🚨 點滴仍未更換", body, 5);
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
