# 智慧點滴注射預警系統 Smart IV Drip Alert

以 ESP32 搭配上、下兩組非接觸式紅外線液位感測器，雙階段偵測點滴袋剩餘液量，
透過蜂鳴器、LED、手機網頁與推播通知（ntfy / Telegram）即時提醒醫護人員更換點滴。

**👉 [手機監控網頁線上展示](https://youenc120-ux.github.io/smart-iv-drip-alert/)**（展示模式，下方按鈕可切換模擬狀態）

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

### 手機推播設定

- **ntfy**（最簡單）：手機安裝 ntfy App，訂閱一個自訂主題（例如 `iv-301a-k7m2q9`），
  把同一個名稱填入 `secrets.h` 的 `NTFY_TOPIC`。主題名稱等於密碼，請取別人猜不到的。
- **Telegram**：向 @BotFather 建立機器人取得 Token，填入 `TG_TOKEN` 與 `TG_CHAT_ID`。

## 檔案說明

| 檔案 | 說明 |
|---|---|
| `SmartIVDrip.ino` | ESP32 主程式 |
| `phone.html` | 手機監控網頁原始檔（直接用瀏覽器開啟即為展示模式） |
| `index_html.h` | 由 `make_html.ps1` 自動產生，燒錄用的網頁內容 |
| `make_html.ps1` | 修改 `phone.html` 後執行，更新 `index_html.h` 與 `docs/index.html` |
| `secrets.example.h` | 個人設定範本 |
| `docs/` | GitHub Pages 線上展示 |

## 已知限制

- 兩個開關式感測器只能分辨三段液位，無法量測確切剩餘毫升數
- 手機網頁需開著才會發出聲音；鎖定螢幕請搭配 ntfy 或 Telegram 推播
- 熱點模式沒有網際網路，無法推播
- 推播使用 HTTPS 但未驗證伺服器憑證（原型簡化）

## 授權

[MIT License](LICENSE)
