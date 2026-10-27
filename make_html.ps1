# 把 phone.html 轉成 index_html.h，讓 ESP32 燒錄時一起帶進去，
# 並複製到 docs/device.html 給 GitHub Pages 線上展示用（docs/index.html 是推播 App，不要覆蓋）。
# 修改 phone.html 後執行一次：powershell -ExecutionPolicy Bypass -File make_html.ps1
$dir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$html = Get-Content -Raw -Encoding UTF8 (Join-Path $dir 'phone.html')
$utf8 = New-Object System.Text.UTF8Encoding $false
$out  = "// 由 make_html.ps1 自動產生，請改 phone.html，不要直接改這個檔案`r`n" +
        "#pragma once`r`n" +
        "const char INDEX_HTML[] PROGMEM = R`"HTML(" + $html + ")HTML`";`r`n"
[System.IO.File]::WriteAllText((Join-Path $dir 'index_html.h'), $out, $utf8)

$docs = Join-Path $dir 'docs'
New-Item -ItemType Directory -Force $docs | Out-Null
[System.IO.File]::WriteAllText((Join-Path $docs 'device.html'), $html, $utf8)
Write-Host "已產生 index_html.h 與 docs/device.html"
