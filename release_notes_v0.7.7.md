**Bahasa Indonesia** | [English](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.7/docs/releases/v0.7.7/en.md)

# Arisa32 v0.7.7 - Save lintas bahasa Dal Segno, galeri CG, dan waktu mulai game

Rilis 28 November 2026 memperbaiki pemuatan save saat pemain Dal Segno berganti antara Inggris resmi dan patch Indonesia. Di D.C.4 dan D.C.4 Plus Harmony, slot dari segmen D.C. kini menampilkan judul skenarionya. Waktu dari menekan **Jalankan** hingga frame pertama juga lebih singkat pada enam game yang diuji.

### Dal Segno MangaGamer · Save dan Load lintas bahasa

- Pada checkpoint dialog yang didukung, save dari **OFFICIAL ENG** bisa dilanjutkan dengan **ID PATCH**, dan sebaliknya. Sebelumnya, terjemahan yang lebih panjang atau pendek dapat menggeser posisi dialog di berkas MES sehingga Load gagal membaca checkpoint.
- Saat memuat slot, Arisa32 mencocokkan posisi simpan dengan skenario yang aktif. Nama pembicara dan dialog mengikuti bahasa yang dipilih. Pemain bisa menyimpan kembali di bahasa kedua dan melanjutkan di bahasa pertama.
- Jika Arisa32 dapat menulis ke folder save game, kedua bahasa memakai folder itu bersama. Jika Arisa32 menyimpan slot di penyimpanan internal aplikasi, tiap bahasa memiliki folder sendiri. Untuk memindahkan slot, gunakan **Ekspor simpanan** di bahasa asal, pilih bahasa tujuan, lalu gunakan **Impor simpanan**. Impor mengganti berkas bernama sama di folder tujuan.
- Jika penerjemah merevisi teks sambil mempertahankan susunan perintah skenario, Load menampilkan kalimat yang sudah direvisi. Jika patch tidak menyertakan satu berkas MES, dialog pada skenario itu memakai data Inggris resmi. Kompatibilitas ini berlaku untuk perubahan teks pada checkpoint yang didukung. Patch yang mengubah alur atau pilihan cerita memerlukan pengujian tersendiri.

### Dal Segno MangaGamer · Dialog panjang di textbox

- Dialog satu atau dua baris memakai posisi semula. Saat teks Inggris atau Indonesia membungkus menjadi tiga baris atau lebih, baris pertama dapat naik sehingga baris terakhir tetap muat. Ruang untuk nama pembicara tetap tersedia.
- Posisi dihitung dari dialog lengkap sejak huruf pertama muncul. Kalimat yang ditampilkan bertahap tetap berada di tempat ketika baris ketiga mulai terisi. Aturan yang sama berlaku untuk pemisah baris di skrip dan pembungkusan otomatis.

### Galeri CG dan judul slot D.C.4

- Saat CG dibuka dari galeri, hanya ada satu tombol kembali pada kontrol melayang. Tombol itu mengembalikan pemain ke galeri. **Back / Side Menu** atau **Back / Upper Menu** muncul kembali sesuai profil setelah pemain keluar dari tampilan CG. Menyembunyikan dan menampilkan ulang kontrol saat melihat gambar juga mengikuti aturan ini.
- Pada layar Save dan Load **D.C.4** serta **D.C.4 Plus Harmony**, slot dari segmen D.C. kini menampilkan **Ｄ．Ｃ．** sesuai judul Jepang untuk `DC4_DC99990101A`. Jika tabel Indonesia berisi judul ganda `D.C. / D.C.`, slot menampilkannya sekali sebagai **D.C.**

### Koleksi game dan deteksi KiriKiri

- Pemindaian otomatis dan pemindaian seluruh penyimpanan melewati **D.C.4 Sweet Harmony** (`DC4_SH`) dan **D.C.4 Fortunate Departures** (`DC4_FD`). Keduanya menggunakan KiriKiri. Jika folder dipilih manual atau entri lama dibuka, Arisa32 menjelaskan bahwa game memakai KiriKiri dan aplikasi mendukung ADV32.
- Nama folder bukan satu-satunya penanda. Pemindai juga memeriksa tanda pengenal di awal arsip XP3 atau keberadaan skrip `startup.tjs`. Dalam pengujian, folder KiriKiri yang diganti namanya dan diberi `MES/start.mes` tetap dilewati, sementara game ADV32 di folder sebelah ditemukan.

### Waktu mulai game di Android

Arisa32 mengurangi pemeriksaan berulang pada folder MES, memakai ulang profil selama game berjalan, dan memuat gambar Backlog saat Backlog pertama kali dibuka. D.C. II Plus Communication memiliki 1.560 entri MES pada instalasi uji. Median waktu mulainya turun dari **2,66 menjadi 1,99 detik**.

| Game | Sebelum | Sesudah |
| --- | ---: | ---: |
| D.C. II Plus Communication | 2,66 dtk | 1,99 dtk |
| D.C.4 | 1,26 dtk | 1,03 dtk |
| D.C.4 Plus Harmony | 1,32 dtk | 0,98 dtk |
| D.C. II To You | 0,53 dtk | 0,45 dtk |
| D.C. II Dearest Marriage | 1,26 dtk | 1,11 dtk |
| Dal Segno (Inggris resmi) | 1,15 dtk | 1,09 dtk |

### Cakupan pengujian

- Setiap angka adalah median enam peluncuran per game pada tiap build di iQOO Z9x 5G. Urutan build lama dan baru bergantian. Pengukuran dimulai dari koleksi yang sudah terbuka hingga frame pertama tampil, dengan cache penyimpanan aktif. Untuk **D.C. II To You**, frame pertama berasal dari film pembuka. [Laporan pengukuran](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.7/docs/startup-iqoo-2026-11-28.md)
- Pengujian save Dal Segno mencakup perpindahan Inggris → Indonesia → Inggris, revisi teks yang lebih panjang, dan skenario yang hanya tersedia dalam bahasa Inggris. Pengujian tata letak mencakup dialog tiga baris saat masih diketik.
- Di HP, sembilan pemeriksaan mencakup pemilih folder, penjelajah folder Arisa32, dan entri koleksi lama untuk dua alias D.C.4 KiriKiri serta satu folder uji yang diganti namanya. Pemeriksaan pemindaian campuran juga memastikan game ADV32 tetap ditemukan.

### Unduhan Android

Kedua APK memakai **versionCode 25** dan mendukung ARM64 serta ARMv7 pada Android 5.0 atau lebih baru (API 21+).

| Berkas | Kegunaan |
| --- | --- |
| [Arisa32-v0.7.7.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.7/Arisa32-v0.7.7.apk) | Untuk bermain dan memperbarui aplikasi |
| [Arisa32-v0.7.7-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.7/Arisa32-v0.7.7-debug.apk) | Build debug untuk pengujian dan diagnosis |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.7/SHA256SUMS.txt) | Checksum SHA-256 kedua APK |

Pasang APK rilis di atas instalasi Arisa32 yang ada untuk mempertahankan data aplikasi. [Halaman rilis v0.7.7](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.7)
