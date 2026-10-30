// 推播接收用 Service Worker：顯示通知，並把每則警報存起來給 App 畫面顯示
const STORE = 'iv-alerts';
const KEY = 'alerts.json';
const PREFIX = { empty: '🚨 ', low: '⚠️ ', fault: '❗ ', normal: '✅ ', test: '🔔 ' };

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));

async function readAlerts() {
  const cache = await caches.open(STORE);
  const res = await cache.match(KEY);
  return res ? res.json() : [];
}

async function saveAlert(alert) {
  const list = await readAlerts();
  list.unshift(alert);
  const cache = await caches.open(STORE);
  await cache.put(KEY, new Response(JSON.stringify(list.slice(0, 50)), {
    headers: { 'Content-Type': 'application/json' },
  }));
}

// 裝置送來的內容是中文；App 切到英文時，通知改用英文說明
const BODY_EN = {
  low: 'Stage 1: level low. Prepare a replacement bag.', empty: 'Stage 2: the IV bag is almost empty. Replace it now!',
  fault: 'Sensor reading is abnormal. Check the sensor placement.', normal: 'Level back to normal (replaced or resolved).',
  test: 'Test alert: notifications from this device are working.',
};

async function readLang() {
  try {
    const res = await (await caches.open(STORE)).match('lang.json');
    return res ? res.json() : 'zh';
  } catch { return 'zh'; }
}

self.addEventListener('push', (e) => {
  let d = {};
  try { d = e.data ? e.data.json() : {}; } catch { d = { body: e.data ? e.data.text() : '' }; }

  // iOS 規定每則推播都必須顯示通知，否則會取消訂閱
  const show = readLang().then((lang) => {
    // 英文模式把「301 病房 A 床」轉成「Room 301 Bed A」
    const m = lang === 'en' && /^\s*(\S+?)\s*病房\s*(\S+?)\s*床\s*$/.exec(d.bed || '');
    const bed = m ? `Room ${m[1]} Bed ${m[2]}` : d.bed || (lang === 'en' ? 'IV Monitor' : '點滴監控');
    const body = lang === 'en' ? BODY_EN[d.state] || d.body || '' : d.body || '';
    return self.registration.showNotification((PREFIX[d.state] || '') + bed, {
      body,
      tag: 'iv-' + bed,              // 同一床只保留最新一則
      renotify: true,
      requireInteraction: d.state === 'empty',
      icon: 'icon-192.png',
      badge: 'icon-192.png',
      timestamp: (d.t || 0) * 1000 || Date.now(),
      data: { url: d.url || '' },
    });
  });

  const record = saveAlert({ ...d, bed: d.bed || '點滴監控', received: Date.now() })
    .then(() => self.clients.matchAll({ type: 'window', includeUncontrolled: true }))
    .then((clients) => clients.forEach((c) => c.postMessage({ type: 'alert' })));

  e.waitUntil(Promise.all([show, record]));
});

self.addEventListener('notificationclick', (e) => {
  e.notification.close();
  e.waitUntil((async () => {
    const clients = await self.clients.matchAll({ type: 'window', includeUncontrolled: true });
    if (clients.length) return clients[0].focus();
    return self.clients.openWindow('./');
  })());
});
