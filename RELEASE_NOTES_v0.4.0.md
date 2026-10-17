# Arisa32 v0.4.0 - CIRCUS adv32 Runtime for Da Capo & Dal Segno Series

> [!IMPORTANT]
> ### 🤖 Full LLM Generated / 100% Dibuat oleh AI (LLM)
> Seluruh kode sumber, arsitektur sistem, reverse-engineering opcode MES adv32, dekoder aset, antarmuka Android/JNI, serta dokumentasi pada repositori ini **dibuat secara penuh oleh Large Language Model (LLM)**.
>
> ### 🎮 Kompatibilitas Game / Game Compatibility: Da Capo & Dal Segno Series (CIRCUS adv32)
> Runtime engine mendukung visual novel engine **CIRCUS adv32**, difokuskan secara khusus pada seri **Da Capo** dan **Dal Segno**.

---

## 🌟 What's New / Catatan Rilis v0.4.0

### 1. 🎬 Opening Video Playback & Black Screen Fix
- **Perbaikan Black Screen Opening 3 (D.C.4 Plus Harmony)**: Teridentifikasi bahwa `DC4_OP3.mpg` menggunakan enkoding `mpeg2video` (XDCAM EX) yang tidak didukung oleh hardware video decoder pada mayoritas perangkat Android modern, sehingga hanya menghasilkan audio tanpa video.
- **Resolusi Movie Modular**: Alur pemilihan video opening dibuat modular (`resolve_movie_file`, `is_op3_movie`, `is_opening_movie`, `get_base_movie_dir`) tanpa if/else bersarang. Untuk Opening 3 diprioritaskan format H.264 MP4 (`.mp4`), sedangkan video opening lainnya memprioritaskan `.mpg`.
- **Bypass Overlay untuk Opening Video**: Video opening selalu dimuat langsung dari direktori video game asli (`Advdata/MOVIE`), mencegah bentrok dengan file terjemahan overlay.

### 2. 💾 Penanganan Save Game Modular & Lokasi Didalam AdvData
- **Prioritas Lokasi Didalam AdvData**: Save game diprioritaskan tersimpan di dalam folder `AdvData` (`Advdata/SAVEDATA` atau `AdvData/savedata`), sesuai tata letak Circus engine asli.
- **Parsing Nomor Slot Presisi (`extractSlotNumber`)**: Mengeliminasi tabrakan pencocokan nomor slot (`DC401.dat` tidak lagi bertabrakan dengan `DC411.dat`, dan `DC404.dat` tidak bertabrakan dengan `DC414.dat`).
- **Pembersihan Logika Destruktif**: Menghapus seluruh rutinitas penghapusan file otomatis, menjamin integritas dan keamanan seluruh file save pengguna.

### 3. 🖼️ Perbaikan Preview Gambar pada Load Screen
- **Dukungan Format Bitmap Penuh**: `copy_thumbnail_bitmap` pada NDK native mendukung decoding format `RGBA_8888` maupun `RGB_565` serta penyesuaian dimensi dan stride otomatis.
- **Pencarian Thumbnail Modular (`findThumbnailImage`)**: Mendukung format `.png`, `.PNG`, `.jpg`, `.JPG`, `.jpeg`, `.JPEG`.
- **Thumbnail Otomatis**: Menampilkan tangkapan layar thumbnail berwarna asli pada setiap slot di layar Load/Save.

### 4. 📱 Dukungan Android 17 & Media Modern
- Target compile SDK 36 (Android 17) dengan deklarasi `android:enableOnBackInvokedCallback="true"` untuk animasi gestur predictive back.
- Granular media permissions (`READ_MEDIA_AUDIO`, `READ_MEDIA_VIDEO`, `READ_MEDIA_IMAGES`) untuk Android 13-17.

### 5. 🧩 Modular Opcode Dispatch Architecture (Anti-Spaghetti Code)
- Pemecahan dan modularisasi alur eksekusi `SceneRunner::advance_to_next_text()` dari blok monolithic `if/else if` menjadi sub-dispatcher terisolasi yang terstruktur dan mudah dibaca:
  - `dispatch_timing_command()`: Penanganan timer, wait, dan transisi sinkron.
  - `dispatch_flow_command()`: Navigasi alur skrip, jumping, call, dan branching.
  - `dispatch_layer_command()`: Pengendalian layer latar belakang, surface, dan blend state.
  - `dispatch_sprite_command()`: Penanganan slot standing character sprite, koordinat, dan ekspresi.
  - `dispatch_camera_command()`: Pergerakan kamera, panning, dan zoom view.
  - `dispatch_ui_chip_command()`: Elemen UI interaktif, button chip, dan quick menu widget.
  - `dispatch_text_command()`: Rendering dialog, nama karakter, dan voice tagging.

### 6. 👥 Sprite Layer Stacking & Lingering Ghosting Fix (Semua Karakter)
- **Perbaikan Opcode `0x40` (Cut-in Layer)**: Berdasarkan hasil reverse-engineering dari `DC4.EXE` (VA `0x42CD43`), opcode `0x40` menargetkan cut-in slot 25 (`effect_sprite_`), bukan sprite slot 0 (`sprites_[0]`). Ini mengatasi bug di mana karakter dari scene sebelumnya (seperti Suginami) tiba-tiba muncul kembali saat cut-in efek dipanggil.
- **Auto-Reset Inactive Sprites**: Pada transisi scene berganti (`0x37`, `0xB4`, `0x49`, `0x30`), slot sprite yang tidak lagi aktif otomatis dibersihkan dan disembunyikan.

---

## 📦 Download Assets
- `Arisa32-v0.4.0.apk`: Paket instalasi rilis mandiri Android (mendukung Android 5.0+ / API 21 - API 36, arsitektur `arm64-v8a` & `armeabi-v7a`).
- `Arisa32-v0.4.0-debug.apk`: Standalone debug build package dengan logcat verbose logging aktif.
