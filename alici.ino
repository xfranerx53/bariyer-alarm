// ALICI (ESP32 #2)  |  Sürüm 3.4
// Görevi: vericiden ESP-NOW ile alarm almak, siren rölesini çekmek,
// Firebase'e bildirmek, panelden gelen komutları (sustur, test, süre, devre dışı) uygulamak.
// YENİ (v3): Wi-Fi telefondan ayarlanır. BOOT tuşuna kart açıkken 3 saniye basınca
//            kart "Bariyer-Alici-Kurulum" adında kendi Wi-Fi ağını açar, telefondan bağlanıp
//            yeni Wi-Fi bilgisi girilir.
// v3.3/3.4:  Otomatik susturma. Siren ve ışığın panelde seçilen süreleri (ikisi de) dolunca alarm,
//            sen butona basmasan bile kendiliğinden susturulur ("Alarmı sustur" otomatik basılır).
//            Siren veya alarm ışığı süresiz (boş/0) ise otomatik susturma yoktur.
// YENİ (v3.4): Zamanlama. Panelden seçilen saat aralığı ve günlerde koruma devrededir, dışında alarm vermez.
//            Saat NTP ile alınır (Türkiye saati, UTC+3). Saat alınamazsa güvenli tarafta kalır: koruma açık.
// Röle modülü: IN1 -> D26 (siren), IN2 -> D25 (ışık), VCC -> VIN, GND -> GND
// Siren ve ışık ayrı ayrı ayarlanır: sürekli/aralıklı, aralık süresi, en fazla süre (panelden).
// Wi-Fi ve ESP şifresi aynı klasördeki secrets.h dosyasındadır (ilk açılışta kullanılır).
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_now.h>
#include <time.h>
#include "secrets.h"

const char* API_KEY = "AIzaSyDGam4xOxat4xeGIWihrkyggBBBn3Vp3mk";
const char* FW = "3.4";                 // yazılım sürümü (panelde görünür)
const char* DB_URL  = "https://bariyer-alarm-default-rtdb.europe-west1.firebasedatabase.app";

const int SIREN_PIN = 26;
const int LIGHT_PIN = 25;       // Işık rölesi (IN2)
const bool ACTIVE_LOW = true;   // Röle LOW'da çekiyorsa true
const int BTN_PIN = 0;          // Kart üzerindeki BOOT tuşu
const int LED_PIN = 2;          // Kart üzerindeki mavi LED (varsa)

const char* AP_NAME = "Bariyer-Alici-Kurulum";

typedef struct { uint32_t magic; uint8_t type; uint8_t contact; } Msg;
const uint32_t MAGIC = 0xB4A10002;       // verici ile aynı olmalı
const uint8_t T_ALARM = 1, T_BEAT = 2;

Preferences prefs;
WebServer server(80);
DNSServer dns;
String wifiSsid = "", wifiPass = "";
volatile bool portalMode = false;
volatile unsigned long restartAt = 0;
unsigned long lastRetry = 0;

String idToken = "";
unsigned long tokenTime = 0;

volatile bool armed = true;          // Firebase'den okunur; okunamazsa etkin sayılır
volatile bool sirenOn = false;
volatile bool fbPending = false;     // Firebase'e yazılacak alarm var
volatile bool offPending = false;    // süre dolduğu için Firebase'e "siren kapalı" yazılacak
volatile bool vSeen = false;         // vericiden kalp atışı geldi
volatile uint8_t vContact = 0;       // 1 = bariyer kontağı açık
volatile unsigned long testUntil = 0;
unsigned long lastStatus = 0, lastMacPrint = 0;
volatile int sirenSeconds = 60;      // panelden ayarlanır, 0 = susturana kadar
volatile bool schedOn = false;       // zamanlama açık mı (panelden)
volatile int schedStartMin = 0;      // koruma başlangıcı (gün başından dakika)
volatile int schedEndMin = 0;        // koruma bitişi (başlangıçla aynıysa 24 saat)
volatile int schedDays = 127;        // gün maskesi: bit0 = Pazartesi ... bit6 = Pazar
volatile bool schedActiveNow = true; // şu an zamanlamaya göre koruma devrede mi
volatile unsigned long alarmSeq = 0; // kabul edilen her yeni alarmda artar (otomatik susturma sayacını yeniler)
volatile bool mutePending = false;   // otomatik susturuldu, Firebase'e yazılacak

// Siren ve ışık ayarları (ikisi birbirinden bağımsız, panelden gelir)
volatile bool sirenRepeat = true;        // bariyer tekrar kesilirse siren yeniden çalsın (panelden)
volatile unsigned long rxAlarms = 0;     // vericiden alınan toplam alarm mesajı sayısı
volatile unsigned long ignoredAlarms = 0;// 'tekrar çalma' kapalıyken yok sayılan alarmlar
volatile bool alarmActive = false;       // alarm verildi ve henüz susturulmadı (siren süresi dolsa da sürer)
volatile bool sirenBlink = false;        // false = sürekli, true = aralıklı
volatile unsigned long sirenMs = 1000;   // aralıklı modda çalma/susma süresi (ms)
volatile bool lightManual = false;       // panelden "Işığı aç"
volatile bool lightBlink = false;        // false = sürekli, true = aralıklı
volatile bool lightWithAlarm = true;     // alarmda ışık kendiliğinden yansın
volatile unsigned long lightMs = 1000;   // aralıklı modda yanma/sönme süresi (ms)
volatile int lightSeconds = 0;           // alarmda ışık en fazla kaç sn yansın, 0 = alarm sürdüğü sürece

// ---------------------------------------------------------------- röle
void relay(int pin, bool on) { digitalWrite(pin, (on != ACTIVE_LOW) ? HIGH : LOW); }

// ---------------------------------------------------------------- zamanlama
bool computeSched() {
  if (!schedOn) return true;
  time_t t = time(nullptr);
  if (t < 1700000000) return true;                 // saat henüz alınamadı: güvenli taraf (koruma açık)
  struct tm tmv; localtime_r(&t, &tmv);
  int day = (tmv.tm_wday + 6) % 7;                 // 0 = Pazartesi
  int prev = (day + 6) % 7;
  int m = tmv.tm_hour * 60 + tmv.tm_min;
  int a = schedStartMin, b = schedEndMin, d = schedDays;
  if (a == b) return (d >> day) & 1;               // başlangıç = bitiş: seçili günlerde 24 saat
  if (a < b) return ((d >> day) & 1) && m >= a && m < b;
  return (((d >> day) & 1) && m >= a) || (((d >> prev) & 1) && m < b);   // gece yarısını aşan aralık
}

// ---------------------------------------------------------------- ESP-NOW
void handleMsg(const uint8_t* data, int len) {
  if (len != sizeof(Msg)) return;
  Msg m; memcpy(&m, data, sizeof(m));
  if (m.magic != MAGIC) return;
  if (m.type == T_ALARM) {
    rxAlarms++;
    if (armed && schedActiveNow) {
      if (!alarmActive || sirenRepeat) { sirenOn = true; alarmActive = true; fbPending = true; alarmSeq++; }
      else ignoredAlarms++;     // aynı alarm içinde tekrar kesinti: siren yeniden çalmaz
    }
  } else if (m.type == T_BEAT) {
    vContact = m.contact; vSeen = true;
  }
}
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) { handleMsg(data, len); }
#else
void onRecv(const uint8_t* mac, const uint8_t* data, int len) { handleMsg(data, len); }
#endif
void initEspNow() {
  esp_now_deinit();
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW baslatilamadi!"); return; }
  esp_now_register_recv_cb(onRecv);
}

// ---------------------------------------------------------------- Wi-Fi bilgisi (kalıcı hafıza)
void loadCreds() {
  prefs.begin("wifi", false);
  if (!prefs.getBool("cfg", false)) {          // hiç kurulum yapılmadı: secrets.h bilgisi
    wifiSsid = WIFI_SSID; wifiPass = WIFI_PASS;
  } else {                                      // telefondan kurulum yapıldı (veya sıfırlandı)
    wifiSsid = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
  }
}

void resetWifi() {
  Serial.println("WIFI SIFIRLANDI: kurulum moduna geciliyor");
  prefs.putBool("cfg", true);
  prefs.putString("ssid", "");
  prefs.putString("pass", "");
  digitalWrite(LED_PIN, HIGH);
  delay(800);
  ESP.restart();
}

// BOOT tuşunu ve LED'i ayrı bir görevde izler (ağ işlemleri sırasında da kaçırmaz)
void uiTask(void*) {
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  unsigned long down = 0, blink = 0;
  bool led = false;
  for (;;) {
    if (digitalRead(BTN_PIN) == LOW) {
      if (!down) down = millis();
      digitalWrite(LED_PIN, HIGH);
      if (millis() - down > 3000) resetWifi();
    } else {
      down = 0;
      if (portalMode) {
        if (millis() - blink > 250) { blink = millis(); led = !led; digitalWrite(LED_PIN, led); }
      } else digitalWrite(LED_PIN, LOW);
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// Siren ve ışık rölelerini ayrı bir görev yönetir: ağ işlemleri sırasında bile
// yanıp sönme düzgün kalır ve alarm gelince röle hemen çeker.
void outputTask(void*) {
  bool prevAlarm = false, prevSiren = false, prevAuto = false;
  unsigned long alarmStart = 0, sirenStartT = 0, autoStart = 0, seenSeq = 0, lastSched = 0;
  for (;;) {
    unsigned long now = millis();
    bool testActive = (long)(testUntil - now) > 0;

    // Zamanlama (saniyede bir)
    if (now - lastSched > 1000) { lastSched = now; schedActiveNow = computeSched(); }

    // Otomatik susturma: siren ve ışık süreleri dolunca alarm kendiliğinden susturulur
    // (siren süresiz veya alarm ışığı süresizse otomatik susturma yok)
    int autoSec = 0;
    {
      int sl = sirenSeconds, ll = lightWithAlarm ? lightSeconds : 0;
      bool unlimited = (sl <= 0) || (lightWithAlarm && ll <= 0);
      if (!unlimited) autoSec = sl > ll ? sl : ll;
    }
    if (alarmActive && (!prevAuto || alarmSeq != seenSeq)) autoStart = now;   // yeni alarm: sayaç baştan
    prevAuto = alarmActive; seenSeq = alarmSeq;
    if (alarmActive && autoSec > 0 && now - autoStart >= (unsigned long)autoSec * 1000UL) {
      mutePending = true;     // önce bayrak, sonra kapat (ana döngü araya girip geri açmasın)
      alarmActive = false;
      sirenOn = false;
      prevAuto = false;
    }

    // Siren süre sınırı (20 ms hassasiyet, internet olmasa da çalışır)
    if (sirenOn && !prevSiren) sirenStartT = now;
    prevSiren = sirenOn;
    if (sirenOn && sirenSeconds > 0 && now - sirenStartT >= (unsigned long)sirenSeconds * 1000UL) {
      offPending = true;      // önce bayrak, sonra kapat (ana döngü araya girip geri açmasın)
      sirenOn = false;
      prevSiren = false;
    }

    // SIREN
    bool sOut = sirenOn || testActive;
    if (sOut && sirenBlink) { unsigned long ms = sirenMs < 100 ? 100 : sirenMs; sOut = ((now / ms) % 2) == 0; }
    relay(SIREN_PIN, sOut);

    // IŞIK
    if (alarmActive && !prevAlarm) alarmStart = now;
    prevAlarm = alarmActive;
    bool aLight = false;
    if (lightWithAlarm) {
      if (alarmActive) aLight = (lightSeconds <= 0) || (now - alarmStart <= (unsigned long)lightSeconds * 1000UL);
      if (testActive) aLight = true;
    }
    bool lOut = lightManual || aLight;
    if (lOut && lightBlink) { unsigned long ms = lightMs < 100 ? 100 : lightMs; lOut = ((now / ms) % 2) == 0; }
    relay(LIGHT_PIN, lOut);

    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// ---------------------------------------------------------------- kurulum sayfası
String esc(String s) {
  s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;"); s.replace("\"", "&quot;");
  return s;
}

String portalPage(const String& msg) {
  String h = F("<!DOCTYPE html><html lang=tr><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Bariyer Kurulum</title><style>body{font-family:system-ui,sans-serif;background:#eef1f4;margin:0;padding:18px;color:#14202b}"
    ".c{max-width:420px;margin:0 auto;background:#fff;border-radius:16px;padding:20px}h1{font-size:1.2rem;margin-top:0}"
    "select,input,button{width:100%;padding:13px;margin:6px 0;font-size:1rem;border:1px solid #cfd6dd;border-radius:10px;box-sizing:border-box}"
    "button{background:#14202b;color:#fff;border:0;font-weight:700}a{color:#14202b}.m{color:#c62828;font-weight:600}small{color:#5d6b78}</style></head>"
    "<body><div class=c><h1>Alıcı Wi-Fi Kurulumu</h1>");
  if (msg != "") h += "<p class=m>" + msg + "</p>";
  h += F("<form method=POST action=/save><label><small>Wi-Fi ağını seç</small></label><select name=s>");
  WiFi.disconnect();                 // süren bağlanma denemesi taramayı bozmasın
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    String name = WiFi.SSID(i);
    if (name == "") continue;
    h += "<option value=\"" + esc(name) + "\">" + esc(name) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  WiFi.scanDelete();
  h += F("</select><label><small>Listede yoksa ağ adını elle yaz</small></label><input name=m placeholder='Ağ adı (isteğe bağlı)'>"
         "<label><small>Wi-Fi şifresi</small></label><input name=p type=password placeholder='Şifre'>"
         "<button>Kaydet</button></form><p><a href=/>Listeyi yenile</a></p>"
         "<small>Not: Kart sadece 2.4 GHz ağlara bağlanır, 5 GHz ağlar görünmez.</small></div></body></html>");
  return h;
}

void handleSave() {
  String s = server.arg("m"); s.trim();
  if (s == "") s = server.arg("s");
  String p = server.arg("p");
  if (s == "") { server.send(200, "text/html; charset=utf-8", portalPage("Ağ adı boş olamaz.")); return; }
  prefs.putBool("cfg", true);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  Serial.println("Yeni Wi-Fi kaydedildi: " + s);
  server.send(200, "text/html; charset=utf-8",
    F("<!DOCTYPE html><html lang=tr><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'></head>"
      "<body style='font-family:system-ui;padding:24px'><h2>Kaydedildi</h2><p>Kart yeniden başlıyor. Telefonunu normal Wi-Fi ağına geri bağla. "
      "Yaklaşık 30 saniye sonra panelde Alıcı \"Çevrimiçi\" görünmeli.</p></body></html>"));
  restartAt = millis() + 2500;
}

void startPortal() {
  Serial.println("KURULUM MODU: telefonundan 'Bariyer-Alici-Kurulum' agina baglan (sifresiz)");
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  WiFi.softAP(AP_NAME, NULL, 1);          // şifresiz (açık) ağ
  delay(300);
  dns.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", portalPage("")); });
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound([]() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.begin();
  portalMode = true;
  initEspNow();                    // mod değişince ESP-NOW'u yeniden başlat; siren kurulum sırasında da çalışır
  lastRetry = millis();
}

// ---------------------------------------------------------------- Wi-Fi / Firebase
bool connectWifi(unsigned long ms) {
  if (wifiSsid == "") return false;
  Serial.println("WiFi baglaniyor: " + wifiSsid);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < ms) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

bool signIn() {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h;
  h.begin(c, String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + API_KEY);
  h.addHeader("Content-Type", "application/json");
  String body = String("{\"email\":\"") + ESP_EMAIL + "\",\"password\":\"" + ESP_PASS + "\",\"returnSecureToken\":true}";
  int code = h.POST(body);
  String res = h.getString();
  h.end();
  if (code != 200) { Serial.printf("Giris hatasi %d\n", code); return false; }
  int i = res.indexOf("\"idToken\": \"");
  if (i < 0) return false;
  i += 12;
  idToken = res.substring(i, res.indexOf("\"", i));
  tokenTime = millis();
  Serial.println("Firebase girisi OK");
  return true;
}

void ensureAuth() {
  if (portalMode) return;
  if (WiFi.status() != WL_CONNECTED && wifiSsid != "") {
    if (connectWifi(15000)) Serial.printf("WiFi OK, kanal: %d\n", WiFi.channel());
  }
  if (WiFi.status() == WL_CONNECTED && (idToken == "" || millis() - tokenTime > 3000000UL)) signIn();
}

String dbGet(const String& path) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h;
  h.begin(c, String(DB_URL) + path + ".json?auth=" + idToken);
  int code = h.GET();
  String r = (code == 200) ? h.getString() : "";
  if (code == 401) idToken = "";
  h.end();
  return r;
}

bool dbSend(const char* method, const String& path, const String& json) {
  WiFiClientSecure c; c.setInsecure();
  HTTPClient h;
  h.begin(c, String(DB_URL) + path + ".json?auth=" + idToken);
  h.addHeader("Content-Type", "application/json");
  int code = h.sendRequest(method, json);
  h.end();
  if (code == 401) idToken = "";
  return code == 200;
}
bool dbPatch(const String& path, const String& json) { return dbSend("PATCH", path, json); }
bool dbPost(const String& path, const String& json) { return dbSend("POST", path, json); }

void logEvent(const char* type, const char* by) {
  dbPost("/log", String("{\"type\":\"") + type + "\",\"by\":\"" + by + "\",\"t\":{\".sv\":\"timestamp\"}}");
}

// ---------------------------------------------------------------- başlangıç
void setup() {
  Serial.begin(115200);
  digitalWrite(SIREN_PIN, ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(LIGHT_PIN, ACTIVE_LOW ? HIGH : LOW);
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  relay(SIREN_PIN, false);
  relay(LIGHT_PIN, false);

  configTime(3 * 3600, 0, "pool.ntp.org", "time.google.com");   // Türkiye saati (UTC+3)
  loadCreds();
  WiFi.mode(WIFI_STA);
  initEspNow();
  xTaskCreate(uiTask, "ui", 4096, NULL, 1, NULL);
  xTaskCreate(outputTask, "out", 4096, NULL, 1, NULL);

  Serial.printf("ALICI yazilim v%s\n", FW);
  Serial.print("ALICI MAC: ");
  Serial.println(WiFi.macAddress());

  if (wifiSsid == "") {
    startPortal();
  } else if (connectWifi(20000)) {
    Serial.printf("WiFi OK, kanal: %d\n", WiFi.channel());
    signIn();
  } else {
    Serial.println("WiFi baglanamadi, kurulum moduna geciliyor");
    startPortal();
  }
}

void loop() {
  if (restartAt && millis() > restartAt) ESP.restart();

  // ---- Kurulum modu: telefon sayfası + kayıtlı ağı arka planda tekrar dene
  if (portalMode) {
    dns.processNextRequest();
    server.handleClient();
    if (wifiSsid != "" && WiFi.softAPgetStationNum() == 0 && millis() - lastRetry > 30000) {
      lastRetry = millis();
      WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());     // modem sonradan açılırsa kendiliğinden döner
    }
    if (WiFi.status() == WL_CONNECTED && !sirenOn) {
      Serial.println("Kayitli aga baglandi, yeniden baslatiliyor");
      delay(500);
      ESP.restart();
    }
    delay(5);
    return;
  }

  // ---- Normal çalışma
  if (millis() - lastMacPrint > 30000) {
    lastMacPrint = millis();
    Serial.printf("ALICI MAC: %s  kanal:%d  sinyal:%d dBm\n", WiFi.macAddress().c_str(), WiFi.channel(), WiFi.RSSI());
  }

  ensureAuth();
  if (idToken != "") {
    // 1) Bekleyen alarmı Firebase'e bildir
    if (fbPending) {
      Serial.printf("ESP-NOW ALARM! Firebase'e yaziliyor (alinan toplam: %lu, yok sayilan: %lu)\n", rxAlarms, ignoredAlarms);
      if (dbPatch("/alarm", "{\"triggered\":true,\"siren\":true,\"lastTrigger\":{\".sv\":\"timestamp\"}}")) {
        fbPending = false;
        logEvent("alarm", "verici");
      }
    }
    // 2) Süre dolduysa Firebase'e "siren kapalı" yaz
    if (offPending) {
      if (dbPatch("/alarm", "{\"siren\":false}")) { offPending = false; logEvent("timeout", "alici"); }
    }
    // 2b) Otomatik susturuldu: Firebase'de de alarmı temizle (bu arada yeni alarm geldiyse yazma)
    if (mutePending) {
      if (alarmActive) mutePending = false;
      else if (dbPatch("/alarm", "{\"siren\":false,\"triggered\":false}")) { mutePending = false; logEvent("automute", "alici"); }
    }
    // 3) Panel durumunu oku
    String r = dbGet("/alarm");
    if (r != "") {
      armed = r.indexOf("\"armed\":true") >= 0;
      bool fbSiren = r.indexOf("\"siren\":true") >= 0;
      int i = r.indexOf("\"sirenSeconds\":");
      sirenSeconds = (i >= 0) ? r.substring(i + 15).toInt() : 60;
      schedOn = r.indexOf("\"schedOn\":true") >= 0;
      int z = r.indexOf("\"schedStart\":");
      schedStartMin = (z >= 0) ? r.substring(z + 13).toInt() : 0;
      z = r.indexOf("\"schedEnd\":");
      schedEndMin = (z >= 0) ? r.substring(z + 11).toInt() : 0;
      z = r.indexOf("\"schedDays\":");
      schedDays = (z >= 0) ? (r.substring(z + 12).toInt() & 127) : 127;
      sirenRepeat = r.indexOf("\"sirenRepeat\":false") < 0;
      sirenBlink = r.indexOf("\"sirenMode\":\"blink\"") >= 0;
      int k = r.indexOf("\"sirenMs\":");
      long sms = (k >= 0) ? r.substring(k + 10).toInt() : 1000;
      if (sms < 100) sms = 100;
      if (sms > 60000) sms = 60000;
      sirenMs = sms;
      int q = r.indexOf("\"lightSeconds\":");
      lightSeconds = (q >= 0) ? r.substring(q + 15).toInt() : 0;
      lightManual = r.indexOf("\"lightOn\":true") >= 0;
      lightBlink = r.indexOf("\"lightMode\":\"blink\"") >= 0;
      lightWithAlarm = r.indexOf("\"lightAlarm\":false") < 0;
      int j = r.indexOf("\"lightMs\":");
      long lms = (j >= 0) ? r.substring(j + 10).toInt() : 1000;
      if (lms < 100) lms = 100;
      if (lms > 60000) lms = 60000;
      lightMs = lms;
      if (r.indexOf("\"test\":true") >= 0) {
        int tsec = (sirenSeconds > 0) ? (sirenSeconds > 10 ? 10 : (int)sirenSeconds) : 3;
        testUntil = millis() + (unsigned long)tsec * 1000UL;
        Serial.printf("PANEL TESTI: siren %d sn\n", tsec);
        dbPatch("/alarm", "{\"test\":false}");
      }
      bool fbTrig = r.indexOf("\"triggered\":true") >= 0;
      if (!fbPending && !offPending && !mutePending) { sirenOn = fbSiren; alarmActive = fbTrig; }   // yazılmamış yerel alarmın üzerine yazma
      Serial.println(sirenOn ? "SIREN ACIK" : "siren kapali");
    }
    // 4) Cihaz durumunu panele bildir (20 sn'de bir)
    if (lastStatus == 0 || millis() - lastStatus > 20000) {
      String j = String("{\"alici\":{\"lastSeen\":{\".sv\":\"timestamp\"},\"rssi\":") + WiFi.RSSI() + ",\"fw\":\"" + FW + "\"}";
      if (vSeen) {
        vSeen = false;
        j += String(",\"verici\":{\"lastSeen\":{\".sv\":\"timestamp\"},\"contact\":") + (vContact ? "true" : "false") + "}";
      }
      j += "}";
      if (dbPatch("/status", j)) lastStatus = millis();
    }
  }
  
  delay(1000);
}
