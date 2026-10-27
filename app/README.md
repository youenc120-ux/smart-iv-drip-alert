# 點滴監控 App（Android / iOS）

用 Flutter 撰寫，同一套程式可產生 Android 與 iPhone App，直接連線病房內的 ESP32 監測裝置。

## 功能

- 同時監看多床，最需要處理的床位自動排在最上面
- 狀態變化時跳出系統通知（偏低、耗盡、異常、離線、恢復正常）
- 第二階段用**鬧鐘音量**連續響鈴，手機切靜音也會響；按「確認」停止並讓現場蜂鳴器靜音
- 連續 3 次連不到裝置判定離線並通知
- 展示模式：不需硬體，點床位即可切換模擬狀態
- 自動支援深色模式

## 建置

1. 安裝 [Flutter](https://docs.flutter.dev/get-started/install)
   （Android 需另裝 Android Studio；**iOS 需要 Mac + Xcode**，Windows 無法建置 iPhone 版）
2. 在 `app` 資料夾執行一次設定腳本（產生 android / ios 專案並套用權限設定）：

   ```
   powershell -ExecutionPolicy Bypass -File setup.ps1
   ```

   Mac 上可改為手動執行 `flutter create --org io.github.youenc120ux --platforms android,ios .`，
   再依 `setup.ps1` 內容修改 `Info.plist` 與 `AppDelegate.swift`。
3. 手機接上電腦（開啟 USB 偵錯），執行 `flutter run`
4. 產生安裝檔：`flutter build apk --release`，檔案在 `build/app/outputs/flutter-apk/`

## 使用

1. 手機與 ESP32 連上同一個 Wi-Fi
2. 按「新增床位」，輸入 ESP32 序列埠顯示的位址（熱點模式為 `192.168.4.1`）
3. 長按床位可移除；右上角設定可開關鈴聲、螢幕常亮、展示模式

## 限制

- App 需開著才會持續監測；長時間在背景時系統可能暫停 App（iOS 尤其嚴格），
  需要鎖定螢幕也收到警報請同時啟用裝置端的 ntfy / Telegram 推播
- 本系統為專題原型，非醫療器材
