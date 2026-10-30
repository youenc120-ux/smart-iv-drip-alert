// 裝置設定頁：床位名稱、Wi-Fi、網址、配對碼、點滴參數（存在快閃記憶體，免重新燒錄）
// 中英文切換：與監控頁共用瀏覽器裡的 lang 設定
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
header{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:8px;padding:max(10px,env(safe-area-inset-top)) 12px 10px 8px;
  background:color-mix(in srgb,var(--bg) 85%,transparent);-webkit-backdrop-filter:blur(14px);backdrop-filter:blur(14px)}
header a{display:inline-flex;align-items:center;gap:2px;min-height:44px;padding:0 10px;color:var(--blue);text-decoration:none;font-weight:600}
header h1{font-size:18px;margin:0;flex:1}
.lang{font:inherit;font-size:14px;font-weight:700;width:44px;height:44px;border:0;border-radius:50%;background:var(--surface);
  box-shadow:var(--shadow);color:var(--blue);cursor:pointer}
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
  <a href="/"><svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="m15 18-6-6 6-6"/></svg><span data-i="back">監控</span></a>
  <h1 data-i="title">裝置設定</h1>
  <button class="lang" id="langBtn" type="button" aria-label="切換語言 Switch language">EN</button>
</header>
<main>
<form id="f" novalidate>
  <h2 data-i="secBed">床位</h2>
  <div class="card">
    <div class="field">
      <label for="bed" data-i="bed">床位名稱</label>
      <input id="bed" name="bed" maxlength="20" required data-ph="bedPh" placeholder="301 病房 A 床">
      <p class="help" data-i="bedHelp">會顯示在監控頁和推播通知上。</p>
    </div>
  </div>

  <h2 data-i="secNet">網路</h2>
  <div class="card">
    <div class="field">
      <label for="ssid" data-i="ssid">Wi-Fi 名稱</label>
      <input id="ssid" name="ssid" autocomplete="off" autocapitalize="off" spellcheck="false">
      <button class="btn soft" type="button" id="scanBtn" data-i="scan">掃描附近的 Wi-Fi</button>
      <div class="nets" id="nets"></div>
      <p class="help" data-i="ssidHelp">只支援 2.4GHz。需要網頁登入的學校 Wi-Fi 無法使用。</p>
    </div>
    <div class="field">
      <label for="pass" data-i="pass">Wi-Fi 密碼</label>
      <input id="pass" name="pass" type="password" autocomplete="new-password" data-ph="keepPh" placeholder="不修改請留空">
    </div>
    <div class="field">
      <label for="host" data-i="host">裝置網址</label>
      <div class="suffix"><span>http://</span><input id="host" name="host" autocapitalize="off" spellcheck="false" maxlength="30"><span>.local</span></div>
      <p class="help" id="hostHelp" data-i="hostHelp">用小寫英文、數字和 -。同一個 Wi-Fi 的手機可以直接用這個網址開啟。</p>
    </div>
  </div>

  <h2 data-i="secIv">點滴參數（預估剩餘時間用）</h2>
  <div class="card">
    <div class="field">
      <label for="rate" data-i="rate">醫囑滴速</label>
      <div class="suffix"><input id="rate" name="rate" type="number" inputmode="numeric" min="0" max="2000"><span data-i="mlh">ml/小時</span></div>
      <p class="help" id="rateHelp"></p>
    </div>
    <div class="field">
      <label for="mlBetween" data-i="between">上位到下位之間的液量</label>
      <div class="suffix"><input id="mlBetween" name="mlBetween" type="number" inputmode="numeric" min="1" max="2000"><span>ml</span></div>
    </div>
    <div class="field">
      <label for="mlBelow" data-i="below">下位以下的剩餘液量</label>
      <div class="suffix"><input id="mlBelow" name="mlBelow" type="number" inputmode="numeric" min="1" max="2000"><span>ml</span></div>
      <p class="help" data-i="volHelp">可以對照點滴袋上的刻度，看兩個感測器各在幾 ml 的位置。</p>
    </div>
  </div>

  <h2 data-i="secSec">安全</h2>
  <div class="card">
    <div class="field">
      <label for="pin" data-i="pin">新的配對碼</label>
      <input id="pin" name="pin" type="password" inputmode="numeric" maxlength="8" autocomplete="new-password" data-ph="pinPh" placeholder="4–8 位數字，不修改請留空">
      <p class="help" data-i="pinHelp">設定後，手機連結推播和修改設定時都要輸入。</p>
      <label class="check" id="clearRow" hidden><input type="checkbox" id="clearPin"><span data-i="clearPin">取消配對碼</span></label>
    </div>
    <div class="field" id="authRow" hidden>
      <label for="auth" data-i="auth">目前的配對碼</label>
      <input id="auth" name="auth" type="password" inputmode="numeric" maxlength="8" autocomplete="current-password">
      <p class="help" data-i="authHelp">儲存設定前需要輸入。</p>
    </div>
  </div>

  <button class="btn primary" type="submit" id="save" data-i="save">儲存設定</button>
  <p class="msg" id="msg" role="status" aria-live="polite"></p>
</form>
</main>
<script>
const $ = (id) => document.getElementById(id);
const L = {
  zh: {
    back: '監控', title: '裝置設定', secBed: '床位', bed: '床位名稱', bedPh: '301 病房 A 床', bedHelp: '會顯示在監控頁和推播通知上。',
    secNet: '網路', ssid: 'Wi-Fi 名稱', scan: '掃描附近的 Wi-Fi', rescan: '重新掃描', scanning: '掃描中…', noNets: '附近找不到 Wi-Fi',
    open: '（未加密）', ssidHelp: '只支援 2.4GHz。需要網頁登入的學校 Wi-Fi 無法使用。', pass: 'Wi-Fi 密碼', keepPh: '不修改請留空',
    host: '裝置網址', hostHelp: '用小寫英文、數字和 -。同一個 Wi-Fi 的手機可以直接用這個網址開啟。',
    hostAp: '目前是熱點模式。設定好 Wi-Fi 後，裝置會重新開機並連上網路。',
    secIv: '點滴參數（預估剩餘時間用）', rate: '醫囑滴速', mlh: 'ml/小時',
    rateHelp: (r) => r ? `填 0 代表未設定，會改用實測滴速（上一袋約 ${r} ml/小時）。` : '填 0 代表未設定，會改用實測滴速（目前還沒有實測資料）。',
    between: '上位到下位之間的液量', below: '下位以下的剩餘液量', volHelp: '可以對照點滴袋上的刻度，看兩個感測器各在幾 ml 的位置。',
    secSec: '安全', pin: '新的配對碼', pinPh: '4–8 位數字，不修改請留空', pinHelp: '設定後，手機連結推播和修改設定時都要輸入。',
    clearPin: '取消配對碼', auth: '目前的配對碼', authHelp: '儲存設定前需要輸入。', save: '儲存設定',
    saved: '已儲存。', savedRestart: (h) => `已儲存。裝置正在重新開機，約 20 秒後請改用 http://${h}.local 開啟。`,
    netErr: '無法連線到裝置，請再試一次。', loadErr: '讀取設定失敗，請重新整理。',
  },
  en: {
    back: 'Monitor', title: 'Device settings', secBed: 'Bed', bed: 'Bed name', bedPh: 'Room 301 Bed A', bedHelp: 'Shown on the monitor page and in alerts.',
    secNet: 'Network', ssid: 'Wi-Fi name', scan: 'Scan for Wi-Fi', rescan: 'Scan again', scanning: 'Scanning…', noNets: 'No Wi-Fi found nearby',
    open: ' (open)', ssidHelp: '2.4 GHz only. School Wi-Fi that needs a web login will not work.', pass: 'Wi-Fi password', keepPh: 'Leave blank to keep',
    host: 'Device address', hostHelp: 'Lowercase letters, digits and -. Phones on the same Wi-Fi can open the device at this address.',
    hostAp: 'Hotspot mode. After you set up Wi-Fi, the device restarts and joins the network.',
    secIv: 'IV settings (for time-left estimate)', rate: 'Prescribed rate', mlh: 'ml/h',
    rateHelp: (r) => r ? `0 = not set; the measured rate is used instead (last bag ~${r} ml/h).` : '0 = not set; the measured rate is used instead (no measurement yet).',
    between: 'Volume between upper and lower sensor', below: 'Volume left below the lower sensor', volHelp: 'Use the scale on the IV bag to see how many ml each sensor sits at.',
    secSec: 'Security', pin: 'New pairing code', pinPh: '4–8 digits, leave blank to keep', pinHelp: 'Required to link phones for alerts and to change settings.',
    clearPin: 'Remove pairing code', auth: 'Current pairing code', authHelp: 'Required before saving.', save: 'Save settings',
    saved: 'Saved.', savedRestart: (h) => `Saved. The device is restarting; in about 20 s open http://${h}.local`,
    netErr: "Can't reach the device. Please try again.", loadErr: 'Could not load settings. Please refresh.',
  },
};
// 裝置回傳的中文錯誤訊息
const ERR_EN = {
  '需要配對碼': 'Pairing code required', '配對碼錯誤': 'Wrong pairing code', '錯誤次數太多，請 1 分鐘後再試': 'Too many attempts. Try again in 1 minute.',
  '床位名稱請輸入 1–20 個字': 'Bed name must be 1–20 characters', '裝置網址只能用小寫英文、數字和 -': 'Address may only use lowercase letters, digits and -',
  'Wi-Fi 名稱太長': 'Wi-Fi name is too long', '配對碼請設 4–8 位數字': 'Pairing code must be 4–8 digits', '配對碼只能用數字': 'Pairing code must be digits only',
  '液量請輸入 1–2000 ml': 'Volume must be 1–2000 ml', '滴速請輸入 0–2000 ml/小時': 'Rate must be 0–2000 ml/h',
};
let lang = (() => { try { return localStorage.getItem('lang'); } catch { return null; } })() || (/^zh/i.test(navigator.language) ? 'zh' : 'en');
function t(k, ...a) { const v = L[lang][k] ?? L.zh[k] ?? k; return typeof v === 'function' ? v(...a) : v; }
function tErr(e) { return lang === 'en' ? ERR_EN[e] || e : e; }
let cfg = {}, scanned = false, lastMsg = null;

function applyLang() {
  document.documentElement.lang = lang === 'zh' ? 'zh-Hant' : 'en';
  document.title = t('title');
  document.querySelectorAll('[data-i]').forEach((e) => { e.textContent = t(e.dataset.i); });
  document.querySelectorAll('[data-ph]').forEach((e) => { e.placeholder = t(e.dataset.ph); });
  $('langBtn').textContent = lang === 'zh' ? 'EN' : '中';
  $('rateHelp').textContent = t('rateHelp', cfg.lastRate);
  if (cfg.ap) $('hostHelp').textContent = t('hostAp');
  if (scanned) $('scanBtn').textContent = t('rescan');
  document.querySelectorAll('#nets button').forEach((b) => { b.textContent = b.dataset.ssid + (b.dataset.lock === '1' ? '' : t('open')); });
  if (lastMsg) show(...lastMsg);
}
$('langBtn').addEventListener('click', () => {
  lang = lang === 'zh' ? 'en' : 'zh';
  try { localStorage.setItem('lang', lang); } catch {}
  applyLang();
});

async function load() {
  cfg = await (await fetch('/config', { cache: 'no-store' })).json();
  for (const k of ['bed', 'host', 'ssid', 'rate', 'mlBetween', 'mlBelow']) $(k).value = cfg[k];
  $('authRow').hidden = $('clearRow').hidden = !cfg.pinSet;
  applyLang();
}

async function scan() {
  const b = $('scanBtn');
  b.disabled = true; b.textContent = t('scanning');
  for (let i = 0; i < 15; i++) {
    const d = await (await fetch('/scan', { cache: 'no-store' })).json();
    if (!d.scanning) {
      $('nets').innerHTML = '';
      d.list.sort((x, y) => y.rssi - x.rssi).forEach((n) => {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.dataset.ssid = n.ssid;
        btn.dataset.lock = n.lock ? '1' : '0';
        btn.textContent = n.ssid + (n.lock ? '' : t('open'));
        btn.setAttribute('aria-pressed', n.ssid === $('ssid').value);
        btn.onclick = () => {
          $('ssid').value = n.ssid;
          document.querySelectorAll('#nets button').forEach((x) => x.setAttribute('aria-pressed', x === btn));
          $('pass').focus();
        };
        $('nets').appendChild(btn);
      });
      if (!d.list.length) $('nets').textContent = t('noNets');
      break;
    }
    await new Promise((r) => setTimeout(r, 700));
  }
  scanned = true;
  b.disabled = false; b.textContent = t('rescan');
}

// msg 為文字鍵或裝置錯誤訊息；記住以便切換語言時重新顯示
function show(ok, key, arg, raw) {
  lastMsg = [ok, key, arg, raw];
  const m = $('msg');
  m.className = 'msg ' + (ok ? 'ok' : 'err');
  m.textContent = raw ? tErr(raw) : t(key, arg);
}

$('scanBtn').onclick = scan;
$('f').onsubmit = async (e) => {
  e.preventDefault();
  const body = new URLSearchParams(new FormData($('f')));
  if ($('clearPin').checked) body.set('clearPin', '1');
  $('save').disabled = true;
  try {
    const d = await (await fetch('/config', { method: 'POST', body })).json();
    if (!d.ok) { show(false, null, null, d.error); if (d.needPin) $('auth').focus(); return; }
    $('pass').value = $('pin').value = $('auth').value = '';
    if (d.restart) show(true, 'savedRestart', $('host').value);
    else { show(true, 'saved'); load(); }
  } catch {
    show(false, 'netErr');
  } finally {
    $('save').disabled = false;
  }
};
applyLang();
load().catch(() => show(false, 'loadErr'));
</script>
</body></html>)HTML";
