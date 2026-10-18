# DURUM (Son güncelleme: 18 Ekim 2026)

> **Yeni bir Claude sohbetine başlıyorsan:** Bu repo zip'ini sohbete yükle. Claude bu dosyayı okuyup kaldığımız yerden devam eder.
> **Claude için kural:** Her cevabın sonunda güncel `DURUM.md` dosyasını ver. Kullanıcı bunu GitHub'a yükleyecek.

## 0. Dosya düzeni (tek paket, hepsi repoda, düz klasör)
```
index.html               panel (GitHub Pages yayınlar)
alici.ino + secrets.h    alıcı ESP32 kodu (COM3) + Wi-Fi/ESP şifresi
verici.ino               verici ESP32 kodu (COM6)
database.rules.json      Firebase Realtime Database kuralları (Firebase konsoluna yapıştırılan kopya)
ABT-I100-MANUAL.pdf      bariyerin kullanım kılavuzu
DURUM.md                 bu dosya
README.md
```
Kullanıcı dosyaları tek tek GitHub'a yüklüyor (zip değil; değişen dosyalar tek tek verilir). Arduino IDE'de `.ino` dosyası kendi adındaki klasörde olmalı (`alici/alici.ino`, `verici/verici.ino`) ve `secrets.h` alici.ino ile aynı klasörde durmalı.

## 1. Kullanıcı ve çalışma şekli
- Kullanıcı artık **hem yazılımı hem donanım bağlantılarını tek başına** yapıyor (daha önce donanımı başka biri yapıyordu, o kişi projeden ayrıldı). Kablolama tariflerini doğrudan kullanıcıya, her kabloyu tek tek, hangi uçtan hangi uca gittiğini yazarak ver. Her bağlantıdan önce "elektriği kes" hatırlat.
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
- Kurallar: `.read` ve `.write` = `auth != null` (sadece giriş yapanlar). Aynısı repoda **`database.rules.json`** dosyasında durur. Kural değişirse hem Firebase konsolunda (Realtime Database > Kurallar > Yayınla) hem bu dosyada güncellenmeli; yeni sohbette tekrar anlatmaya gerek yok.
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

## 5b. Bariyer ve güç kaynağı bilgileri (kılavuzdan ve fotoğraflardan)
- Bariyer: Opax **ABT-i100**, çift ışınlı, IP65. Besleme **DC 9-30V veya AC 9-20V**. Kılavuz: `docs/ABT-I100-MANUAL.pdf`.
- **Verici (transmitter) birimi:** sadece 2 terminal: ①② = POWER.
- **Alıcı (receiver) birimi:** 5 terminal: ①② = POWER, **③ = COM, ④ = NC, ⑤ = NO** (ALARM rölesi, kuru kontak, **30V DC / 1A max**). Bu modelde **tamper (kapak) terminali yok**.
- Alarm çıkışı normalde **NC**: normal durumda COM–NC kapalı, ışın kesilince veya kablo kopunca açılır. Kodumuz buna göre yazıldı (D27 INPUT_PULLUP, COM→GND).
- Bu "alıcı" birim (bariyerin) ile bizim ESP32 "alıcı" kartı farklı şeyler: karışmasın. Bariyerin alıcı biriminin alarm rölesi, **bizim ESP32 vericisine** bağlanacak.
- Bariyerin 12V'u mevcut güç kaynağından: **Formel FT01-002 LED driver**, giriş 220-240V AC, çıkış **12V DC 3A (36W)**. Vidalı terminal bloğu 5 yuvalı (soldan ortadaki boş); sağdaki yeşil LED ve ayar vidası yanında. Terminal etiketleri fotoğrafta okunmuyor; kullanıcıdan güç kaynağına dokunmaması istendi (220V içerir).
- **Kullanıcıda multimetre yok.** Doğrulama panelden yapılır: "Bariyer ışını: Normal / Kesik" göstergesi.
- Verici ESP32 için güç: şimdilik **telefon şarj adaptörü + Type-C kablo** (220V ile uğraşmamak için). Dışarı kurulumda buck converter düşünülecek.

## 6. Yazılım durumu
| Parça | Dosya | Durum |
|---|---|---|
| Panel (web) | `index.html` | **v2 yayında ve çalışıyor** (kullanıcının ekran görüntüsüyle doğrulandı). Küçük düzeltme: siren süre listesi açıkken kendiliğinden sıfırlanmıyor (bu sürümde, GitHub'a yüklenmesi gerek). |
| Alıcı kodu | `alici.ino` | **v3.3 hazır, henüz yüklenmedi** (v3: telefondan Wi-Fi kurulumu + ışık kontrolü; v3.3: otomatik susturma). v2 yüklü ve çalışıyor (COM3). Verici koduna dokunulmadı, tekrar yüklemek gerekmez. |
| Verici kodu | `verici.ino` | **v2 yüklü ve çalışıyor** (COM6). |

v2 özellikleri (v3 ile panele **ayrı Siren ve Işık ayar kartları** eklendi. İkisinde de: Sürekli/Aralıklı mod, aralık süresi 0,1-60 sn (sayı kutusu), en fazla süre (saniye, boş/0 = sınırsız). Işıkta ek olarak: elle aç/kapat, alarmda otomatik yanma. Siren ve ışığın süreleri birbirinden bağımsızdır (siren süresi dolsa da alarm susturulana kadar ışık yanabilir)): panelde cihaz durumu (alıcı/verici çevrimiçi, Wi-Fi sinyali, bariyer kontağı), siren testi butonu, siren süre sınırı ayarı (varsayılan 60 sn, 0 = susturana kadar), olay geçmişi, tarayıcı bildirimi, şifre sıfırlama, bağlantı uyarısı.

Verici ve alıcı aynı mesaj sürümünü kullanır (`MAGIC` 0xB4A10002). Kod değişirse ikisi birlikte yüklenmeli.

## 6a. Otomatik susturma ve susturma sesi (v3.3 alıcı + panel)
- Panel > Siren ayarları: **"Alarm kendiliğinden susturulsun (saniye)"** kutusu. Boş/0 = kapalı (eski davranış: alarm sen butona basana kadar "susturulmadı" kalır). Süre yazılırsa, alarm butona basılmasa bile o süre sonunda **susturuldu** olur: siren ve ışık durur, `triggered` ve `siren` Firebase'de `false` olur, olay kaydına "Alarm kendiliğinden susturuldu" (`automute`) düşer.
- Sayaç **alıcı kartında** çalışır (internet/panel olmasa da). Ayarın okunması için alıcının internete bağlı olması gerekir; bir kez okunduktan sonra internet kopsa bile geçerlidir. Her yeni alarm (tekrar çalma açıksa) sayacı baştan başlatır.
- Siren süre sınırından farkı: siren süre sınırı sadece sireni durdurur, alarm "susturulmadı" kalır. Otomatik susturma alarm durumunun kendisini de kapatır. İkisi bağımsız ayarlanır (otomatik susturma süresi siren süresinden kısaysa siren de o anda durur).
- **Susturma sesi:** panel, alarm susturulunca (elle veya kendiliğinden) kısa bir "bip-bip" çalar. Kutucuk: "Alarm susturulunca bu cihazda ses çalsın" (varsayılan açık, her telefon/tarayıcı için ayrı, tarayıcı hafızasında). Sayfa açık olmalı; tarayıcı, hiç dokunulmamış sekmede sesi engelleyebilir (ilk dokunuştan sonra çalışır).
- Sistemi devre dışı bırakmak bip çalmaz (sadece alarm susturulunca çalar).

## 6b. Alıcıda telefondan Wi-Fi kurulumu (v3)
- **Sıfırlama:** Alıcı ESP32 açıkken kart üzerindeki **BOOT** tuşuna **3 saniye** basılı tut (LED yanar, kart yeniden başlar). Kart açılırken BOOT'a basma (yükleme moduna girer).
- **Kurulum modu:** Kart kendi Wi-Fi ağını açar: ağ adı `Bariyer-Alici-Kurulum`, **şifresiz (açık ağ)** (kullanıcı bilerek böyle istedi; kurulum modundayken yakındaki biri Wi-Fi bilgisini değiştirebilir). Telefon bağlanınca kurulum sayfası kendiliğinden açılır (açılmazsa tarayıcıya `192.168.4.1`). Ağ listesinden seç, şifreyi yaz, Kaydet. Kart yeniden başlar ve yeni ağa bağlanır. Kurulum modunda mavi LED yanıp söner.
- Bilgi kalıcı hafızada (NVS/Preferences) saklanır. `alici/secrets.h` içindeki Wi-Fi sadece **hiç kurulum yapılmamışsa** ilk açılışta kullanılır. Bir kez sıfırlama/kurulum yapıldıktan sonra hafızadaki bilgi geçerlidir.
- Açılışta Wi-Fi'ye 20 sn içinde bağlanamazsa kurulum modu otomatik açılır; kayıtlı ağı 30 sn'de bir arka planda tekrar dener (telefon bağlı değilse) ve bağlanırsa kendiliğinden yeniden başlar. Yani modem sonradan açılırsa kurulum gerekmez.
- **Kurulum modunda da siren çalışır:** ESP-NOW alarmı alınır, röle çeker, süre sınırı işler. Sadece Firebase/panel o sırada çalışmaz.
- Kart sadece 2.4 GHz ağlara bağlanır.

## 7. Firebase veri yapısı
```
alarm/   armed(bool) siren(bool) triggered(bool) lastTrigger(ms) test(bool) sirenSeconds(sayı)
         sirenRepeat(bool, yoksa true) sirenMode("steady"|"blink") sirenMs(ms, 100-60000) sirenSeconds(sn, 0/boş=susturana kadar, yoksa 60)
         lightOn(bool) lightMode("steady"|"blink") lightMs(ms, 100-60000) lightAlarm(bool, yoksa true) lightSeconds(sn, 0=alarm sürdüğü sürece)
         autoMuteSeconds(sn, 0/boş=kendiliğinden susturma yok)
status/  alici{lastSeen,rssi,fw}  verici{lastSeen,contact}
log/     <otomatik>{type,by,t}   type: alarm|mute|automute|arm|disarm|test|timeout|light_on|light_off
```
`test`, `sirenSeconds`, `status`, `log` ilk kullanımda kendiliğinden oluşur, elle eklemek gerekmez.

## 8. Doğrulananlar / doğrulanmayanlar
- ✅ Firebase kurulumu, kurallar, `alarm` verisi
- ✅ Panel v2 girişi ve görünümü (Korunuyor, cihazlar, test butonu)
- ✅ Alıcı ve verici v2 derlendi, yüklendi, panelde ikisi de "Çevrimiçi"
- ✅ **ESP-NOW çalışıyor:** verici kalp atışları alıcıya ulaşıp panele yansıyor (verici → alıcı yolu kanıtlı)
- ✅ Panelden "Siren testi" → alıcı rölesi "tık" yapıyor (panel → Firebase → alıcı → röle yolu kanıtlı)
- ✅ Panelde bariyer ışını "Kesik / bağlı değil" görünüyor. Bu **doğru**: verici D27'ye henüz hiçbir şey bağlı değil, pin boşta HIGH okuyor.
- ❓ Gerçek alarm (D27 kontağı açılınca) uçtan uca henüz denenmedi. Bariyer bağlanınca denenecek.
- ❓ Siren süre ayarı panelde 30 sn görünüyordu; varsayılan 60. Kullanıcı seçmiş olabilir, önemli değil.
- ❌ Bariyer bağlanmadı, siren ve ışık yok

## 9. Sıradaki adımlar
0. **Alıcıya v3.3 yükle** (COM3; `alici.ino` + `secrets.h` aynı klasörde) ve BOOT tuşuyla Wi-Fi kurulumunu bir kez dene. Sonuç henüz bildirilmedi.
1. **Bariyeri vericiye bağla** (kılavuza göre terminaller belli): bariyerin ALICI biriminde ③ COM → verici ESP32 GND, ④ NC → verici D27. Kullanıcı jumper kablonun bir ucunu kesip soyarak vidalı terminale takacak. Verici USB şarj adaptörüyle beslenecek. Önce elektrik kapalı bağlanır.
2. Panelde "Bariyer ışını: Normal" görünmeli (ışınlar hizalıyken). Ters görünürse NC/NO karışmıştır.
3. Bariyer bağlanınca panelde "Bariyer ışını: Normal" görünmeli. Işın kesilince alarm: verici → alıcı röle "tık" → panelde alarm ve olay kaydı. Panelden sustur.
4. Çift ışınlı bariyerde tek ışın mı yoksa iki ışın birlikte mi alarm veriyor, kılavuzdan/denemeyle öğren.
5. Siren (12V DC) geldiğinde röle kanal 1'e bağla: 12V(+)→COM, NO→siren(+), siren(−)→12V(−).
6. **Işık:** yazılımı hazır (alıcı v3 + panel): röle kanal 2 (IN2→D25), panelden aç/kapat, sürekli/aralıklı, aralık süresi, alarmda otomatik yanma. Donanım: 12V(+)→Röle2 COM, Röle2 NO→ışık(+), ışık(−)→12V(−). Kullanıcı ışığı şu an test ediyor; sonucu henüz bildirilmedi.
7. Sahaya kurulum: su geçirmez kutu, güç, Wi-Fi sinyali (alıcı şu an -68 dBm, orta), bariyer hizalaması.
8. Güvenlik: ESP şifresi ve Wi-Fi şifresi değiştirilecek (kullanıcı kendisi yapacak).

## 10. Bilinen sınırlar
- Alıcı internetsizken panel bilgisi güncellenmez; siren yine çalar, siren süre sınırıyla kendiliğinden susar.
- Siren/alarm, panelden susturmak veya "kendiliğinden susturma" süresi yazmak dışında ancak elektriği keserek susturulur (fiziksel sustur butonu yok).
- ESP-NOW menzili açık alanda yaklaşık 50-100 m.
