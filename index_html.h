// 由 make_html.ps1 自動產生，請改 phone.html，不要直接改這個檔案
#pragma once
const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#1565c0">
<title>點滴監控</title>
<style>
:root{--bg:#eef2f7;--card:#fff;--ink:#1c2430;--sub:#6b7685;--line:#e6eaf0;--blue:#1565c0;--ok:#2e9d5b;--warn:#e09400;--bad:#d63031;--fault:#6c5ce7;--off:#8a94a3;--liq:#7fb8f0}
@media (prefers-color-scheme:dark){:root{--bg:#0f141b;--card:#1a212b;--ink:#e8edf3;--sub:#9aa5b3;--line:#2a3340;--liq:#4a8fd6}}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"Noto Sans TC","Microsoft JhengHei",sans-serif;background:var(--bg);color:var(--ink);-webkit-tap-highlight-color:transparent}
header{position:sticky;top:0;z-index:2;background:var(--blue);color:#fff;padding:14px 16px;padding-top:max(14px,env(safe-area-inset-top));display:flex;align-items:center;justify-content:space-between}
header b{font-size:18px;letter-spacing:.5px}
.net{display:flex;align-items:center;gap:6px;font-size:13px}
.net svg{width:18px;height:18px}
.wrap{max-width:440px;margin:0 auto;padding:12px 16px 32px}
.card{background:var(--card);border-radius:16px;padding:16px;margin-bottom:12px;box-shadow:0 1px 3px #0000000f}
.sub{color:var(--sub);font-size:13px}
h2{font-size:14px;margin:0 0 10px;color:var(--sub);font-weight:600}
.bed{display:flex;justify-content:space-between;align-items:baseline;gap:8px}
.bed h1{font-size:17px;margin:0}
.banner{border-radius:16px;padding:16px;color:#fff;display:flex;gap:14px;align-items:center;margin-bottom:12px;transition:background .3s}
.banner svg{width:34px;height:34px;flex:none}
.banner .t{font-size:20px;font-weight:700}
.banner .d{font-size:13px;opacity:.92;margin-top:2px}
.normal{background:var(--ok)}.low{background:var(--warn)}.fault{background:var(--fault)}.offline{background:var(--off)}
.empty{background:var(--bad);animation:pulse 1s infinite}
@keyframes pulse{50%{filter:brightness(1.3)}}
.bag{display:flex;gap:18px;align-items:center}
.bag svg{width:112px;flex:none}
#fill{transition:transform .8s ease,fill .3s}
.lvl{flex:1;min-width:0}
.lvl .v{font-size:22px;font-weight:700;margin:2px 0 10px}
.steps{display:flex;gap:4px}
.steps i{flex:1;height:8px;border-radius:4px;background:var(--line);transition:background .3s}
.row{display:flex;justify-content:space-between;align-items:center;padding:12px 0;border-bottom:1px solid var(--line);font-size:15px}
.row:first-child{padding-top:0}.row:last-child{border:0;padding-bottom:0}
.pill{font-size:13px;padding:4px 10px;border-radius:999px;background:var(--line);white-space:nowrap}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;vertical-align:1px}
button{font:inherit;border:0;border-radius:12px;padding:14px;width:100%;font-size:16px;font-weight:600;cursor:pointer}
button:disabled{opacity:.4;cursor:default}
.mute{background:var(--ink);color:var(--card)}
.sound{background:var(--line);color:var(--ink);margin-top:8px}
button,.link{display:flex;align-items:center;justify-content:center;gap:8px;min-height:48px}
.link{border-radius:12px;padding:14px;font-size:16px;font-weight:600;text-decoration:none;box-sizing:border-box}
.ic{width:20px;height:20px;flex:none;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.push h2{margin-bottom:4px}
.push .link{margin-top:12px}
@media (prefers-reduced-motion:reduce){.empty{animation:none}#fill{transition:none}}
.log{list-style:none;margin:0;padding:0;max-height:240px;overflow:auto}
.log li{display:flex;gap:12px;padding:8px 0;border-bottom:1px solid var(--line);font-size:14px}
.log li:last-child{border:0}
.log time{color:var(--sub);font-variant-numeric:tabular-nums;flex:none}
.demo{display:none}
.demo .btns{display:flex;gap:6px}
.demo button{padding:10px 4px;font-size:13px;background:var(--line);color:var(--ink)}
</style>
</head>
<body>
<header>
  <b>點滴監控</b>
  <span class="net"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"><path d="M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0"/><circle cx="12" cy="19" r="1.2" fill="currentColor"/></svg><span id="net">連線中</span></span>
</header>

<div class="wrap">
  <div class="banner offline" id="banner">
    <svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 22a2.5 2.5 0 0 0 2.45-2h-4.9A2.5 2.5 0 0 0 12 22zm7-6V11a7 7 0 0 0-5.5-6.84V3.5a1.5 1.5 0 0 0-3 0v.66A7 7 0 0 0 5 11v5l-2 2v1h18v-1z"/></svg>
    <div><div class="t" id="st">連線中…</div><div class="d" id="since">正在取得點滴狀態</div></div>
  </div>

  <div class="card bed"><h1 id="bed">—</h1><span class="sub" id="upd"></span></div>

  <div class="card bag">
    <svg viewBox="0 0 120 192" aria-hidden="true">
      <defs><clipPath id="clip"><path d="M30 20h60q12 0 12 12v98q0 22-22 30H40q-22-8-22-30V32q0-12 12-12z"/></clipPath></defs>
      <rect x="51" y="3" width="18" height="17" rx="5" fill="none" stroke="var(--sub)" stroke-width="3"/>
      <g clip-path="url(#clip)"><rect id="fill" x="0" y="0" width="120" height="200" fill="var(--liq)" style="transform:translateY(160px)"/></g>
      <path d="M30 20h60q12 0 12 12v98q0 22-22 30H40q-22-8-22-30V32q0-12 12-12z" fill="none" stroke="var(--sub)" stroke-width="3"/>
      <rect x="53" y="160" width="14" height="14" rx="2" fill="var(--sub)"/>
      <path d="M60 174v18" stroke="var(--sub)" stroke-width="3"/>
      <line x1="10" y1="62" x2="110" y2="62" stroke="var(--warn)" stroke-width="2" stroke-dasharray="5 3"/>
      <line x1="10" y1="122" x2="110" y2="122" stroke="var(--bad)" stroke-width="2" stroke-dasharray="5 3"/>
      <text x="96" y="57" text-anchor="end" font-size="11" font-weight="700" fill="var(--warn)">上位</text>
      <text x="96" y="117" text-anchor="end" font-size="11" font-weight="700" fill="var(--bad)">下位</text>
    </svg>
    <div class="lvl">
      <div class="sub">目前液位</div>
      <div class="v" id="lv">—</div>
      <div class="steps"><i id="s1"></i><i id="s2"></i><i id="s3"></i></div>
      <div class="sub" id="hint" style="margin-top:10px"></div>
    </div>
  </div>

  <div class="card">
    <div class="row"><span>上位感測器</span><span class="pill" id="u">—</span></div>
    <div class="row"><span>下位感測器</span><span class="pill" id="l">—</span></div>
    <div class="row"><span>現場蜂鳴器</span><span class="pill" id="m">—</span></div>
  </div>

  <div class="card">
    <button class="mute" id="muteBtn" onclick="mute()" disabled>
      <svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M8.7 3A6 6 0 0 1 18 8a21.3 21.3 0 0 0 .6 5"/><path d="M17 17H3s3-2 3-9a4.67 4.67 0 0 1 .3-1.7"/><path d="M10.3 21a1.94 1.94 0 0 0 3.4 0"/><path d="m2 2 20 20"/></svg>
      靜音現場警報</button>
    <button class="sound" id="soundBtn" onclick="enableSound()">
      <svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M11 4.7a.7.7 0 0 0-1.2-.5L6.4 7.6A1.4 1.4 0 0 1 5.4 8H3a1 1 0 0 0-1 1v6a1 1 0 0 0 1 1h2.4a1.4 1.4 0 0 1 1 .4l3.4 3.4a.7.7 0 0 0 1.2-.5z"/><path d="M16 9a5 5 0 0 1 0 6"/><path d="M19.4 18.4a9 9 0 0 0 0-12.7"/></svg>
      <span id="soundText">開啟這頁的提示音</span></button>
  </div>

  <div class="card push">
    <h2>手機推播</h2>
    <p class="sub" id="pushInfo">手機鎖定時也能收到警報。</p>
    <a class="sound link" href="https://youenc120-ux.github.io/smart-iv-drip-alert/" target="_blank" rel="noopener">
      <svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9"/><path d="M10.3 21a1.94 1.94 0 0 0 3.4 0"/></svg>
      設定推播（開啟點滴警報 App）</a>
  </div>

  <div class="card">
    <h2>事件紀錄</h2>
    <ul class="log" id="log"><li class="sub">尚無事件</li></ul>
  </div>

  <div class="card demo" id="demo">
    <h2>展示模式：點按切換模擬狀態</h2>
    <div class="btns">
      <button data-s="normal">充足</button><button data-s="low">偏低</button>
      <button data-s="empty">耗盡</button><button data-s="fault">異常</button>
    </div>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
// 用電腦直接開檔案、在 GitHub Pages 上，或網址加 ?demo，就進入展示模式（不需要硬體）
const DEMO = !location.protocol.startsWith('http') || location.hostname.endsWith('github.io') || location.search.includes('demo');

const INFO = {
  normal: { lv: '高於上位', hint: '液量充足，持續監測中', fill: 38,  n: 3, c: 'var(--ok)' },
  low:    { lv: '上、下位之間', hint: '請準備更換點滴',   fill: 92,  n: 2, c: 'var(--warn)' },
  empty:  { lv: '低於下位', hint: '請立即更換點滴！',     fill: 140, n: 1, c: 'var(--bad)' },
  fault:  { lv: '無法判斷', hint: '請檢查感測器安裝位置', fill: 92,  n: 0, c: 'var(--fault)' }
};
const TEXT = { normal: '液量充足', low: '第一階段：液量偏低', empty: '第二階段：請立即更換', fault: '感測器異常' };

let last = null, ctx = null, fails = 0;
const demo = { state: 'normal', since: Date.now(), muteUntil: 0 };

function enableSound() {
  ctx = ctx || new (window.AudioContext || window.webkitAudioContext)();
  ctx.resume();
  beep(1);
  $('soundText').textContent = '這頁的提示音已開啟';
  $('soundBtn').disabled = true;
}
function beep(n, f = 1000) {
  if (!ctx) return;
  const t = ctx.currentTime;
  for (let i = 0; i < n; i++) {
    const o = ctx.createOscillator(), g = ctx.createGain();
    o.frequency.value = f; g.gain.value = 0.3;
    o.connect(g).connect(ctx.destination);
    o.start(t + i * 0.3); o.stop(t + i * 0.3 + 0.15);
  }
}
function vib(p) { if (navigator.vibrate) navigator.vibrate(p); }
function clock() { return new Date().toLocaleTimeString('zh-TW', { hour12: false }); }
function fmt(s) { const m = Math.floor(s / 60); return m ? m + ' 分 ' + (s % 60) + ' 秒' : s + ' 秒'; }

function addLog(text) {
  const ul = $('log');
  if (ul.firstElementChild && ul.firstElementChild.classList.contains('sub')) ul.innerHTML = '';
  const li = document.createElement('li');
  li.innerHTML = '<time>' + clock() + '</time><span></span>';
  li.lastChild.textContent = text;
  ul.prepend(li);
  while (ul.children.length > 30) ul.lastChild.remove();
}
function sensor(el, ok) {
  el.innerHTML = '<span class="dot" style="background:' + (ok ? 'var(--ok)' : 'var(--bad)') + '"></span>' + (ok ? '有液體' : '無液體');
}
function signal(d) {
  if (d.ap) return '熱點模式';
  if (d.rssi > -60) return '訊號強';
  if (d.rssi > -75) return '訊號中';
  return '訊號弱';
}

function render(d) {
  const i = INFO[d.state];
  $('banner').className = 'banner ' + d.state;
  $('st').textContent = d.text;
  $('since').textContent = '已持續 ' + fmt(d.since);
  $('bed').textContent = d.bed;
  $('upd').textContent = '更新於 ' + clock();
  $('net').textContent = signal(d);

  $('fill').style.transform = 'translateY(' + i.fill + 'px)';
  $('fill').style.fill = d.state === 'fault' ? 'var(--off)' : 'var(--liq)';
  $('lv').textContent = i.lv;
  $('lv').style.color = i.c;
  $('hint').textContent = i.hint;
  ['s1', 's2', 's3'].forEach((s, k) => $(s).style.background = k < i.n ? i.c : '');

  sensor($('u'), d.upper);
  sensor($('l'), d.lower);
  $('m').textContent = d.muted ? '靜音中 ' + d.muteLeft + ' 秒' : (d.state === 'normal' ? '待命' : '警報中');
  $('muteBtn').disabled = d.state === 'normal' || d.muted;
  $('pushInfo').textContent = d.push ? '已綁定 ' + d.push + ' 支手機，警報會即時推播。' : '尚未綁定手機。手機鎖定時也能收到警報。';
  document.title = (d.state === 'empty' ? '🔴 ' : d.state === 'low' ? '🟡 ' : '') + '點滴監控';

  if (d.state !== last) {
    if (last !== null || d.state !== 'normal') addLog(d.text);
    if (d.state === 'low')   { beep(2, 880);  vib(300); }
    if (d.state === 'empty') { beep(5, 1200); vib([300, 100, 300, 100, 300]); }
    if (d.state === 'fault') { beep(1, 600); }
    last = d.state;
  } else if (d.state === 'empty' && !d.muted) {
    beep(1, 1200);
  }
}

function offline() {
  $('banner').className = 'banner offline';
  $('st').textContent = '裝置離線';
  $('since').textContent = '請確認裝置電源與 Wi-Fi';
  $('net').textContent = '離線';
  addLog('與裝置失去連線');
  beep(3, 500); vib(500);
  last = null;
}

function demoStatus() {
  const now = Date.now(), s = demo.state, muted = now < demo.muteUntil;
  return {
    bed: '301 病房 A 床（展示）', state: s, text: TEXT[s],
    upper: s === 'normal' || s === 'fault', lower: s === 'normal' || s === 'low',
    muted, muteLeft: muted ? Math.ceil((demo.muteUntil - now) / 1000) : 0,
    since: Math.floor((now - demo.since) / 1000), rssi: -55, ap: false
  };
}

async function poll() {
  if (DEMO) { render(demoStatus()); return; }
  try {
    const r = await fetch('/status', { cache: 'no-store' });
    render(await r.json());
    fails = 0;
  } catch (e) {
    if (++fails === 3) offline();
  }
}

function mute() {
  if (DEMO) {
    demo.muteUntil = Date.now() + (demo.state === 'empty' ? 60000 : 300000);
    poll();
    return;
  }
  fetch('/mute', { method: 'POST' }).then(poll);
}

if (DEMO) {
  $('demo').style.display = 'block';
  document.querySelectorAll('#demo button').forEach(b => b.onclick = () => {
    if (b.dataset.s !== demo.state) {
      const worse = Object.keys(TEXT).indexOf(b.dataset.s) > Object.keys(TEXT).indexOf(demo.state);
      if (worse) demo.muteUntil = 0;
      demo.state = b.dataset.s;
      demo.since = Date.now();
    }
    poll();
  });
}
setInterval(poll, 1000);
poll();
</script>
</body>
</html>
)HTML";
