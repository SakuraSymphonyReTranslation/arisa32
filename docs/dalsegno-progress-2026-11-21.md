# Dal Segno: catatan lanjutan 21 November 2026

Catatan ini memuat checkpoint historis 21 November dan lanjutan sebelum commit
24 November. Status gabungan terbaru, audit semua commit, APK dan instalasi ada
di [handoff 24 November](dalsegno-handoff-2026-11-24.md). Angka pengujian dan hash
APK pada bagian historis tidak menyatakan status build terbaru.

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

## Verifikasi checkpoint 21 November 2026

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

APK pada checkpoint 21 November: `android/app/build/outputs/apk/debug/app-debug.apk`.
Output tersebut telah digantikan build 24 November yang dicatat di bawah.
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

## Lanjutan 24 November 2026: popup Android dan font native

Screenshot pengguna memperlihatkan fallback generik “Return to this backlog
scene?” dengan checkbox “Don't ask again”. Penyebabnya ada di startup JNI:
`e->confirm.load(e->data_dir)` hanya dijalankan ketika `menu_ok` bernilai true.
Dal Segno menggunakan menu scripted asli sehingga `menu_ok` false; atlas
konfirmasi Dal Segno yang sudah didukung core tidak pernah dimuat oleh Android.
Startup sekarang memuat konfirmasi untuk Dal Segno juga.

Pada HP, popup kini menggunakan atlas asli dengan teks “Proceed?” dan Yes/No.
Dialog SceneJump berada di posisi atas (top 164; pusat Yes 540,276 dan No 726,276).
Posisi Yes lama pada y=380 tidak bereaksi. Dialog nama/save tetap memakai posisi
bawah (top 268; Yes y=348..412). Jangan menyamakan hitbox kedua jenis dialog.

Font PC dipastikan melalui executable dan pembacaan memori proses: PC memuat
`AdvData/GRP/mplus-1c-heavy.ttf`, face M+ 1c heavy, ukuran file 1.731.500 byte.
Android kini memilih font ini sebagai default Dal Segno melalui `GameFonts` dan
`DalSegnoProfile.resolveDefaultFont`. Pilihan font eksplisit tetap dipertahankan;
game lain dan data tanpa font native tetap memakai fallback sebelumnya.
Picker menampilkan “M+ 1c Heavy (Game default)”.

Assertion Java awal gagal karena membandingkan string path dengan casing berbeda
di Windows. Resolver menunjuk file yang sama; pemeriksaan sekarang membandingkan
identitas `File`. Tidak ada perubahan resolver untuk menyamarkan kegagalan tes.

| Pemeriksaan lanjutan | Hasil |
| --- | --- |
| Gradle `assembleDebug -ParisaAbi=arm64-v8a --max-workers=2` | PASS, 1m9s |
| Java ProfileChecks dengan EXE asli | PASS, 533 pemeriksaan |
| CTest desktop terbaru | 44 PASS, 0 gagal, 1 SKIP dari 45; 121,78 detik |
| Native save desktop asli | PASS, 00/01/02/03/05/06 dan profil sistem |
| JNI HP: popup dengan override MSGothic | PASS, atlas asli, posisi atas, cancel, No, Yes, Back |
| JNI HP: default M+ 1c heavy | PASS, resolver produksi menunjuk font asli yang dapat dibaca |
| JNI HP: nama | PASS, validasi, edit, cancel, No, Default, Yes dan speaker cerita |
| JNI HP: relaunch nama | PASS, Budi/Adam dipulihkan setelah `app_process` baru |
| JNI HP: enam native save dan companion | PASS, teks/speaker/pixel clean scene identik |
| JNI HP: backlog | PASS, replay voice, cancel/drag, posisi lama inert, No mempertahankan posisi, Yes memulihkan teks/speaker checkpoint sebelumnya, Back |
| `git diff --check` | PASS, hanya peringatan konversi CRLF |

APK yang diuji: `android/app/build/outputs/apk/debug/app-debug.apk`.
Package `org.dc4engine`, version 0.7.4/code 22, ABI arm64-v8a.
SHA256: `37AEE83F61A73F0AE034D617E28907048F0CC52E34872FA2C99B3152D214AF1F`.
JNI SHA256: `90B2DE4920FB410F6C7BF508B1A14A6B85E689424F73903FB2FA8E5655F3720F`.
Build ini belum dipasang ke aplikasi produksi di HP. Tes menggunakan JNI dari
APK melalui `app_process`; save/profile pemain tidak disentuh. Tidak ada commit,
push, tag atau release baru pada lanjutan ini.

Harness yang dipelihara: `tests/android/run_dalsegno_probe.py` dan
`tests/android/dalsegno-probe/org/dc4engine/Arisa32Activity.java`; cara pakai ada di
`tests/android/README.md`. Bukti sementara:

- MSGothic/popup: `C:\Users\adamb\AppData\Local\Temp\arisa32-dalsegno-jni-ugje7uus`.
- Default native/seluruh kasus: `C:\Users\adamb\AppData\Local\Temp\arisa32-dalsegno-jni-7oab_mf8`.
- `captures/backlog-confirmation.png` memperlihatkan dialog asli;
  `captures/backlog-jump.png` memperlihatkan checkpoint yang dipulihkan.

Perubahan core sebelumnya juga sudah diperiksa: varian sprite `_B` memakai aset
asli tanpa multiplier skala 1,28; companion v14 menyimpan native camera originals
B8/BC/C0/C4 dan tetap menerima v13; background memakai center pivot native;
preload raw46 tidak memulai voice, raw5C `.` mempertahankan voice dan `-`
menghapusnya, raw06 menghapus asosiasi replay; accessory mengikuti body motion.
Tes motion dan confirm dialog lulus. Perbandingan body yang sudah berhenti
bergerak mendekati PC dalam sekitar 2 pixel; frame pan stabil berada pada detik
48, bukan 47 yang masih bergerak atau 49 yang mulai reset.

Review visual memakai artwork, warna, komposisi dan font game asli sesuai arah
pengguna (ENERGY 2 / RHYTHM 1 / MOTION 1). Interaksi popup yang diubah telah
diverifikasi satu per satu melalui JNI. Keyboard/picker Activity dan seluruh
playthrough tetap belum tercakup. Ini verifikasi perbaikan popup, bukan klaim
kesetaraan seluruh game.

Delivery gate pada lingkup perbaikan loading popup:

| Blok | Hasil dan bukti |
| --- | --- |
| Hard Gate | PASS pada perubahan ini: APK dibangun, artwork game asli dimuat, popup muat dalam canvas 1280x720, semua gesture konfirmasi yang diubah diuji |
| Purpose-Gate | PASS: atlas, warna dan font berasal dari game PC untuk memenuhi referensi pengguna; tidak ada dekorasi baru |
| Liveliness | PASS: ENERGY 2 / RHYTHM 1 / MOTION 1; motif gear dan fokus Yes/No tetap memakai komposisi asli |
| Craftsmanship / Quality Locks | PASS pada perilaku yang diubah: posisi atas/bawah berbeda, cancel dan No menjaga state, Yes mengembalikan checkpoint yang benar; hasil dicatat oleh harness |

Sheet kamera sementara juga telah diregenerasi dengan pan pada detik 48:
`C:\Users\adamb\AppData\Local\Temp\arisa32-dalsegno-20261123\camera-comparison.png`.
Row pan memakai 419 inlier SIFT, scale 1,00153 dan translasi sekitar +1,39/-0,19
pixel. `pc-pan-stable.png` sekarang memakai frame 48 detik yang benar.

Temuan awal lanjutan: satu baris backlog menampilkan literal `#h` pada akhir
“The island where no one is ever unhappy”. Temuan ini diperbaiki pada lanjutan
berikut. Choice checkpoint, seluruh transisi dan batas export PC tetap terbuka.

## Koreksi inline image 24 November 2026

Analisis DSAO.exe membuktikan `#h` dan alias `#b` memuat `GRP/bar.CRX`, bukan
teks biasa. Handler `0x43099D..0x430AA7` memeriksa `#h` di `0x4309B3` dan memanggil
loader `0x43C080` melalui `0x4309CD`. Aset asli berukuran 19x22 RGBA dengan warna
dan outline sendiri. Renderer DS kini memakai aset tersebut untuk dialog dan
backlog, dengan advance 19 pixel, wrapping dan reveal satu karakter. Teks mentah
di scene/save tetap mempertahankan tag; hanya tampilan DS yang mengubahnya.
Profil lain dan tag yang belum dikenal tetap memakai perilaku sebelumnya.

Tes desktop `dalsegno_text_test` lulus setelah fixture MES diperbaiki: spasi DS
harus diencode sebagai `$` karena byte nol adalah terminator. Probe backlog HP
pada APK terbaru lulus, termasuk checkpoint garis, replay, cancel/drag, popup
atas, No/Yes dan Back. Bukti:
`C:\Users\adamb\AppData\Local\Temp\arisa32-dalsegno-jni-9hl5pgkt`.
Capture `backlog-inline-bar.png` tidak lagi menampilkan `#h`. Posisi vertikal garis
sesuai baris referensi PC setelah title bar 26 pixel dan posisi row diselaraskan;
geometri font seluruh kalimat belum dinyatakan identik.

Tag gambar lain `#s` (`dbr2.crx`), `#e` (`dbr.crx`) dan `#o` (`onp.crx`) ditemukan
di executable tetapi belum diimplementasikan atau diuji. Gunakan handoff terbaru
untuk hash APK, hasil suite akhir, commit dan status instalasi.
