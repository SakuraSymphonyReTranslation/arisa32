# Dal Segno: catatan lanjutan 21 November 2026

Ini pekerjaan dukungan Dal Segno MangaGamer, bukan release baru. Versi aplikasi
tetap 0.7.4. Dal Segno dan Dal Segno in Future (DSIF) adalah dua game berbeda.
Dukungan penuh DSIF belum diimplementasikan atau diuji dengan data asli.

## Implementasi

- Deteksi folder bebas memeriksa isi `MES/main.mes` dan `DS_1129B_CM.mes`,
  keduanya dengan header TE6 `0x009F9A`. Pencarian menerima perbedaan casing.
  Nama folder atau `DS.exe` saja tidak cukup. Scan otomatis masih dangkal;
  direktori lebih dalam dapat dipilih melalui folder picker.
- Penanda DSIF pada path, executable atau skenario mengeluarkan game dari profil
  Dal Segno. JNI menolak inisialisasi sebelum audio/menu dibuat. Scene, toolbar,
  save screen dan backlog juga memeriksa identitas. Ini pengaman pemisahan,
  bukan implementasi engine DSIF. Data DSIF asli diperlukan untuk profil sendiri.
- Helper pencarian file sekarang bernama `GameDataFiles.child`.
  DalSegnoProfile tidak lagi merujuk helper `Dc2PlusCommunicationProfile`.
- Menu mengikuti skrip asli. Upper menu memiliki Save, Load, Config, hide textbox,
  title dan Close. Floating Back / Upper menu membuka menu atas pada cerita dan
  menjadi Back pada menu. Tiga titik dan tombol Back bisa hide/show bersama.
- Dialog custom name Android menyediakan nama depan/belakang, keyboard virtual,
  Next/Done, validasi dan batas 19 byte per nama. Default `Takamura Atsuya`.
  Yes/No tetap memakai tampilan game. Snapshot VM DS v2 menyimpan nama;
  snapshot VM generik tetap v1.
- Config: lima tab, sebelas slider, konfirmasi serta pemulihan cerita/tampilan
  ketika keluar dari Config. Konfirmasi name/title/end ditangani profil DS.
- Backlog memakai LOGBACK/LOGCHIP, lima baris, replay voice/jump dan scrollbar.
- Music Mode memakai 42 track asli, semuanya dapat dipilih sejak awal:
  play/stop/restart, checkbox, All On/Off, playlist, next/previous, repeat satu
  lagu, repeat daftar, random, scrollbar dan penanda lagu aktif. Volume BGM/Movie
  mendukung tombol +/- dan slider. Mapping BGM memiliki celah ID 23, 26 dan 30.
- Save/load memakai aset asli dan sepuluh halaman. Latest, Auto, Quick berada
  di halaman pertama, diikuti 97 slot biasa sampai `DSAO99.dat`. Latest/Auto
  tidak ditulis melalui tombol save biasa. Batas indeks diperiksa di Java/native;
  cache Activity tetap 101 entri dengan entri di luar slot DS berisi null.
- Judul save/load berasal dari skenario. TSV DS berisi 280 judul, termasuk
  `DS_1129B_CM` = `Welcome to Paradise`. Ekstraksi Inggris dari EXE menerima `#`
  dan tidak memakai field Jepang klasik untuk DS.
- Import/export native save mempertahankan format PC. Companion canonical
  `DSAO03.arisa.dat`; companion lama `.dat.arisa` tetap dapat dibaca. Export PC
  mengecualikan tambahan Arisa; export Arisa mempertahankannya.
- Snapshot scene v13 memulihkan posisi sprite/state DS. Mapping body/accessory
  dan camera placement DS dipisahkan dari pivot generik. Audio autosave dipulihkan
  sebelum skrip dilanjutkan agar tidak menimpa BGM baru yang dimulai skrip.
- `system-profile.dat` DS menyimpan nama, settings, progress dan pilihan Music
  Mode. Jika template native `system.dat` tersedia, field yang dipahami dioverlay
  sambil mempertahankan field lain, termasuk 51 field nama/voice karakter.

## Penomoran native save

| File | Fungsi |
| --- | --- |
| `DSAO00.dat` | Latest copy |
| `DSAO01.dat` | Autosave |
| `DSAO02.dat` | Quicksave |
| `DSAO03.dat` | Slot biasa pertama |
| `DSAO99.dat` | Slot biasa ke-97, halaman 10/10 |
| `DSAO03.arisa.dat` | Companion Arisa slot pertama |
| `system-profile.dat` | Profil khusus Arisa |
| `system.dat` | Sistem native PC jika ada template |

Native save yang diperiksa berukuran `0x11AF8` (72.440 byte), timestamp `0x4566`.
Resume/id/index/text PC berada pada `0x83C/0x840/0x844/0x848`. Raw `0x03` mencatat
checkpoint dialog, `0x04` memperbarui read ranges, `0x1A` checkpoint autosave.
Jangan menyamakan ketiga operasi ini.

## Sumber analisis

Game PC asli hanya dibaca, tidak ditimpa:
`E:\Games\[170630] [MangaGamer] D.S. -Dal Segno-\[170630] [MangaGamer] D.S. -Dal Segno-`.
Data berada di `AdvData`, save di `AdvData\savedata`.

- `20261121-0705-21.5323578.mp4`: 3.969 frame, 30 fps, 132,3 detik, startup/cerita/menu/nama.
- `20261121-0756-29.8552790.mp4`: 2.318 frame, 30 fps, sekitar 77,27 detik, Music Mode.

`tools/analyze_dalsegno_video.py` mendecode tiap frame, menghasilkan CSV perubahan
dan contact sheet per detik. Semua frame diproses, tetapi tidak semua diperiksa
manual. Temp sesi:
`C:\Users\adamb\AppData\Local\Temp\arisa32-dalsegno-video-20261121`.
Gunakan subfolder `story-reference` dan `music-reference`; sebagian contact sheet
di root Temp berasal dari rekaman 0552 yang berbeda.

## Verifikasi akhir

| Pemeriksaan | Hasil |
| --- | --- |
| Build desktop seluruh target | PASS, reuse `build/desktop`, 81 pekerjaan incremental selesai |
| CTest lintas profil | 43 PASS, 0 gagal, 1 SKIP dari 44 tes, 120,11 detik |
| Tes yang dilewati | `dc2ty_voice_test`, `DC2TY_DATA` belum dikonfigurasi |
| Scene DS dengan data PC asli | PASS, title gates, custom name/Yes/No/Default, upper menu, Config dan 11 slider; 88 action Config diperiksa |
| Music Mode data PC asli | PASS, seluruh 42 track play/stop/restart, durasi/nonzero PCM, checklist, playlist/repeat/random dan volume |
| Save screen dengan aset DS asli | PASS, semua 10 halaman, 97 slot, shortcut, atlas dan batas indeks |
| Native save DS | PASS, file asli 00/01/02/03/05: PC export/import dan companion, kesetaraan frame serta teks |
| Profil sistem DS | PASS, round trip settings/progress dan template native; 51 field nama karakter dipertahankan |
| Java ProfileChecks | PASS, 524 pemeriksaan termasuk folder bebas, casing, DSIF dan judul EXE asli |
| Java ExportChecks | PASS, 349 pemeriksaan, termasuk companion DS dan pemisahan export PC/Arisa |
| Android floating navigation | PASS, 532 pemeriksaan pada package QA, DC4/DC4PH/DC2PC/DC2DM/DS, hide/show, keyboard dan ukuran dialog |
| Android debug produksi | PASS, package `org.dc4engine`, version 0.7.4/code 22, arm64-v8a |
| Pemasangan HP | PASS, `adb install -r`, tanpa clear data; Activity cold launch `Status: ok`, proses hidup |
| `git diff --check` | PASS |

APK final: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA256: `96970669D0F0DD2B44173EBAA3CF7C126992A07C09E779029851D993E39F564C`.
HP mencatat pembaruan pada 21 November 2026 pukul 17:16:13 zona lokal.
Log hasil tersedia di Temp sesi, prefix `final-`. QA memakai aplikasi terpisah;
tes floating controls tidak mengubah save game produksi.

CTest mencakup VM/audio/save, DC4 sprite/effects/message face, DC2PC menu/gallery,
DC2DM menu/transition/minigame/font serta DC2TY profile/sprite/save/menu/choice.
Ini bukti tidak ditemukan regresi pada cakupan tersebut, bukan jaminan setiap
game, setiap ending, semua fitur DSIF atau playthrough lengkap.

Review antislop sesi ini: build dan pemeriksaan interaksi di atas PASS; referensi
UI adalah aset/rekaman asli yang diminta pengguna, ENERGY 2 / RHYTHM 1 / MOTION 1.
Layout mempertahankan atlas/warna native untuk kecocokan game; dialog nama memakai
kontrol Android berlabel agar keyboard dapat digunakan. Verifikasi visual penuh
dan seluruh keyboard/relaunch Dal Segno di perangkat masih terbuka. Ini checkpoint
pekerjaan untuk dilanjutkan sesuai permintaan pengguna, bukan klaim delivery gate
visual lengkap atau release yang selesai.

## Pekerjaan yang masih perlu dilanjutkan

1. Transisi/lingkaran putih belum terbukti setara frame demi frame. Analisis raw
   `0x93` dan `0x27` pada prologue. Target handler DSAO.exe:
   `0x93 -> 0x4250F6`, `0x27 -> 0x421604`, `0x0E -> 0x420C19`.
   Temp berisi `disasm.py`, `op-targets.txt` dan disassembly terkait.
2. Choice checkpoint native save memerlukan analisis raw `0x04`/`0x53` dan
   rekonstruksi saat import. Jangan mengklaim save pilihan native setara.
   Handler: `0x53 -> 0x4204AB`, `0x04 -> 0x4242F3`, `0x03 -> 0x42421D`.
3. Bandingkan sprite body/accessory/camera dengan rekaman yang benar. Round trip
   save tidak membuktikan seluruh animasi setara PC. Slot 2..9 -> sprite 0..7;
   19 -> 12; 23 -> 13. Accessory 6..9 mengikuti body 0..3. Renderer native:
   `0x4C4C + n*0x1B0`; x/y 0/4, offset C/10, alpha 50, scale 54/58,
   pivot 60/64, draw 80, priority 1A8.
4. Tambahkan tes relaunch nama, checklist/repeat/scroll Music Mode dan replay/jump
   backlog melalui JNI. Tes native dan floating controls saja tidak membuktikan
   seluruh interaksi Android tersebut.
5. Periksa font save/load terhadap screenshot PC. Geometry/atlas sudah sesuai;
   font QA MSGothic masih lebih tipis daripada referensi native.
6. Export PC belum mendukung background multilayer, effect slot 16..18, movie
   aktif atau timed wait. Companion Arisa menyimpan state lebih lengkap.
7. Belum ada bukti playthrough lengkap, semua ending/unlock atau bebas regresi
   untuk seluruh judul. Ulangi tes relevan jika jalur bersama berubah.

## Cara melanjutkan

- Repo aktif `E:\Games\Adv32roid`; lokasi Documents menunjuk Git. Baca `AGENTS.md`.
  Mode antislop sesi ini during, dipilih pengguna.
- Desktop: `cmd /c build.bat 4`, reuse `build/desktop`; tes:
  `ctest --test-dir build/desktop --output-on-failure -j 2`.
- Jalankan `scene_test --dalsegno`, `--dalsegno-music`, `--dalsegno-native-save`
  dengan path `AdvData` PC di atas. `save_screen_test` juga menerima path data.
- Android dari `android`:
  `gradlew.bat assembleDebug -ParisaAbi=arm64-v8a --max-workers=2`.
- HP iQOO I2219, serial `10DF1N05QN000H8`, package `org.dc4engine`.
  QA terpisah `org.dc4engine.updateqa` dan `.test`.
- Data HP `/storage/emulated/0/Gamehub/[MangaGamer] D.S. -Dal Segno-/AdvData`.
- Tidak ada release/tag baru. APK tidak masuk Git; output standar
  `android/app/build/outputs/apk/debug/app-debug.apk`.
- Perubahan plugin build Android dan alat cleanup yang telah ada sebelumnya
  dipertahankan di working tree, tidak dicampur ke commit Dal Segno.
  Build sesi ini menggunakan AGP lokal 8.13.2 di `android/build.gradle`;
  baseline Git masih 8.5.2. Periksa perbedaan ini jika membangun dari clone baru.
