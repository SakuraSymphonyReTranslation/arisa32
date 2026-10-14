# Arisa32 v0.3.0 - CIRCUS adv32 Runtime for Da Capo & Dal Segno Series

> [!IMPORTANT]
> ### 🤖 Full LLM Generated / 100% Dibuat oleh AI (LLM)
> Seluruh kode sumber, arsitektur sistem, reverse-engineering opcode MES adv32, dekoder aset, antarmuka Android/JNI, serta dokumentasi pada repositori ini **dibuat secara penuh oleh Large Language Model (LLM)**.
>
> ### 🎮 Kompatibilitas Game / Game Compatibility: Da Capo & Dal Segno Series (CIRCUS adv32)
> Runtime engine mendukung visual novel engine **CIRCUS adv32**, difokuskan secara khusus pada seri **Da Capo** dan **Dal Segno**.

---

## 🌟 What's New / Catatan Rilis v0.3.0

### 1. 🎬 In-Scene & Cutscene Video Playback (D.C.4 & D.C.4 Plus Harmony)
- Pemutaran video pembuka (*opening*), animasi perkenalan, dan cutscene in-game pada skrip adv32 kini dapat berjalan secara otomatis dan lancar.
- Pipeline video decoding dan presentasi frame terintegrasi langsung dengan `SceneRunner` dan Surface rendering Android.

### 2. 🎨 Text Rendering Color Fix (In-Game Notification Pop-up)
- Memperbaiki anomali warna render teks pada pop-up notifikasi dalam game (seperti notifikasi quick save, auto-mode indicator, dan banner sistem).
- Pengaturan format channel warna ARGB/RGBA diselaraskan dengan shader blending adv32.

### 3. ✨ D.C.4 Plus Harmony Full Compatibility
- Perbaikan kompatibilitas menyeluruh untuk **D.C.4 ~Da Capo 4~ Plus Harmony** yang sebelumnya mengalami kendala pembacaan data.
- Dukungan pembacaan file archive berukuran lebih dari 2 GB (`VOICE.DAT` 64-bit seek) dan registrasi prefix partisi audio `LVOICE\\`.
- Deteksi skrip rute eksklusif Plus Harmony (`dc4_asu*`) berjalan optimal.

### 4. 🎵 Dynamic Audio BGM Switching
- Pembaruan penanganan audio BGM: transisi musik latar belakang kini otomatis berganti dan sinkron ketika berpindah antar menu utama, title screen, dan scenario (teruji pada *D.C.4* dan *D.C.4 Plus Harmony*).

### 5. 🗑️ Game Library "Remove Game" Feature
- Penambahan tombol aksi **REMOVE** pada kartu Game Library di antarmuka utama.
- Pengguna dapat menghapus game yang terdaftar secara aman. Untuk game yang disalin ke memori internal, tersedia opsi pembersihan data game untuk menghemat ruang penyimpanan.

### 6. 💾 Direct External Storage Support
- Dukungan pemutaran langsung (*direct playback*) dari MicroSD dan USB OTG Flashdrive tanpa perlu menyalin file ke memori internal perangkat.
- Mendukung pemindaian volume dan navigasi direktori eksternal secara otomatis.

### 7. 🌸 Stable Character Introduction & PCK Effect Transitions
- Animasi efek transisi (`AdvData\GRP\EFF\*.PCK`) seperti perkenalan karakter kini berjalan stabil dan natural sesuai durasi aslinya.
- Touch guard: animasi efek tidak lagi terputus secara tidak sengaja (*skip*) ketika pengguna menyentuh layar saat dialog/transisi sedang berlangsung.

---

## 🔬 Experimental & Status Seri Lain

- **D.C. II ~Da Capo II~ Dearest Marriage**: Dukungan masih berstatus **Experimental** (perbaikan parsing button chip dan navigasi menu awal).
- **Seri Da Capo & Dal Segno Lainnya**: Katalog game telah dikenali, namun kompatibilitas penuh skrip & aset untuk seri lainnya masih dalam tahap pengujian bertahap dan belum diverifikasi secara menyeluruh.

---

## 📦 Download Assets
- `Arisa32-v0.3.0.apk`: Paket instalasi rilis mandiri Android (mendukung Android 5.0+ / API 21 - API 36, arsitektur `arm64-v8a` & `armeabi-v7a`).
- `Arisa32-v0.3.0-debug.apk`: Standalone debug build package.
