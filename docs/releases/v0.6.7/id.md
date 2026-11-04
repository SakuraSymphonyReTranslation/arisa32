**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.7 - COMU, FXAA Android 5+, dan Pengaturan Tampilan

Versi 0.6.7 memperbaiki phonechat/COMU D.C.4 dan D.C.4 Plus Harmony, menyediakan FXAA untuk Android 5 ke atas, serta menambahkan pengaturan ukuran font, efek visual, dan mode layar. Rilis ini juga memperbaiki referensi suara dan urutan gambar karakter.

### Phonechat / COMU

- Spasi manual dan pemenggalan baris pada patch yang mengatur posisi teks sendiri tetap dipertahankan, termasuk patch bahasa Indonesia.
- Format pesan Sakuramusubi kini mengenali pemisah kata dan 11 nama pengirim dalam huruf Latin. Pesan membungkus pada batas kata dan tampil dengan potret pengirim yang sesuai.
- Font proporsional memakai jarak huruf aslinya dan ukuran hingga 22 piksel, dengan maksimal tiga baris. Pada `default.ttf` Pretendard JP Bold yang diuji, 505 pesan memakai ukuran 18 sampai 22 piksel agar huruf kecil lebih mudah dibaca.
- MS Gothic mempertahankan tata letak COMU dengan jarak tetap dan maksimal dua baris. Ukuran COMU disesuaikan dengan gelembung chat secara terpisah dari pengaturan ukuran dialog biasa.

### Tampilan dan font

- FXAA tersedia pada Android 5 sampai 12 melalui OpenGL ES 2.0. Android 13 ke atas tetap memakai jalur RuntimeShader. Off dan Bilinear tetap tersedia, dengan Bilinear sebagai bawaan dan cadangan saat FXAA tidak dapat digunakan.
- Pilih Fit to Screen untuk mempertahankan proporsi atau Stretch untuk memenuhi area layar. Pengaturan tersimpan per game, langsung berlaku saat bermain, dan digunakan juga untuk pemetaan sentuhan, overlay, serta tampilan video. Margin hitam yang merupakan bagian dari gambar game tetap ada.
- Ukuran dialog DC4/DC4PH dapat dipilih dari 18 sampai 22 untuk MS Gothic, font sistem, dan font kustom. Nilainya tersimpan per game dan berlaku setelah game dibuka ulang; ukuran bawaan tetap 22.

### Efek visual, suara, dan karakter

- Memulihkan kelopak sakura dan cahaya melayang berdasarkan perintah skrip serta atlas asli game, termasuk animasi, kepadatan, dan pergantian efek. Impor/ekspor save PC dan save Arisa32 baru mempertahankan keadaan efek yang didukung.
- Pengaturan Visual effect mengendalikan kelopak dan cahaya melayang pada DC4/DC4PH. Perubahan saat bermain langsung berlaku; mematikan efek menyembunyikan dan menjeda partikelnya. Preferensi Sakura Off dari build pengujian sebelumnya tetap dihormati.
- Narasi dan dialog tanpa suara tidak lagi mengambil referensi replay suara dari dialog karakter sebelumnya. Suara yang sedang berjalan tetap mengikuti pengaturan pemutaran.
- Urutan sprite mengikuti prioritas yang ditentukan skrip, termasuk Gen di belakang Mitsumi. Prioritas dipertahankan saat karakter disembunyikan, diganti, atau dipulihkan dari save.

### Progres, validasi, dan batasan

Pemeriksaan progres memperkuat cakupan perbaikan dari 0.6.5. Pilihan Sorane Ending 2 pada DC4/DC4PH mengikuti syarat Arisu Ending 1; data PC yang sudah memenuhi syarat tetap membukanya. Menu D.C. mengikuti delapan flag ending yang diperlukan. Rilis ini tidak menghapus progres tamat pengguna secara otomatis.

Pengujian mencakup renderer COMU dengan 505 pesan, perbandingan gambar desktop dan Android, suara, sprite, efek, save, progres, pengaturan layar, dan pilihan font. Jalur GLES untuk Android lama diuji pada perangkat Android 16 serta diperiksa terhadap API minimum 21. Pengujian langsung pada perangkat Android 5 sampai 12 belum berhasil dilakukan; kompatibilitas setiap driver lama, FPS, dan konsumsi baterai belum dipastikan.

Ini bukan pengujian seluruh rute atau semua patch. Save lama yang sudah menyimpan pengirim COMU atau referensi suara yang salah tidak dimigrasikan otomatis; muat titik sebelum percakapan terkait untuk merekam keadaan baru. Dukungan DC2 Dearest Marriage tetap beta. File game, patch terjemahan, dan font kustom disediakan terpisah.

Lihat [verifikasi rilis 0.6.7](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.7/docs/release-qa-0.6.7.md) untuk hasil dan batas pengujian.

### Unduhan dan pemasangan

[Buka halaman unduhan v0.6.7](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.7)

- `Arisa32-v0.6.7.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.7-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Pasang pembaruan tanpa menghapus aplikasi untuk mempertahankan data. Pembaruan di dalam aplikasi tersedia sejak 0.6.4; Android tetap meminta konfirmasi pemasangan. Android 5.0+; arm64-v8a dan armeabi-v7a.
