// 複製這個檔案並改名為 secrets.h，填入自己的設定。
// secrets.h 已列在 .gitignore，不會被上傳到 GitHub。
#pragma once

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* AP_PASS   = "change-me-123";   // 熱點模式密碼，至少 8 碼

// Telegram（選用，留空 = 不使用）：BotFather 建立機器人取得 Token，chat id 可填個人或群組
const char* TG_TOKEN   = "";
const char* TG_CHAT_ID = "";

// 無線更新（OTA）密碼：從 Arduino IDE 透過 Wi-Fi 燒錄時要輸入，請改成自己的
const char* OTA_PASS = "change-me-ota";

// 網頁推播金鑰：執行 make_vapid.ps1 會自動產生並寫在下面
const char* VAPID_PUBLIC  = "";
const char* VAPID_PRIVATE = "";
