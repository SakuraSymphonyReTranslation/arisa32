**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.5 - Perbaikan Progres Rute dan Map Selection DC4

Versi 0.6.5 memperbaiki syarat pembukaan rute pada D.C.4 dan D.C.4 Plus Harmony. Profil baru tidak lagi dianggap sudah tamat, sementara profil yang sudah 100% tetap memiliki seluruh aksesnya.

### Syarat rute dan menu D.C.

- Menu D.C. tetap tersembunyi dan tidak dapat diaktifkan sebelum syarat ending dalam skrip asli terpenuhi.
- Pembukaan rute Arisu/Arisa dan pilihan tambahan Sorane mengikuti progres ending yang disimpan game. Flag tamat yang belum ada kini bernilai nol.
- Memuat save atau mereset konfigurasi tidak lagi membuka semua rute secara paksa.

### Map selection

- Pada empat peta tanggal 8, 9, 11, dan 12 Februari, karakter, penanda lokasi, serta pilihan tambahan tetap tersembunyi sebelum syarat empat heroine awal terpenuhi.
- Saat terkunci, peta memakai gambar asli dengan tiga pilihan. Setelah terbuka, gambar asli yang memuat penanda tambahan muncul bersama pilihan keempat.
- Pilihan yang tidak tersedia tidak dapat dipicu. Skor peta dan percabangan tetap mengikuti pilihan pemain serta aturan skrip game.

### Save dan progres tamat

- Progres ending berlaku di seluruh profil dan tetap tersimpan saat memuat save lama, mengulang dari backlog, atau mereset konfigurasi. Skor lokal peta tetap mengikuti save yang dimuat.
- Profil Android yang lebih lama tidak lagi menimpa progres tamat yang sudah diimpor dari data PC dengan nilai nol.
- Save dan flag tamat yang sudah tersimpan tidak dihapus. Profil 100% tetap terbuka seluruhnya.

Profil lama mungkin sudah menyimpan flag tamat akibat bug versi sebelumnya. Nilainya tidak dapat dibedakan dengan progres tamat asli, sehingga 0.6.5 tidak mereset atau mengoreksinya secara otomatis. Untuk memulihkan progres parsial yang diketahui benar, impor data progres PC yang sesuai ke profil emulator baru setelah mencadangkan data Anda.

### Validasi dan batasan

Pengujian mencakup 24 rangkaian tes desktop, 1.348 kombinasi pilihan peta dari kedua game, syarat ending, serta simpan/pulihkan progres. Gambar menu dan keempat peta diperiksa dalam keadaan terkunci dan terbuka pada kedua game. Kode JNI Android lulus 24 kasus percabangan peta, empat kasus penguncian progres, sembilan kasus replay, dan pemeriksaan spasi CP932 pada perangkat Android.

Pengujian gambar memakai renderer desktop dengan aset game asli; pengujian JNI Android memakai skrip uji terpisah. Ini bukan pengujian setiap rute sampai tamat atau setiap perangkat. Perbaikan kompatibilitas ini khusus DC4/DC4PH. Perbaikan dari 0.6.4 tetap tersedia, dan dukungan DC2DM tetap beta.

Lihat [rincian progres DC4](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.5/docs/dc4-progression.md) dan [verifikasi paket 0.6.5](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.5/docs/release-qa-0.6.5.md).

### Unduhan dan pemasangan

[Buka halaman unduhan v0.6.5](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.5)

- `Arisa32-v0.6.5.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.5-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Pasang pembaruan tanpa menghapus aplikasi untuk mempertahankan data. Arisa32 0.6.4 dapat mengunduh pembaruan di dalam aplikasi; konfirmasi pemasangan tetap ditangani Android.

Android 5.0+; arm64-v8a dan armeabi-v7a. File game dan patch terjemahan disediakan terpisah.
