# Changelog

## 0.6.2 - 2026-10-30

### D.C. II Dearest Marriage: compatibility beta

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
- Library APK release 0.6.2 juga lulus sembilan kasus replay, pemeriksaan spasi CP932, serta probe video dan audio di Android. Kedua APK lulus pemeriksaan versi, arsitektur, dan tanda tangan. Lihat [verifikasi rilis 0.6.2](docs/release-qa-0.6.2.md).
- Pengujian sprite, credits, ending, dan kompatibilitas sebelumnya tercatat di [laporan kompatibilitas](docs/dc2dm-dc4-compatibility-validation.md), beserta batas pengujiannya.
- Probe Android tersebut tidak mencakup seluruh siklus Activity atau seluruh rute permainan. Impor save native PC DC2DM belum didukung; Save/Load memakai format portable Arisa32. Pemetaan seluruh portrait backlog belum terverifikasi.
- Backend video desktop masih mendukung MPEG-1. Patch DC2DM berisi MP4/H.264 memerlukan backend FFmpeg Android.
- APK mendukung Android 5.0+ dengan `arm64-v8a` dan `armeabi-v7a`. File game dan patch terjemahan disediakan terpisah.

### Unduhan

- `Arisa32-v0.6.2.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.2-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

## 0.6.1 — 2026-10-28

### Fitur baru dan peningkatan UI

- **Pemeriksaan pembaruan otomatis (Auto Check Update)**:
  - Arisa32 kini secara otomatis memeriksa ketersediaan rilis terbaru dari repositori GitHub saat aplikasi diluncurkan (apabila opsi aktif).
  - Notifikasi rilis baru disajikan melalui status bar Android (system notification), notifikasi toast, dan dialog interaktif lengkap dengan ringkasan catatan rilis (changelog) serta tombol langsung untuk mengunduh APK rilis terbaru.
  - Opsi pengaturan On / Off tersedia pada menu Pengaturan (Settings), lengkap dengan tombol untuk memeriksa pembaruan secara manual sewaktu-waktu.
- **FPS Meter dinamis di pojok frame layar**:
  - Penambahan penghitung frame rate real-time yang diperbarui setiap 500 ms dengan pembacaan delta render yang akurat dan stabil.
  - Posisi FPS meter mengikuti batas kotak frame aktif visual novel secara dinamis:
    - Pada game berasio 4:3 seperti D.C. II P.C. (`dc2pc`), indikator menempel tepat di pojok kanan atas bidang permainan ($X = 1120$), tidak lagi keluar ke area pilar hitam.
    - Pada game berasio 16:9 seperti D.C.4 dan D.C.4 Plus Harmony (`dc4` / `dc4ph`), indikator menempel di pojok kanan atas layar penuh ($X = 1280$).
    - Otomatis beradaptasi secara mulus saat pemutaran video / movie cutscene dengan rasio khusus dan saat perubahan mode windowed/fullscreen.
  - Tombol sakelar On / Off tersedia di menu Pengaturan aplikasi dan menu gelembung (in-game Extra menu).

### Backend Video FFmpeg & Optimasi Ukuran File APK

- **Migrasi backend pemutaran video dari LibVLC ke FFmpeg**:
  - Menggantikan ketergantungan LibVLC dengan build FFmpeg kustom yang ramping, dikonfigurasi khusus hanya untuk menyertakan komponen, demuxer, dan codec video/audio yang benar-benar dibutuhkan oleh engine visual novel (MPEG-1/2, H.264, WMV, AAC, MP3, Vorbis, WMA).
  - Mengurangi ukuran file APK (filesize) secara signifikan karena membuang modul-modul eksternal yang tidak diperlukan.
  - Integrasi rendering langsung ke pipeline grafis native Arisa32 dengan sinkronisasi frame audio-video yang presisi serta pemakaian memori yang lebih efisien.

### Pembatasan Save / Load pada Scene Replay & Scenario Mode (Akurasi Native PC)

- **Penonaktifan Save, Load, QuickSave, dan QuickLoad saat Replay/Scenario Mode**:
  - Mengikuti perilaku asli engine Circus di versi PC (native Windows), fitur persistence (Save, Load, QuickSave, QuickLoad) kini otomatis dinonaktifkan (`disabled`) saat user berada dalam mode Scene Replay atau Scenario Replay.
  - Tombol QuickSave/QuickLoad dan tombol aksi terkait di taskbar/sidebar disembunyikan atau dinonaktifkan agar tidak dapat dieksekusi, mencegah rusaknya save data atau state flag cerita saat memutar ulang adegan/scene.
  - Menolak operasi `save_state` dan `load_state` ketika flag replay terdeteksi aktif, menjaga integritas alur permainan persis seperti game aslinya di PC.

### Penyempurnaan Engine & Stabilitas

- Pemeliharaan stabilitas rendering kamera, Event CG, background, dan penempatan sprite karakter pada seluruh profile game yang didukung.
- Seluruh 18 suite unit test desktop (`ctest`) lulus 100%.

### Unduhan

- `Arisa32-v0.6.1.apk`: paket instalasi release untuk penggunaan biasa.
- `Arisa32-v0.6.1-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

## 0.6.0 — 2026-10-26

### Dukungan D.C. II P.C. (Plus Communication)

- **D.C. II P.C. \~Da Capo II Plus Communication\~ resmi didukung.** Dukungan mencakup skrip dan teks, dialog/pilihan, gambar karakter dan ekspresi, musik/suara, title screen, sidebar, Save/Load, backlog, config, dan menu Extra/replay.
- Perbaikan navigasi menu Extra, scrolling replay, pemulihan snapshot lama, dan tombol Back/sidebar Android.
- Progres serta flag rute yang tersimpan dipulihkan tanpa mengubah flag nol menjadi unlock.

### Save dan pemulihan

- DC2PC, D.C.4, dan D.C.4 Plus Harmony menyimpan slot dalam format PC `.dat`, dengan pendamping `.dat.arisa` untuk keadaan tambahan Arisa32.
- Penomoran save mengikuti format setiap game. DC2PC memakai penomoran halaman asli dan mempertahankan `DC2PC01.dat` sebagai salinan Latest, terpisah dari slot reguler.
- Folder SaveData yang sudah tersedia digunakan kembali; save `.dc4sav` lama tetap menjadi fallback dan dimigrasikan tanpa menimpa slot yang sudah ada.
- Judul skenario dan thumbnail ditampilkan pada slot. Tabel judul bawaan mempercepat pembukaan menu Load pertama.
- Perbaikan posisi, skala, pivot, cropping, dan kamera karakter saat memuat save native D.C.4/Plus Harmony.
- Perbaikan suara pengumuman Save/Load yang sebelumnya dapat dimulai dua kali, serta waktu transisi saat aplikasi dijeda/dilanjutkan.

### Ekspor simpanan

- Popup ekspor menyediakan dua pilihan: **Dengan .dat.arisa** untuk backup lengkap Arisa32 dan **Tanpa .dat.arisa** untuk arsip yang lebih kecil.
- Tanpa pendamping, save `.dat` pada game yang didukung tetap bisa di-Load. Posisi audio dan sebagian keadaan tambahan Arisa32 tidak dipulihkan; musik dapat mulai ulang. Game PC mengabaikan `.dat.arisa`.
- Kedua pilihan tetap menyertakan simpanan lain, preview, pengaturan, unlock, dan riwayat baca jika tersedia.
- Popup proses menampilkan mode ekspor, nama berkas, persentase/jumlah berkas, dan konfirmasi selesai. Nama ZIP membedakan mode `with_arisa` dan `without_arisa`.

### Tampilan aplikasi

- Ikon default aplikasi memakai artwork karakter baru, dengan ikon adaptive dan ukuran untuk berbagai kepadatan layar Android.
- Ikon di kiri judul Arisa32 pada Game Library memakai ikon yang sama, dengan ukuran dan posisi sejajar dengan judul.

### Validasi dan catatan

- Validasi: **15/15 tes core C++, 288 pemeriksaan profil Android, dan 217 pemeriksaan ekspor ZIP lulus**. APK debug dan release berhasil dibangun. Sampel save native DC2PC/D.C.4/Plus Harmony juga telah dimuat dan dilanjutkan pada pengujian perangkat/PC.
- Beberapa keadaan animasi/renderer dan riwayat native masih memiliki batasan; hasil sampel tidak menjamin semua checkpoint. Pemulihan film setelah proses aplikasi berakhir masih belum didukung.
- APK mendukung Android 5.0+ (`arm64-v8a` dan `armeabi-v7a`). File game dan paket terjemahan disediakan terpisah.

### Unduhan

- `Arisa32-v0.6.0.apk`: paket instalasi release untuk penggunaan biasa.
- `Arisa32-v0.6.0-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.
