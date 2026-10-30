**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.1 - Pembaruan Otomatis, FPS Dinamis, dan Pembatasan Save/Load Replay

### Fitur baru dan peningkatan UI

- **Pemeriksaan pembaruan otomatis (Auto Check Update)**:
  - Arisa32 kini secara otomatis memeriksa ketersediaan rilis terbaru dari repositori GitHub saat aplikasi diluncurkan (apabila opsi aktif).
  - Notifikasi rilis baru disajikan melalui status bar Android (system notification), notifikasi toast, dan dialog interaktif lengkap dengan ringkasan catatan rilis (changelog) serta tombol langsung untuk mengunduh APK rilis terbaru.
  - Opsi pengaturan On / Off tersedia pada menu Pengaturan (Settings), lengkap dengan tombol untuk memeriksa pembaruan secara manual sewaktu-waktu.
- **FPS Meter dinamis di pojok frame layar**:
  - Penambahan penghitung frame rate real-time yang diperbarui setiap 500 ms dengan pembacaan delta render yang akurat dan stabil.
  - Posisi FPS meter mengikuti batas kotak frame aktif visual novel secara dinamis:
    - Pada game berasio 4:3 seperti D.C. II P.C. (`dc2pc`), indikator menempel tepat di pojok kanan atas bidang permainan (`X = 1120`), tidak lagi keluar ke area pilar hitam.
    - Pada game berasio 16:9 seperti D.C.4 dan D.C.4 Plus Harmony (`dc4` / `dc4ph`), indikator menempel di pojok kanan atas layar penuh (`X = 1280`).
    - Otomatis beradaptasi secara mulus saat pemutaran video / movie cutscene dengan rasio khusus dan saat perubahan mode windowed/fullscreen.
  - Tombol sakelar On / Off tersedia di menu Pengaturan aplikasi dan menu gelembung (in-game Extra menu).

### Backend video FFmpeg dan ukuran APK

- **Migrasi backend pemutaran video dari LibVLC ke FFmpeg**:
  - Menggantikan ketergantungan LibVLC dengan build FFmpeg kustom yang ramping, dikonfigurasi khusus hanya untuk menyertakan komponen, demuxer, dan codec video/audio yang benar-benar dibutuhkan oleh engine visual novel (MPEG-1/2, H.264, WMV, AAC, MP3, Vorbis, WMA).
  - Mengurangi ukuran file APK (filesize) secara signifikan karena membuang modul-modul eksternal yang tidak diperlukan.
  - Integrasi rendering langsung ke pipeline grafis native Arisa32 dengan sinkronisasi frame audio-video yang presisi serta pemakaian memori yang lebih efisien.

### Pembatasan Save/Load pada Scene Replay dan Scenario Mode

- **Penonaktifan Save, Load, QuickSave, dan QuickLoad saat Replay/Scenario Mode**:
  - Mengikuti perilaku asli engine Circus di versi PC (native Windows), fitur persistence (Save, Load, QuickSave, QuickLoad) kini otomatis dinonaktifkan (`disabled`) saat user berada dalam mode Scene Replay atau Scenario Replay.
  - Tombol QuickSave/QuickLoad dan tombol aksi terkait di taskbar/sidebar disembunyikan atau dinonaktifkan agar tidak dapat dieksekusi, mencegah rusaknya save data atau state flag cerita saat memutar ulang adegan/scene.
  - Menolak operasi `save_state` dan `load_state` ketika flag replay terdeteksi aktif, menjaga integritas alur permainan persis seperti game aslinya di PC.

### Stabilitas engine

- Pemeliharaan stabilitas rendering kamera, Event CG, background, dan penempatan sprite karakter pada seluruh profile game yang didukung.
- Seluruh 18 suite unit test desktop (`ctest`) lulus 100%.

### Unduhan dan checksum

[Buka halaman unduhan v0.6.1](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.1)

- `Arisa32-v0.6.1.apk`: paket instalasi release untuk penggunaan biasa.
- `Arisa32-v0.6.1-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.
