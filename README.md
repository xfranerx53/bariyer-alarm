# bariyer-alarm
Panel: https://xfranerx53.github.io/bariyer-alarm/

Opax ABT-i100 bariyer + 2 adet ESP32 (verici/alıcı, ESP-NOW) + Firebase + GitHub Pages paneli.

- `index.html`: kontrol paneli (GitHub Pages yayınlar)
- `verici.ino`: bariyer tarafındaki ESP32
- `alici.ino`: siren tarafındaki ESP32. `secrets.h` repoda yok: `secrets.example.h` örneğini kopyalayıp `secrets.h` yap (bilgisayarında kalır)
- `database.rules.json`: Firebase veritabanı kuralları
- `DURUM.md`: projenin güncel durumu. Yeni bir Claude sohbetine bu repoyu verirsen buradan devam eder.
