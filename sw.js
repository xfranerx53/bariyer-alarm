// Bariyer Alarm - servis çalışanı (PWA)
// Amaç: uygulama kabuğu (index.html, simgeler) ve Firebase betikleri hafızada tutulur;
// internet yokken uygulama boş ekran yerine açılır. Canlı veri (alarm durumu) asla hafızadan verilmez.
const VERSION = "v1";
const SHELL = "bariyer-shell-" + VERSION;
const LIBS  = "bariyer-libs-" + VERSION;
const FILES = ["./", "./index.html", "./manifest.webmanifest", "./icon-192.png", "./icon-512.png", "./apple-touch-icon.png"];

self.addEventListener("install", e => {
  e.waitUntil(caches.open(SHELL).then(c => c.addAll(FILES)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", e => {
  e.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.filter(k => k !== SHELL && k !== LIBS).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", e => {
  const req = e.request;
  if (req.method !== "GET") return;
  const url = new URL(req.url);

  // Firebase betik dosyaları (sürüm numaralı, değişmez): hafızadan, yoksa ağdan al
  if (url.hostname === "www.gstatic.com" && url.pathname.startsWith("/firebasejs/")) {
    e.respondWith(
      caches.open(LIBS).then(async c => {
        const hit = await c.match(req);
        if (hit) return hit;
        const res = await fetch(req);
        if (res.ok) c.put(req, res.clone());
        return res;
      })
    );
    return;
  }

  // Sadece kendi sitemizin dosyaları; Firebase veri/oturum istekleri hiç karışılmaz
  if (url.origin !== self.location.origin) return;

  // Sayfanın kendisi: önce ağ (hep güncel gelsin), ağ yoksa hafızadaki kopya
  if (req.mode === "navigate") {
    e.respondWith(
      fetch(req).then(res => {
        if (res.ok) { const copy = res.clone(); caches.open(SHELL).then(c => c.put("./index.html", copy)); }
        return res;
      }).catch(() => caches.match("./index.html").then(r => r || caches.match("./")))
    );
    return;
  }

  // Simgeler, manifest: hafızadan, arkadan güncelle
  e.respondWith(
    caches.match(req).then(hit => {
      const net = fetch(req).then(res => {
        if (res.ok) { const copy = res.clone(); caches.open(SHELL).then(c => c.put(req, copy)); }
        return res;
      }).catch(() => hit);
      return hit || net;
    })
  );
});
