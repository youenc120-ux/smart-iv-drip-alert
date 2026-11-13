// 相機節點的校正頁：即時影像、拖曳設定量測範圍、記錄滿 / 空、補光燈
#pragma once
const char CAM_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>點滴相機</title>
<style>
:root{--bg:#f4f6fa;--surface:#fff;--ink:#16202c;--sub:#5d6878;--line:#dfe4ea;--blue:#1565c0;--blue-soft:#e6effb;
  --ok:#17803f;--bad:#b42318;--shadow:0 1px 2px #0000000d,0 4px 16px #00000010}
@media (prefers-color-scheme:dark){:root{--bg:#0e131a;--surface:#18202a;--ink:#e9eef4;--sub:#9aa6b4;--line:#2c3644;
  --blue:#7fb2f5;--blue-soft:#1b2a3d;--ok:#5fd08f;--bad:#ff8a80;--shadow:none}}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"PingFang TC","Noto Sans TC",sans-serif;background:var(--bg);color:var(--ink);font-size:16px;line-height:1.5}
header{display:flex;align-items:center;gap:8px;padding:max(12px,env(safe-area-inset-top)) 16px 8px}
header h1{font-size:19px;margin:0;flex:1}
.lang{font:inherit;font-size:14px;font-weight:700;width:44px;height:44px;border:0;border-radius:50%;background:var(--surface);box-shadow:var(--shadow);color:var(--blue);cursor:pointer}
main{max-width:520px;margin:0 auto;padding:0 16px calc(32px + env(safe-area-inset-bottom))}
.card{background:var(--surface);border-radius:18px;padding:16px;margin-bottom:12px;box-shadow:var(--shadow)}
h2{font-size:15px;color:var(--sub);margin:0 0 10px;font-weight:600}
.view{position:relative;width:100%;aspect-ratio:4/3;background:#000;border-radius:12px;overflow:hidden;touch-action:none}
.view img,.view canvas{position:absolute;inset:0;width:100%;height:100%}
.big{display:flex;align-items:baseline;gap:8px}
.big b{font-size:44px;line-height:1.1;font-variant-numeric:tabular-nums}
.sub{color:var(--sub);font-size:14px;margin:4px 0 0}
.btn{font:inherit;font-size:15px;font-weight:700;border:0;border-radius:12px;min-height:48px;width:100%;cursor:pointer;background:var(--blue-soft);color:var(--blue)}
.btn:disabled{opacity:.5}
.two{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.status{font-size:14px;font-weight:600;margin:10px 0 0}
.ok{color:var(--ok)}.bad{color:var(--bad)}
input[type=range]{width:100%;height:44px}
ol{padding-left:20px;margin:0;color:var(--sub);font-size:14px}
ol li{margin:4px 0}
</style></head><body>
<header><h1 data-i="title">點滴相機</h1><button class="lang" id="langBtn" type="button">EN</button></header>
<main>
  <section class="card">
    <div class="big"><b id="level">—</b><span id="levelSub" class="sub"></span></div>
    <p class="status" id="calStatus" role="status" aria-live="polite"></p>
  </section>

  <section class="card">
    <h2 data-i="viewTitle">畫面與量測範圍</h2>
    <div class="view" id="view"><img id="snap" alt=""><canvas id="cv" width="320" height="240"></canvas></div>
    <p class="sub" data-i="dragHelp">在畫面上拖曳，框出點滴袋要量測的範圍（從袋子上方到下方），再按「儲存範圍」。</p>
    <div class="two"><button class="btn" id="saveRoi" type="button" data-i="saveRoi" disabled>儲存範圍</button>
      <button class="btn" id="resetRoi" type="button" data-i="resetRoi" disabled>取消</button></div>
  </section>

  <section class="card">
    <h2 data-i="calTitle">校正</h2>
    <ol><li data-i="cal1">點滴袋裝滿水，按「記錄：滿」。</li><li data-i="cal2">把水倒掉（或換空袋子），按「記錄：空」。</li>
      <li data-i="cal3">袋子後面放黑白橫條紋卡、固定相機與光線，判斷會更準。</li></ol>
    <div class="two"><button class="btn" id="calFull" type="button" data-i="calFull">記錄：滿</button>
      <button class="btn" id="calEmpty" type="button" data-i="calEmpty">記錄：空</button></div>
  </section>

  <section class="card">
    <h2 data-i="ledTitle">補光燈</h2>
    <input type="range" id="led" min="0" max="255" step="5" aria-labelledby="ledLabel">
    <p class="sub" id="ledLabel" data-i="ledHelp">光線不穩定時打開，讓每次拍到的亮度一致。</p>
  </section>
</main>
<script>
const $ = (id) => document.getElementById(id);
const L = {
  zh: { title: '點滴相機', viewTitle: '畫面與量測範圍', dragHelp: '在畫面上拖曳，框出點滴袋要量測的範圍（從袋子上方到下方），再按「儲存範圍」。',
    saveRoi: '儲存範圍', resetRoi: '取消', calTitle: '校正', cal1: '點滴袋裝滿水，按「記錄：滿」。', cal2: '把水倒掉（或換空袋子），按「記錄：空」。',
    cal3: '袋子後面放黑白橫條紋卡、固定相機與光線，判斷會更準。', calFull: '記錄：滿', calEmpty: '記錄：空', ledTitle: '補光燈',
    ledHelp: '光線不穩定時打開，讓每次拍到的亮度一致。', levelSub: '液位', needCal: (f, e) => `還需要記錄：${[f ? '' : '滿', e ? '' : '空'].filter(Boolean).join('、')}`,
    sepOk: (s) => `校正完成，滿／空差異 ${s}%，可以判斷。`, sepWeak: (s, m) => `滿／空差異只有 ${s}%（建議至少 ${m}%）：放條紋卡、調整角度或補光後重新校正。`,
    saved: '已儲存。', roiSaved: '範圍已儲存，請重新校正滿／空。', offline: '連不到相機' },
  en: { title: 'IV Camera', viewTitle: 'Image and measuring area', dragHelp: 'Drag on the image to frame the part of the IV bag to measure (top to bottom), then tap Save area.',
    saveRoi: 'Save area', resetRoi: 'Cancel', calTitle: 'Calibration', cal1: 'Fill the IV bag with water, then tap "Record: full".', cal2: 'Empty the bag (or use an empty one), then tap "Record: empty".',
    cal3: 'A black-and-white striped card behind the bag, a fixed camera and steady light make it more accurate.', calFull: 'Record: full', calEmpty: 'Record: empty', ledTitle: 'Light',
    ledHelp: 'Turn it on if the lighting changes, so every photo has the same brightness.', levelSub: 'level', needCal: (f, e) => `Still to record: ${[f ? '' : 'full', e ? '' : 'empty'].filter(Boolean).join(', ')}`,
    sepOk: (s) => `Calibrated. Full/empty differ by ${s}%, good enough.`, sepWeak: (s, m) => `Full/empty differ by only ${s}% (at least ${m}% recommended). Add the striped card, adjust the angle or light, and calibrate again.`,
    saved: 'Saved.', roiSaved: 'Area saved. Please record full and empty again.', offline: "Can't reach the camera" },
};
let lang = (() => { try { return localStorage.getItem('lang'); } catch { return null; } })() || (/^zh/i.test(navigator.language) ? 'zh' : 'en');
const t = (k, ...a) => { const v = L[lang][k] ?? k; return typeof v === 'function' ? v(...a) : v; };
let st = null, draft = null, note = '';

function applyLang() {
  document.documentElement.lang = lang === 'zh' ? 'zh-Hant' : 'en';
  document.title = t('title');
  document.querySelectorAll('[data-i]').forEach((e) => { e.textContent = t(e.dataset.i); });
  $('langBtn').textContent = lang === 'zh' ? 'EN' : '中';
  $('langBtn').setAttribute('aria-label', lang === 'zh' ? '切換成英文' : 'Switch to Chinese');
  if (st) render();
}
$('langBtn').onclick = () => { lang = lang === 'zh' ? 'en' : 'zh'; try { localStorage.setItem('lang', lang); } catch {} applyLang(); };

// 畫面上的量測範圍、亮度曲線與液位線
function draw() {
  const c = $('cv').getContext('2d');
  c.clearRect(0, 0, 320, 240);
  if (!st) return;
  const r = draft || st;
  c.lineWidth = 2;
  c.strokeStyle = draft ? '#ffd54f' : '#4fc3f7';
  c.strokeRect(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0);
  if (draft) return;
  // 亮度曲線（畫在範圍右側）
  c.strokeStyle = '#ffffffaa'; c.lineWidth = 1; c.beginPath();
  st.profile.forEach((v, i) => { const x = Math.min(318, st.x1 + 4 + v / 255 * 40); i ? c.lineTo(x, st.y0 + i) : c.moveTo(x, st.y0 + i); });
  c.stroke();
  if (st.level >= 0) {
    const y = st.y1 - st.level / 100 * (st.y1 - st.y0);
    c.strokeStyle = '#66bb6a'; c.lineWidth = 3;
    c.beginPath(); c.moveTo(st.x0 - 10, y); c.lineTo(st.x1 + 10, y); c.stroke();
  }
}

function render() {
  $('level').textContent = st.level >= 0 ? st.level + '%' : '—';
  $('levelSub').textContent = t('levelSub');
  const s = $('calStatus');
  const pct = Math.round(st.sep * 100), min = Math.round(st.minSep * 100);
  if (!st.full || !st.empty) { s.className = 'status bad'; s.textContent = (note ? note + ' ' : '') + t('needCal', st.full, st.empty); }
  else if (st.sep < st.minSep) { s.className = 'status bad'; s.textContent = (note ? note + ' ' : '') + t('sepWeak', pct, min); }
  else { s.className = 'status ok'; s.textContent = (note ? note + ' ' : '') + t('sepOk', pct); }
  if (document.activeElement !== $('led')) $('led').value = st.led;
  draw();
}

async function poll() {
  if (document.hidden) return;
  try { st = await (await fetch('/state', { cache: 'no-store' })).json(); render(); }
  catch { $('calStatus').className = 'status bad'; $('calStatus').textContent = t('offline'); }
}

// 影像：上一張載入完才拿下一張
function nextSnap() { $('snap').src = '/snap?t=' + Date.now(); }
$('snap').onload = () => setTimeout(nextSnap, 300);
$('snap').onerror = () => setTimeout(nextSnap, 2000);

// 拖曳設定範圍（座標換算成 320×240）
function toImg(e) {
  const b = $('view').getBoundingClientRect();
  return { x: Math.round((e.clientX - b.left) / b.width * 320), y: Math.round((e.clientY - b.top) / b.height * 240) };
}
let start = null;
$('view').addEventListener('pointerdown', (e) => { start = toImg(e); $('view').setPointerCapture(e.pointerId); });
$('view').addEventListener('pointermove', (e) => {
  if (!start) return;
  const p = toImg(e);
  draft = { x0: Math.max(0, Math.min(start.x, p.x)), x1: Math.min(320, Math.max(start.x, p.x)),
            y0: Math.max(0, Math.min(start.y, p.y)), y1: Math.min(240, Math.max(start.y, p.y)) };
  $('saveRoi').disabled = $('resetRoi').disabled = false;
  draw();
});
$('view').addEventListener('pointerup', () => { start = null; });
$('resetRoi').onclick = () => { draft = null; $('saveRoi').disabled = $('resetRoi').disabled = true; draw(); };
$('saveRoi').onclick = async () => {
  const d = await (await fetch('/roi', { method: 'POST', body: new URLSearchParams(draft) })).json().catch(() => ({}));
  if (d.ok) { draft = null; note = t('roiSaved'); $('saveRoi').disabled = $('resetRoi').disabled = true; }
  poll();
};

async function cal(which) {
  const d = await (await fetch('/cal', { method: 'POST', body: new URLSearchParams({ which }) })).json().catch(() => ({}));
  note = d.ok ? t('saved') : '';
  poll();
}
$('calFull').onclick = () => cal('full');
$('calEmpty').onclick = () => cal('empty');
$('led').onchange = () => fetch('/led', { method: 'POST', body: new URLSearchParams({ v: $('led').value }) });

applyLang();
nextSnap();
poll();
setInterval(poll, 1000);
</script>
</body></html>)HTML";
