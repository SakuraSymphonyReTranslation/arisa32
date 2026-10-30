**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.2 - Beta Kompatibilitas DC2DM

### D.C. II Dearest Marriage: beta kompatibilitas

Dukungan D.C. II Dearest Marriage (DC2DM) masih dalam tahap penambahan kompatibilitas dan beta test. Menurut pengujian pengguna, game sudah playable penuh, tetapi dukungannya belum sepenuhnya stabil.

- Arisa32 kini mengenali profil dan skrip DC2DM, dengan dukungan menu asli, dialog, pilihan, backlog, serta Save/Load portable Arisa32.
- Perbaikan posisi sprite, pergantian pose, urutan layer, dan trigger muncul/hilang. Aksesori seperti topi mengikuti posisi dan zoom karakter; penghapusan karakter ikut membersihkan aksesori.
- Scenario Mode / Scene Replay memblokir sidemenu, Save/Load, dan Quick Save/Load, termasuk pintasan dan dialog konfirmasi. Tombol QS/QL memakai tampilan nonaktif; cerita normal tetap dapat mengakses fitur tersebut.
- Opening memprioritaskan `.mpg` di folder MOVIE, dengan `.mp5` sebagai fallback. Backend FFmpeg Android mengenali isi file, termasuk patch `.mpg` yang sebenarnya berisi MP4/H.264 dan AAC.
- Ending native DC2DM menampilkan credits dan montase dari aset PCK, termasuk alur selesai normal dan replay. Sidemenu tidak dapat dibuka selama ending.

### Bug DC2DM yang masih diketahui

- Ukuran font pada menu masih terlalu besar atau tampak ter-zoom.
- Warna teks pada popup notifikasi masih belum sesuai.
- Suara karakter sering berulang.
- Masih ada bug UI dan kompatibilitas lain selama beta test. Status playable penuh tidak berarti seluruh bug telah selesai diperbaiki.

### D.C.4, Plus Harmony, dan pemutaran video

- Patch Inggris Sakuramusubi untuk D.C.4 Plus Harmony tetap memakai CP932. Arisa32 kini menangani penanda spasi khususnya agar tidak tampil sebagai kotak dan agar tanda kutip pembuka Jepang tetap bersama awal kalimat. Tidak perlu memilih encoding baru.
- Ending setelah segmen D.C. pada D.C.4 Plus Harmony kini menerima halaman credits `GRP/ED/ED.PCK` setinggi 721 piksel, lalu melanjutkan scrolling dan kembali ke menu utama.
- Perbaikan penyelesaian video Android agar decoder memproses frame dan audio yang masih tersisa sebelum melaporkan selesai, termasuk setelah pause mendekati akhir.
- `KotoRI_uta02.mpg` dan `dc4_mv001.mpg` dari D.C.4 / Plus Harmony telah diputar sampai selesai dengan audio pada pengujian Android.

### Validasi dan batasan

- Pemeriksaan ulang untuk 0.6.2: 22/22 suite CTest desktop, 317 pemeriksaan profil game Android, dan 253 pemeriksaan ekspor save lulus.
- Library APK release 0.6.2 juga lulus sembilan kasus replay, pemeriksaan spasi CP932, serta probe video dan audio di Android. Kedua APK lulus pemeriksaan versi, arsitektur, dan tanda tangan. Lihat [verifikasi rilis 0.6.2](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.2/docs/release-qa-0.6.2.md).
- Pengujian sprite, credits, ending, dan kompatibilitas sebelumnya tercatat di [laporan kompatibilitas](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.2/docs/dc2dm-dc4-compatibility-validation.md), beserta batas pengujiannya.
- Probe Android tersebut tidak mencakup seluruh siklus Activity atau seluruh rute permainan. Impor save native PC DC2DM belum didukung; Save/Load memakai format portable Arisa32. Pemetaan seluruh portrait backlog belum terverifikasi.
- Backend video desktop masih mendukung MPEG-1. Patch DC2DM berisi MP4/H.264 memerlukan backend FFmpeg Android.
- APK mendukung Android 5.0+ dengan `arm64-v8a` dan `armeabi-v7a`. File game dan patch terjemahan disediakan terpisah.

### Unduhan

[Buka halaman unduhan v0.6.2](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.2)

- `Arisa32-v0.6.2.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.2-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.
