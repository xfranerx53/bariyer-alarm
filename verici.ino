// VERICI (ESP32 #1)  |  Sürüm 2
// Görevi: bariyer kontağını izlemek, ışın kesilince alıcıya ESP-NOW ile alarm yollamak,
// 20 saniyede bir "çalışıyorum" bilgisi (kalp atışı) yollamak. Wi-Fi/internet gerekmez.
// Bağlantı: Bariyer COM -> GND, Bariyer NC -> D27
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
#if __has_include(<esp_mac.h>)
#include <esp_mac.h>
#endif

// Alıcının MAC adresi (alıcının Serial Monitor'ünde yazar)
uint8_t ALICI_MAC[6] = {0x88, 0xF1, 0x55, 0x32, 0x03, 0x70};

const int BEAM_PIN = 27;                  // NC kontak, INPUT_PULLUP
const unsigned long COOLDOWN = 3000;      // iki alarm arası en az 3 sn
const unsigned long BEAT_EVERY = 20000;   // kalp atışı aralığı

typedef struct { uint32_t magic; uint8_t type; uint8_t contact; } Msg;
const uint32_t MAGIC = 0xB4A10002;        // alıcı ile aynı olmalı
const uint8_t T_ALARM = 1, T_BEAT = 2;

volatile int sendStatus = -1;             // -1 bekliyor, 1 ulaştı, 0 ulaşmadı
volatile bool pending = false;
volatile unsigned long lastEdge = 0;
unsigned long lastSend = 0, lastBeat = 0;
int retries = 0, beatFail = 0;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onSent(const wifi_tx_info_t* info, esp_now_send_status_t status) { sendStatus = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 0; }
#else
void onSent(const uint8_t* mac, esp_now_send_status_t status) { sendStatus = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 0; }
#endif

// Kesme: kısa röle darbelerini de yakalar
void IRAM_ATTR onBeam() {
  unsigned long now = millis();
  if (now - lastEdge > 30) { pending = true; lastEdge = now; }
}

bool trySend(uint8_t type) {
  Msg m = { MAGIC, type, (uint8_t)(digitalRead(BEAM_PIN) == HIGH ? 1 : 0) };
  sendStatus = -1;
  if (esp_now_send(ALICI_MAC, (uint8_t*)&m, sizeof(m)) != ESP_OK) return false;
  unsigned long t = millis();
  while (sendStatus == -1 && millis() - t < 300) delay(2);
  return sendStatus == 1;
}

bool setChannel(int ch) { return esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) == ESP_OK; }

// Önce mevcut kanalda dener, olmazsa 1-13 kanallarını gezip alıcıyı bulur
bool sendWithScan(uint8_t type) {
  for (int i = 0; i < 3; i++) if (trySend(type)) return true;
  for (int ch = 1; ch <= 13; ch++) {
    setChannel(ch);
    for (int i = 0; i < 2; i++) if (trySend(type)) { Serial.printf("Alici kanal %d'de bulundu\n", ch); return true; }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BEAM_PIN, INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  delay(200);
  esp_wifi_start();
  Serial.printf("Kanal ayari: %s\n", setChannel(1) ? "OK" : "HATA");

  uint8_t me[6];
  esp_read_mac(me, ESP_MAC_WIFI_STA);
  Serial.printf("VERICI MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", me[0], me[1], me[2], me[3], me[4], me[5]);

  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW baslatilamadi!"); return; }
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, ALICI_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("Alici eklenemedi!");
  else Serial.println("Alici eklendi");

  attachInterrupt(digitalPinToInterrupt(BEAM_PIN), onBeam, RISING);  // LOW->HIGH = kontak açıldı
  Serial.println("Verici hazir");
}

void loop() {
  if (pending && millis() - lastSend > COOLDOWN) {
    pending = false;
    Serial.println("ISIN KESILDI!");
    if (sendWithScan(T_ALARM)) {
      Serial.println("Alarm aliciya ulasti");
      lastSend = millis();
      retries = 0;
      pending = false;
    } else {
      Serial.println("ALARM ULASMADI, tekrar denenecek");
      if (++retries < 10) { pending = true; delay(1000); } else retries = 0;
    }
  }

  // Kalp atışı: panelde "verici çevrimiçi" ve bariyer durumu görünsün
  if (!pending && millis() - lastBeat > BEAT_EVERY) {
    lastBeat = millis();
    if (trySend(T_BEAT) || trySend(T_BEAT)) beatFail = 0;
    else if (++beatFail >= 2) { if (sendWithScan(T_BEAT)) beatFail = 0; }
  }
  delay(20);
}
