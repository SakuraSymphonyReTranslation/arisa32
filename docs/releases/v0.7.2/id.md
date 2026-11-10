**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.7.2 - Dukungan DC2TY, Save Format PC, dan Perbaikan Teks DC2DM

Rilis 11 November 2026 ini menambahkan dukungan awal D.C. II To You ~Da Capo II To You~ (DC2TY), termasuk menu asli, decoder voice, gambar pergantian tanggal, serta save dengan judul skenario dan format PC. Preview CONFIG dan backlog D.C. II Dearest Marriage (DC2DM) juga lebih mudah dibaca dengan MS Gothic maupun Pretendard JP Bold.

### D.C. II To You

- Opening, animasi Title, dan pemilih empat cerita mengikuti alur game. Latar Title berubah dari hujan menjadi cerah setelah keempat cerita selesai. Pilih cerita untuk melihat pratinjau, lalu pilih lagi untuk memulainya.
- Extra, CG Mode, Scene Replay, Music Mode, dan Config memakai skrip serta artwork asli. Impor `system.dat` mengambil progres PC dan konfigurasi awal; membuka game lagi mempertahankan progres yang lebih jauh dan pengaturan Arisa32 yang lebih baru.
- Decoder baru mendukung codec voice asli DC2TY, termasuk dialog dan pengumuman menu. Pengaturan volume serta mute per karakter berlaku pada dialog; mute karakter tidak membisukan pengumuman menu. Masalah codec ini tidak memerlukan penyalinan ulang berkas game.
- Music Mode menampilkan judul lagu yang dipilih pada kotak atas, mempertahankannya saat hover atau halaman berubah, dan menjalankan Play, Stop, Repeat, serta All Play sesuai skrip.
- Scene Replay mempertahankan tanggal, label Prologue/Epilogue, dan bingkai skenario saat disorot atau ditekan. Baris kosong pada halaman terakhir tidak lagi menjadi tombol. Pilihan dialog memakai keadaan tekan asli dan menempatkan teks Latin sesuai panel.
- Konfirmasi reset Config mempertahankan latar menu dan mencegah slider di belakangnya menerima input. Tombol Yes menjalankan reset asli tanpa konfirmasi kedua.
- Sprite close-up memakai ukuran bitmap sebenarnya untuk posisi dan ekspresinya. Pratinjau CG memakai gambar sumber, termasuk gerakan vertikal pada CG tinggi.
- Pergantian adegan menangani mask berarah, efek pecahan layar, dan dissolve. Kembali ke Title mengikuti efek serta waktu tunggu skrip; pause menahan waktu presentasi.
- Gambar pergantian tanggal (eyecatch) kini tampil lengkap dan menunggu input. Keempat variasi latar, angka tanggal, hari, serta label Prologue/Epilogue memakai aset asli game.

### Save DC2TY dengan judul skenario dan format PC

- Save/Load menampilkan judul skenario yang sedang dimainkan, tanggal simpan, dan ikon pilihan. Tabel judul mengikuti 129 entri executable asli, termasuk `始まりの始まり` untuk awal Hydrangea.
- Quicksave memakai `dc2ty00.dat`; salinan save manual terakhir memakai `dc2ty01.dat`. Sebanyak 48 slot manual mengikuti penomoran PC: halaman pertama memakai `dc2ty02.dat` sampai `dc2ty04.dat`, halaman kedua mulai `dc2ty08.dat`. Angka yang dilewati mengikuti susunan slot PC.
- Setiap `.dat` memakai struktur native PC dan memiliki companion `.dat.arisa` untuk state tambahan Arisa32, termasuk gerakan sprite, kamera, dan posisi putar BGM serta SE berulang. Jika PC mengubah `.dat`, Arisa32 mengabaikan companion lama yang tidak cocok.
- Save lama `.dc4sav` tetap dapat dimuat. Penyimpanan berikutnya memakai nama native; backup ZIP menyertakan `.dat.arisa`.

Untuk memindahkan slot ke PC, salin `.dat` ke folder save game PC. Saat memindahkan save antarinstalasi Arisa32, bawa pasangan `.dat` dan `.dat.arisa`. Perhatikan batas verifikasi PC di bawah sebelum mengandalkan cross-save.

### Keterbacaan dan sentuhan backlog DC2DM

- Teks Latin MS Gothic pada preview CONFIG dan backlog dirasterisasi langsung pada resolusi keluaran. Huruf lebih jelas dengan grid, posisi, dan waktu tampil asli.
- Pretendard JP Bold memakai ukuran yang lebih terbaca dan lebar huruf proporsional pada preview CONFIG serta backlog. Pembungkusan teks mengikuti lebar huruf; nama pembicara tetap berada dalam area nama dan ikon voice tetap terlihat.
- Backlog merespons ketukan cepat dan gerakan jari untuk memilih baris. Melepas jari, menggulir, atau mengganti mode cursor mempertahankan pilihan sesuai perilaku menu asli. Baca dari sini kembali ke baris pertama yang terlihat.

### Batas dukungan

- DC2TY masih merupakan dukungan awal; seluruh rute belum diverifikasi sampai selesai. Artwork backlog native DC2TY belum didukung.
- Dalam emulasi terisolasi, reader dan loader dari executable PC asli menerima struktur save hasil ekspor serta menyalin seluruh isinya. Pemulihan adegan dan kelanjutan permainan penuh pada game PC belum diverifikasi. Impor menolak konfigurasi native yang belum dapat dipulihkan, yaitu offset latar/pan bukan nol, chip pada plane 0, atau chip bersyarat. Rincian tersedia pada [catatan kompatibilitas save DC2TY](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.2/docs/dc2ty-save-compatibility.md).
- Companion menyimpan nama resource voice dialog untuk diputar ulang dari awal. Posisi voice yang sedang diputar belum disimpan.
- Pengujian voice mencakup seluruh 7.800 entri arsip asli dan probe decoder Android. Pemutaran melalui menu game ke speaker belum diuji langsung pada revisi ini. Lihat [laporan voice DC2TY](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.2/docs/dc2ty-voice-analysis.md).

### Unduhan dan pemasangan

[Buka halaman unduhan v0.7.2](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.2)

- `Arisa32-v0.7.2.apk`: paket rilis untuk penggunaan biasa.
- `Arisa32-v0.7.2-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Kedua APK bersifat universal, masing-masing berisi ARM64 dan ARMv7, serta mendukung Android 5.0+ (API 21). Pembaru di dalam aplikasi mengunduh paket rilis universal.

Pasang pembaruan tanpa menghapus aplikasi agar data tetap tersimpan. Berkas game dan patch terjemahan disediakan terpisah.
