# 智慧點滴注射預警系統 Smart IV Drip Alert

以 ESP32 搭配上、下兩組非接觸式紅外線液位感測器，雙階段偵測點滴袋剩餘液量，
透過蜂鳴器、LED、手機網頁與**網頁推播**即時提醒醫護人員更換點滴。
推播由 ESP32 直接加密送到 Apple / Google 的推播服務，不經過任何第三方通知平台。

- **[點滴警報推播 App](https://youenc120-ux.github.io/smart-iv-drip-alert/)**：加入 iPhone 主畫面即可接收推播（[展示畫面](https://youenc120-ux.github.io/smart-iv-drip-alert/?demo)）
- **[裝置監控頁展示](https://youenc120-ux.github.io/smart-iv-drip-alert/device.html)**：ESP32 內建的即時監控頁面（展示模式）

> ⚠️ 本專案為學生專題原型，僅供教學與研究，**不是醫療器材**，未經任何醫療認證，請勿用於實際病患照護。測試請使用清水。

## 功能

| 狀態 | 判斷條件 | 現場警示 | 手機 |
|---|---|---|---|
| 液量充足 | 上、下位都偵測到液體 | 無 | 綠色 |
| 第一階段：液量偏低 | 上位無液、下位有液 | 響一聲，之後每 30 秒提醒；警示燈恆亮 | 黃色 + 推播 |
| 第二階段：請立即更換 | 上、下位都無液 | 每秒一聲連續警報；警示燈快閃 | 紅色 + 推播，未處理每 2 分鐘再推播 |
| 感測器異常 | 上位有液、下位無液（物理上不可能） | 短提示音；警示燈慢閃 | 紫色 + 推播 |

- 液位需穩定 1.5 秒才判定，濾除液面晃動與氣泡
- 手機網頁每秒更新，可遠端靜音（第二階段最多靜音 1 分鐘，狀態惡化時自動解除）
- 連不上 Wi-Fi 時自動開啟熱點 `IV-Monitor`，手機連線後開 `http://192.168.4.1`
- 推播在背景工作執行，網路延遲不會影響警報

## 硬體

| 零件 | 數量 |
|---|---|
| ESP32-WROOM-32 開發板（DevKit） | 1 |
| TCRT5000 紅外線反射感測模組 | 2 |
| 5V 主動式蜂鳴器 + 2N2222 電晶體 + 1kΩ 電阻 | 1 組 |
| LED（綠 Power、藍 Wi-Fi、紅 Alert）+ 330Ω 電阻 | 3 組 |

### 接線

```
TCRT5000（上位）  VCC→3V3   GND→GND   DO→GPIO32
TCRT5000（下位）  VCC→3V3   GND→GND   DO→GPIO33

蜂鳴器：  5V → 蜂鳴器(+)，蜂鳴器(−) → 2N2222 集極(C)
          GPIO25 → 1kΩ → 2N2222 基極(B)，射極(E) → GND

Wi-Fi LED：  GPIO26 → 330Ω → LED → GND
Alert LED：  GPIO27 → 330Ω → LED → GND
Power LED：  3V3    → 330Ω → LED → GND
```

## 使用方式

1. 安裝 [Arduino IDE](https://www.arduino.cc/en/software)，於開發板管理員安裝 **esp32 by Espressif**（3.x 版）
2. 下載本專案，把 `secrets.example.h` 複製一份改名為 `secrets.h`，填入 Wi-Fi 與推播設定
3. 開啟 `SmartIVDrip.ino`，開發板選 **ESP32 Dev Module**，上傳
4. 打開序列埠監控視窗（115200），依顯示的網址用手機開啟監控網頁
5. 校正：序列埠每秒顯示感測器電位，對照「有液 / 無液」調整 TCRT5000 上的可調電阻，
   並設定程式中的 `LIQUID_LEVEL`

### 手機推播設定（網頁推播）

1. 第一次使用先執行 `make_vapid.ps1` 產生推播金鑰（私鑰寫入 `secrets.h`，公鑰寫入 `docs/config.js`），再燒錄
   > 如果你 fork 這個專案，需要把 `docs/config.js` 一起推上自己的 GitHub Pages
2. iPhone（iOS 16.4 以上）用 Safari 開啟 [推播 App](https://youenc120-ux.github.io/smart-iv-drip-alert/)，
   「分享 → 加入主畫面」，再從主畫面開啟（Android 用 Chrome 直接開啟即可）
3. 按「開啟推播」並允許通知
4. 手機與 ESP32 連上同一個 Wi-Fi，輸入裝置位址按「連結」，會跳到裝置頁面完成綁定並收到一則測試通知

運作方式：推播 App（HTTPS）向瀏覽器取得推播訂閱 → 跳轉到 ESP32 的 `/pair` 頁面交給裝置保存 →
液位變化時 ESP32 依 RFC 8291 加密內容、以 VAPID（RFC 8292）簽章，直接送到手機的推播服務。
每台裝置最多綁定 10 支手機，資料存在快閃記憶體，斷電不會消失。

- **Telegram**（選用）：向 @BotFather 建立機器人取得 Token，填入 `secrets.h` 的 `TG_TOKEN` 與 `TG_CHAT_ID`
## 手機 App

[`app/`](app/) 資料夾是用 Flutter 撰寫的 Android / iOS 雙平台 App：多床同時監看、系統通知、
第二階段鬧鐘音量響鈴、一鍵確認靜音。建置方式見 [app/README.md](app/README.md)。

## 檔案說明

| 檔案 | 說明 |
|---|---|
| `SmartIVDrip.ino` | ESP32 主程式 |
| `phone.html` | 手機監控網頁原始檔（直接用瀏覽器開啟即為展示模式） |
| `index_html.h` | 由 `make_html.ps1` 自動產生，燒錄用的網頁內容 |
| `webpush.h` | 網頁推播：加密（RFC 8291）與 VAPID 簽章（RFC 8292） |
| `pair_html.h` | 手機綁定推播用的配對頁 |
| `make_vapid.ps1` | 產生推播金鑰 |
| `make_html.ps1` | 修改 `phone.html` 後執行，更新 `index_html.h` 與 `docs/device.html` |
| `secrets.example.h` | 個人設定範本 |
| `docs/` | GitHub Pages：推播 App（`index.html`、`sw.js`）與裝置頁展示 |

## 已知限制

- 兩個開關式感測器只能分辨三段液位，無法量測確切剩餘毫升數
- 裝置監控頁需開著才會發出聲音；鎖定螢幕請使用推播 App
- 熱點模式沒有網際網路，無法推播
- 推播 App 只顯示收到的推播，無法得知裝置是否斷線（裝置監控頁可以）
- 任何連上同一個 Wi-Fi 的人都能綁定裝置（原型簡化）
- 推播使用 HTTPS 但未驗證伺服器憑證（原型簡化）

## 授權

[MIT License](LICENSE)
