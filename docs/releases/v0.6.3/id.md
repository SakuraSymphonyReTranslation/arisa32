**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.3 - Perbaikan DC2DM dan Navigasi Game

Versi 0.6.3 membawa perbaikan D.C. II Dearest Marriage, konfirmasi D.C.4 dan Plus Harmony, penghalusan gambar, serta kontrol game yang bisa disembunyikan dan dipindahkan.

### D.C. II Dearest Marriage

- Popup konfirmasi memakai tulisan dari gambar asli game, sehingga bentuk dan warna caption mengikuti aset PC, termasuk saat bahasa aplikasi diganti.
- Pratinjau teks CONFIG memakai ukuran dan jarak teks yang sesuai, tampil bertahap mengikuti pengaturan kecepatan teks, dan menampilkan spasi patch terjemahan dengan benar tanpa underscore pengganti spasi. Arah pengaturan transparansi pratinjau dan jendela dialog juga diperbaiki.
- Perbaikan ukuran glyph, jarak huruf dialog, penempatan nama pembicara, serta posisi dan reset gambar tanggal.
- Backlog mengikuti empat baris, warna, area tombol, dan atlas portrait asli game. Facelog Yuto kini memakai portrait yang sesuai; lingkaran merah muda kosong adalah Yoshiyuki.
- Portrait pada jendela dialog dimuat dari arsip FACE yang benar, termasuk Yuto dan ekspresi Maya berkacamata. Posisi, pemotongan gambar, pembersihan portrait, serta pemulihannya pada save baru diperbaiki.
- Perintah preload suara tidak lagi memutar suara lebih awal. Dialog tanpa suara juga tidak mewarisi rekaman suara baris sebelumnya untuk diputar ulang dari backlog.
- Back / Sidemenu tersedia saat cerita normal. Pembatasan sidemenu dan Save/Load pada Scene Replay tetap berlaku.

Perbaikan kompatibilitas di atas khusus untuk profil DC2DM. Status dukungannya tetap beta.

### Konfirmasi D.C.4 dan D.C.4 Plus Harmony

- Pengaturan Confirmations On/Off di CONFIG kini digunakan oleh dialog terkait, termasuk Save/Load, quick save/load, kembali ke judul, replay, dan keluar.
- Pilihan untuk tidak menanyakan lagi tersimpan langsung. Mengaktifkan kembali konfirmasi dari CONFIG membuat dialog muncul lagi; All On, All Off, dan reset tetap mengikuti pengaturan asli game.

### Kontrol mengambang dan auto-hide

1. Titik tiga dan Back / Sidemenu mulai di pojok kiri atas saat game dibuka, mengikuti batas aman layar dan kamera/cutout. Posisi lama yang terlalu ke tengah tidak dipulihkan lagi.
2. Opsi **Sembunyikan otomatis tombol game** menyembunyikan kontrol setelah empat detik. Geser titik tiga atau Back / Sidemenu ke kiri untuk menyembunyikannya secara manual.
3. Penanda di tepi kiri saat kontrol tersembunyi bisa ditarik ke atas atau bawah, misalnya untuk menjauhkannya dari tombol Game Booster. Posisi penanda tersimpan dan menyesuaikan tinggi layar.
4. Ketuk atau geser penanda ke kanan untuk memunculkan kontrol kembali. Tarikan vertikal hanya memindahkan penanda, tanpa membuka kontrol atau meneruskan sentuhan ke game.
5. Pada DC4/DC4PH, titik tiga ikut bersembunyi dan muncul kembali. Pada DC2 Plus Communication/DC2 Dearest Marriage, titik tiga dan Back / Sidemenu bersembunyi dan muncul bersama. Tombol yang terlihat masih bisa dipindahkan selama sesi bermain.

### Penghalusan gambar dan FPS Meter

- Pengaturan penghalusan gambar berlaku untuk semua profil game: **Off**, **Bilinear** sebagai bawaan, dan **FXAA** pada Android 13 atau lebih baru. Pilihan tersimpan dan dapat diubah saat bermain.
- Filter diterapkan ke hasil render game, termasuk backlog, popup, menu, dan cerita. FXAA kembali ke Bilinear jika dukungan GPU yang dibutuhkan tidak tersedia. Video MediaPlayer dan kontrol antarmuka Android memakai jalur render masing-masing.
- FXAA menghaluskan sebagian tepi gambar; filter ini tidak mengubah file tekstur, menambah detail, atau menjamin hasil yang sama pada setiap gambar. Teks tertentu dapat terlihat lebih lembut. Dampak FPS dan baterai belum diukur.
- Duplikasi FPS Meter di pengaturan dihapus sehingga hanya satu entri yang tampil.

### Batasan

- DC2DM tetap beta. Pengujian mencakup adegan dan kasus yang tercatat, bukan seluruh rute, perangkat, atau patch terjemahan. Raster font Android masih dapat berbeda dari PC.
- Save lama yang belum menyimpan portrait tidak dapat memulihkan gambar yang tidak tercatat; masuk kembali ke adegan untuk memuatnya. Save baru menyimpan portrait tersebut.
- Impor save native PC DC2DM belum didukung; gunakan format portable Arisa32. Backend video desktop masih MPEG-1, sedangkan patch berisi MP4/H.264 memerlukan FFmpeg Android.
- Pemindahan penanda dilakukan manual; Arisa32 tidak mengatur posisi overlay Game Booster.

### Validasi

Rangkaian perbaikan ini telah melalui 22 suite CTest desktop, 431 pemeriksaan navigasi Android, dan 187 pemeriksaan penghalusan gambar/FPS. Pengujian navigasi memakai kontrol Activity asli di area game sintetis dengan beberapa ukuran pada I2219/API 36. Angka tersebut berasal dari rangkaian pengujian terpisah; bukan klaim pengujian semua game sampai selesai.

Lihat [verifikasi paket 0.6.3](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.3/docs/release-qa-0.6.3.md), [rincian perbaikan DC2DM](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.3/docs/dc2dm-fixes-validation.md), dan [konfirmasi serta navigasi](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.3/docs/dc4-confirmations-navigation.md).

### Unduhan

[Buka halaman unduhan v0.6.3](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.3)

- `Arisa32-v0.6.3.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.3-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Android 5.0+; arm64-v8a dan armeabi-v7a. File game dan patch terjemahan disediakan terpisah.
