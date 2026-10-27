# 第一次使用：產生 Android / iOS 專案檔並套用必要設定
# 在 app 資料夾執行：powershell -ExecutionPolicy Bypass -File setup.ps1
$ErrorActionPreference = 'Stop'
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $dir
$utf8 = New-Object System.Text.UTF8Encoding $false

function Patch($path, [scriptblock]$fn) {
    if (-not (Test-Path $path)) { Write-Host "找不到 $path，略過"; return }
    $s = [IO.File]::ReadAllText($path)
    $n = & $fn $s
    if ($n -ne $s) { [IO.File]::WriteAllText($path, $n, $utf8); Write-Host "已修改 $path" }
}

flutter create --org io.github.youenc120ux --project-name iv_drip_monitor --platforms android,ios .
Remove-Item -ErrorAction SilentlyContinue test\widget_test.dart

# Android：網路、通知權限、允許連線區域網路的 HTTP（ESP32 沒有 HTTPS）
Patch 'android\app\src\main\AndroidManifest.xml' {
    param($s)
    if ($s -match 'usesCleartextTraffic') { return $s }
    $perm = "    <uses-permission android:name=`"android.permission.INTERNET`"/>`n" +
            "    <uses-permission android:name=`"android.permission.POST_NOTIFICATIONS`"/>`n" +
            "    <uses-permission android:name=`"android.permission.WAKE_LOCK`"/>`n"
    $s = ([regex]'<application').Replace($s, $perm + '    <application android:usesCleartextTraffic="true"', 1)
    $s -replace 'android:label="[^"]*"', 'android:label="點滴監控"'
}

# Android：flutter_local_notifications 需要 core library desugaring
Patch 'android\app\build.gradle.kts' {
    param($s)
    if ($s -match 'desugar') { return $s }
    $s = ([regex]'compileOptions \{').Replace($s, "compileOptions {`n        isCoreLibraryDesugaringEnabled = true", 1)
    $s + "`ndependencies {`n    coreLibraryDesugaring(`"com.android.tools:desugar_jdk_libs:2.1.4`")`n}`n"
}
Patch 'android\app\build.gradle' {
    param($s)
    if ($s -match 'desugar') { return $s }
    $s = ([regex]'compileOptions \{').Replace($s, "compileOptions {`n        coreLibraryDesugaringEnabled true", 1)
    $s + "`ndependencies {`n    coreLibraryDesugaring 'com.android.tools:desugar_jdk_libs:2.1.4'`n}`n"
}

# iOS：允許連線區域網路、App 名稱
Patch 'ios\Runner\Info.plist' {
    param($s)
    if ($s -match 'NSAllowsLocalNetworking') { return $s }
    $add = "`t<key>NSAppTransportSecurity</key>`n`t<dict>`n`t`t<key>NSAllowsLocalNetworking</key>`n`t`t<true/>`n`t</dict>`n" +
           "`t<key>NSLocalNetworkUsageDescription</key>`n`t<string>連線到病房內的點滴監測裝置</string>`n"
    $s = ([regex]'</dict>\s*</plist>').Replace($s, $add + "</dict>`n</plist>", 1)
    [regex]::Replace($s, '(<key>CFBundleDisplayName</key>\s*<string>)[^<]*', '${1}點滴監控')
}

# iOS：App 開著時也顯示通知
Patch 'ios\Runner\AppDelegate.swift' {
    param($s)
    if ($s -match 'UNUserNotificationCenter') { return $s }
    $s = $s -replace 'import Flutter', "import Flutter`nimport UserNotifications"
    [regex]::Replace($s, '(\n(\s*)GeneratedPluginRegistrant\.register\([^\n]*\))',
        "`$1`n`$2UNUserNotificationCenter.current().delegate = self as? UNUserNotificationCenterDelegate")
}

flutter pub get
Write-Host "`n完成！接上手機後執行：flutter run"
