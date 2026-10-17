# DURUM (Son güncelleme: 17 Ekim 2026)

> **Yeni bir Claude sohbetine başlıyorsan:** Bu repo zip'ini sohbete yükle. Claude bu dosyayı okuyup kaldığımız yerden devam eder.
> **Claude için kural:** Her cevabın sonunda güncel `DURUM.md` dosyasını ver. Kullanıcı bunu GitHub'a yükleyecek.

## 0. Dosya düzeni (tek paket, hepsi repoda)
```
index.html            panel (GitHub Pages yayınlar)
alici/alici.ino       alıcı ESP32 kodu (COM3)  + alici/secrets.h (Wi-Fi/ESP şifresi)
verici/verici.ino     verici ESP32 kodu (COM6)
DURUM.md              bu dosya
README.md
```
Kullanıcı tek zip'i açıp içindekileri GitHub'a sürükleyerek yüklüyor. `alici` ve `verici` klasörleri Arduino IDE'de doğrudan açılabilir (klasör adı = .ino adı olmalı).

## 1. Kullanıcı ve çalışma şekli
- Kullanıcı **kod/yazılım tarafını** yönetiyor. Donanım bağlantılarını **başka bir kişi** yapıyor, kullanıcı ona talimat aktarıyor.
- Kullanıcının elektronik/yazılım bilgisi **sıfır** seviyesinde. Anlatım: sade Türkçe, yabancı terim yok (varsa açıkla), **adım adım, hiçbir adımı atlamadan**.
- Sonuç odaklı ilerlemek istiyor. Gereksiz uzun test listeleri istemiyor. Testleri mümkünse **panelden** (GitHub Pages) yapmak istiyor.
- Sohbet süresi çabuk doluyor. Bu yüzden her cevapta güncel DURUM.md verilecek.
- Kullanıcı çoğu zaman Serial Monitor çıktısını veya ekran görüntüsünü gönderiyor. Hata kopyalarsa oradan devam et.

## 2. Proje amacı
Dış ortamda **Opax ABT-i100** (kablolu, ayaklı, 2 ışınlı kızılötesi bariyer) ışını kesilince **uzaktaki siren** çalacak. Sonra **ışık** eklenecek. Sırayla ilerliyoruz: önce siren, sonra ışık.

## 3. Mimari
```
BARİYER --(kuru kontak NC/COM)--> VERİCİ (ESP32 #1)
VERİCİ --ESP-NOW (MAC adresiyle, internet yok)--> ALICI (ESP32 #2) --> RÖLE --> SİREN (+ileride ışık)
ALICI --Wi-Fi--> FIREBASE Realtime Database <--> PANEL (GitHub Pages, index.html)
```
- Verici internete bağlanmaz. Alarmı ESP-NOW ile doğrudan alıcıya yollar. İnternet kopsa bile siren çalar.
- Alıcı Wi-Fi'ye bağlı. Alarmı Firebase'e yazar, panelden gelen komutları (sustur, devre dışı, test, süre) okur.
- Verici alıcıyı **kanal taramasıyla** bulur (alıcı router kanalındadır, router kanalı değişebilir).
- Verici 20 sn'de bir "kalp atışı" yollar (çevrimiçi bilgisi + bariyer kontağı durumu). Alıcı bunu Firebase `status` altına yazar.

## 4. Hesaplar ve adresler (gizli bilgi YOK)
- GitHub: `xfranerx53/bariyer-alarm`, panel: https://xfranerx53.github.io/bariyer-alarm/
- Firebase proje: `bariyer-alarm`, Realtime Database: `https://bariyer-alarm-default-rtdb.europe-west1.firebasedatabase.app`
- Firebase kullanıcıları: panel girişi `ferhatbostan53@gmail.com`, ESP'ler için `esp@bariyer.com`
- Kurallar: `.read` ve `.write` = `auth != null` (sadece giriş yapanlar)
- Wi-Fi ve ESP şifreleri `alici/secrets.h` dosyasında ve **repoya dahil** (kullanıcı bilinçli olarak böyle istedi, güvenlik sorununu sonra kendisi ayarlayacak). Repo herkese açıksa bu bilgiler görünür; kullanıcıya bir kez hatırlat, ısrar etme.
- Önerilen: `esp@bariyer.com` şifresi zayıf (kısa), Wi-Fi şifresi de sohbette paylaşıldı. İleride değiştirilirse `secrets.h` güncellenip alıcıya tekrar yüklenmeli.

## 5. Donanım durumu
- **Alıcı** ESP-WROOM-32 (30 pin, CH340, Type-C): COM3, MAC `88:F1:55:32:03:70`. 2 kanallı 5V optokuplörlü röle bağlı: **VCC→VIN, GND→GND, IN1→D26 (siren), IN2→D25 (ışık)**. Röle "tık" sesiyle çalıştığı doğrulandı.
- **Verici** ESP-WROOM-32: COM6, MAC `88:F1:55:32:72:20`. Şu an **hiçbir şeye bağlı değil** (sadece USB). Bariyer henüz bağlanmadı.
- Kullanıcıda **siren ve ışık yok** (henüz alınmadı). Testler sadece rölenin "tık" sesiyle yapılıyor.
- Röle ACTIVE_LOW (LOW'da çeker); kodda `ACTIVE_LOW = true`.
- Planlanan: siren 12V DC, ışık 12V DC. Röle COM/NO terminalleri üzerinden 12V adaptörle beslenecek.
- Bariyer bağlantısı planı: bariyer COM→verici GND, bariyer NC→verici D27. Bariyerin 12V'u ESP32'ye ASLA bağlanmayacak. Terminal fotoğrafı henüz görülmedi, etiketler teyit edilecek.
- Bilgisayar: Windows, Arduino IDE 2.3.10, board "ESP32 Dev Module".

## 6. Yazılım durumu
| Parça | Dosya | Durum |
|---|---|---|
| Panel (web) | `index.html` | **v2 hazır, henüz GitHub'a yüklenmedi.** v1 yayında ve çalışıyor (giriş, alarm, sustur, devre dışı doğrulandı). |
| Alıcı kodu | `alici/alici.ino` | **v2 hazır, henüz yüklenmedi.** v1 (Firebase→röle) alıcıda çalışıyor. |
| Verici kodu | `verici/verici.ino` | **v2 hazır, henüz yüklenmedi.** v1 yüklü, "Verici hazir" yazıyor. |

v2 ile gelenler: panelde cihaz durumu (alıcı/verici çevrimiçi, Wi-Fi sinyali, bariyer kontağı), siren testi butonu, siren süre sınırı ayarı (varsayılan 60 sn, 0 = susturana kadar), olay geçmişi (log), tarayıcı bildirimi, şifre sıfırlama, bağlantı uyarısı.

**Önemli:** v2 verici ve alıcı **birlikte** yüklenmeli (mesaj biçimi değişti, `MAGIC` 0xB4A10002). Biri eskiyse haberleşmez.

## 7. Firebase veri yapısı
```
alarm/   armed(bool) siren(bool) triggered(bool) lastTrigger(ms) test(bool) sirenSeconds(sayı)
status/  alici{lastSeen,rssi}  verici{lastSeen,contact}
log/     <otomatik>{type,by,t}   type: alarm|mute|arm|disarm|test|timeout
```
`test`, `sirenSeconds`, `status`, `log` ilk kullanımda kendiliğinden oluşur, elle eklemek gerekmez.

## 8. Doğrulananlar / doğrulanmayanlar
- ✅ Firebase kurulumu, kurallar, `alarm` verisi
- ✅ Panel (v1) girişi ve alarm/sustur/devre dışı akışı
- ✅ Alıcı: Firebase `siren=true` → röle çekiyor
- ✅ Verici ve alıcı derleniyor ve açılıyor
- ❓ **ESP-NOW ile verici→alıcı alarm iletimi gerçek kartlarda henüz test sonucu bildirilmedi**
- ❓ v2 kodlarının derleme sonucu henüz bilinmiyor (derleme hatası çıkarsa kullanıcı yapıştıracak)
- ❌ Bariyer bağlanmadı, siren ve ışık yok

## 9. Sıradaki adımlar
1. v2 alıcı + verici yükle, `index.html` v2'yi GitHub'a yükle.
2. Panelden doğrula: cihazlar "Çevrimiçi", "Siren testi" röleyi tıklatıyor.
3. Vericide D27–GND jumper ile alarm testi (jumper çekilince röle çekmeli, panelde alarm ve olay görünmeli).
4. Bariyer terminallerini (fotoğrafla) teyit edip bariyeri bağla.
5. Siren (12V) geldiğinde röle kanal 1'e bağla.
6. **Işık ekle:** alıcıda D25/IN2 rölesi, panelden açma/kapama ve alarmda yanma. Henüz yazılmadı.
7. Sahaya kurulum: kutu, güç (buck converter veya 5V adaptör), Wi-Fi sinyali, bariyer hizalaması.
8. Güvenlik: ESP şifresini değiştir, Wi-Fi şifresi sohbette paylaşıldığı için değiştirmeyi düşün.

## 10. Bilinen sınırlar
- Alıcı internetsizken panel bilgisi güncellenmez; siren yine çalar, siren süre sınırıyla kendiliğinden susar.
- Alıcı sirenin susturulması için panel veya elektriği kesmek gerekir (fiziksel sustur butonu yok).
- ESP-NOW menzili açık alanda yaklaşık 50-100 m.
