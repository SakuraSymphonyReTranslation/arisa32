# Changelog

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
