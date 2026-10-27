// 複製這個檔案並改名為 secrets.h，填入自己的設定。
// secrets.h 已列在 .gitignore，不會被上傳到 GitHub。
#pragma once

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* AP_PASS   = "change-me-123";   // 熱點模式密碼，至少 8 碼

// ===== 手機推播（留空 = 不使用，兩種可同時開）=====
// ntfy：手機裝 ntfy App，訂閱同一個主題名稱即可。主題名稱等於密碼，請取別人猜不到的
const char* NTFY_TOPIC = "";        // 例如 "iv-301a-k7m2q9"
// Telegram：BotFather 建立機器人取得 Token，chat id 可填個人或群組
const char* TG_TOKEN   = "";
const char* TG_CHAT_ID = "";
