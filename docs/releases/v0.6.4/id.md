**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.4 - Perbaikan Rute dan Sakura DC4, Pembaruan dalam Aplikasi

Versi 0.6.4 memperbaiki percabangan rute, efek sakura, dan beberapa sprite pada D.C.4 serta D.C.4 Plus Harmony. Arisa32 juga dapat mengunduh pembaruan dari dalam aplikasi dengan progress bar, lalu membuka konfirmasi pemasangan Android.

### Percabangan rute DC4 dan DC4PH

- Pilihan pada peta kini ikut menentukan kelompok rute heroine melalui perintah skenario yang sebelumnya belum ditangani. Pemilihan memakai skor tertinggi dan aturan urutan asli saat skor seri, tanpa menghapus skor pilihan.
- Pemulihan save sebelum percabangan mempertahankan hasil pilihan. Jika save sudah berada pada rute yang salah, muat save sebelum percabangan kelompok rute dan lanjutkan dari sana; pembaruan tidak memindahkan save tersebut ke rute lain secara otomatis.

### Kelopak sakura dan transisi

- Mengembalikan kelopak sakura yang bergerak di dalam adegan serta efek transisi yang memakai atlas dan mask asli game, termasuk bagian putih kelopak dan transparansinya.
- Durasi mengikuti parameter skrip dan aturan waktu yang diperiksa pada versi PC. Kelopak kecil dapat tetap bergerak setelah sapuan transisi utama selesai; efek tidak dipercepat untuk segera mengganti layar.
- Save baru menyimpan keadaan efek. Save lama tetap dapat dibaca, tetapi save yang belum merekam kelopak aktif perlu menjalankan kembali bagian skenario yang memulai efek tersebut.

### Zoom Nino dan sprite jam tangan

- Memperbaiki pusat zoom pada dua tampilan Nino yang dilaporkan. Koreksi berlaku pada jalur gambar CRX lepas DC4/DC4PH yang terverifikasi, dengan pusat sumbu nol mengikuti titik tengah gambar.
- Sprite jam tangan yang muncul setelah efek suara kini mengikuti perintah tampil/sembunyi yang benar, termasuk saat skenario baru saja memuat sprite efek lain.
- Perbaikan mengikuti perintah dan data gambar, sehingga tidak bergantung pada dialog Jepang, Indonesia, atau English. Delapan frame kontrol dari zoom lain tetap identik dalam pengujian.

### Pembaruan dari dalam aplikasi

- Pemeriksaan otomatis atau manual menawarkan versi baru. Tekan **Perbarui** untuk mengunduh di Arisa32, dengan progress bar, jumlah unduhan, serta pilihan batal dan coba lagi.
- Aplikasi memeriksa ukuran dan checksum unduhan jika tersedia, identitas paket, versi yang lebih baru, serta sertifikat penandatangan sebelum menyiapkan pemasangan.
- Unduhan memakai cache privat. Arisa32 menghapus APK sementara setelah menyerahkannya ke sesi pemasangan Android, dan membersihkan unduhan gagal atau dibatalkan. Tidak ada APK yang disimpan ke folder Downloads.
- Sesi yang sudah siap dipasang dapat dilanjutkan setelah proses Arisa32 berhenti, termasuk ketika ponsel menutup aplikasi saat izin pemasangan berubah. Unduhan yang masih berlangsung belum dapat dilanjutkan setelah proses berhenti; mulai unduhan lagi.
- Android tetap menampilkan konfirmasi pemasangan dan, bila belum diizinkan, halaman **Instal aplikasi tidak dikenal**. Peringatan umum pada halaman izin tersebut berasal dari Android dan tidak dapat dihapus oleh Arisa32. Pembaruan berikutnya langsung menuju konfirmasi jika izin sudah aktif.
- Alur unduhan tidak membuka browser atau pengelola unduhan lain. Tombol **Lihat Rilis** tetap tersedia jika ingin membuka halaman GitHub.

Fitur pengunduh ini tersedia mulai 0.6.4. Pemasangan 0.6.4 dari versi lama masih mengikuti alur pembaruan versi yang sedang terpasang. Pasang APK dengan sertifikat yang sama tanpa menghapus aplikasi untuk mempertahankan data.

### Validasi dan batasan

Pengujian mencakup 1.348 jalur pilihan peta dari data DC4/DC4PH, termasuk simpan/pulihkan, serta pemeriksaan efek dan sprite yang dicatat. Pengujian pembaruan mencakup 84 assertion unduhan pada host, 162 assertion dialog rilis, 61 assertion perangkat sampai persiapan pemasangan, 8 assertion pemulihan/pembatalan, dan pemasangan nyata pada aplikasi QA terpisah dari kode versi 10 ke 11.

- Ini bukan pengujian setiap rute sampai tamat, setiap patch, atau setiap perangkat. Perbaikan kompatibilitas DC4/DC4PH di atas tidak mengubah profil game lain secara umum.
- Waktu efek mengikuti aturan yang telah diperiksa, tetapi lintasan kelopak acak tidak harus identik dengan PC. Rilis ini tidak menjamin kesamaan setiap frame atau 60 FPS. Efek lain yang belum dipetakan tetap di luar cakupan.
- Perbaikan DC2DM, penghalusan gambar, konfirmasi, dan navigasi dari 0.6.3 tetap tersedia. Dukungan DC2DM tetap beta.
- Uji pemasangan memakai Android 16. Kompatibilitas SDK minimum diperiksa saat build; perangkat Android lama belum diuji langsung untuk fitur pembaruan ini.

Lihat [verifikasi paket 0.6.4](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/release-qa-0.6.4.md), [rute](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/dc4-route-branching-validation.md), [sakura](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/dc4-sakura-validation.md), [sprite](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/dc4-sprite-validation.md), dan [pembaruan aplikasi](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/in-app-updates.md) untuk cakupan dan hasil pemeriksaan.

### Unduhan

[Buka halaman unduhan v0.6.4](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.4)

- `Arisa32-v0.6.4.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.4-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Android 5.0+; arm64-v8a dan armeabi-v7a. File game dan patch terjemahan disediakan terpisah.
