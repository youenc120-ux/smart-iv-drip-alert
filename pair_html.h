// 手機配對頁：推播網頁（HTTPS）把訂閱資料放在網址 # 後面跳轉過來，
// 這頁再用同網域請求交給 ESP32 保存。# 後面的內容不會經過網路傳送。
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
a.btn{display:block;margin-top:20px;padding:14px;border-radius:12px;background:var(--blue);color:#fff;text-decoration:none;font-weight:600;min-height:44px;box-sizing:border-box}
</style></head><body>
<main class="card" role="status" aria-live="polite">
  <svg id="ic" viewBox="0 0 24 24" fill="none" stroke="var(--blue)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9" stroke-dasharray="4 3"/></svg>
  <h1 id="t">連結中…</h1>
  <p id="d">正在把這支手機加入推播名單</p>
  <a id="back" class="btn" hidden>返回推播 App</a>
</main>
<script>
const OK = '<circle cx="12" cy="12" r="10"/><path d="m9 12 2 2 4-4"/>';
const ERR = '<circle cx="12" cy="12" r="10"/><path d="m15 9-6 6"/><path d="m9 9 6 6"/>';
const p = new URLSearchParams(location.hash.slice(1));
history.replaceState(null, '', location.pathname);  // 清掉網址上的訂閱資料
function show(ok, title, desc) {
  const ic = document.getElementById('ic');
  ic.innerHTML = ok ? OK : ERR;
  ic.setAttribute('stroke', ok ? 'var(--ok)' : 'var(--bad)');
  document.getElementById('t').textContent = title;
  document.getElementById('d').textContent = desc;
  const r = p.get('r') || '';
  if (r.startsWith('https://')) { const b = document.getElementById('back'); b.href = r; b.hidden = false; }
}
(async () => {
  const unsub = p.get('do') === 'unsub';
  if (!p.get('e')) return show(false, '缺少配對資料', '請從推播 App 按「連結裝置」開啟這一頁。');
  try {
    const r = await fetch(unsub ? '/unsubscribe' : '/subscribe', {
      method: 'POST',
      body: new URLSearchParams({ endpoint: p.get('e'), p256dh: p.get('k') || '', auth: p.get('a') || '' })
    });
    const d = await r.json();
    if (!d.ok) return show(false, '連結失敗', d.error || '請重新操作一次。');
    if (unsub) show(true, '已取消連結', d.bed + ' 不會再推播到這支手機。');
    else show(true, '連結成功', d.bed + ' 的點滴警報會推播到這支手機，馬上會收到一則測試通知。');
  } catch (e) {
    show(false, '無法連線到裝置', '請確認手機與裝置在同一個 Wi-Fi。');
  }
})();
</script>
</body></html>)HTML";
