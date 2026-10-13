# Arisa32 v0.2.0 - CIRCUS adv32 Runtime for Da Capo & Dal Segno Series

> [!IMPORTANT]
> ### 🤖 Full LLM Generated / 100% Dibuat oleh AI (LLM)
> Seluruh kode sumber, arsitektur sistem, reverse-engineering opcode MES adv32, dekoder aset, antarmuka Android/JNI, serta dokumentasi pada repositori ini **dibuat secara penuh oleh Large Language Model (LLM)**.
>
> ### 🎮 Kompatibilitas Game / Game Compatibility: Da Capo & Dal Segno Series (CIRCUS adv32)
> Runtime engine mendukung judul visual novel yang ditenagai oleh engine **CIRCUS adv32**, difokuskan secara khusus pada seri **Da Capo** dan **Dal Segno**.

---

## 🌟 What's New / Catatan Rilis

### 1. 🎮 Expanded Multi-Game Catalog (21 Official Titles)
Katalog Game Library kini mendukung dan mengenali 21 judul resmi dari seri Da Capo dan Dal Segno dengan deteksi otomatis skrip MES:
- **Da Capo 4 Series**:
  - `D.C.4 ~Da Capo 4~`
  - `D.C.4 ~Da Capo 4~ Plus Harmony` *(didukung sebagai judul terpisah dengan deteksi rute skrip `dc4_asu*`)*
- **Da Capo III Series**:
  - `D.C. III ~Da Capo III~`
  - `D.C. III R ~Da Capo III R~`
  - `D.C. III P.P. ~Da Capo III Platinum Partner~`
  - `D.C. III With You ~Da Capo III With You~`
  - `D.C. III Dream Days ~Da Capo III Dream Days~`
- **Da Capo II Series**:
  - `D.C. II ~Da Capo II~`
  - `D.C. II Spring Celebration ~Da Capo II Spring Celebration~`
  - `D.C. II P.C. ~Da Capo II Plus Communication~`
  - `D.C. II To You ~Da Capo II To You~`
  - `D.C. II Fall in Love ~Da Capo II Fall in Love~`
  - `D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~`
- **Da Capo (Original) Series**:
  - `D.C. ~Da Capo~`
  - `D.C.P.C. ~Da Capo Plus Communication~`
  - `D.C. White Season ~Da Capo White Season~`
  - `D.C. Summer Vacation ~Da Capo Summer Vacation~`
  - `D.C.A.S. ~Da Capo After Seasons~`
  - `D.C. Dream X'mas ~Da Capo Dream X'mas~`
- **Dal Segno Series**:
  - `D.S. -Dal Segno-`
  - `D.S. i.F. -Dal Segno- in Future`

*(Catatan: Seluruh varian edisi dikelompokkan ke judul utama. Judul non-adv32 seperti D.C. 4 Fortunate Departures menggunakan engine berbeda dan tidak termasuk dalam lingkup runtime ini).*

---

### 2. 💾 External Storage Direct Playback (MicroSD & USB OTG)
- Mendukung pemutaran langsung (**Direct Playback**) dari penyimpanan eksternal MicroSD dan flashdisk USB OTG (`/storage/XXXX-XXXX`).
- File game dieksekusi langsung tanpa perlu disalin ke memori internal aplikasi, menghemat ruang penyimpanan perangkat secara signifikan.
- Dilengkapi pemindai otomatis drive penyimpanan (*Storage Volume Scanner*) dan penjelajah folder bawaan (*In-app Folder Browser*).

---

### 3. 🔤 Pre-bundled Default Japanese Font (`MSGOTHIC.TTC`)
- Font standar `MSGOTHIC.TTC` terpasang otomatis dari dalam paket APK, memastikan tampilan teks dialog visual novel CIRCUS rapi dan autentik secara instan tanpa perlu menyalin font manual.
- Tetap menyediakan opsi untuk beralih ke font sistem bawaan Android (*System Default*) atau mengimpor font kustom (`.ttf`, `.otf`, `.ttc`).

---

### 4. 🗑️ Library Management & Duplicate Prevention
- Kartu game kini memiliki tombol aksi langsung: **LAUNCH** (Jalankan), **MANAGE** (Kelola), dan **REMOVE** (Hapus).
- Saat menghapus game yang disalin ke memori internal, pengguna diberikan opsi untuk membersihkan file salinan guna membebaskan ruang memori.
- Untuk game di penyimpanan eksternal, penghapusan hanya menghapus entri dari daftar; file asli di flashdisk/MicroSD dan data save tetap aman.
- Pencegahan otomatis registrasi duplikat berdasarkan jalur direktori (*canonical path*).

---

### 5. 🏗️ Arisa32 Architecture & JNI Update
- Refactoring penamaan modul runtime C++ dan JNI menjadi `arisa32_jni.cpp` dan `Arisa32Activity`.
- Mempertahankan backward compatibility untuk `Dc4Activity` agar shortcut atau intent lama tetap berfungsi.
- Mendukung arsitektur `arm64-v8a` dan `armeabi-v7a` dengan kesiapan Android 16 (API 36) dan page size 16 KB.

---

## 📦 Asset
- `Arisa32-v0.2.0-debug.apk`: Standalone debug APK untuk Android 5.0+ (ARM64 / ARMv7).
