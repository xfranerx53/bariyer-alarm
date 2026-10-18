// ALICI (ESP32 #2)  |  Sürüm 3
// Görevi: vericiden ESP-NOW ile alarm almak, siren rölesini çekmek,
// Firebase'e bildirmek, panelden gelen komutları (sustur, test, süre, devre dışı) uygulamak.
// YENİ (v3): Wi-Fi telefondan ayarlanır. BOOT tuşuna kart açıkken 3 saniye basınca
//            kart "Bariyer-Alici-Kurulum" adında kendi Wi-Fi ağını açar, telefondan bağlanıp
//            yeni Wi-Fi bilgisi girilir.
// Röle modülü: IN1 -> D26 (siren), IN2 -> D25 (ışık, henüz kullanılmıyor), VCC -> VIN, GND -> GND
// Wi-Fi ve ESP şifresi aynı klasördeki secrets.h dosyasındadır (ilk açılışta kullanılır).
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_now.h>
#include "secrets.h"

const char* API_KEY = "AIzaSyDGam4xOxat4xeGIWihrkyggBBBn3Vp3mk";
const char* DB_URL  = "https://bariyer-alarm-default-rtdb.europe-west1.firebasedatabase.app";

const int SIREN_PIN = 26;
const int LIGHT_PIN = 25;       // Işık adımında kullanılacak
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
bool wasOn = false;
unsigned long sirenStart = 0, lastStatus = 0, lastMacPrint = 0;
int sirenSeconds = 60;               // panelden ayarlanır, 0 = susturana kadar

// ---------------------------------------------------------------- röle
void relay(int pin, bool on) { digitalWrite(pin, (on != ACTIVE_LOW) ? HIGH : LOW); }
void applyRelay() {
  bool testActive = (long)(testUntil - millis()) > 0;
  relay(SIREN_PIN, sirenOn || testActive);
}

// ---------------------------------------------------------------- ESP-NOW
void handleMsg(const uint8_t* data, int len) {
  if (len != sizeof(Msg)) return;
  Msg m; memcpy(&m, data, sizeof(m));
  if (m.magic != MAGIC) return;
  if (m.type == T_ALARM) {
    if (armed) { sirenOn = true; applyRelay(); fbPending = true; }
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

  loadCreds();
  WiFi.mode(WIFI_STA);
  initEspNow();
  xTaskCreate(uiTask, "ui", 4096, NULL, 1, NULL);

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

void sirenLogic() {
  // Siren süre sınırı (internet olmasa da çalışır)
  if (sirenOn && !wasOn) sirenStart = millis();
  wasOn = sirenOn;
  if (sirenOn && sirenSeconds > 0 && millis() - sirenStart > (unsigned long)sirenSeconds * 1000UL) {
    sirenOn = false;
    offPending = true;
    Serial.println("Siren suresi doldu, kapatildi");
  }
  applyRelay();
}

void loop() {
  if (restartAt && millis() > restartAt) ESP.restart();

  // ---- Kurulum modu: telefon sayfası + kayıtlı ağı arka planda tekrar dene
  if (portalMode) {
    dns.processNextRequest();
    server.handleClient();
    sirenLogic();
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

  sirenLogic();
  ensureAuth();
  if (idToken != "") {
    // 1) Bekleyen alarmı Firebase'e bildir
    if (fbPending) {
      Serial.println("ESP-NOW ALARM! Firebase'e yaziliyor");
      if (dbPatch("/alarm", "{\"triggered\":true,\"siren\":true,\"lastTrigger\":{\".sv\":\"timestamp\"}}")) {
        fbPending = false;
        logEvent("alarm", "verici");
      }
    }
    // 2) Süre dolduysa Firebase'e "siren kapalı" yaz
    if (offPending) {
      if (dbPatch("/alarm", "{\"siren\":false}")) { offPending = false; logEvent("timeout", "alici"); }
    }
    // 3) Panel durumunu oku
    String r = dbGet("/alarm");
    if (r != "") {
      armed = r.indexOf("\"armed\":true") >= 0;
      bool fbSiren = r.indexOf("\"siren\":true") >= 0;
      int i = r.indexOf("\"sirenSeconds\":");
      sirenSeconds = (i >= 0) ? r.substring(i + 15).toInt() : 60;
      if (r.indexOf("\"test\":true") >= 0) {
        testUntil = millis() + 3000;
        Serial.println("PANEL TESTI: siren 3 sn");
        dbPatch("/alarm", "{\"test\":false}");
      }
      if (!fbPending && !offPending) sirenOn = fbSiren;   // yazılmamış yerel alarmın üzerine yazma
      Serial.println(sirenOn ? "SIREN ACIK" : "siren kapali");
    }
    // 4) Cihaz durumunu panele bildir (20 sn'de bir)
    if (lastStatus == 0 || millis() - lastStatus > 20000) {
      String j = String("{\"alici\":{\"lastSeen\":{\".sv\":\"timestamp\"},\"rssi\":") + WiFi.RSSI() + "}";
      if (vSeen) {
        vSeen = false;
        j += String(",\"verici\":{\"lastSeen\":{\".sv\":\"timestamp\"},\"contact\":") + (vContact ? "true" : "false") + "}";
      }
      j += "}";
      if (dbPatch("/status", j)) lastStatus = millis();
    }
  }
  applyRelay();
  delay(1000);
}
