/*
 * 智慧點滴注射預警系統：相機節點（AI Thinker ESP32-CAM）
 *
 * 用影像判斷點滴袋液位百分比，主控 ESP32 每秒讀取 http://iv-cam.local/level。
 *
 * 判斷方式：
 *   1. 在設定的範圍（ROI）內，計算每一列的平均亮度，得到一條「上到下的亮度曲線」
 *   2. 校正時記錄「滿」與「空」兩條曲線
 *   3. 找一條分界線，讓分界以上最像「空」、以下最像「滿」，分界位置就是液位
 *   比較時用亮度的對數差並扣掉整體偏移，自動曝光讓畫面整體變亮變暗也不影響結果。
 *
 * 建議：點滴袋後面放一張黑白橫條紋卡（條紋約 5 mm），水會放大、扭曲條紋，差異更明顯；
 *       相機與點滴袋都要固定，光線盡量穩定（可開補光燈）。
 *
 * 燒錄：開發板選「AI Thinker ESP32-CAM」，需要 USB 轉接板（ESP32-CAM-MB）。
 * 設定：把 secrets.example.h 複製成 secrets.h，填入與主控相同的 Wi-Fi。
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <math.h>
#include <algorithm>
#include "esp_camera.h"
#include "img_converters.h"
#include "secrets.h"
#include "cam_html.h"

const char* HOSTNAME = "iv-cam";  // http://iv-cam.local

// AI Thinker ESP32-CAM 腳位
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define FLASH_LED_PIN   4

const int W = 320, H = 240;              // QVGA 灰階
const float MIN_SEPARATION = 0.08f;      // 滿 / 空曲線平均差異小於 8% 就提醒調整
const unsigned long ANALYZE_MS = 500;

WebServer server(80);
Preferences prefs;

// 校正與設定（存在快閃記憶體）
int roiX0 = 120, roiX1 = 200, roiY0 = 30, roiY1 = 210;  // 量測範圍（像素）
int ledLevel = 0;                                        // 補光燈 0–255
uint8_t calFull[H], calEmpty[H];                         // 每列平均亮度
bool hasFull = false, hasEmpty = false;

// 量測結果
int level = -1;              // 液位百分比，-1 = 無法判斷
float separation = 0;        // 滿 / 空曲線的平均差異
uint8_t profile[H];          // 最新的每列亮度
unsigned long lastOk = 0;
int history[5] = { -1, -1, -1, -1, -1 };

int roiRows() { return roiY1 - roiY0; }

void loadSettings() {
  prefs.begin("cam", true);
  roiX0 = prefs.getInt("x0", roiX0); roiX1 = prefs.getInt("x1", roiX1);
  roiY0 = prefs.getInt("y0", roiY0); roiY1 = prefs.getInt("y1", roiY1);
  ledLevel = prefs.getInt("led", 0);
  hasFull = prefs.getBytes("full", calFull, sizeof(calFull)) == (size_t)roiRows();
  hasEmpty = prefs.getBytes("empty", calEmpty, sizeof(calEmpty)) == (size_t)roiRows();
  prefs.end();
}

void saveRoi() {
  prefs.begin("cam", false);
  prefs.putInt("x0", roiX0); prefs.putInt("x1", roiX1);
  prefs.putInt("y0", roiY0); prefs.putInt("y1", roiY1);
  prefs.remove("full"); prefs.remove("empty");  // 範圍改了，舊的校正失效
  prefs.end();
  hasFull = hasEmpty = false;
}

void setLed(int v) {
  ledLevel = constrain(v, 0, 255);
  ledcWrite(FLASH_LED_PIN, ledLevel);
}

bool initCamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM; c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_GRAYSCALE;
  c.frame_size = FRAMESIZE_QVGA;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  return esp_camera_init(&c) == ESP_OK;
}

// 計算 ROI 內每一列的平均亮度
bool captureProfile(uint8_t* out) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;
  bool ok = fb->width == W && fb->height == H;
  if (ok) {
    for (int y = roiY0; y < roiY1; y++) {
      const uint8_t* row = fb->buf + y * W;
      uint32_t sum = 0;
      for (int x = roiX0; x < roiX1; x++) sum += row[x];
      out[y - roiY0] = sum / (roiX1 - roiX0);
    }
    // 上下各一列平滑，減少雜訊
    int n = roiRows();
    uint8_t tmp[H];
    memcpy(tmp, out, n);
    for (int i = 1; i < n - 1; i++) out[i] = (tmp[i - 1] + 2 * tmp[i] + tmp[i + 1]) / 4;
  }
  esp_camera_fb_return(fb);
  return ok;
}

// 找分界線：上面 k 列用「空」、下面用「滿」比對，取對數差的變異數最小者（可抵消整體亮度變化）
int estimateLevel(const uint8_t* p) {
  int n = roiRows();
  static double sE[H + 1], sE2[H + 1], sF[H + 1], sF2[H + 1];
  sE[0] = sE2[0] = sF[0] = sF2[0] = 0;
  for (int i = 0; i < n; i++) {
    double lp = log(max(1, (int)p[i]));
    double dE = lp - log(max(1, (int)calEmpty[i])), dF = lp - log(max(1, (int)calFull[i]));
    sE[i + 1] = sE[i] + dE; sE2[i + 1] = sE2[i] + dE * dE;
    sF[i + 1] = sF[i] + dF; sF2[i + 1] = sF2[i] + dF * dF;
  }
  int bestK = 0;
  double best = 1e18;
  for (int k = 0; k <= n; k++) {
    double s = sE[k] + (sF[n] - sF[k]);
    double s2 = sE2[k] + (sF2[n] - sF2[k]);
    double var = s2 - s * s / n;
    if (var < best) { best = var; bestK = k; }
  }
  return (int)lround(100.0 * (n - bestK) / n);
}

float calSeparation() {
  int n = roiRows();
  double sum = 0;
  for (int i = 0; i < n; i++) sum += fabs(log(max(1, (int)calFull[i])) - log(max(1, (int)calEmpty[i])));
  return n ? sum / n : 0;
}

int median5(int* a) {
  int b[5];
  memcpy(b, a, sizeof(b));
  std::sort(b, b + 5);
  return b[2];
}

void analyze() {
  if (!captureProfile(profile)) { level = -1; return; }
  if (!hasFull || !hasEmpty) { level = -1; return; }
  memmove(history, history + 1, sizeof(int) * 4);
  history[4] = estimateLevel(profile);
  for (int& h : history) if (h < 0) h = history[4];  // 剛開機時用第一筆填滿
  level = median5(history);
  lastOk = millis();
}

// ===== 網頁 =====
void sendJson(const String& j) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json; charset=utf-8", j);
}

void handleLevel() {
  bool fresh = millis() - lastOk < 3000;
  sendJson("{\"level\":" + String(fresh ? level : -1) + ",\"ok\":" + (fresh && level >= 0 ? "true" : "false") +
           ",\"cal\":" + (hasFull && hasEmpty ? "true" : "false") + "}");
}

void handleState() {
  String j = "{\"level\":" + String(level) + ",\"full\":" + (hasFull ? "true" : "false") +
             ",\"empty\":" + (hasEmpty ? "true" : "false") + ",\"sep\":" + String(separation, 3) +
             ",\"minSep\":" + String(MIN_SEPARATION, 3) + ",\"led\":" + String(ledLevel) +
             ",\"x0\":" + roiX0 + ",\"x1\":" + roiX1 + ",\"y0\":" + roiY0 + ",\"y1\":" + roiY1 +
             ",\"w\":" + W + ",\"h\":" + H + ",\"profile\":[";
  for (int i = 0; i < roiRows(); i++) { if (i) j += ","; j += profile[i]; }
  sendJson(j + "]}");
}

void handleSnap() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { server.send(503, "text/plain", "camera error"); return; }
  uint8_t* jpg = nullptr;
  size_t len = 0;
  bool ok = frame2jpg(fb, 70, &jpg, &len);
  esp_camera_fb_return(fb);
  if (!ok) { server.send(500, "text/plain", "jpeg error"); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char*)jpg, len);
  free(jpg);
}

void handleRoi() {
  int x0 = server.arg("x0").toInt(), x1 = server.arg("x1").toInt();
  int y0 = server.arg("y0").toInt(), y1 = server.arg("y1").toInt();
  if (x0 < 0 || y0 < 0 || x1 > W || y1 > H || x1 - x0 < 8 || y1 - y0 < 20) return sendJson("{\"ok\":false}");
  roiX0 = x0; roiX1 = x1; roiY0 = y0; roiY1 = y1;
  saveRoi();
  sendJson("{\"ok\":true}");
}

void handleCal() {
  String which = server.arg("which");
  uint8_t* dst = which == "full" ? calFull : which == "empty" ? calEmpty : nullptr;
  if (!dst || !captureProfile(dst)) return sendJson("{\"ok\":false}");
  prefs.begin("cam", false);
  prefs.putBytes(which.c_str(), dst, roiRows());
  prefs.end();
  (which == "full" ? hasFull : hasEmpty) = true;
  if (hasFull && hasEmpty) separation = calSeparation();
  for (int& h : history) h = -1;
  sendJson("{\"ok\":true}");
}

void handleLed() {
  setLed(server.arg("v").toInt());
  prefs.begin("cam", false);
  prefs.putInt("led", ledLevel);
  prefs.end();
  sendJson("{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  loadSettings();
  ledcAttach(FLASH_LED_PIN, 5000, 8);
  setLed(ledLevel);

  if (!initCamera()) {
    Serial.println("[相機] 初始化失敗，請檢查排線與供電（建議 5V 2A）");
  }
  if (hasFull && hasEmpty) separation = calSeparation();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);  // 關閉省電，網頁與影像比較順
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("連線 Wi-Fi");
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) { delay(300); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) {
    MDNS.begin(HOSTNAME);
    MDNS.addService("http", "tcp", 80);
    Serial.printf("\n已連線，開啟 http://%s.local 或 http://%s\n", HOSTNAME, WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n連不上 Wi-Fi，請檢查 secrets.h");
  }

  server.on("/", []() { server.send_P(200, "text/html; charset=utf-8", CAM_HTML); });
  server.on("/level", handleLevel);
  server.on("/state", handleState);
  server.on("/snap", handleSnap);
  server.on("/roi", HTTP_POST, handleRoi);
  server.on("/cal", HTTP_POST, handleCal);
  server.on("/led", HTTP_POST, handleLed);
  server.begin();
}

void loop() {
  server.handleClient();
  static unsigned long last = 0;
  if (millis() - last >= ANALYZE_MS) {
    last = millis();
    analyze();
  }
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }
}
