**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.6 - Teks DC4 Lebih Besar dan Penanda Kontrol Transparan

Versi 0.6.6 memperbesar teks dialog dan nama karakter pada D.C.4 dan D.C.4 Plus Harmony. Penanda biru di tepi layar juga tampil lebih samar saat kontrol Arisa32 tersembunyi agar tidak terlalu mengganggu permainan.

### Teks D.C.4 dan Plus Harmony

- Ukuran bawaan dialog Latin, termasuk bahasa Indonesia dan Inggris, naik dari 20 menjadi 22 piksel pada kanvas game. Nama karakter naik dari 18 menjadi 22 piksel.
- Jendela dialog Android memakai MS Gothic agar bentuk dan jarak huruf lebih mendekati versi PC. Font menu tetap memakai pengaturan sebelumnya.
- Pemenggalan baris, tampilan teks bertahap, dan pemulihan teks dari save memakai ukuran serta jarak huruf yang sama. Pemeriksaan adegan mencakup dialog tiga baris dan posisi nama di dalam nameplate.

### Penanda pembuka kontrol

- Penanda biru di tepi kiri memakai opasitas 40% saat tidak digunakan. Penanda tetap terlihat dan kembali ke opasitas penuh saat disentuh atau mendapat fokus keyboard.
- Ketuk atau geser ke kanan untuk membuka kontrol. Tarik ke atas atau bawah untuk memindahkan penanda; posisinya tetap tersimpan.
- Area sentuh tetap 48 x 64 dp. Auto-hide empat detik, gerakan menyembunyikan kontrol, serta navigasi keyboard tetap tersedia.

### Validasi dan batasan

Tes render teks dan regresi adegan lulus. Pemeriksaan gambar memakai aset asli DC4 bahasa Jepang dan DC4PH bahasa Indonesia, termasuk ukuran nama serta batas dialog tiga baris. Pengujian pada I2219 dengan Android 16 lulus 431 pemeriksaan navigasi untuk DC4, DC4PH, DC2 Plus Communication, dan DC2 Dearest Marriage. Gambar dari perangkat juga mengonfirmasi penanda samar saat diam dan terang saat mendapat fokus.

Raster font Android masih dapat berbeda dari PC. Pengujian ini mencakup adegan dan kasus yang tercatat, bukan seluruh rute, patch, atau perangkat. Perubahan ukuran teks khusus DC4/DC4PH; penanda kontrol berlaku untuk profil yang memakai kontrol mengambang. Perbaikan progres rute dan peta dari 0.6.5 tetap tersedia, dan dukungan DC2DM tetap beta.

Lihat [verifikasi paket 0.6.6](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.6/docs/release-qa-0.6.6.md).

### Unduhan dan pemasangan

[Buka halaman unduhan v0.6.6](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.6)

- `Arisa32-v0.6.6.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.6-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Pasang pembaruan tanpa menghapus aplikasi untuk mempertahankan data. Arisa32 0.6.4 atau lebih baru dapat mengunduh pembaruan di dalam aplikasi; konfirmasi pemasangan tetap ditangani Android.

Android 5.0+; arm64-v8a dan armeabi-v7a. File game dan patch terjemahan disediakan terpisah.
