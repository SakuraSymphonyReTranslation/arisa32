**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.5.5 - Runtime CIRCUS adv32 untuk Seri Da Capo dan Dal Segno

> [!IMPORTANT]
> ### Seluruhnya dibuat oleh AI (LLM)
> Seluruh kode sumber, arsitektur sistem, reverse-engineering opcode MES adv32, dekoder aset, antarmuka Android/JNI, serta dokumentasi pada repositori ini **dibuat secara penuh oleh Large Language Model (LLM)**.
>
> ### Kompatibilitas game: seri Da Capo dan Dal Segno (CIRCUS adv32)
> Runtime engine mendukung visual novel engine **CIRCUS adv32**, difokuskan secara khusus pada seri **Da Capo** dan **Dal Segno**.

---

## Catatan rilis v0.5.5

Rilis **v0.5.5** merupakan pembaruan terfokus (*targeted hotfix release*) yang secara khusus menyelesaikan masalah sinkronisasi audio dan video pada adegan pertunjukan lagu Kotori di prolog **D.C.4 ~Da Capo 4~** dan **D.C.4 ~Da Capo 4~ Plus Harmony**.

---

### Perbaikan audio prolog Kotori (D.C.4 dan D.C.4 Plus Harmony)

#### Masalah yang diperbaiki

- **Audio terpotong dan lirik berulang dua kali:**
  Sebelumnya, saat prolog dimulai, lirik lagu terdengar terpotong dan terulang kembali dua kali (*"kimi no tsusumu kimi no tsusumu chisana sakura no yume"*). Hal ini disebabkan oleh timing jeda intro skrip yang terlalu panjang sehingga BGM 43 (`043.pcm`) sudah terlanjur melantunkan vokal sebelum video diputar, lalu BGM mendadak dimatikan saat video `KotoRI_uta01.mpg` mulai memutar audionya sendiri.
- **Audio berhenti mendadak setelah video selesai:**
  Ketika video animasi selesai diputar (durasi 22.7 detik), lagu langsung mati seketika dan meninggalkan adegan dialog setelahnya dalam keheningan total. Seharusnya di versi PC, lagu Kotori terus mengalun mengiringi seluruh dialog hingga adegan berpindah ke slide transisi `BG\BLACK`.

#### Implementasi

- **Sinkronisasi Presisi Timing Opcode `0x15(72)`:**
  Menyelaraskan durasi tunggu intro skrip secara akurat ke **12.580 ms (12,58 detik)** khusus untuk D.C.4 & D.C.4 Plus Harmony. Waktu ini berhimpitan sempurna dengan mulainya lirik vokal pada BGM 43 (detik 12,70) dan beat drop pada video performa Kotori.
- **Satu sumber audio yang terus berjalan:**
  Saat memutar cutscene lagu Kotori (`KotoRI_uta01.mpg` / `KotoRI_uta02.mpg`), engine audio tidak mematikan BGM 43 (`stop_group(Bgm)` dilewati) dan video diputar secara hening dengan sinkronisasi clock visual. BGM 43 menjadi satu-satunya sumber audio berkualitas tinggi yang mengalir mulus tanpa potongan dan otomatis terus berlanjut mengiringi dialog bernyanyi hingga transisi slide skenario selesai.
- **Sinkronisasi saat video dilewati:**
  Apabila pengguna men-tap layar untuk melewati (*skip*) video, posisi audio BGM 43 otomatis dimajukan (`seek_pcm`) menyamai durasi akhir video agar dialog berikutnya tetap sinkron.
- **Penanganan pemutar Java (`Arisa32Activity.java`):**
  Menambahkan pengecekan `isKotoriSongMovie` pada fallback MediaPlayer dan LibVLC untuk mematikan volume video (`setVolume(0)`) dan menahan perebutan *audio focus* agar tidak menginterupsi audio native BGM 43.
- **Pembatasan berdasarkan profil game:**
  Seluruh logika perbaikan dibungkus dengan guard `is_dc4_or_dc4ph_game(...)` berbasis deteksi file signature skrip unik (`dc4_start.mes`, `dc4_sen20181213b.mes`, `DC4.exe`, `DC4PHDL.exe`), sehingga perilaku ini hanya diterapkan pada D.C.4 dan D.C.4 Plus Harmony, dengan judul Circus lain (D.C. II, D.C. III, Dal Segno, dll.) berada di luar jalur perbaikan tersebut.

---

### Unduhan

[Buka halaman unduhan v0.5.5](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.5.5)

- `Arisa32-v0.5.5.apk`: Paket instalasi rilis mandiri Android (mendukung Android 5.0+ / API 21 - API 36, arsitektur `arm64-v8a` & `armeabi-v7a`).
- `Arisa32-v0.5.5-debug.apk`: Paket debug mandiri dengan pencatatan logcat terperinci.
