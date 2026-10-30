# Arisa32

**Bahasa Indonesia** | [English](README.en.md)

Arisa32 adalah implementasi ulang runtime visual novel CIRCUS adv32 dalam C++17 untuk seri Da Capo dan Dal Segno. Arisa32 berjalan di Windows dengan SDL2 dan di Android dengan NativeActivity + JNI, serta membaca skrip MES, gambar CRX/CRM, dan audio XPCM langsung dari instalasi game.

**Rilis terbaru: [v0.6.2](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.2)**. Rilis ini menambahkan dukungan beta D.C. II Dearest Marriage, perbaikan teks Sakuramusubi, serta perbaikan pemutaran opening dan ending. Baca [catatan rilis](docs/releases/v0.6.2/id.md) untuk rincian dan bug yang masih ada. Riwayat versi tersedia di [catatan perubahan](CHANGELOG.md).

> [!IMPORTANT]
> Seluruh kode sumber, arsitektur sistem, rekayasa balik opcode MES adv32, dekoder aset, antarmuka Android/JNI, dan dokumentasi repositori ini dibuat oleh Large Language Model (LLM).
>
> Nama proyek ini adalah Arisa32, menggantikan nama internal `dc4engine`. Paket Android tetap memakai identifier `org.dc4engine` untuk menjaga kompatibilitas data simpan dan lokasi berkas aplikasi.

> [!NOTE]
> Repositori ini tidak menyertakan aset game. Anda harus memiliki salinan sah dari game yang didukung. Arisa32 membaca skrip, gambar, dan audio dari folder `AdvData` tanpa mengubah aset tersebut. Arisa32 dapat menulis data simpan di folder game; lihat [data simpan](#data-simpan).

## Game yang didukung

Arisa32 mengenali 21 judul resmi berbasis CIRCUS adv32 dari seri Da Capo dan Dal Segno melalui deteksi skrip dan aset:

### Seri Da Capo 4

- `D.C.4 ~Da Capo 4~`
- `D.C.4 ~Da Capo 4~ Plus Harmony` (judul terpisah dengan deteksi rute skrip khusus)

### Seri Da Capo III

- `D.C. III ~Da Capo III~`
- `D.C. III R ~Da Capo III R~`
- `D.C. III P.P. ~Da Capo III Platinum Partner~`
- `D.C. III With You ~Da Capo III With You~`
- `D.C. III Dream Days ~Da Capo III Dream Days~`

### Seri Da Capo II

- `D.C. II ~Da Capo II~`
- `D.C. II Spring Celebration ~Da Capo II Spring Celebration~`
- `D.C. II P.C. ~Da Capo II Plus Communication~`
- `D.C. II To You ~Da Capo II To You~`
- `D.C. II Fall in Love ~Da Capo II Fall in Love~`
- `D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~` (beta)

Menurut pengujian pengguna, D.C. II Dearest Marriage sudah dapat dimainkan sampai selesai. Dukungan ini masih berstatus beta: font menu terlalu besar, warna teks beberapa popup belum sesuai, suara karakter dapat berulang, dan masih ada bug lain. Lihat [bug DC2DM yang masih diketahui](CHANGELOG.md#bug-dc2dm-yang-masih-diketahui).

### Seri Da Capo pertama

- `D.C. ~Da Capo~`
- `D.C.P.C. ~Da Capo Plus Communication~`
- `D.C. White Season ~Da Capo White Season~`
- `D.C. Summer Vacation ~Da Capo Summer Vacation~`
- `D.C.A.S. ~Da Capo After Seasons~`
- `D.C. Dream X'mas ~Da Capo Dream X'mas~`

### Seri Dal Segno

- `D.S. -Dal Segno-`
- `D.S. i.F. -Dal Segno- in Future`

Edisi Standard, Limited, Deluxe, dan Download dikelompokkan ke judul utamanya. Game yang memakai engine lain, seperti D.C.4 Fortunate Departures, tidak didukung.

## Platform dan fitur

### Windows

- Render 1280×720 dengan letterbox untuk mempertahankan rasio gambar.
- Render teks memakai font Windows, dengan MS Gothic sebagai pilihan bawaan dan opsi font TrueType lain.
- Pemutaran BGM, efek suara, dan suara karakter dari audio XPCM.
- Sistem Save/Load dengan 100 slot, quicksave, dan riwayat dialog.
- Layar judul, pengaturan, galeri, dan pemilih adegan.

### Android

- Pustaka game yang menyimpan daftar game dan mendeteksi 21 judul di atas secara otomatis.
- Pembacaan langsung dari MicroSD dan USB OTG (`/storage/XXXX-XXXX`) tanpa harus menyalin semua berkas ke penyimpanan internal.
- Font bawaan `MSGOTHIC.TTC` untuk teks Jepang, serta pilihan font sistem atau font TTF/OTF/TTC sendiri.
- Penghapusan game dari pustaka dengan pilihan untuk menghapus berkas salinan, serta deteksi entri duplikat.
- Antarmuka `ArisaUi` bertema adv32/CIRCUS dengan ikon lambang Arisa.
- Impor dan ekspor arsip data simpan dalam format ZIP.
- Kontrol sentuh layar penuh, riwayat dialog, dan skip dialog.
- Target Android API 36, mendukung Android 5.0 / API 21 ke atas, `arm64-v8a` dan `armeabi-v7a`, serta ukuran halaman memori 16 KB.

## Build dan jalankan di Windows

Pasang Visual Studio atau Visual Studio Build Tools dengan komponen **Desktop development with C++** dan **C++ CMake tools for Windows**, lalu jalankan dari folder repositori:

```bat
build.bat
build\desktop\arisa32_desktop.exe "D:\Games\DC4\AdvData"
```

`build.bat` mendeteksi Visual Studio dan membuat build Release x64 di `build/desktop`. Header dan pustaka impor SDL2 tersedia di `third_party/sdl2`; proses build menyalin `SDL2.dll` ke samping `arisa32_desktop.exe`.

Anda juga dapat menentukan lokasi game lewat variabel lingkungan `DC4_ADVDATA`:

```bat
set "DC4_ADVDATA=D:\Games\DC4\AdvData"
build\desktop\arisa32_desktop.exe
```

Jika Anda tidak memberikan lokasi, aplikasi mencari folder `DC4/AdvData` di sekitar direktori kerja dan executable. Untuk D.C.4, adegan pembuka bawaan adalah `dc4_kyo20190204a1.mes`.

## Build untuk Android

Untuk membuat APK debug di Linux, macOS, atau Windows melalui Git Bash / MSYS2:

```bash
bash android/build_apk.sh
```

Skrip menghasilkan `android/Arisa32-debug.apk` dari proyek Gradle yang sama dengan Android Studio. Siapkan:

- Android SDK dengan platform `android-36` dan Build Tools yang dibutuhkan Gradle.
- Android NDK `27.3.13750724`.
- CMake `3.22.1`.
- JDK 17 atau lebih baru.

APK siap pasang juga tersedia di [GitHub Releases](https://github.com/SakuraSymphonyReTranslation/arisa32/releases). Lihat [panduan Android](android/README.md) untuk izin aplikasi, pengaturan penyimpanan, dan pemasangan lewat ADB.

## Kontrol membaca di desktop

| Input | Tindakan |
| --- | --- |
| Klik kiri, Space, Enter | Tampilkan seluruh teks yang sedang diketik, lalu lanjutkan dialog |
| Klik kanan, H, tombol OFF/Close | Sembunyikan atau tampilkan jendela pesan |
| A, tombol AUTO | Lanjutkan otomatis setelah teks dan suara selesai |
| S, tombol SKIP; tahan Ctrl | Lewati dialog |
| V, tombol VOICE | Putar ulang suara dialog saat ini |
| F5, tombol QUICK SAVE | Buat quicksave |
| F9, tombol QUICK LOAD | Muat quicksave |
| Tombol SAVE / LOAD | Buka layar Save/Load dengan 100 slot |
| 1-9 / 0 di Save/Load | Pilih slot 1-10 pada halaman saat ini |
| Kiri/Kanan, Page Up/Down di Save/Load | Ganti halaman; berputar di antara 10 halaman |
| Tab di Save/Load | Beralih antara layar Save dan Load |
| B, tombol BACKLOG, roda mouse ke atas | Buka riwayat dialog |
| Roda mouse, tombol panah, Page Up/Down | Gulir riwayat dialog |
| Klik pilihan atau tekan 1-9 | Pilih jawaban |
| F11, Alt+Enter | Beralih ke atau dari layar penuh |
| F1 | Tampilkan dialog kontrol |
| Esc | Tutup riwayat dialog, batalkan auto/skip, atau keluar |

## Data simpan

Format simpan bergantung pada game:

| Game | Format |
| --- | --- |
| D.C. II P.C., D.C.4, D.C.4 Plus Harmony | Format PC `.dat`, dengan berkas pendamping `.dat.arisa` untuk keadaan tambahan Arisa32 |
| D.C. II Dearest Marriage | Format portabel `.dc4sav`; impor save PC belum didukung |
| Profil tanpa dukungan format PC | Format portabel `.dc4sav`, dengan berkas pratinjau dan metadata |

Arisa32 dapat membaca dan menulis format PC untuk tiga judul pada baris pertama. Berkas `.dat.arisa` menyimpan keadaan tambahan, termasuk posisi pemutaran audio. Game PC mengabaikan berkas pendamping ini. Arisa32 tetap dapat memuat `.dat` tanpa pendampingnya, tetapi musik dapat mulai dari awal dan sebagian keadaan tambahan tidak tersedia.

Di Windows, Arisa32 memakai folder simpan game jika tersedia, lalu mencoba membuat folder simpan di dalam `AdvData`. `build/desktop/saves/` menjadi lokasi cadangan. Gunakan `DC4_SAVE_DIR` untuk menentukan lokasi sendiri. Di Android, Arisa32 memakai folder simpan game jika dapat ditulisi, atau penyimpanan internal khusus aplikasi jika tidak.

Ekspor ZIP di Android menyediakan pilihan dengan atau tanpa `.dat.arisa`. Pilihan dengan pendamping menyimpan cadangan lebih lengkap untuk dipulihkan di Arisa32.

## Pengembangan dan diagnostik

Contoh berikut memakai D.C.4. Tentukan lokasi `AdvData` lewat `DC4_ADVDATA` seperti pada petunjuk Windows di atas.

```bat
rem Langsung membuka adegan tertentu:
build\desktop\arisa32_desktop.exe --scene dc4_kyo20190204a2

rem Mengambil tangkapan layar 1280x720 tanpa membuka jendela:
build\desktop\arisa32_desktop.exe --scene dc4_kyo20190204a2 --line 0 --shot work\scene.bmp

rem Memeriksa layar judul atau menguji audio:
build\desktop\arisa32_desktop.exe --title
build\desktop\arisa32_desktop.exe --testaudio

rem Menampilkan seluruh opsi CLI:
build\desktop\arisa32_desktop.exe --help
```

Jalankan pengujian regresi CTest dari lingkungan yang menyediakan compiler C++, CMake, dan Ninja:

```sh
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop --parallel
ctest --test-dir build/desktop --output-on-failure
```

## Struktur repositori

- `core/`: VM skrip C++17, interpreter MES, dekoder CRX/CRM, audio XPCM, dan render adegan.
- `sdl_desktop/`: Aplikasi desktop SDL2, pengelolaan jendela, pemutaran audio, riwayat dialog, dan tangkapan layar.
- `android/`: Aplikasi Android, penghubung JNI, komponen `ArisaUi`, dan pustaka game.
- `tools/`: Disassembler `mesdis`, alat pemeriksaan aset `crx_tool`, `crm_tool`, `voicetool`, dan alat analisis lain.
- `tests/`: Pengujian unit dan regresi untuk opcode VM, data simpan, audio, dan pemuatan sumber daya.
- `docs/`: Dokumentasi rekayasa balik, tabel dispatch opcode, dan temuan tentang engine.

## Status dan batasan

- Cakupan proyek adalah skrip dan aset CIRCUS adv32 untuk seri Da Capo dan Dal Segno. Game dengan engine lain tidak didukung.
- Alur dialog, percabangan pilihan, suara karakter, transisi latar, CG, BGM, dan menu sistem sudah tersedia. Sebagian opcode dan perilaku game masih memerlukan pemeriksaan. Audit dalam [RE-HANDOFF.md](docs/RE-HANDOFF.md) bagian 12 mencatat sekitar 39% kemunculan opcode belum memiliki jalur kode eksplisit pada 13 Oktober 2026; angka historis ini bukan pengukuran cakupan rilis saat ini.
- Kompatibilitas save PC terbatas pada judul yang tercantum di [data simpan](#data-simpan). DC2DM masih memakai save portabel dan memiliki bug UI serta suara.
- Arisa32 adalah proyek rekayasa balik independen dengan pendekatan clean-room untuk riset dan pelestarian, bukan produk resmi CIRCUS. Proyek ini masih dalam pengembangan.

## Hak cipta dan merek dagang

Nama **D.C. ~Da Capo~**, **D.C. II**, **D.C. III**, **D.C.4**, **D.S. -Dal Segno-**, serta **CIRCUS / adv32** dan aset game terkait adalah kekayaan intelektual CIRCUS.

Proyek ini bertujuan untuk pendidikan, interoperabilitas, dan pelestarian. Repositori tidak menyertakan atau mendistribusikan aset game berhak cipta, termasuk skrip `.mes`, gambar `.crx`/`.crm`, dan audio `.xpcm`/`VOICE.DAT`. Pengguna harus menyediakan salinan game sah milik sendiri.
