# Scenario Mode dan kartu tanggal Dal Segno

Dua rekaman dari folder Downloads memperlihatkan interaksi galeri yang sama dengan progres dan pilihan replay yang berbeda. Seluruh 681 frame PC dan 1.231 frame Arisa32 didekode berurutan. Nomor frame dimulai dari nol. Kolom waktu memakai timestamp video masing-masing. Selisih antargambar diukur pada area game yang dinormalisasi ke 320 × 180 piksel. CSV mencatat selisih warna rata-rata terhadap frame sebelumnya pada seluruh layar, area judul dan area navigasi, serta fraksi layar hitam. [Pengukuran per frame](dalsegno-scenario-frames.csv) · [Perbandingan visual](dalsegno-scenario-replay.jpg)

| Rekaman dan frame | Waktu | Tampilan |
| --- | --- | --- |
| PC 65, 120, 160 | 2,17 / 4,00 / 5,33 dtk | “Some Odd Classmates”, lalu judul kosong saat kursor di luar thumbnail, lalu “Invitation to the Student Council Room”. |
| PC 300 dan 390 | 10,00 / 13,00 dtk | Konfirmasi Proceed, kemudian replay di kamar. |
| Arisa32 150 | 5,13 dtk | Beberapa judul dan penanda format `@s` tercetak pada posisi yang sama. |
| Arisa32 450 | 14,49 dtk | Thumbnail galeri masih tergambar di atas latar hitam saat replay dimulai. |
| Arisa32 480–570 | 15,79–18,73 dtk | Tombol navigasi galeri masih terlihat di pantai. |
| Arisa32 810 | 27,24 dtk | Pemain sudah kembali ke galeri. Suara ombak masih berulang. |
| Arisa32 1050–1230 | 35,21–40,95 dtk | Peta 2 Desember masih menampilkan navigasi galeri dan suara ombak dari replay sebelumnya. |

## Judul dan input replay

Skrip galeri menempatkan judul thumbnail di posisi (820, 64). Perintah `0x69` memasangkan judul dengan tombol sebelumnya dan menentukan keadaan hover atau ditekan. Renderer kini mengikuti pasangan itu serta membaca `@s` sebagai penanda format. Mouse, stylus yang mendukung hover, kursor virtual, dan jari yang sedang menekan memakai posisi pada viewport game.

Saat Proceed dikonfirmasi, Arisa32 melepas thumbnail, teks dan tombol navigasi. Saat skrip Dal Segno berganti, area sentuh menu lama ikut dilepas. Sebelumnya area sentuh yang tersisa membuat layar tanggal dianggap sebagai menu sehingga ketukan di peta diabaikan. Toolbar dialog dipasang kembali ketika dialog muncul.

## Semua kartu tanggal

Dari 310 file MES, 177 skrip menggunakan `SYSTEM\EYECATCHCHIP`. Ada 172 kartu bertanggal yang mencakup 62 tanggal berbeda, serta lima kartu tanpa angka tanggal. Enam latar `EYECATCHBACK01` sampai `06` dipakai masing-masing oleh 19, 37, 31, 31, 31, dan 28 skrip. Angka bulan dan hari dibaca dari variabel skrip, termasuk untuk file Januari yang namanya diawali `ds_13`. [Daftar lengkap skrip, tanggal dan latar](dalsegno-scenario-dates.csv)

Pada ke-177 skrip tersebut, blok kartu memuat lembar gambar pada offset 448. Sesuai pengaturan game, blok memilih tunggu berbatas waktu atau tunggu ketukan `0x13(0,1)` pada offset 2144. Perbaikan menyentuh kepemilikan tombol menu. Durasi yang ditulis di skrip tetap menentukan lama tampil kartu. [Tangkapan keenam latar di ponsel](dalsegno-scenario-route-dates.jpg)

| Latar | Contoh yang diuji lewat JNI di ponsel I2219 |
| --- | --- |
| 01, rute bersama | 30 November dan 1 Desember dari checkpoint cerita, serta 2 Desember lewat thumbnail ke-10 di Scenario Mode. |
| 02, HM | 19 Desember dan kartu tanpa tanggal di `ds_1411d_hm`. |
| 03, HD | 18 Januari. |
| 04, IO | 1 Januari. |
| 05, NE | 20 Desember. |
| 06, AM | 24 Desember. |

Dengan pengaturan tunggu input, satu ketukan melanjutkan masing-masing kartu uji ke kalimat pertama yang ditulis di skrip. Pada tujuh contoh checkpoint tambahan, ketukan diberikan tepat di bekas posisi panah halaman. Pada replay 2 Desember, ketukan di tengah peta membuka dialog kelas, dan End Scene mengembalikan pemain ke halaman galeri semula.

Pengujian cerita biasa di desktop juga menjalankan akhir `DS_1130D_CM`. Sesudah dialog terakhir pada 30 November, skrip berpindah sendiri ke `DS_1201A_CM`, menunggu input pada kartu 1 Desember, lalu melanjutkan ke “The sun's piercing rays were full of summer energy.” Posisi bekas panah galeri tidak memiliki target sentuh di kartu itu.

## Thumbnail gadis berambut putih

Di halaman pertama Others, sel ke-10 berjudul “The Barrier Sits on the Roof”. Skrip `sce_cm.mes` menampilkannya jika flag `98[228]` aktif dan membuka `DS_1202B_CM`. Profil PC yang diperiksa membuka tujuh thumbnail pertama. Profil ponsel membuka sepuluh. Slot ponsel `DSAO07.dat` menunjuk ke dialog cerita dalam `DS_1202B_CM` dengan flag replay nol. Riwayat ponsel memuat 145 posisi dialog berbeda untuk skrip itu. Membuka slot cerita yang lebih awal tetap memakai progres galeri global. Pengujian menggunakan salinan save.

## Ombak

Pencocokan gelombang audio mono pada 4 kHz dengan aset `SE/291.pcm` mengidentifikasi loop sepanjang 8,175 detik di saluran efek 2. Pada detik ke-17 dan ke-27 rekaman Android, korelasi ternormalisasinya 0,86 dan 0,76. Pada detik ke-35, 37 dan 39 nilainya 0,96. Posisi cuplikannya mengikuti panjang loop, termasuk ketika galeri dan peta sudah terbuka.

End Scene melompat langsung ke `SC_END.mes`, melewati perintah penghenti efek dalam cerita. Skrip menu itu menghentikan voice lalu memulai BGM. Arisa32 kini menghentikan efek berulang milik adegan saat keluar dari replay atau kembali ke judul. Saat load, mixer mengikuti daftar efek yang tersimpan di save tujuan. Pengujian mixer memakai salinan checkpoint dengan ombak dan salinan tanpa ombak.
