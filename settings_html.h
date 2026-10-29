// 裝置設定頁：床位名稱、Wi-Fi、網址、配對碼、點滴參數（存在快閃記憶體，免重新燒錄）
#pragma once
const char SETTINGS_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>裝置設定</title>
<style>
:root{--bg:#f4f6fa;--surface:#fff;--ink:#16202c;--sub:#5d6878;--line:#dfe4ea;--blue:#1565c0;--blue-soft:#e6effb;
  --ok:#17803f;--bad:#b42318;--shadow:0 1px 2px #0000000d,0 4px 16px #00000010}
@media (prefers-color-scheme:dark){:root{--bg:#0e131a;--surface:#18202a;--ink:#e9eef4;--sub:#9aa6b4;--line:#2c3644;
  --blue:#7fb2f5;--blue-soft:#1b2a3d;--ok:#5fd08f;--bad:#ff8a80;--shadow:none}}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"PingFang TC","Noto Sans TC",sans-serif;background:var(--bg);color:var(--ink);
  font-size:16px;line-height:1.5;-webkit-tap-highlight-color:transparent}
header{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:8px;padding:max(10px,env(safe-area-inset-top)) 8px 10px;
  background:color-mix(in srgb,var(--bg) 85%,transparent);-webkit-backdrop-filter:blur(14px);backdrop-filter:blur(14px)}
header a{display:inline-flex;align-items:center;gap:2px;min-height:44px;padding:0 10px;color:var(--blue);text-decoration:none;font-weight:600}
header h1{font-size:18px;margin:0}
.ic{width:22px;height:22px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
main{max-width:460px;margin:0 auto;padding:4px 16px calc(40px + env(safe-area-inset-bottom))}
h2{font-size:14px;font-weight:600;color:var(--sub);margin:22px 6px 8px}
.card{background:var(--surface);border-radius:18px;padding:4px 16px;box-shadow:var(--shadow)}
.field{padding:12px 0;border-top:1px solid var(--line)}
.field:first-child{border-top:0}
label{display:block;font-weight:600;font-size:15px;margin-bottom:6px}
input{font:inherit;font-size:17px;width:100%;min-height:48px;padding:10px 12px;border-radius:12px;border:1.5px solid var(--line);
  background:var(--bg);color:var(--ink)}
input:focus{outline:none;border-color:var(--blue);box-shadow:0 0 0 3px var(--blue-soft)}
.suffix{display:flex;align-items:center;gap:8px}
.suffix span{color:var(--sub);white-space:nowrap}
.help{font-size:13px;color:var(--sub);margin:6px 0 0}
.nets{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.nets button{font:inherit;font-size:14px;min-height:40px;padding:6px 12px;border-radius:999px;border:1.5px solid var(--line);
  background:var(--surface);color:var(--ink);cursor:pointer}
.nets button[aria-pressed=true]{border-color:var(--blue);background:var(--blue-soft);color:var(--blue)}
.btn{font:inherit;font-size:16px;font-weight:700;border:0;border-radius:14px;min-height:52px;width:100%;cursor:pointer;
  display:flex;align-items:center;justify-content:center;gap:8px}
.primary{background:var(--blue);color:#fff;margin-top:22px}
@media (prefers-color-scheme:dark){.primary{background:#2a66b8}}
.soft{background:var(--blue-soft);color:var(--blue);min-height:44px;font-size:15px;margin-top:8px}
.btn:disabled{opacity:.5}
.check{display:flex;align-items:center;gap:10px;min-height:44px;font-size:15px}
.check input{width:22px;height:22px;min-height:0}
.msg{margin:14px 4px 0;font-size:15px;font-weight:600}
.msg.ok{color:var(--ok)}.msg.err{color:var(--bad)}
.msg:empty{display:none}
[hidden]{display:none!important}
:focus-visible{outline:3px solid var(--blue);outline-offset:2px}
</style></head><body>
<header>
  <a href="/" aria-label="返回監控頁"><svg class="ic" viewBox="0 0 24 24"><path d="m15 18-6-6 6-6"/></svg>監控</a>
  <h1>裝置設定</h1>
</header>
<main>
<form id="f" novalidate>
  <h2>床位</h2>
  <div class="card">
    <div class="field">
      <label for="bed">床位名稱</label>
      <input id="bed" name="bed" maxlength="20" required placeholder="301 病房 A 床">
      <p class="help">會顯示在監控頁和推播通知上。</p>
    </div>
  </div>

  <h2>網路</h2>
  <div class="card">
    <div class="field">
      <label for="ssid">Wi-Fi 名稱</label>
      <input id="ssid" name="ssid" autocomplete="off" autocapitalize="off" spellcheck="false">
      <button class="btn soft" type="button" id="scanBtn">掃描附近的 Wi-Fi</button>
      <div class="nets" id="nets"></div>
      <p class="help">只支援 2.4GHz。需要網頁登入的學校 Wi-Fi 無法使用。</p>
    </div>
    <div class="field">
      <label for="pass">Wi-Fi 密碼</label>
      <input id="pass" name="pass" type="password" autocomplete="new-password" placeholder="不修改請留空">
    </div>
    <div class="field">
      <label for="host">裝置網址</label>
      <div class="suffix"><span>http://</span><input id="host" name="host" autocapitalize="off" spellcheck="false" maxlength="30"><span>.local</span></div>
      <p class="help" id="hostHelp">用小寫英文、數字和 -。同一個 Wi-Fi 的手機可以直接用這個網址開啟。</p>
    </div>
  </div>

  <h2>點滴參數（預估剩餘時間用）</h2>
  <div class="card">
    <div class="field">
      <label for="rate">醫囑滴速</label>
      <div class="suffix"><input id="rate" name="rate" type="number" inputmode="numeric" min="0" max="2000"><span>ml/小時</span></div>
      <p class="help" id="rateHelp">填 0 代表未設定，會改用實測滴速。</p>
    </div>
    <div class="field">
      <label for="mlBetween">上位到下位之間的液量</label>
      <div class="suffix"><input id="mlBetween" name="mlBetween" type="number" inputmode="numeric" min="1" max="2000"><span>ml</span></div>
    </div>
    <div class="field">
      <label for="mlBelow">下位以下的剩餘液量</label>
      <div class="suffix"><input id="mlBelow" name="mlBelow" type="number" inputmode="numeric" min="1" max="2000"><span>ml</span></div>
      <p class="help">可以對照點滴袋上的刻度，看兩個感測器各在幾 ml 的位置。</p>
    </div>
  </div>

  <h2>安全</h2>
  <div class="card">
    <div class="field">
      <label for="pin">新的配對碼</label>
      <input id="pin" name="pin" type="password" inputmode="numeric" maxlength="8" autocomplete="new-password" placeholder="4–8 位數字，不修改請留空">
      <p class="help">設定後，手機連結推播和修改設定時都要輸入。</p>
      <label class="check" id="clearRow" hidden><input type="checkbox" id="clearPin">取消配對碼</label>
    </div>
    <div class="field" id="authRow" hidden>
      <label for="auth">目前的配對碼</label>
      <input id="auth" name="auth" type="password" inputmode="numeric" maxlength="8" autocomplete="current-password">
      <p class="help">儲存設定前需要輸入。</p>
    </div>
  </div>

  <button class="btn primary" type="submit" id="save">儲存設定</button>
  <p class="msg" id="msg" role="status" aria-live="polite"></p>
</form>
</main>
<script>
const $ = (id) => document.getElementById(id);
let cfg = {};

async function load() {
  cfg = await (await fetch('/config', { cache: 'no-store' })).json();
  for (const k of ['bed', 'host', 'ssid', 'rate', 'mlBetween', 'mlBelow']) $(k).value = cfg[k];
  $('authRow').hidden = $('clearRow').hidden = !cfg.pinSet;
  $('rateHelp').textContent = cfg.lastRate
    ? `填 0 代表未設定，會改用實測滴速（上一袋約 ${cfg.lastRate} ml/小時）。`
    : '填 0 代表未設定，會改用實測滴速（目前還沒有實測資料）。';
  if (cfg.ap) $('hostHelp').textContent = '目前是熱點模式。設定好 Wi-Fi 後，裝置會重新開機並連上網路。';
}

async function scan() {
  const b = $('scanBtn');
  b.disabled = true; b.textContent = '掃描中…';
  for (let i = 0; i < 15; i++) {
    const d = await (await fetch('/scan', { cache: 'no-store' })).json();
    if (!d.scanning) {
      $('nets').innerHTML = '';
      d.list.sort((x, y) => y.rssi - x.rssi).forEach((n) => {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.textContent = n.ssid + (n.lock ? '' : '（未加密）');
        btn.setAttribute('aria-pressed', n.ssid === $('ssid').value);
        btn.onclick = () => {
          $('ssid').value = n.ssid;
          document.querySelectorAll('#nets button').forEach((x) => x.setAttribute('aria-pressed', x === btn));
          $('pass').focus();
        };
        $('nets').appendChild(btn);
      });
      if (!d.list.length) $('nets').textContent = '附近找不到 Wi-Fi';
      break;
    }
    await new Promise((r) => setTimeout(r, 700));
  }
  b.disabled = false; b.textContent = '重新掃描';
}

function show(ok, text) { const m = $('msg'); m.className = 'msg ' + (ok ? 'ok' : 'err'); m.textContent = text; }

$('scanBtn').onclick = scan;
$('f').onsubmit = async (e) => {
  e.preventDefault();
  const body = new URLSearchParams(new FormData($('f')));
  if ($('clearPin').checked) body.set('clearPin', '1');
  $('save').disabled = true;
  try {
    const d = await (await fetch('/config', { method: 'POST', body })).json();
    if (!d.ok) { show(false, d.error); if (d.needPin) $('auth').focus(); return; }
    $('pass').value = $('pin').value = $('auth').value = '';
    if (d.restart) {
      show(true, `已儲存。裝置正在重新開機，約 20 秒後請改用 http://${$('host').value}.local 開啟。`);
    } else {
      show(true, '已儲存。');
      load();
    }
  } catch {
    show(false, '無法連線到裝置，請再試一次。');
  } finally {
    $('save').disabled = false;
  }
};
load().catch(() => show(false, '讀取設定失敗，請重新整理。'));
</script>
</body></html>)HTML";
