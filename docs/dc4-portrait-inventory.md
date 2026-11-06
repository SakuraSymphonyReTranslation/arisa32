# Analisis potret textbox dan variasi ekspresi DC4 / DC4PH

Pemeriksaan 6 November 2026. Cakupan mengikuti permintaan terakhir pada chat sebelumnya: menganalisis potret selain Kasumi dan mencari variasi yang tidak tampil. Source engine dan APK tidak diubah atau dipasang dalam pemeriksaan ini.

## Hasil utama

Skenario yang dipindai memakai potret untuk 22 nama karakter. Seluruh 488 referensi DC4 dan 517 referensi Plus Harmony ditemukan, berhasil didecode, dan tampil pada uji renderer dari working tree saat ini. Tidak ditemukan kebutuhan untuk menambahkan daftar dukungan per karakter atau per ekspresi. Jalur slot 31 memuat referensi FACE secara umum.

APK pada perangkat masih mempunyai hash yang sama persis dengan APK yang diperiksa dalam analisis video sebelumnya. APK tersebut berbeda dari APK perbaikan potret DC4. Karena itu, keberhasilan renderer desktop pada pemeriksaan ini belum membuktikan bahwa aplikasi yang terpasang sudah dapat menampilkan potret.

## Seluruh karakter

“Variasi” berarti pasangan arsip dan entry gambar unik setelah awalan path dinormalisasi. Angka ini mencakup ekspresi, pose, dan kostum, bukan jumlah emosi wajah. Sebanyak 488 referensi literal DC4 mewakili 484 pasangan gambar, sementara 517 referensi PH mewakili 513 pasangan. ASA dan ASU dikelompokkan sebagai Arisu sesuai nama pembicara; TIY mencakup Chiyoko dan CHOCO. Nama dicocokkan dengan overlay Inggris, bukan ditebak dari gambar.

| Karakter | Variasi DC4 | Variasi PH | Pemuatan DC4 | Pemuatan PH | Keluarga FACE yang dirujuk |
| --- | ---: | ---: | ---: | ---: | --- |
| Alice | 4 | 4 | 6 | 6 | ALI_A |
| Arisa | 6 | 6 | 8 | 8 | ARK_A, ARK_D |
| Arisu | 53 | 62 | 136 | 156 | ASA_A, ASA_B, ASA_C, ASU_A, ASU_C, ASU_F |
| Azusa | 2 | 2 | 2 | 2 | KNB_A |
| Chiyoko / CHOCO | 56 | 60 | 98 | 102 | TIY_A, TIY_B, TIY_F |
| Hiyori | 81 | 83 | 182 | 181 | HIY_A, HIY_B, HIY_C, HIY_D |
| Izumi | 5 | 5 | 7 | 7 | IZU_B |
| Kanata | 19 | 19 | 79 | 79 | KAN_A, KAN_B |
| Kasumi | 4 | 4 | 23 | 23 | KSS_A |
| Lala | 1 | 1 | 1 | 1 | HK1_A |
| Lili | 1 | 1 | 1 | 1 | HK2_A |
| Lulu | 1 | 1 | 1 | 1 | HK3_A |
| Masayoshi | 1 | 1 | 1 | 1 | ARF_A |
| Miu | 30 | 33 | 43 | 45 | MIU_A, MIU_B |
| Naozumi | 4 | 4 | 84 | 84 | KSB_A |
| Nino | 96 | 98 | 225 | 225 | NIN_A, NIN_B, NIN_C, NIN_D, NIN_E |
| Reiji | 4 | 4 | 6 | 6 | NIF_A |
| Saki | 3 | 3 | 10 | 10 | UKT_A |
| Shiina | 37 | 43 | 74 | 79 | SII_A, SII_B, SII_C, SII_D |
| Sorane | 55 | 58 | 127 | 137 | SOR_A, SOR_C, SOR_F, SOR_G, SOR_H |
| Suginami | 18 | 18 | 110 | 110 | SGN_A, SGN_B |
| Towako | 3 | 3 | 6 | 6 | NIM_A |
| **Total** | **484** | **513** | **1.230** | **1.270** | **46 keluarga DC4; 47 PH** |

Jumlah pemuatan adalah hitungan instruksi statis pada semua MES, termasuk cabang yang saling eksklusif. Itu bukan jumlah dialog yang akan terlihat dalam satu playthrough.

Empat alias terdapat pada kedua edisi: `FACE\BST\NIN_A1_AB05`, `FACE\BST\NIN_A1_AB12`, `FACE\BST\NIN_A2_AB10`, dan `FACE\FACET\KAN_B1_A04`. Masing-masing menunjuk arsip dan entry yang sama dengan referensi tanpa awalan BST/FACET. Seluruh bentuk alias ikut diuji dan tampil. Karena itu Nino mempunyai 96/98 variasi dari 99/101 referensi literal; Kanata mempunyai 19 variasi dari 20 referensi literal.

![Contoh aset potret untuk 22 nama karakter](E:/Games/Adv32roid/build/portrait-audit/all-characters.jpg)

Gambar di atas dan lembar ekspresi di bawah merupakan susunan hasil ekstraksi aset asli. Keduanya bukan screenshot gameplay atau artwork baru.

## Ekspresi dan aturan kemunculan

Skrip memuat `3C`, slot `31`, dan nama resource lengkap seperti `FACE\NIN_A1_AB10,FACE\NIN_A`. Pola skenario memakai perintah commit `64 "."` sebelum dialog. Renderer mengganti gambar menurut referensi itu. Nama pembicara tidak memilih potret secara otomatis.

Potret utama dilepas ketika dialog dilanjutkan. Dialog berikutnya perlu memuat ulang potret, termasuk jika gambarnya sama. Perintah `64 "-"` membersihkan potret secara eksplisit. Slot 32 tersedia pada implementasi sebagai lapisan tambahan; tidak ada pemuatan slot 32 dalam MES yang dipindai.

Ada kasus yang menunjukkan mengapa nama pembicara tidak dapat dijadikan aturan: pada overlay Inggris `dc4_mib20190315c.mes`, offset 42627 memuat `HIY_B1_AM16`; dialog berikutnya pada offset 42713 masih memakai nama Ichito. Potret tersebut memang disebut skrip. Pemeriksaan ini tidak menganggap perbedaan nama itu sebagai aset yang salah.

Kasumi mempunyai empat referensi, A01 sampai A04. A04 dipakai pada `dc4_mid20190407b.mes`, offset 8591 di data Jepang, untuk dialog tentang membawa tabir surya. Jadi tiga ekspresi pada cuplikan video sebelumnya belum mencakup semua variasi Kasumi.

![Contoh variasi ekspresi dari aset asli](E:/Games/Adv32roid/build/portrait-audit/expression-examples.jpg)

## Contoh lokasi skenario

Offset berikut relatif terhadap code stream MES data Jepang DC4, bukan posisi absolut dalam berkas atau nomor dialog. Entri pertama dipakai sebagai titik pemeriksaan; daftar lengkap kejadian dan referensinya ada dalam inventaris JSON.

| Karakter | MES | Offset | Referensi contoh |
| --- | --- | ---: | --- |
| Alice | `dc4_kyo20190205a.mes` | 13318 | `FACE\ALI_A1_A01` |
| Arisa | `dc4_asa20190429b.mes` | 15410 | `FACE\ARK_A1_A06` |
| Arisu | `dc4_asa20190429b.mes` | 19968 | `FACE\ASU_A1_A16` |
| Azusa | `dc4_miu20190321b.mes` | 18208 | `FACE\KNB_A1_A02` |
| Chiyoko / CHOCO | `dc4_kyo20190208c1_m3.mes` | 8272 | `FACE\TIY_B1_A02` |
| Hiyori | `dc4_asa20200406a.mes` | 33132 | `FACE\HIY_B2_AM02` |
| Izumi | `dc4_asa20200406a.mes` | 32374 | `FACE\IZU_B1_A01` |
| Kanata | `dc4_asa20200406a.mes` | 33020 | `FACE\KAN_B1_A06` |
| Kasumi | `dc4_mid20190330b.mes` | 2521 | `FACE\KSS_A1_A01` |
| Lala | `dc4_mib20190304c.mes` | 14112 | `FACE\HK1_A1_A02` |
| Lili | `dc4_mib20190304c.mes` | 14222 | `FACE\HK2_A1_A02` |
| Lulu | `dc4_mib20190304c.mes` | 14332 | `FACE\HK3_A1_A02` |
| Masayoshi | `dc4_mid20190324b.mes` | 14366 | `FACE\ARF_A1_A05` |
| Miu | `dc4_hiy20190315d1.mes` | 5119 | `FACE\MIU_B2_AM03` |
| Naozumi | `dc4_asu20190430d.mes` | 2985 | `FACE\KSB_A1_A01` |
| Nino | `dc4_asa20200406a.mes` | 31189 | `FACE\NIN_C1_AB06` |
| Reiji | `dc4_mia20190303b.mes` | 13562 | `FACE\NIF_A1_A05` |
| Saki | `dc4_kyo20190210b.mes` | 6503 | `FACE\UKT_A1_A01` |
| Shiina | `dc4_kyo20190209b_m3.mes` | 11791 | `FACE\SII_A1_A09` |
| Sorane | `dc4_asu20190430c.mes` | 28131 | `FACE\SOR_C1_A06` |
| Suginami | `dc4_asa20190430b.mes` | 10379 | `FACE\SGN_B1_A03` |
| Towako | `dc4_mia20190306c.mes` | 16777 | `FACE\NIM_A1_A05` |

## Tambahan Plus Harmony dan aset yang tidak dirujuk

Plus Harmony memakai seluruh 488 referensi DC4, ditambah 29 referensi: Arisu +9, Shiina +6, Chiyoko/CHOCO +4, Miu +3, Sorane +3, Hiyori +2, dan Nino +2. Satu keluarga baru yang dirujuk adalah `ASU_F`, melalui `ASU_F3_AK04`. Semua tambahan ini ikut diuji.

Direktori FACE berisi lebih banyak gambar daripada yang dipanggil skenario: 3.358 entri DC4 dan 5.228 entri PH tidak disebut langsung oleh pemuatan slot 31/32 pada MES yang dipindai. PH memiliki 73 arsip FACE, sementara skrip memanggil 47 keluarga. Keberadaan file yang tidak dirujuk langsung tidak membuktikan ada fitur yang belum diimplementasikan; beberapa juga dapat menjadi basis gambar diferensial. Pemeriksaan ini tidak memaksa gambar tersebut muncul.

Pada slot tubuh 4 sampai 8, inventaris tidak menemukan referensi BST untuk Naozumi, Saki, Lala, Lili, dan Lulu. Karakter lain dalam tabel mempunyai referensi tubuh. Ini mendukung penggunaan potret untuk sebagian karakter tanpa sprite tubuh dalam skenario, tetapi potret juga banyak dipakai karakter utama. Temuan ini terbatas pada nama keluarga dan slot yang dipindai.

## Verifikasi renderer

| Pemeriksaan | DC4 | Plus Harmony |
| --- | ---: | ---: |
| Referensi dimuat dan didecode | 488/488 | 517/517 |
| Piksel opak yang dibandingkan | 13,134,486 | 14,301,721 |
| Piksel opak berbeda | 0 | 0 |
| Piksel nontransparan di luar layar | 0 | 0 |
| Potret dilepas pada dialog berikutnya | 488/488 | 517/517 |

Probe membentuk MES diagnostik yang memutar semua referensi secara berurutan, masing-masing diikuti dialog tanpa potret. Setiap gambar hasil pemuatan dibandingkan dengan decode langsung dari arsip FACE; piksel opaknya kemudian dibandingkan dengan framebuffer 1280×720 pada posisi native. Ini memeriksa pergantian referensi, pemilihan direktori FACE, perataan, hasil gambar, dan pelepasan potret.

Origin native adalah `(0,495)` dan perataan bawah memakai tinggi canvas dari metadata gambar, dengan rumus `y = 495 + 225 - canvas_height + crop_y`. DC4 mempunyai canvas 174, 185, 218, 220, dan 225 piksel dalam referensi ini; seluruh referensi PH memakai tinggi 225. Header arsip sendiri memakai canvas registrasi 2048, sehingga nilai metadata gambar perlu diprioritaskan.

Build target `arisa32_core` dan `dc4_message_face_test` selesai. Uji lifecycle yang sudah ada juga lulus untuk kedua format MES, termasuk textbox tersembunyi, lapisan tambahan, explicit clear, aset hilang, serta save/load. Tidak ada perubahan source engine dalam pekerjaan ini.

Batas hasil: semua referensi diuji dalam MES diagnostik, bukan seluruh cabang cerita dimainkan ulang. Piksel semitransparan tidak dibandingkan secara numerik pada framebuffer; isi RGBA hasil pemuatan tetap dibandingkan penuh dengan hasil decoder. Kesesuaian artwork decoder terhadap executable PC belum diuji untuk setiap variasi. Pemeriksaan perangkat kali ini hanya membaca identitas APK, bukan menguji gameplay Android.

## APK pada perangkat

Perangkat terhubung melaporkan `org.dc4engine`, versi 0.6.8, versionCode 15. SHA256 base.apk: `ce4c5ae613653946b6401b8f877f95ce0608897dd7404233c9ad608b7b3594bd`.

Hash itu sama dengan salinan APK yang dianalisis sebelumnya. Pemeriksaan simbol pada salinan tersebut tidak menemukan `dc4_message_faces`, `dispatch_dc4_message_face`, atau `draw_dc4_message_face`, sementara jalur DC2DM ada. APK perbaikan sebelumnya memiliki SHA256 `e8ce56f5c98f74bafb56d808bb6f3cc2c262f846ce6a91419cd85479ffb4fc5e` dan mengekspor ketiga simbol DC4 tersebut.

Dengan demikian, APK terpasang belum sama dengan hasil perbaikan yang diuji sebelumnya. Ini menjelaskan mengapa masalah dapat mengenai semua karakter pengguna potret, bukan hanya Kasumi. Langkah implementasi berikutnya adalah menggunakan build yang memuat jalur DC4 lalu memvalidasi adegan nyata di aplikasi, bukan menambah daftar karakter satu per satu.

## Bukti dan reproduksi

- [Inventaris semua referensi, lokasi skrip, dan entri arsip](E:/Games/Adv32roid/build/portrait-audit/inventory.json)
- [Hasil render DC4 per referensi](E:/Games/Adv32roid/build/portrait-audit/dc4/render.tsv)
- [Hasil render Plus Harmony per referensi](E:/Games/Adv32roid/build/portrait-audit/dc4ph/render.tsv)
- [Ringkasan hasil dan hash source/library/probe](E:/Games/Adv32roid/build/portrait-audit/results.json)
- [Identitas APK terpasang saat pemeriksaan ini](E:/Games/Adv32roid/build/portrait-audit/installed-apk.json)
- [Perbandingan APK dan simbol sebelumnya](E:/Games/Adv32roid/build/face-video-analysis/apk-comparison.json)
- [Log uji lifecycle](E:/Games/Adv32roid/build/portrait-audit/lifecycle-test.log)
- [Kode inventaris](E:/Games/Adv32roid/build/portrait-audit/inventory.py)
- [Kode probe renderer](E:/Games/Adv32roid/build/portrait-audit/render_probe.cpp)
- [Build probe](E:/Games/Adv32roid/build/portrait-audit/build_probe.bat)

Parser membaca seluruh code stream, termasuk setelah NOP: 511 MES DC4, 536 PH, 478 overlay Inggris, dan 511 overlay Indonesia. Tidak ada kegagalan parsing. Kedua overlay mempunyai himpunan 488 referensi yang sama dengan DC4. Overlay Inggris tidak berisi GRP/FACE sendiri; pengujian aset dilakukan pada data Jepang lengkap. Referensi Indonesia juga ditemukan di arsip FACE lokalnya.

## Delivery gate antislop

Cakupan gate adalah laporan dan lembar bukti ini. Arah visual berasal dari artwork game dan identifikasi resource: gambar asli, label nama serta ID, ruang netral untuk membandingkan ekspresi. ENERGY 2, RHYTHM 1, MOTION 1. Tidak ada rancangan UI aplikasi baru.

| Item | Hasil dan bukti |
| --- | --- |
| R-02, R-15, R-16 | PASS: kalimat laporan faktual, tanpa em dash, slogan, atau CTA promosi |
| R-03 | PASS dalam cakupan: lembar bukti diperiksa sebagai gambar; label terbaca dan gambar tidak keluar sel. Renderer melaporkan nol piksel nontransparan di luar frame |
| R-17, R-18, R-36, R-38, C-5 | PASS: tabel berasal dari JSON/TSV dan hash perangkat; batas verifikasi disebutkan |
| R-23 | PASS: gambar merupakan ekstraksi aset yang diminta untuk dianalisis; tidak membuat karakter, avatar, logo, atau artwork baru |
| R-24, R-26, R-28, R-32, R-34, C-2 | PASS dalam cakupan: tidak menambahkan kontrol, navigasi, FAQ, tema, atau interaksi aplikasi |
| R-25 | PASS dalam cakupan: label gelap pada latar netral terang; warna teks game tidak diubah |
| R-27, C-4 | PASS dalam cakupan: pelepasan potret diuji pada 1.005 pemuatan; uji lifecycle yang ada lulus. Bukan audit seluruh state aplikasi |
| R-33, R-35 | PASS: probe C++ tersimpan, dikompilasi, dan dijalankan; hasil final 488/488 dan 517/517 lulus. Tidak mengirim build aplikasi baru |
| R-37 | PASS: lembar bukti mengikuti artwork asli, dengan label untuk penelusuran ke skrip; tidak mengubah desain game |
| R-01, R-04, R-06 sampai R-10, R-12 sampai R-14, R-19, R-22 | PASS dalam cakupan: susunan berulang dipakai untuk perbandingan aset; tanpa dekorasi, ikon generik, glow, atau animasi |
| Liveliness | PASS dalam cakupan: identitas visual berasal dari karakter asli; dials 2/1/1 sesuai bukti statis; fokus tiap sel adalah ekspresi dan ID referensinya |
| C-1, C-3, R-05, R-11, R-20, R-21, R-29 sampai R-31 | PASS dalam cakupan: warna dan bentuk artwork dipertahankan; komposisi laporan mengikuti inventaris dan bukti, tanpa komponen produk baru |

Gate ini tidak menyatakan bahwa seluruh UI Android, semua rute cerita, atau semua perilaku mesin PC sudah terverifikasi.
