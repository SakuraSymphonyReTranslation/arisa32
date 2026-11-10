# Profil DC2TY di Arisa32

Implementasi memakai rekaman pengguna, MES/aset di `E:\Games\DC2TY\AdvData`, dan pemeriksaan struktur executable asli. Analisis rekaman terpisah ada di [dc2ty-recording-analysis.md](dc2ty-recording-analysis.md). Semua 8.455 frame didekode dan diukur; inspeksi visual memakai sampel seluruh durasi dan frame terpilih, bukan pemeriksaan manual setiap frame.

Tindak lanjut 10 November 2026: laporan voice yang belum terdengar mengungkap codec XPCM `0x0303` yang belum didukung. Decoder telah ditambahkan, seluruh 7.800 voice berhasil didekode, dan APK perbaikan dipasang ke HP. Detail, hasil uji decoder Android, dan hash build perbaikan voice ada di [dc2ty-voice-analysis.md](dc2ty-voice-analysis.md). Hasil dan hash di bawah mencatat build awal sebelum perbaikan voice.

Pembaruan save PC dan judul skenario dipasang ke HP pada 10 November 2026 pukul 23:40:41 (UTC+7). Format, verifikasi, batas dukungan, serta hash APK tersebut tercatat di [dc2ty-save-compatibility.md](dc2ty-save-compatibility.md).

## Perilaku yang ditambahkan

- Profil Android `dc2ty` dikenali dari header MES `0x5F69`, dengan nama **D.C. II To You ~Da Capo II To You~**. Parser mengutamakan header legacy yang tepat agar byte skrip tidak salah dikenali sebagai dialek modern.
- Opening `MOVIE/dc2ty_op.mpg` diputar sebelum title. Selesai, skip, dan kegagalan pemutar menuju alur title yang sama. Callback Java boleh selesai secara sinkron saat inisialisasi; flag pemutar dipasang sebelum memanggil Java agar callback tidak hilang.
- Title mengikuti bank progres native. Latar hujan berlaku sebelum keempat cerita selesai; latar cerah berlaku setelah keempat flag selesai terkumpul. Kembali dari cerita juga menjalankan animasi title.
- Animasi title menggunakan aset langit, pan sekolah dengan jejak gambar, fase menetap, lalu logo/menu. Durasi logis native pada kecepatan normal adalah 6.445 ms; pengumuman dimulai 600 ms sebelum tombol muncul. Rekaman memperlihatkan 7,93 detik, sehingga durasi wall-clock rekaman tidak dipaksakan menjadi konstanta engine.
- Pemilih cerita memakai empat pilihan, pratinjau terlebih dahulu, lalu aktivasi kedua untuk memulai. Crossfade 255 ms mengunci input sementara. Back mengembalikan pratinjau ke pemilih lalu title; panah dan Enter/Space mendukung pilihan melalui keyboard desktop.
- Extra, CG Mode, Scene Replay, Music Mode, Config, pilihan suara sistem, dan mute 16 karakter memakai skrip/artwork native. Pengumuman Continue, Extra, CG, Music, Scene, Config, simpan/muat yang diterima, sampel volume, serta pilihan karakter mengikuti aset `sys_*` milik DC2TY.
- Mute dialog tidak membisukan pengumuman menu akibat status karakter terakhir. Dialog berikutnya mengambil kembali pengaturan volume/mute karakter yang sesuai.

## Kejernihan dan animasi CG

Pratinjau penuh mengambil gambar CRX sumber, bukan thumbnail galeri. DC2TY memakai renderer galeri DC2PC, termasuk jalur framebuffer beresolusi sesuai viewport pada Android dan desktop. Android membatasi framebuffer ini sampai 3.840 x 2.160; batas tersebut tidak menambahkan detail melebihi aset asli.

CG tinggi yang mempunyai perintah pan mengikuti urutan skrip dan input lanjut, mempertahankan rasio gambar serta menelusuri bagian gambar yang berada di luar viewport. Pengujian memakai aset asli berukuran 800 x 947 dengan rentang pan 347 piksel. Piksel awal, tengah (offset 40), dan akhir (offset 347) dibandingkan langsung dengan hasil decode sumber pada framebuffer 640 x 360 dan 1.920 x 1.080. Gambar CG tersebut tidak disalin ke laporan.

## Progres, konfigurasi, dan save

Importer membaca struktur `system.dat` DC2TY berukuran 1.848 byte. File ditemukan di Savegame, SaveData, atau jalur native `dacapo2_ty/dacapo2_ty`, termasuk di bawah AdvData. Pemeriksaan format diselesaikan sebelum mengubah state. Flag penyelesaian/CG/scene/music digabung tanpa menurunkan progres; konfigurasi native hanya diambil pada impor pertama bila belum ada profil emulator. Membuka game lagi tidak menimpa pilihan suara dan pengaturan yang lebih baru.

Posisi native yang dikonfirmasi dari reader/writer executable:

| Isi | Offset | Jumlah |
| --- | --- | --- |
| Bank 100 | `0x70` | 100 byte |
| CG | `0xD4` | 512 byte |
| Scene | `0x2D4` | 1.024 byte |
| Music | `0x6D4` | 100 byte |

Reader asli memeriksa jumlah byte yang dibaca; validasi panjang persis merupakan kebijakan importer Arisa32. Pengaturan DWORD pada header dipetakan terpisah. Flag music native bank 95 dipetakan ke bank katalog host 96 agar tidak menimpa pengaturan suara host.

Tindak lanjut save menambahkan baca/tulis slot PC DC2TY: `dc2tyxx.dat` berukuran `0x2944`, dengan `.dat.arisa` untuk state tambahan Arisa32. Snapshot `.dc4sav` lama tetap dapat dibaca. Nama skenario, penomoran slot, ikon, dan salinan save terakhir mengikuti PC; detail dan batas verifikasinya ada di [dc2ty-save-compatibility.md](dc2ty-save-compatibility.md). Artwork backlog native DC2TY belum didukung oleh `LogScreen`; bagian tersebut tidak ditampilkan dalam rekaman dan tidak termasuk perubahan ini.

## Verifikasi

| Pemeriksaan | Hasil |
| --- | --- |
| Parser/corpus DC2TY | PASS: 153 skrip, 182.740 perintah, 18.726 label; batas perintah/label dan identitas profil valid |
| Opening dengan MoviePlayer desktop | PASS sampai selesai: 4.318 frame video, 6.355.584 frame audio; alur skrip movie ke main, dialogue, dan snapshot diuji. Penolakan save PC pada build awal digantikan dukungan native pada tindak lanjut save. |
| Menu DC2TY dengan data asli | PASS: title hujan/cerah dan waktu pengumuman, selector sentuh/keyboard, 16 suara, Config, impor atomik/preservasi settings, piksel sumber CG/pan, dispatch replay dan larangan save saat replay |
| Native system.dat melalui host SDL | PASS: screenshot headless menunjukkan title cerah dari progres PC yang terpasang; output/saves pengujian diarahkan ke Temp |
| Profil Android | PASS: 349 pemeriksaan Java lokal |
| Regresi CTest | 36 dari 37 tes lulus pada putaran penuh; kegagalan tombol rute DC2PC diperbaiki, lalu tes menu DC2PC, menu/profil DC2TY, dan system menu lulus pada putaran akhir |
| Alur DC2PC dengan data asli | PASS: `dc2pc_flow_test` selesai dengan exit 0; fixture galeri mengikuti transisi dan siklus CG asli 1223, 1224, 1230 |
| Audio/font/media | PASS: termasuk mute semua aktor DC2TY, pengumuman sistem, MS Gothic/Pretendard DC2DM, backlog DC2DM, galeri DC2PC, parser/VM, dan antrean audio movie |
| Build | PASS: desktop dan APK debug arm64-v8a + armeabi-v7a |
| Whitespace | `git diff --check` PASS |

Pengujian awal menggunakan renderer/headless desktop dan pemeriksaan Java lokal; pemasangan APK serta probe decoder di HP dilakukan pada tindak lanjut yang ditautkan di atas. Urutan callback MediaPlayer saat runtime serta interaksi Activity/GLES akhir belum diuji pada HP. Jalur callback ditinjau secara terpisah; hasil decode MoviePlayer desktop tidak dianggap sebagai bukti runtime FFmpeg Android.

Pemeriksaan tambahan menemukan guard rute DC2PC yang memakai flag bank modern di profil klasik. Guard tersebut kini khusus profil non-klasik sehingga tombol native yang telah terbuka dapat menjalankan skripnya. Tes alur galeri lama juga diperbaiki agar menunggu transisi dan mengikuti siklus asli tiga CG yang dibuka pada fixture.

APK: `E:\Games\Adv32roid\android\app\build\outputs\apk\debug\app-debug.apk`, versi 0.7.1 / code 19, 18.540.350 byte.

```text
APK SHA256
696e5b77147cece6caa2b3b5ed1a3d209c4ac695e2f16442f8e6a02b61f7de80

arm64-v8a libarisa32.so SHA256
ebb2fb9197bcda1e03981b783ec21200de6d8201ef57d9e4c09f66e5a17a8ebe

armeabi-v7a libarisa32.so SHA256
6bb3d36943072e8c30e376e6188bd475db616fb03fc4fa9eb43f37ff66225078
```

Keputusan visual mengikuti referensi pengguna: warna, font artwork, posisi, ornamen, dan rasio 4:3 dipertahankan karena merupakan tampilan game asli. Gerakan title dan CG ada untuk mereproduksi skrip, tanpa animasi tambahan. Arah perubahan ini memakai ENERGY 1 / RHYTHM 1 / MOTION 2.

## Antislop during: delivery gate

- Hard Gate PASS dalam cakupan implementasi: state terkunci/terbuka, aksi menu, Back, keyboard selector, fase input terkunci, impor rusak, dan rendering CG diuji. Build dijalankan. Tidak ada copy, statistik, atau aset visual buatan baru. Ini tidak mengklaim audit aksesibilitas seluruh artwork game ataupun pengujian UI HP.
- Purpose Gate PASS: artwork/warna/komposisi berasal dari game yang diminta; animasi berasal dari skrip dan rekaman. Keputusan rasio 4:3 serta sampling sumber CG menjaga tampilan dan detail aset.
- Liveliness PASS: ENERGY 1 / RHYTHM 1 / MOTION 2 untuk perubahan ini; pusat perhatian dan ornamen mengikuti native, gerakan title/CG mempunyai fungsi yang dikonfirmasi skrip.
- Craftsmanship PASS: renderer diperiksa pada title, Voice Config, selector dan pratinjaunya; CG diperiksa lewat piksel sumber pada dua resolusi; pengaturan dan progres tetap konsisten saat impor ulang. Batas verifikasi perangkat dan backlog native dinyatakan eksplisit; verifikasi slot save PC dicatat pada tindak lanjut save.
