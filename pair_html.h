// 手機配對頁：推播網頁（HTTPS）把訂閱資料放在網址 # 後面跳轉過來，
// 這頁再用同網域請求交給 ESP32 保存。# 後面的內容不會經過網路傳送。
// 語言跟隨推播 App 傳來的 l 參數（中 / 英）
#pragma once
const char PAIR_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>連結裝置</title>
<style>
:root{--bg:#eef2f7;--card:#fff;--ink:#1c2430;--sub:#5b6676;--ok:#2e9d5b;--bad:#c62828;--blue:#1565c0}
@media (prefers-color-scheme:dark){:root{--bg:#0f141b;--card:#1a212b;--ink:#e8edf3;--sub:#9aa5b3}}
body{margin:0;font-family:system-ui,-apple-system,"PingFang TC","Noto Sans TC",sans-serif;background:var(--bg);color:var(--ink);display:grid;place-items:center;min-height:100vh;padding:16px;box-sizing:border-box}
.card{background:var(--card);border-radius:20px;padding:28px 24px;max-width:360px;width:100%;text-align:center;box-shadow:0 2px 12px #0000001a}
svg{width:56px;height:56px}
h1{font-size:22px;margin:12px 0 8px}
p{color:var(--sub);line-height:1.6;margin:0 0 8px}
.btn{display:block;width:100%;margin-top:20px;padding:14px;border:0;border-radius:12px;background:var(--blue);color:#fff;text-decoration:none;
  font:inherit;font-weight:600;min-height:48px;box-sizing:border-box;cursor:pointer}
form{text-align:left;margin-top:16px}
label{display:block;font-weight:600;margin-bottom:6px}
input{font:inherit;font-size:20px;letter-spacing:4px;width:100%;min-height:52px;padding:10px 14px;border-radius:12px;border:1.5px solid #cfd6df;
  background:var(--bg);color:var(--ink);box-sizing:border-box;text-align:center}
</style></head><body>
<main class="card" role="status" aria-live="polite">
  <svg id="ic" viewBox="0 0 24 24" fill="none" stroke="var(--blue)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9" stroke-dasharray="4 3"/></svg>
  <h1 id="t">連結中…</h1>
  <p id="d">正在把這支手機加入推播名單</p>
  <form id="pinForm" hidden>
    <label for="pin" id="pinLabel">配對碼</label>
    <input id="pin" type="password" inputmode="numeric" maxlength="8" autocomplete="one-time-code" placeholder="請輸入 4–8 位數字">
    <button class="btn" type="submit" id="pinBtn">連結</button>
  </form>
  <a id="back" class="btn" hidden>返回推播 App</a>
</main>
<script>
const OK = '<circle cx="12" cy="12" r="10"/><path d="m9 12 2 2 4-4"/>';
const ERR = '<circle cx="12" cy="12" r="10"/><path d="m15 9-6 6"/><path d="m9 9 6 6"/>';
const p = new URLSearchParams(location.hash.slice(1));
history.replaceState(null, '', location.pathname);  // 清掉網址上的訂閱資料
const L = {
  zh: {
    linking: '連結中…', linkingD: '正在把這支手機加入推播名單', pin: '配對碼', pinPh: '請輸入 4–8 位數字', link: '連結', back: '返回推播 App',
    noData: '缺少配對資料', noDataD: '請從推播 App 按「連結裝置」開啟這一頁。', enterPin: '輸入配對碼', enterPinD: '這台裝置需要配對碼，請向負責的護理人員取得。',
    fail: '連結失敗', retry: '請重新操作一次。', unlinked: '已取消連結', unlinkedD: (b) => `${b} 不會再推播到這支手機。`,
    linked: '連結成功', linkedD: (b) => `${b} 的點滴警報會推播到這支手機，馬上會收到一則測試通知。`,
    noConn: '無法連線到裝置', noConnD: '請確認手機與裝置在同一個 Wi-Fi。',
  },
  en: {
    linking: 'Linking…', linkingD: 'Adding this phone to the alert list', pin: 'Pairing code', pinPh: 'Enter 4–8 digits', link: 'Link', back: 'Back to the alerts app',
    noData: 'Pairing data missing', noDataD: 'Open this page from the alerts app with "Link device".', enterPin: 'Enter pairing code', enterPinD: 'This device needs a pairing code. Ask the nurse in charge.',
    fail: 'Linking failed', retry: 'Please try again.', unlinked: 'Unlinked', unlinkedD: (b) => `${b} will no longer send alerts to this phone.`,
    linked: 'Linked', linkedD: (b) => `Alerts from ${b} will be pushed to this phone. A test alert is on its way.`,
    noConn: "Can't reach the device", noConnD: 'Make sure the phone and the device are on the same Wi-Fi.',
  },
};
const ERR_EN = { '配對碼錯誤': 'Wrong pairing code', '錯誤次數太多，請 1 分鐘後再試': 'Too many attempts. Try again in 1 minute.',
  '不支援這個瀏覽器的推播服務': "This browser's push service isn't supported", '訂閱資料格式錯誤': 'Invalid subscription data' };
let lang = p.get('l') || (() => { try { return localStorage.getItem('lang'); } catch { return null; } })() || (/^zh/i.test(navigator.language) ? 'zh' : 'en');
if (!L[lang]) lang = 'zh';
const t = (k, ...a) => { const v = L[lang][k]; return typeof v === 'function' ? v(...a) : v; };
const tErr = (e) => (lang === 'en' ? ERR_EN[e] || e : e);
document.documentElement.lang = lang === 'zh' ? 'zh-Hant' : 'en';
document.title = t('link');
document.getElementById('t').textContent = t('linking');
document.getElementById('d').textContent = t('linkingD');
document.getElementById('pinLabel').textContent = t('pin');
document.getElementById('pin').placeholder = t('pinPh');
document.getElementById('pinBtn').textContent = t('link');
document.getElementById('back').textContent = t('back');

function show(ok, title, desc) {
  const ic = document.getElementById('ic');
  ic.innerHTML = ok ? OK : ERR;
  ic.setAttribute('stroke', ok ? 'var(--ok)' : 'var(--bad)');
  document.getElementById('t').textContent = title;
  document.getElementById('d').textContent = desc;
  const r = p.get('r') || '';
  if (r.startsWith('https://')) { const b = document.getElementById('back'); b.href = r; b.hidden = false; }
}
const unsub = p.get('do') === 'unsub';
async function go(pin) {
  if (!p.get('e')) return show(false, t('noData'), t('noDataD'));
  try {
    const r = await fetch(unsub ? '/unsubscribe' : '/subscribe', {
      method: 'POST',
      body: new URLSearchParams({ endpoint: p.get('e'), p256dh: p.get('k') || '', auth: p.get('a') || '', pin: pin || '' })
    });
    const d = await r.json();
    if (d.needPin) {
      // 裝置設定了配對碼：請使用者輸入後再送一次
      const ic = document.getElementById('ic');
      ic.innerHTML = '<rect width="18" height="11" x="3" y="11" rx="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>';
      ic.setAttribute('stroke', 'var(--blue)');
      document.getElementById('t').textContent = t('enterPin');
      document.getElementById('d').textContent = pin ? tErr(d.error) : t('enterPinD');
      document.getElementById('pinForm').hidden = false;
      document.getElementById('pin').focus();
      return;
    }
    document.getElementById('pinForm').hidden = true;
    if (!d.ok) return show(false, t('fail'), d.error ? tErr(d.error) : t('retry'));
    if (unsub) show(true, t('unlinked'), t('unlinkedD', d.bed));
    else show(true, t('linked'), t('linkedD', d.bed));
  } catch (e) {
    show(false, t('noConn'), t('noConnD'));
  }
}
document.getElementById('pinForm').onsubmit = (e) => { e.preventDefault(); go(document.getElementById('pin').value.trim()); };
go('');
</script>
</body></html>)HTML";
