# Validasi potret textbox DC4 / DC4PH

Perbaikan ini mengembalikan potret textbox yang dimuat melalui slot 31 dan 32 pada MES `0xAAB6` (DC4) dan `0xABB6` (DC4PH). Kasumi memakai jalur ini, terpisah dari sprite tubuh dan ekspresi karakter. Aset, ukuran, serta posisi potret mengikuti game asli.

Validasi dilakukan pada 6 November 2026. APK berasal dari working tree yang juga memuat perubahan sebelumnya pada navigasi, cursor, dan galeri.

## Hasil pengujian

| Pemeriksaan | Hasil | Bukti |
| --- | --- | --- |
| Build desktop Release | Lulus | `build.bat 2` selesai tanpa error |
| Seluruh CTest | 26/26 lulus | [all-tests.log](E:/Games/Adv32roid/build/dc4-face-validation/all-tests.log) |
| Adegan Kasumi, data DC4 | Lulus, 22.677 piksel potret opak sesuai aset | [dc4-installed-test.log](E:/Games/Adv32roid/build/dc4-face-validation/dc4-installed-test.log) |
| Adegan Kasumi, data DC4PH | Lulus, 22.677 piksel potret opak sesuai aset | [dc4ph-installed-test.log](E:/Games/Adv32roid/build/dc4-face-validation/dc4ph-installed-test.log) |
| Decode referensi FACE DC4 | 488 berhasil, 0 gagal | [dc4-decode.log](E:/Games/Adv32roid/build/dc4-face-validation/dc4-decode.log) |
| Decode referensi FACE DC4PH | 517 berhasil, 0 gagal | [dc4ph-decode.log](E:/Games/Adv32roid/build/dc4-face-validation/dc4ph-decode.log) |
| APK Release, arm64 dan armv7 | Build lulus | `:app:assembleRelease` |
| Library arm64 dari APK, perangkat Android | Lulus | [result.log](E:/Games/Adv32roid/build/dc4-face-validation/android-ec2b2497a5ea/result.log) |

`dc4_message_face_test` memeriksa origin startup, crop CRX, perataan bawah untuk dua tinggi canvas, prioritas direktori FACE ketika nama yang sama ada di BST, posisi yang tidak terpengaruh kamera, pergantian background, sembunyikan/tampilkan textbox, pergantian dialog, aksesori, explicit clear, aset yang hilang, serta save/load portable dan native. Fixture memakai kedua versi MES. Uji format save lama juga memeriksa perintah voice dan lompatan yang melewati pemuatan potret.

Pengujian data terpasang menjalankan `dc4_mid20190330b.mes` hingga referensi `FACE\KSS_A1_A01,FACE\KSS_A` muncul. Pada kedua data yang diuji, dialog tersebut berindeks 3 dalam runner. Snapshot portable baru dan snapshot yang disimulasikan sebagai versi lama menghasilkan frame yang sama setelah dipulihkan. Uji native memeriksa referensi, origin, offset, dan hilangnya potret saat dialog dilanjutkan.

Pemindaian referensi membaca stream awal setiap MES sampai NOP pertama. Jumlah decode di atas merupakan referensi unik yang ditemukan oleh pemindaian tersebut, bukan bukti bahwa seluruh rute game sudah dimainkan.

## Perilaku mesin asli dan implementasi

Disassembly lokal yang digunakan: [DC4](E:/Games/Adv32roid/build/dc4-face-validation/dc4-disasm.txt) dan [DC4PH](E:/Games/Adv32roid/build/dc4-face-validation/dc4ph-disasm.txt).

| Perilaku | Bukti native | Implementasi |
| --- | --- | --- |
| Origin potret | DC4 `0x42D1F3`, DC4PH sekitar `0x42D071`; opcode `0x8D` memakai dua signed word pertama | Origin diambil dari skrip startup dan perintah skenario |
| Perataan bawah | DC4 `0x4313CB..0x431444`, DC4PH `0x431292`; offset Y = `225 - canvas_height` | Crop CRX ditambahkan setelah origin dan offset canvas |
| Lapisan aksesori | DC4 `0x43151D..0x431587` menyalin transform utama | Slot 32 memakai origin dan offset utama saat dimuat |
| Dialog dilanjutkan | DC4 `0x4352D7`, DC4PH `0x435147` melepas slot 31 | Potret utama dan nama resource dihapus ketika dialog diterima |
| Hapus eksplisit | DC4 `0x42C687`; opcode `0x64`, string `-` | Kedua slot dihapus; `.` dan string kosong tidak menghapus |
| Urutan gambar | DC4 `0x422ACA/0x422AD5`, DC4PH `0x42293A/0x422945` | Potret utama lalu aksesori, pada lapisan textbox, tanpa transform kamera |
| Native save | Slot resource 31/32, record renderer 75/76 | Import/export menyimpan resource, origin, offset, alpha, dan canvas |

Kasumi mempunyai crop 204×205 dengan offset `(48,20)`, canvas setinggi 225, dan origin startup `(0,495)`. Hasil akhirnya berada di `(48,515)`, sampai batas bawah frame 720.

Snapshot portable DC4/DC4PH kini memakai versi 10 untuk menandai dukungan potret. Reader tetap menerima versi terdahulu. Untuk record potret kosong pada save lama, pemulihan membaca perintah visual tanpa menjalankan ulang VM, audio, atau perubahan progress. Pemulihan ini terbatas pada informasi yang masih tersedia di skrip; riwayat cabang dan transform dari skenario sebelumnya tidak tersimpan dalam save lama.

## Pemeriksaan gambar

Tangkapan berikut diperiksa pada frame 1280×720. Potret berada di kiri textbox dan tidak menutupi dialog atau toolbar. Referensi PC pengguna memiliki resolusi berbeda, sehingga pembandingan dilakukan pada posisi serta proporsi potret, bukan kesamaan seluruh frame atau waktu efek lingkungan.

- [DC4, Kasumi pada dialog sapaan](E:/Games/Adv32roid/build/dc4-face-validation/kasumi-translated.png)
- [DC4PH, Kasumi pada dialog sapaan](E:/Games/Adv32roid/build/dc4-face-validation/kasumi-dc4ph.png)
- [DC4, narasi berikutnya tanpa potret](E:/Games/Adv32roid/build/dc4-face-validation/narration-translated.png)
- [Referensi PC dari pengguna](E:/Games/Adv32roid/build/dc4-face-validation/pc-reference.png)

## APK dan reproduksi Android

[Arisa32-dc4-message-face.apk](E:/Games/Adv32roid/build/dc4-face-validation/Arisa32-dc4-message-face.apk), 13.332.790 byte.

SHA256 APK:

```text
e8ce56f5c98f74bafb56d808bb6f3cc2c262f846ce6a91419cd85479ffb4fc5e
```

SHA256 `lib/arm64-v8a/libarisa32.so`:

```text
5d74ef6032772417a0f75ae2d58da2ab4410a1bde52eda817b1b0a4d46b3ab04
```

Probe dijalankan pada serial `10DF1N05QN000H8` dengan:

```powershell
python tests/android/run_message_face_probe.py --serial 10DF1N05QN000H8
```

Probe mengekstrak library dari APK, mengompilasi test dengan NDK, lalu menjalankannya dalam direktori tersendiri di `/data/local/tmp`. Pemeriksaan ini tidak memasang APK atau mengubah save aplikasi pengguna. Pesan tentang `win_size.mes` dan voice kosong berasal dari fixture minimal. Uji tetap memeriksa hasil render dan mengembalikan exit code 0.

Batas validasi: UI aplikasi Android belum diuji melalui sesi bermain penuh pada APK ini. Pengujian perangkat memakai library arm64; armv7 berhasil dibangun. Native save diuji melalui import/export engine, belum dimuat kembali lewat executable PC asli.

## Delivery gate antislop

Cakupan gate adalah perubahan potret textbox serta laporan ini. Arah visual berasal dari screenshot PC pengguna dan aset game. Ini restorasi renderer pada desain yang sudah ada. Potret statis mengikuti komposisi native: ENERGY 2, RHYTHM 1, MOTION 1 untuk potret. Efek lingkungan tetap ditangani renderer yang sudah ada.

| Gate | Status dan bukti |
| --- | --- |
| R-02, copy | PASS: laporan memakai kalimat faktual; tidak memakai em dash atau slogan |
| R-03, ruang gambar | PASS: crop Kasumi berada di `(48,515)` sampai batas frame 720; screenshot DC4 dan DC4PH diperiksa |
| R-17, R-18, R-36, R-38, bukti | PASS: angka pengujian berasal dari log yang ditautkan; keterbatasan pengujian dinyatakan |
| R-23, aset | PASS: potret berasal dari FACE milik game, tanpa aset visual baru |
| R-24, R-26, R-28, interaksi | PASS dalam cakupan: tidak menambah navigasi, tombol, form, atau FAQ |
| R-25, R-32, R-34, aksesibilitas | PASS dalam cakupan: perubahan berupa gambar potret; warna teks, input keyboard, dan pilihan tema tidak diubah. Ini bukan audit aksesibilitas seluruh aplikasi |
| R-27, state | PASS: tes memeriksa textbox tersembunyi, potret kosong/hilang, pergantian dialog, dan clear |
| R-33, implementasi | PASS: perilaku ada di source C++ dan digunakan oleh build desktop serta Android |
| R-35, eksekusi | PASS: build desktop/APK, 26 CTest, uji data terpasang, dan probe library APK di Android lulus |
| R-37, arah desain | PASS: posisi, proporsi, dan urutan gambar berasal dari mesin asli serta screenshot pengguna |
| Purpose gate R-01, R-04, R-06..R-10, R-12..R-14, R-19, R-22 | PASS: tidak menambahkan teknik dekoratif; potret memakai gambar dan penempatan native untuk menampilkan pembicara |
| Liveliness | PASS dalam cakupan restorasi: potret memakai identitas karakter game; dialog tetap menjadi fokus, dengan ruang potret khusus di kiri |
| C-1, C-3, R-05, R-11, R-15, R-16, R-20, R-21, R-29..R-31 | PASS: tidak membuat komponen atau tata letak baru; keputusan visual terbatas pada pemulihan artwork dan koordinat asli |
| C-2, C-4, C-5 | PASS dalam cakupan: state potret diuji; hasil serta batas validasi dicatat di atas |
