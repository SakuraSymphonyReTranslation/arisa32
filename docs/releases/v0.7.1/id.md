**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.7.1 - DC2DM Fully Playable dan Perbaikan Font

D.C. II Dearest Marriage kini dapat dimainkan sampai selesai (fully playable). Rilis 8 November 2026 ini memperbaiki perpindahan menu dan riwayat baca DC2DM, menambahkan font Pretendard JP Bold, serta memperbaiki teks COMU dan galeri CG DC2PC. Seluruh perbaikan 0.7.0 tetap disertakan.

### Perbaikan pembaru di dalam aplikasi

- APK v0.7.1 yang diperbarui menambahkan percobaan ulang terbatas dan jalur unduhan cadangan melalui API GitHub resmi saat unduhan langsung mengalami gangguan koneksi atau error sementara, termasuk HTTP 504.
- Pembaru tetap memeriksa ukuran dan checksum unduhan, identitas aplikasi, versi, serta sertifikat APK sebelum pemasangan. Membatalkan pembaruan juga menghentikan percobaan ulang dan membersihkan berkas sementara.

Versi lama seperti v0.6.6 dan v0.6.7 dapat langsung memperbarui ke v0.7.1. Jika jalur unduhan GitHub tidak dapat diakses, gunakan VPN atau jaringan lain untuk mendapatkan APK ini; jalur cadangan baru tersedia setelah APK yang diperbarui terpasang. Unduhan tetap memerlukan koneksi ke server GitHub.

Jika sudah memakai v0.7.1 sebelum perbaikan pembaru ini, unduh APK v0.7.1 yang diperbarui dari halaman rilis lalu pasang tanpa menghapus aplikasi. Pemeriksaan otomatis tidak menawarkan ulang nomor versi yang sama.

### D.C. II Dearest Marriage

- Perpindahan Title ke pilihan cerita mengikuti transisi skrip tanpa jeda hitam tambahan. Latar, logo, dan label cerita ikut meredup saat cerita dimulai; label tidak lagi tertinggal pada layar hitam atau muncul kembali saat opening dibatalkan.
- BGM Title berlanjut saat masuk ke pilihan cerita, tanpa dimulai ulang atau diputar bertumpuk. Kembali ke Title dari opening memakai gambar video yang sedang tampil untuk transisinya.
- Masuk ke MINI GAME tidak lagi menambahkan jeda 1,2 detik dari aplikasi. Skrip asli mengatur perkenalan Otome dan urutan masuk ke suit atau Reversi.
- Riwayat baris yang sudah dibaca tetap dimuat sebelum progres PC diimpor, sehingga startup tidak menimpanya dengan riwayat kosong.
- Dialog, nama pembicara, dan pilihan dengan Pretendard JP Bold memakai ukuran serta jarak huruf yang sesuai dengan font. Teks Latin tidak lagi diperkecil atau diberi jarak tetap yang membuat huruf tampak renggang.

### Font Pretendard JP Bold dan teks COMU

- Pretendard JP Bold tersedia di pemilih font per game pada Android, beserta lisensi SIL Open Font License 1.1. MS Gothic tetap menjadi font bawaan; memilih Pretendard untuk satu game tidak mengubah pilihan game lain.
- Teks Latin COMU pada patch DC4/DC4PH yang memakai spasi padding kini mengikuti lebar huruf Pretendard. Ukuran 16 piksel, lebar spasi 8 piksel, dan susunan dua baris tetap dipertahankan.
- Perbaikan dikenali dari identitas font dan isi teks patch, sehingga tetap berlaku jika nama berkas font atau folder game berubah. Tata letak Sakuramusubi dan font lain tetap memakai jalurnya masing-masing.

### Galeri CG D.C. II Plus Communication

- Pergantian CG mengikuti urutan skrip, termasuk perpindahan antarkelompok thumbnail, melewati gambar terkunci, dan kembali ke awal sesuai siklus yang ditentukan game.
- CG tinggi memakai potongan awal dan gerakan vertikal asli. Ketukan pertama memulai gerakan; ketukan selama gerakan tidak melompati gambar. Setelah gerakan selesai, ketukan berikutnya membuka CG selanjutnya.
- Pratinjau dibentuk dari gambar sumber pada ukuran tampilan yang diminta. Penyimpanan hasil pengambilan sampel mengurangi pekerjaan render saat gambar bergerak; perubahan ini tidak menambah detail di luar aset asli.

README Bahasa Indonesia dan English juga menjelaskan pemisahan data game asli dalam `AdvData` dan folder patch terjemahan, serta pemilihan bahasanya di aplikasi.

### Pengujian dan batasan

Laporan fitur mencatat pengujian desktop dan probe Android untuk [menu dan riwayat baca DC2DM](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/dc2dm-menu-font-validation.md), [tata letak Pretendard](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/dc2dm-pretendard-layout-validation.md), [teks COMU](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/comu-pretendard-padding-validation.md), dan [galeri DC2PC](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/dc2pc-cgmode-validation.md). Hasil tersebut berasal dari build pengembangan yang disebutkan di setiap laporan. Pemeriksaan paket 0.7.1 dicatat terpisah dalam [laporan rilis](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/main/docs/release-qa-0.7.1.md).

- Beberapa pesan COMU pada patch DC4/DC4PH memiliki baris baru eksplisit yang menghasilkan baris ketiga. Baris itu masih terpotong oleh area dua baris asli; rilis ini tidak menulis ulang teks patch.
- Progres global DC2DM tetap disimpan dalam `system-profile.dat`; ekspor balik ke `system.dat` belum tersedia. Pemuatan slot hasil ekspor langsung dengan executable PC belum diverifikasi.
- Pengujian fitur pada perangkat memakai ARM64. Dukungan ARMv7 belum diuji pada perangkat 32-bit. Pemeriksaan terarah ini tidak menjamin kesamaan setiap piksel dengan PC atau mencakup semua varian patch dan perangkat.

### Unduhan dan pemasangan

[Buka halaman unduhan v0.7.1](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.1)

- `Arisa32-v0.7.1.apk`: paket rilis untuk penggunaan biasa.
- `Arisa32-v0.7.1-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Kedua APK bersifat universal, masing-masing berisi ARM64 dan ARMv7, serta mendukung Android 5.0+ (API 21).

Pembaru di dalam aplikasi mengunduh paket rilis universal.

Pasang pembaruan tanpa menghapus aplikasi agar data tetap tersimpan. Berkas game dan patch terjemahan disediakan terpisah.
