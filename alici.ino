// ALICI (ESP32 #2)  |  Sürüm 2
// Görevi: vericiden ESP-NOW ile alarm almak, siren rölesini çekmek,
// Firebase'e bildirmek, panelden gelen komutları (sustur, test, süre, devre dışı) uygulamak.
// Röle modülü: IN1 -> D26 (siren), IN2 -> D25 (ışık, henüz kullanılmıyor), VCC -> VIN, GND -> GND
// Wi-Fi ve ESP şifresi aynı klasördeki secrets.h dosyasındadır.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include "secrets.h"

const char* API_KEY = "AIzaSyDGam4xOxat4xeGIWihrkyggBBBn3Vp3mk";
const char* DB_URL  = "https://bariyer-alarm-default-rtdb.europe-west1.firebasedatabase.app";

const int SIREN_PIN = 26;
const int LIGHT_PIN = 25;       // Işık adımında kullanılacak
const bool ACTIVE_LOW = true;   // Röle LOW'da çekiyorsa true

typedef struct { uint32_t magic; uint8_t type; uint8_t contact; } Msg;
const uint32_t MAGIC = 0xB4A10002;       // verici ile aynı olmalı
const uint8_t T_ALARM = 1, T_BEAT = 2;

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

void relay(int pin, bool on) { digitalWrite(pin, (on != ACTIVE_LOW) ? HIGH : LOW); }
void applyRelay() {
  bool testActive = (long)(testUntil - millis()) > 0;
  relay(SIREN_PIN, sirenOn || testActive);
}

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
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi baglaniyor...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(300);
    if (WiFi.status() == WL_CONNECTED) Serial.printf("WiFi OK, kanal: %d\n", WiFi.channel());
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

void setup() {
  Serial.begin(115200);
  digitalWrite(SIREN_PIN, ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(LIGHT_PIN, ACTIVE_LOW ? HIGH : LOW);
  pinMode(SIREN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  relay(SIREN_PIN, false);
  relay(LIGHT_PIN, false);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) Serial.println("ESP-NOW baslatilamadi!");
  esp_now_register_recv_cb(onRecv);

  Serial.print("ALICI MAC: ");
  Serial.println(WiFi.macAddress());
  ensureAuth();
}

void loop() {
  if (millis() - lastMacPrint > 30000) {
    lastMacPrint = millis();
    Serial.printf("ALICI MAC: %s  kanal:%d  sinyal:%d dBm\n", WiFi.macAddress().c_str(), WiFi.channel(), WiFi.RSSI());
  }

  // Siren süre sınırı (internet olmasa da çalışır)
  if (sirenOn && !wasOn) sirenStart = millis();
  wasOn = sirenOn;
  if (sirenOn && sirenSeconds > 0 && millis() - sirenStart > (unsigned long)sirenSeconds * 1000UL) {
    sirenOn = false;
    offPending = true;
    Serial.println("Siren suresi doldu, kapatildi");
  }
  applyRelay();

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
