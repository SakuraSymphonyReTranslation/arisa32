# Arisa32 v0.4.0 - CIRCUS adv32 Runtime for Da Capo & Dal Segno Series

> [!IMPORTANT]
> ### 🤖 Full LLM Generated / 100% Dibuat oleh AI (LLM)
> Seluruh kode sumber, arsitektur sistem, reverse-engineering opcode MES adv32, dekoder aset, antarmuka Android/JNI, serta dokumentasi pada repositori ini **dibuat secara penuh oleh Large Language Model (LLM)**.
>
> ### 🎮 Kompatibilitas Game / Game Compatibility: Da Capo & Dal Segno Series (CIRCUS adv32)
> Runtime engine mendukung visual novel engine **CIRCUS adv32**, difokuskan secara khusus pada seri **Da Capo** dan **Dal Segno**.

---

## 🌟 What's New / Catatan Rilis v0.4.0

### 1. 🧩 Modular Opcode Dispatch Architecture (Anti-Spaghetti Code)
- Pemecahan dan modularisasi alur eksekusi `SceneRunner::advance_to_next_text()` dari blok monolithic `if/else if` menjadi sub-dispatcher terisolasi yang terstruktur dan mudah dibaca:
  - `dispatch_timing_command()`: Penanganan timer, wait, dan transisi sinkron.
  - `dispatch_flow_command()`: Navigasi alur skrip, jumping, call, dan branching.
  - `dispatch_layer_command()`: Pengendalian layer latar belakang, surface, dan blend state.
  - `dispatch_sprite_command()`: Penanganan slot standing character sprite, koordinat, dan ekspresi.
  - `dispatch_camera_command()`: Pergerakan kamera, panning, dan zoom view.
  - `dispatch_ui_chip_command()`: Elemen UI interaktif, button chip, dan quick menu widget.
  - `dispatch_text_command()`: Rendering dialog, nama karakter, dan voice tagging.
- Virtual machine tetap bersih dan fokus pada *generic register* dan *execution cycle* tanpa hardcoded branching yang berisiko bentrok.

### 2. 🛡️ Safe Memory Protection & Anti-SIGBUS Crash (`mem_safe.h`)
- Implementasi pustaka utilitas pembacaan memori aman (`core/include/arisa32/mem_safe.h`): `read_u16_le`, `read_u32_le`, `read_u64_le`, `read_i32_le`, `safe_string`, dan validasi batas buffer (`check_bounds`).
- Menggantikan konversi memori berisiko (`reinterpret_cast` raw unaligned pointer) pada `crm.cpp`, `scene_state.cpp`, `scene.cpp`, dan `system_menu.cpp`.
- Mencegah fatal crash **SIGBUS / Alignment Fault** pada chipset Android ARM (khususnya 32-bit `armeabi-v7a` dan `arm64-v8a`) saat membaca header biner dan save data.

### 3. 👥 Sprite Layer Stacking & Lingering Ghosting Fix (Semua Karakter)
- **Perbaikan Opcode `0x40` (Cut-in Layer)**: Berdasarkan hasil reverse-engineering dari `DC4.EXE` (VA `0x42CD43`), opcode `0x40` menargetkan cut-in slot 25 (`effect_sprite_`), bukan sprite slot 0 (`sprites_[0]`). Ini mengatasi bug di mana karakter dari scene sebelumnya (seperti Suginami) tiba-tiba muncul kembali saat cut-in efek dipanggil.
- **Auto-Reset Inactive Sprites**: Pada transisi scene berganti (`0x37`, `0xB4`, `0x49`, `0x30`), slot sprite yang tidak lagi aktif otomatis dibersihkan dan disembunyikan, kecuali skrip secara eksplisit menyertakan argumen penahan scene (`:n`).
- Mengeliminasi sprite ganda, sprite menumpuk, dan karakter yang tertinggal dari adegan sebelumnya di seluruh karakter (Kanata, Hiyori, Arisu, Suginami, dll.).

### 4. 🇮🇩 Dukungan Modifikasi & Naskah Terjemahan (.MES / Indonesian Translation Patch)
- Dukungan pemuatan skrip `.mes` kustom/modifikasi yang telah diterjemahkan (seperti terjemahan Bahasa Indonesia) baik melalui folder overlay maupun mod patch.
- Rekalkulasi pointer tabel label dinamis dan toleransi encoding teks memastikan skrip terjemahan berjalan normal tanpa crash pada jumping label atau pilihan cerita.

---

## 📦 Download Assets
- `Arisa32-v0.4.0.apk`: Paket instalasi rilis mandiri Android (mendukung Android 5.0+ / API 21 - API 36, arsitektur `arm64-v8a` & `armeabi-v7a`).
- `Arisa32-v0.4.0-debug.apk`: Standalone debug build package dengan logcat debugging aktif.
