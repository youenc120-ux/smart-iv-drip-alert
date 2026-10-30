# 透過 Wi-Fi 更新 ESP32（網頁無線更新，不需要 USB 線）
# 用法：powershell -ExecutionPolicy Bypass -File ota.ps1 [-Device iv-monitor.local 或 IP] [-NoCompile]
# 密碼讀取 secrets.h 的 OTA_PASS；程式庫放在 C:\Users\<你>\Documents\ArduinoLibs
param(
  [string]$Device = 'iv-monitor.local',
  [switch]$NoCompile
)
$ErrorActionPreference = 'Stop'
$dir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$cli   = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
$libs  = Join-Path $env:USERPROFILE 'Documents\ArduinoLibs'  # 不用「我的文件」：它可能被 OneDrive 重新導向
$build = Join-Path $env:TEMP 'SmartIVDrip-ota'  # 放在專案外，避免被當成程式的一部分

$m = [regex]::Match([IO.File]::ReadAllText((Join-Path $dir 'secrets.h')), 'OTA_PASS\s*=\s*"([^"]+)"')
if (-not $m.Success) { throw 'secrets.h 裡找不到 OTA_PASS' }
$pass = $m.Groups[1].Value

if (-not $NoCompile) {
  Write-Host '編譯中…'
  & $cli compile --fqbn esp32:esp32:esp32 --libraries $libs --build-path $build $dir
  if ($LASTEXITCODE) { throw '編譯失敗' }
}

# Windows 有時解析不到 .local，先試著換成 IP
$target = $Device
if ($Device -like '*.local') {
  try { $target = ([Net.Dns]::GetHostAddresses($Device) | Where-Object AddressFamily -eq 'InterNetwork' | Select-Object -First 1).IPAddressToString }
  catch { throw "找不到 $Device，請改用 -Device <IP>（IP 顯示在監控頁設定或序列埠）" }
}

Write-Host "上傳到 $target …（約 10–20 秒，期間燈條會熄滅）"
# 更新模組會檢查 Origin 標頭（防跨站請求），需與裝置位址一致
$ErrorActionPreference = 'Continue'  # curl 的錯誤訊息寫在 stderr，不要讓 PowerShell 當成例外
for ($try = 1; $try -le 2; $try++) {
  $out = & curl.exe -s -S -o NUL -w '%{http_code}' --connect-timeout 10 --max-time 180 -H 'Expect:' -H "Origin: http://$target" `
    -u "admin:$pass" -F "firmware=@$build\SmartIVDrip.ino.bin;type=application/octet-stream" "http://$target/update" 2>&1
  $code = ($out | Where-Object { $_ -is [string] -and $_ -match '^\d{3}$' } | Select-Object -Last 1)
  if ($code -eq '200') { Write-Host '更新成功，裝置正在重新開機（約 15 秒後恢復）。'; exit 0 }
  if ($code -eq '401') { Write-Host '密碼錯誤：請確認 secrets.h 的 OTA_PASS 與板子上的程式一致'; exit 1 }
  Write-Host "第 $try 次上傳失敗：$(($out | Out-String).Trim())"
  if ($try -lt 2) { Write-Host '10 秒後重試…'; Start-Sleep 10 }
}
Write-Host '更新失敗。請確認裝置有通電、和電腦在同一個 Wi-Fi，或改用瀏覽器開 http://<裝置>/update 上傳。'
exit 1