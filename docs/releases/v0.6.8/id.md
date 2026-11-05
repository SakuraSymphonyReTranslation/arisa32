**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.8 - Layar Penuh, COMU, dan Transisi Gear

Versi 0.6.8 menambahkan toggle layar penuh pada Android, memperbaiki phonechat/COMU yang tertinggal setelah pergantian adegan, dan memulihkan transisi gear membesar serta mengecil pada D.C.4 dan D.C.4 Plus Harmony.

### Layar penuh

- Pilihan Layar penuh tersedia di pengaturan setiap game dan menu saat bermain. Aktif menyembunyikan navigation bar Android; Nonaktif menampilkannya kembali. Geser dari tepi layar untuk menampilkan navigasi sementara.
- Pengaturan tersimpan per game dan langsung berlaku saat bermain. Nilai bawaan untuk game yang belum memiliki pengaturan ini adalah Nonaktif.
- Fit to Screen mempertahankan proporsi gambar, sedangkan Stretch memenuhi area layar yang tersedia. Keduanya mengikuti perubahan area layar penuh, termasuk koordinat sentuhan, posisi kontrol mengambang, dan tampilan video.
- Mode layar penuh diterapkan kembali ketika aplikasi mendapat fokus atau dilanjutkan. Navigation bar kembali tampil saat membuka perpustakaan game.

### Phonechat / COMU DC4 dan DC4 Plus Harmony

- Kotak HP dan riwayat COMU dibersihkan ketika skrip menerapkan background berikutnya. Phonechat tidak lagi tertinggal pada narasi setelah percakapan berakhir.
- Gambar adegan sebelumnya tetap memuat HP selama transisi keluar, sampai adegan berikutnya terlihat. Preload background, narasi biasa, dan penanda teks tidak menutup COMU lebih awal.
- Perbaikan mengikuti perintah skrip, sehingga berlaku bersama logika spasi manual patch Indonesia maupun format Sakuramusubi. Pengaturan font dan tata letak teks COMU dari 0.6.7 tetap tersedia.

### Transisi gear

- Mendukung efek gear membesar dan mengecil pada titik yang ditentukan skrip DC4/DC4PH, menggunakan aset asli game.
- Posisi, pusat pembesaran, tahapan skala, dan durasi mengikuti engine PC. Rekaman referensi diperiksa pada bagian transisi, frame demi frame, lalu dicocokkan dengan perintah native dan skrip.
- Efek mendukung penantian skrip, skip, dan pemulihan keadaan dari save. Pengaturan Visual effect untuk sakura dan cahaya melayang tidak menonaktifkan transisi gear.

### Perbaikan sebelumnya

Seluruh perbaikan 0.6.7 tetap disertakan: FXAA Android 5+, ukuran dialog MS Gothic dan font kustom, teks COMU Sakuramusubi yang lebih mudah dibaca, spasi manual patch, efek sakura/cahaya melayang, referensi replay suara, dan prioritas sprite. Lihat [catatan 0.6.7](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.7/docs/releases/v0.6.7/id.md) untuk rinciannya.

### Pengujian dan batasan

Pengujian fitur lulus 440 pemeriksaan layar pada iQOO Z9x dengan Android 16/API 36 serta 20 pemeriksaan preferensi pada host. Regresi COMU, efek, dan scene juga lulus, termasuk aset efek asli kedua game, skip, dan pemulihan save. Dialog layar penuh diperiksa dalam bahasa Indonesia, Inggris, dan Jepang pada ukuran teks 200%.

Android 5 sampai 10 memakai immersive system-UI flags, sedangkan Android 11 ke atas memakai WindowInsetsController. Pengujian langsung dilakukan pada Android 16; seluruh versi Android, rute, dan patch belum diuji. Rekaman memiliki frame berulang dan lompatan, sehingga durasi gear mengikuti engine PC. Dukungan DC2 Dearest Marriage tetap beta. File game, patch terjemahan, dan font kustom disediakan terpisah.

Lihat [verifikasi paket 0.6.8](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.8/docs/release-qa-0.6.8.md) dan [validasi fitur](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.8/docs/fullscreen-comu-gear-validation.md).

### Unduhan dan pemasangan

[Buka halaman unduhan v0.6.8](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.8)

- `Arisa32-v0.6.8.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.8-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Pasang pembaruan tanpa menghapus aplikasi untuk mempertahankan data. Pembaruan di dalam aplikasi tersedia sejak 0.6.4; Android tetap meminta konfirmasi pemasangan. Android 5.0+; arm64-v8a dan armeabi-v7a.
