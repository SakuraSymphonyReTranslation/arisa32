**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.5.0 - Runtime CIRCUS adv32 untuk Seri Da Capo dan Dal Segno

> [!IMPORTANT]
> ### Seluruhnya dibuat oleh AI (LLM)
> Seluruh kode sumber, arsitektur sistem, reverse-engineering opcode MES adv32, dekoder aset, antarmuka Android/JNI, serta dokumentasi pada repositori ini **dibuat secara penuh oleh Large Language Model (LLM)**.
>
> ### Kompatibilitas game: seri Da Capo dan Dal Segno (CIRCUS adv32)
> Runtime engine mendukung visual novel engine **CIRCUS adv32**, difokuskan secara khusus pada seri **Da Capo** dan **Dal Segno**.

---

## Catatan rilis v0.5.0

### Pemutaran video dengan LibVLC

- **Dukungan Penuh Codec MPEG-2 (`mpeg2video`) & MP4 Container**: Mengintegrasikan perpustakaan resmi VideoLAN `org.videolan.android:libvlc-all:3.6.5` yang menyematkan decoder software FFmpeg internal.
- **Solusi Tuntas Black Screen Opening 3 (D.C.4 Plus Harmony)**: File `DC4_OP3.mpg` bawaan dari link patch VNDB / fan translation patch (yang dikemas dalam container MP4 dengan video stream MPEG-2 Main Profile) kini dapat diputar secara lancar dengan video dan audio sinkron **tanpa mengharuskan pengguna melakukan re-encode offline sama sekali**.
- **`VLCVideoLayout` berbasis hardware**: Rendering video memanfaatkan `VLCVideoLayout` berbasis hardware `SurfaceView` yang langsung berkomunikasi dengan komposit SurfaceFlinger Android, menghasilkan pemutaran yang mulus dan hemat konsumsi daya.
- **Pemilihan backend video otomatis**: Video MPEG-1 Program Stream murni tetap diproses instan via C++ `pl_mpeg`, sedangkan video dengan container MP4/MKV atau codec modern/legacy (MPEG-2, MPEG-4, H.264, AAC, AC3) ditangani secara otomatis oleh LibVLC dengan fallback ke MediaPlayer.

### Prioritas direktori video patch dan fallback

- **Aturan berdasarkan bahasa patch**:
  - Pilihan Bahasa Indonesia (`ID PATCH`): Memprioritaskan pencarian video pada `id_Data/Movie` / `id_Data/MOVIE`.
  - Pilihan Bahasa Inggris (`ENG PATCH`): Memprioritaskan pencarian video pada `eng_Data/Movie` / `eng_Data/MOVIE`.
  - Pilihan Bahasa Jepang Asli (`JP Original`): Memprioritaskan pencarian video pada `AdvData/Movie` / `Advdata/MOVIE`.
- **Fallback otomatis**: Apabila file video tidak ditemukan di dalam direktori patch yang bersangkutan, sistem otomatis fallback ke direktori video utama `AdvData/Movie` atau `Advdata/MOVIE`.
- Format video `.mpg` dan `.mpeg` diprioritaskan terlebih dahulu sebelum format video lainnya.

### Profil game dan pemindaian direktori

- **Modularisasi Tanpa Hardcoded**: Mengganti pemindaian hardcoded statis dengan arsitektur terstruktur melalui `GameProfileRegistry`, `Dc4Profile`, `Dc4PlusHarmonyProfile`, dan `GenericCircusProfile`.
- **Pencarian Dinamis Lintas Direktori**: Mendukung pemindaian rekursif otomatis di berbagai lokasi penyimpanan (Internal Storage, Kartu SD Eksternal, USB OTG, folder `Gamehub`, `Games`, `Download`, serta folder kustom pilihan pengguna).

### Pembuatan SaveData dan `system-profile.dat`

- **Regenerasi Folder Otomatis**: Jika folder `SAVEDATA` / `savedata` / `SaveData` dihapus, terhapus, atau belum ada, Arisa32 otomatis mendeteksi profil game dan mengeksekusi `mkdirs()` untuk membuat foldernya kembali.
- **Regenerasi Instan File `system-profile.dat`**: Pada saat inisialisasi game (`dc4Init`), engine secara otomatis mendeteksi ketiadaan `system-profile.dat` dan langsung mengenerate file default saat itu juga, menjamin data konfigurasi dan flags sistem selalu siap pakai.
- Berlaku universal untuk seluruh judul game CIRCUS ADV32 (D.C.4, D.C.4 Plus Harmony, D.C.3, D.C.2, Dal Segno, dll.).

### Build dan kompatibilitas sistem

- Menggunakan compile SDK 36 dengan `versionCode 5` dan `versionName '0.5.0'`.
- Menambahkan aturan kemasan Gradle `pickFirsts += ['**/libc++_shared.so']` untuk menyelaraskan dependensi native C++ STL antara CMake dan LibVLC.

---

### Unduhan

[Buka halaman unduhan v0.5.0](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.5.0)

- `Arisa32-v0.5.0.apk`: Paket instalasi rilis mandiri Android (mendukung Android 5.0+ / API 21 - API 36, arsitektur `arm64-v8a` & `armeabi-v7a`).
- `Arisa32-v0.5.0-debug.apk`: Paket debug mandiri dengan pencatatan logcat terperinci.
