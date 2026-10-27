# 產生網頁推播用的 VAPID 金鑰（P-256）
#   私鑰 -> secrets.h（不上傳 GitHub）
#   公鑰 -> secrets.h 與 docs/config.js（推播網頁訂閱時使用）
# 換金鑰後，所有手機都要重新連結裝置。
# 執行：powershell -ExecutionPolicy Bypass -File make_vapid.ps1
$dir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$utf8 = New-Object System.Text.UTF8Encoding $false

function B64Url([byte[]]$b) { [Convert]::ToBase64String($b).TrimEnd('=').Replace('+', '-').Replace('/', '_') }

$ec = [System.Security.Cryptography.ECDsa]::Create([System.Security.Cryptography.ECCurve+NamedCurves]::nistP256)
$p  = $ec.ExportParameters($true)
$pub  = B64Url ([byte[]](@(4) + $p.Q.X + $p.Q.Y))
$priv = B64Url $p.D

$secrets = Join-Path $dir 'secrets.h'
if (-not (Test-Path $secrets)) { Copy-Item (Join-Path $dir 'secrets.example.h') $secrets }
$s = [IO.File]::ReadAllText($secrets)
$s = [regex]::Replace($s, '(?m)^const char\* VAPID_(PUBLIC|PRIVATE)\s*=.*\r?\n?', '')
$s = $s.TrimEnd() + "`n`n// 網頁推播金鑰（由 make_vapid.ps1 產生，私鑰請勿外流）`n" +
     "const char* VAPID_PUBLIC  = `"$pub`";`n" +
     "const char* VAPID_PRIVATE = `"$priv`";`n"
[IO.File]::WriteAllText($secrets, $s, $utf8)

New-Item -ItemType Directory -Force (Join-Path $dir 'docs') | Out-Null
[IO.File]::WriteAllText((Join-Path $dir 'docs/config.js'),
    "// 由 make_vapid.ps1 產生：網頁推播公鑰（公開資訊，可以上傳）`nwindow.VAPID_PUBLIC_KEY = '$pub';`n", $utf8)

Write-Host "已產生 VAPID 金鑰：secrets.h 與 docs/config.js"
