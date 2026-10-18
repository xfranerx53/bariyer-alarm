# DURUM (Son güncelleme: 18 Ekim 2026, gece)

> **Yeni bir Claude sohbetine başlıyorsan:** Bu repo zip'ini sohbete yükle. Claude bu dosyayı okuyup kaldığımız yerden devam eder.
> **Claude için kural:** Her cevabın sonunda güncel `DURUM.md` dosyasını ver. Kullanıcı bunu GitHub'a yükleyecek.

## 0. Dosya düzeni (tek paket, hepsi repoda, düz klasör)
```
index.html               panel (GitHub Pages yayınlar)
manifest.webmanifest     panelin "uygulama" bilgisi (PWA: ad, simge, tam ekran)
sw.js                    servis çalışanı (PWA: panel kabuğunu hafızada tutar, internet yokken boş ekran yerine açılır)
icons/                   uygulama simgeleri (icon-192, icon-512, icon-maskable-512, apple-touch-icon)
alici.ino                alıcı ESP32 kodu (COM3)
secrets.example.h        secrets.h ÖRNEĞİ (gerçek secrets.h repoda YOK, sadece kullanıcının bilgisayarında)
.gitignore               secrets.h dosyasının GitHub'a yüklenmesini engeller
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
- **Güvenlik (v3.6):** `secrets.h` (Wi-Fi + ESP şifresi) artık repoda tutulmaz: `.gitignore` onu engeller, repoda sadece `secrets.example.h` örneği durur. Gerçek `secrets.h` kullanıcının bilgisayarında, `alici.ino` ile aynı klasörde durur (Arduino derlemesi için gerekli). Daha önce GitHub'a yüklenen şifreler repo geçmişinde ve sohbetlerde kaldığı için **değiştirilmelidir** (adımlar 6d'de). Telegram anahtarı ve nabız adresi repoya yazılmaz, Firebase'de `/secret` altında tutulur (panelden girilir).
- **ÖNEMLİ:** Firebase `apiKey` panelde herkese açıktır (normal). `auth != null` kuralı ancak **yeni kullanıcı kaydı kapalıysa** güvenlidir. Firebase konsolu > Authentication > Settings > User actions > **"Enable create (sign-up)" kapatılmalı**; yoksa herkes kendine hesap açıp veritabanını (Telegram anahtarı dahil) okuyup yazabilir.

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
| Panel (web) | `index.html` + `manifest.webmanifest` + `sw.js` + `icons/` | v2 yayında ve çalışıyor. **Yeni (henüz yüklenmedi):** telefona uygulama olarak kurulum (PWA) ve **birden fazla zamanlama programı**. Bunlar için 4 yeni parça + değişen `index.html` GitHub'a yüklenmeli (bkz. 6e). |
| Alıcı kodu | `alici.ino` | **v3.7 hazır, henüz yüklenmedi** (v3: telefondan Wi-Fi kurulumu + ışık kontrolü; v3.5: ayarlanabilir otomatik susturma + zamanlama; v3.6: Telegram, izleme, düğme, nabız; v3.7: birden fazla zamanlama programı. v3.6 da hiç yüklenmemişti, v3.7 hepsini birlikte getirir). v2 yüklü ve çalışıyor (COM3). Verici koduna dokunulmadı, tekrar yüklemek gerekmez. |
| Verici kodu | `verici.ino` | **v2 yüklü ve çalışıyor** (COM6). |

v2 özellikleri (v3 ile panele **ayrı Siren ve Işık ayar kartları** eklendi. İkisinde de: Sürekli/Aralıklı mod, aralık süresi 0,1-60 sn (sayı kutusu), en fazla süre (saniye, boş/0 = sınırsız). Işıkta ek olarak: elle aç/kapat, alarmda otomatik yanma. Siren ve ışığın süreleri birbirinden bağımsızdır (siren süresi dolsa da alarm susturulana kadar ışık yanabilir)): panelde cihaz durumu (alıcı/verici çevrimiçi, Wi-Fi sinyali, bariyer kontağı), siren testi butonu, siren süre sınırı ayarı (varsayılan 60 sn, 0 = susturana kadar), olay geçmişi, tarayıcı bildirimi, şifre sıfırlama, bağlantı uyarısı.

Verici ve alıcı aynı mesaj sürümünü kullanır (`MAGIC` 0xB4A10002). Kod değişirse ikisi birlikte yüklenmeli.

Panel düzeni: üstte durum, büyük "Alarmı sustur" butonu, iki büyük "Güvenlik açık / Güvenlik kapalı" düğmesi (seçili olan dolu: açık = yeşil, kapalı = kırmızı; kapatırken onay sormaz, kullanıcı istemedi) ve cihazlar; altta **Ayarlar** başlığı altında açılır/kapanır bölümler (Alarmı susturma, Siren, Işık, Zamanlama, Bildirim), en altta Son olaylar. Her bölümün başlığında güncel ayar özeti görünür.

## 6a. Otomatik susturma, susturma sesi ve zamanlama (alıcı v3.6 + panel)
- **Otomatik susturma (alıcı v3.5):** Panel > Ayarlar > "Alarmı susturma": "Otomatik susturma açık" kutusu + **"Kaç saniye sonra kendiliğinden susturulsun?"** kutusu (kullanıcı kendi istediği saniyeyi yazar, örnek 30). **Süre yazılırsa** alarmdan o kadar saniye sonra "Alarmı sustur" kendiliğinden uygulanır. **Boş/0 ise** siren süresi ile alarm ışığı süresinden uzun olanı beklenir; siren süresi boş/0 ise veya alarmda ışık açıkken ışık süresi boş/0 ise otomatik susturma YOKTUR. Kutu kapatılırsa hiç susturulmaz (elle). "Alarmı sustur" butonunun altında geçerli süre yazar.
- Uygulanınca siren ve ışık durur, `triggered` ve `siren` Firebase'de `false` olur, olay kaydına "Alarm kendiliğinden susturuldu" (`automute`) düşer. Sayaç alıcı kartında çalışır (internet/panel olmasa da). Her yeni alarm (tekrar çalma açıksa) sayacı baştan başlatır. Elle açılan ışık (Işığı aç) susturmadan etkilenmez.
- **Susturma sesi:** panel, alarm susturulunca (elle veya kendiliğinden) kısa "bip-bip" çalar. Kutucuk (Siren ayarları): "Alarm susturulunca bu cihazda ses çalsın", varsayılan açık, her telefon/tarayıcı için ayrı (tarayıcı hafızası). Sayfa açık olmalı; tarayıcı dokunulmamış sekmede sesi engelleyebilir. Sistemi devre dışı bırakmak bip çalmaz.
- **Zamanlama (v3.4, v3.7'de çoklu program):** Panel > Zamanlama kartı: "Sadece belirlediğim gün ve saatlerde koru" kutusu ve **en fazla 6 program** ("+ Program ekle"). Her programın başlangıç/bitiş saati, 7 gün düğmesi ve hızlı seçim düğmeleri (Hafta içi, Hafta sonu, Her gün) vardır; 2+ program varsa her birinde "Sil" çıkar. **Programlardan herhangi birinin içindeysen koruma devrededir** (birleşim). Her programın kuralı eskisiyle aynıdır (aşağıda). Hiçbir programın içinde değilken alıcı ESP-NOW alarmını yok sayar (siren çalmaz); panel "Zamanlama dışı" gösterir. Başlangıç = bitiş: seçili günlerde 24 saat. Başlangıç > bitiş: gece yarısını geçer (22:00–06:00; gün, aralığın başladığı güne göre seçilir). Saat NTP ile alınır, **Türkiye saati sabit UTC+3**. Saat alınamazsa koruma açık sayılır (güvenli taraf). Ana şalter "Güvenlik açık / Güvenlik kapalı" düğmeleridir; zamanlama onun içinde çalışır (sistem devre dışıysa zamanlama bir şey değiştirmez). Zamanlama dışına çıkılınca süren bir alarm kendiliğinden kesilmez.

## 6c. Bildirim, izleme, düğme (alıcı v3.6 + panel)
- **Telegram bildirimi:** Alıcı kartı doğrudan Telegram'a mesaj yollar (sunucu gerekmez, panel kapalıyken de çalışır; alıcının internete bağlı olması gerekir). Mesaj gelen olaylar: alarm (20 sn'de en fazla bir), verici 100 sn görünmezse/geri gelirse, ışın `Işın kaç dk kesik kalırsa uyarılsın` süresinden uzun kesik kalırsa (varsayılan 10 dk, 0 = kapalı, ışın bağlı değilken de tetiklenir), alıcı açıldığında (neden: elektrik verildi / yeniden başlatıldı / voltaj düştü / yazılım hatası), alarm susturulunca (düğme veya otomatik). Bilgi Firebase'de `secret/telegram {token, chatId}`; alıcı 60 sn'de bir okur. Panel: Ayarlar > Bildirim ve izleme > Kaydet / Test mesajı / Sil. Kaydedilen değerler ekranda gösterilmez.
- **Telefona uygulama bildirimi (FCM) YOK:** sunucu (Cloud Functions, ücretli plan) gerektirir. Yerine Telegram kullanılır.
- **Alıcı çevrimdışı uyarısı (nabız):** Alıcı kendi kendini haber veremez. Alıcı `secret/ping/url` adresine dakikada bir GET yapar; dış servis (önerilen: healthchecks.io, ücretsiz) haber kesilince e-posta/Telegram ile uyarır. Adres **https://** ile başlamalı. Panelde sayfa açıkken ayrıca alıcı/verici çevrimdışı olunca ekran uyarısı ve tarayıcı bildirimi çıkar.
- **Verici izleme:** Alıcı vericinin kalp atışını (20 sn) izler; 100 sn gelmezse "verici_offline", geri gelince "verici_online" olay kaydı + Telegram.
- **Düğmeyle sustur:** (1) Alıcı kartındaki **BOOT tuşuna kısa basış (1,5 sn'den kısa)** alarmı susturur (3 sn basılı tutmak hâlâ Wi-Fi sıfırlama). (2) İsteğe bağlı harici düğme: bir ucu **D14**, diğer ucu **GND** (elektrik kesikken bağlanır). İnternet olmasa da çalışır; sonra Firebase'e "mute / Alıcı düğmesi" yazılır. Alarm yokken basmak bir şey yapmaz.
- **Elektrik kesintisi:** Yazılım tarafı: alıcı açılınca Telegram + olay kaydı ("boot"). Donanım önerisi (henüz yapılmadı): alıcı ve vericiyi küçük bir UPS/powerbank (şarj olurken çıkış verebilen) üzerinden beslemek; modem de aynı şekilde.

## 6e. Telefona kurulum (PWA) ve çoklu zamanlama (18 Ekim gece)
- **PWA:** Panel artık telefona uygulama olarak eklenebilir (ana ekran simgesi, tarayıcı çubuğu olmadan tam ekran). Panel > Ayarlar > **"Telefona kur"** kartı: Android Chrome'da "Ana ekrana ekle" düğmesi çıkar (çıkmazsa Chrome ⋮ menüsü > Uygulamayı yükle); iPhone'da Safari > Paylaş > Ana Ekrana Ekle (kartta yazar). Uygulama olarak açıkken kart "Kurulu ✓" der.
- Servis çalışanı (`sw.js`): `index.html` önce ağdan alınır (hep güncel), ağ yoksa hafızadaki kopya açılır; simgeler ve Firebase betikleri (sürüm numaralı) hafızada tutulur. Canlı veri (alarm durumu) hafızaya hiç alınmaz. Dosyalar göreli yolla yazıldı, GitHub Pages'in `/bariyer-alarm/` alt yolunda çalışır.
- **PWA bildirim YOK:** uygulama kapalıyken telefona bildirim (FCM) hâlâ sunucu gerektirir; uyarı için Telegram kullanılır (6c). Tarayıcı bildirimi sadece uygulama/sayfa açıkken gelir.
- **Çoklu zamanlama:** bkz. 6a. Panel, alıcı yazılımı v3.7'den eskiyse ve 2+ program varsa Zamanlama kartında uyarı gösterir (eski alıcı sadece ilk programı uygular). Doğrulama: alıcı mantığı ve panel mantığı bilgisayarda aynı 9 gün/saat senaryosuyla denendi, ikisi de doğru. **Gerçek cihazda (alıcıya v3.7 yükleyip) henüz denenmedi.**
- **Yüklenecek dosyalar (GitHub, tek tek):** `index.html` (değişti), `manifest.webmanifest`, `sw.js`, `icons/` içindeki 4 png (yeni; GitHub'da "Add file > Upload files" ile `icons` klasörüyle birlikte sürüklenebilir), `DURUM.md`, `README.md`. Alıcı için `alici.ino` (Arduino'dan COM3'e yükle). Verici değişmedi.
- Panel yayına girince telefondaki eski sayfa bir kez yenilenmeyebilir; uygulamayı kapatıp açmak yeter.

## 6d. Güvenlik temizliği adımları (kullanıcı yapacak)
1. Firebase > Authentication > Settings > User actions: **"Enable create (sign-up)" kapat** (en önemlisi).
2. ESP hesabının şifresini değiştir: Firebase > Authentication > Users > `esp@bariyer.com` > Delete, sonra **Add user** ile aynı e-posta ve yeni uzun şifre. Sonra bilgisayardaki `secrets.h` içindeki `ESP_PASS`'ı güncelle ve `alici.ino`'yu COM3'e tekrar yükle.
3. Modem Wi-Fi şifresini değiştir; alıcıyı BOOT 3 sn ile kurulum moduna alıp yeni şifreyi telefondan gir.
4. GitHub'dan `secrets.h` dosyasını sil (repoda dosyayı aç > çöp kutusu simgesi > Commit). `.gitignore` ve `secrets.example.h` yüklenir. Not: dosya eski commit geçmişinde görünmeye devam eder; bu yüzden 2. ve 3. adım şart.
5. **Uyarı (18 Ekim):** yüklenen repo zip'inde `secrets.h` hâlâ vardı, yani GitHub'da silinmemiş olabilir. Madde 4'ü kontrol et.
6. Not: GitHub Pages ücretsiz hesapta sadece açık (public) repoda çalışır; repoyu gizli yapmak paneli kapatır, o yüzden gizli bilgiler repodan çıkarıldı.

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
         autoMuteOn(bool, yoksa true) autoMuteSeconds(sn, 0/boş=siren+ışık sürelerine göre)
         beamWarnMin(dk, yoksa 10, 0=kapalı) tgTest(bool, panelden Telegram testi)
         schedOn(bool) schedules(yazı: "başlangıç,bitiş,günler;başlangıç,bitiş,günler;...", dakika gün başından, günler maske bit0=Pzt..bit6=Paz; en fazla 6 program; örnek "1320,360,31;0,0,96" = hafta içi 22:00-06:00 + hafta sonu 24 saat)
         schedStart(dk) schedEnd(dk) schedDays(maske) = ESKİ tek zamanlama alanları: `schedules` yoksa alıcı bunları kullanır; panel artık ikisini birlikte yazar (eski alanlara ilk program gider)
secret/  telegram{token,chatId} ping{url}   (sadece giriş yapanlar okur; alıcı 60 sn'de bir okur)
status/  alici{lastSeen,rssi,fw}  verici{lastSeen,contact}
log/     <otomatik>{type,by,t}   type: alarm|mute|automute|arm|disarm|test|timeout|light_on|light_off|boot|verici_offline|verici_online|beam_stuck
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
0. **Alıcıya v3.7 yükle** (COM3; `alici.ino` + `secrets.h` aynı klasörde; v3.6'nın tüm özellikleri dahil) ve BOOT tuşuyla Wi-Fi kurulumunu bir kez dene. Sonra panelde iki program ekleyip "Zamanlama dışı" / "Korunuyor" durumunun doğru değiştiğini kontrol et. Sonuç henüz bildirilmedi.
0b. **Panel dosyalarını GitHub'a yükle** (6e'deki liste), sonra telefonda paneli açıp "Telefona kur" kartından ana ekrana ekle.
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
- Siren/alarm, panelden susturmak veya otomatik susturma süresinin dolmasıyla dışında ancak elektriği keserek susturulur (fiziksel sustur butonu yok).
- Alıcı çevrimdışı uyarısı dış izleme servisi (nabız) olmadan mümkün değil. Telegram mesajı alıcının internetine bağlıdır.
- Zamanlama programı sayısı en fazla 6'dır. Zamanlama saati sabit Türkiye saatidir (UTC+3, yaz/kış saati yok). Alıcı açılışta internetten saati almadan zamanlama uygulayamaz (o sürede koruma açık kalır).
- ESP-NOW menzili açık alanda yaklaşık 50-100 m.
