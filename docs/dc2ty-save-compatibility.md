# Save DC2TY: judul skenario dan format PC

Perubahan mengikuti contoh Save PC dari pengguna, layar Save Arisa32 yang
diminta untuk diperiksa, serta struktur `DC2TY.exe` dan save asli. Rincian
offset dan bukti penomoran ada di [native save reference](dc2ty-native-save-reference.md).

## Nama dan isi file

| Fungsi | File |
| --- | --- |
| Quicksave | `dc2ty00.dat` + `dc2ty00.dat.arisa` |
| Salinan save manual terakhir | `dc2ty01.dat` + `dc2ty01.dat.arisa` |
| Tiga slot manual halaman 1 | `dc2ty02.dat` sampai `dc2ty04.dat`, masing-masing dengan `.arisa` |
| Lima slot manual halaman 2 | `dc2ty08.dat` sampai `dc2ty12.dat`, masing-masing dengan `.arisa` |
| Lima slot manual halaman 10 | `dc2ty72.dat` sampai `dc2ty76.dat`, masing-masing dengan `.arisa` |

Ada 48 slot manual. Nomor mengikuti stride delapan milik PC; angka yang
dilewati bukan slot hilang. Menyimpan manual memperbarui pasangan `01`;
quicksave hanya memperbarui `00`. Penulisan memakai kembali ejaan huruf
besar/kecil file yang sudah ada.

`.dat` memakai struktur native berukuran 10.564 byte dengan header DWORD 1.
Isinya mencakup marker dialog/pilihan, variabel lokal, aset dan geometri sprite,
sheet/chip adegan, BGM/loop SE, ikon, tanggal, dan judul skenario CP932.
Marker cerita menghindari ketergantungan pada offset byte yang berubah akibat
terjemahan; ekspor menghitung offset dari skrip sumber yang sedang dipakai.

`.dat.arisa` menyimpan snapshot Arisa32 yang terikat checksum `.dat`, termasuk
state VM, gerak sprite, kamera, sheet/chip, dan posisi putar BGM/SE berulang.
Jika `.dat` diganti oleh PC, companion lama diabaikan. Voice dialog menyimpan
nama resource untuk diputar ulang dari awal; offset voice yang sedang diputar
belum disimpan.

Chip overlay Arisa32 dipetakan ke plane 1 native, yang digambar setelah
sprite, maksimal 60 chip tanpa kondisi. Import PC dengan chip plane 0,
chip bersyarat, atau offset latar native nonzero ditolak secara eksplisit;
renderer saat ini belum memulihkan konfigurasi tersebut secara setara.
Ketiga save PC pengguna yang diuji tidak memakai konfigurasi itu.

Untuk pindah ke PC, salin `.dat` ke folder save PC. Untuk mempertahankan state
tambahan saat pindah antarinstalasi Arisa32, salin pasangan `.dat` dan
`.dat.arisa`. Backup ZIP Arisa32 menyertakan companion tersebut. Save lama
`.dc4sav` tetap dapat dimuat; penulisan berikutnya memakai nama native.
File save pengguna tidak dikonversi atau ditimpa selama pengujian.

## Metadata dan menu

Save/Load menampilkan judul dari 129 entri tabel executable, misalnya:

- `hyd_0611_a1_avn` → `始まりの始まり`
- `set_0214_f1_214` → `小恋とバレンタイン`
- `set_0613_b1_kko` → `梅雨のある晴れた日`

Nama tersebut juga ditulis ke footer native. Ikon pilihan disimpan ke
bank `9[20]` sebelum snapshot/export, sehingga slot, companion, dan latest-copy
memiliki ikon yang sama. Metadata ikon native mengungguli sidecar `.icon`.
Tanggal native memiliki presisi menit; pemilihan save terbaru memakai tanggal
tersebut, lalu waktu modifikasi file jika menitnya sama.

Halaman pertama memakai artwork Quick/New dan tiga slot manual. Halaman
berikutnya memakai `save_BG01`/`load_BG01` dengan lima slot manual. Susunan,
warna, font artwork, dan area input mengikuti game PC, tanpa aset visual baru.

## Batas pembuktian native

`tools/verify_dc2ty_native_save.py` menjalankan reader `0x426920` dan loader
`0x426860` dari executable asli melalui Unicorn. File disediakan di memori;
probe tidak membuka GUI game atau menulis ke folder save pengguna. Loader
dihentikan pada `0x426580` setelah memeriksa header/panjang baca dan menyalin
seluruh buffer. Ini menguji penerimaan struktur native dan penyalinan buffer;
pemulihan adegan penuh dalam game PC tidak diuji oleh probe ini.

## Verifikasi

| Pemeriksaan | Hasil |
| --- | --- |
| Profil/metadata Android | 619 pemeriksaan Java lulus, termasuk perbandingan 129 judul terhadap executable asli |
| Backup save Android | 319 pemeriksaan lulus, termasuk companion DC2TY/DC2DM |
| Host dan regresi desktop | `save_host_test`, `save_screen_test`, `scene_test`, `dc2pc_menu_test`, `dc2ty_eyecatch_test` lulus |
| Codec native dan companion | Lulus: fixture dialog/pilihan, file rusak, companion tidak cocok, save PC asli `00`/`01`/`02` melalui import/export/import, serta marker 85 dengan perbandingan dialog, sprite, dan framebuffer |
| Posisi audio companion | BGM dan SE berulang kembali ke posisi frame yang disimpan setelah pemutaran sengaja dilanjutkan; mode/id native juga diperiksa |
| Pembaca executable PC asli | Tujuh hasil ekspor dan tiga save PC asli diterima oleh kedua rutin; seluruh 10.564 byte tersalin persis. Tujuh kontrol kegagalan/batas juga lulus |
| Menu DC2TY dengan aset asli | Lulus: 48 slot, aksi Save/Load, pemilihan ikon, halaman 1/2/10, dan perbandingan piksel dengan artwork sumber |
| Inspeksi visual offline | Save/Load halaman 1/2/10 diperiksa; halaman pertama dengan metadata save asli menampilkan judul, tanggal, dan ikon yang sesuai |
| Title desktop baru dibuka | Continue membaca slot native meskipun belum ada skrip cerita aktif; renderer offline memperlihatkan tombol tersedia dengan fixture save native |
| Android | `assembleDebug` final lulus untuk arm64-v8a dan armeabi-v7a; APK berisi 129 judul skenario; verifikasi tanda tangan v1/v2 lulus |

Bukti tampilan: [halaman Save/Load](dc2ty-save-pages-evidence.png).

Pengujian memakai direktori sementara. Satu screenshot HP diambil saat
pengguna meminta pemeriksaan layar Save; bukti hasil perubahan dirender
offline. Keputusan visual mengikuti artwork PC (ENERGY 1 / RHYTHM 1 /
MOTION 1). Pemeriksaan interaksi dan state kosong/terisi di atas memenuhi
delivery gate antislop untuk perubahan Save/Load ini, tanpa mengklaim audit
aksesibilitas seluruh artwork atau verifikasi sentuhan pada HP.

## APK terpasang

Pembaruan dipasang melalui `adb install -r` ke I2219 pada
10 November 2026 pukul 23:40:41 (UTC+7). Package `org.dc4engine`, versi
0.7.1 / code 19. Pemasangan menghasilkan `Success`; SHA-256 `base.apk` di HP
sama dengan APK keluaran build. Pemeriksaan setelah pemasangan hanya memakai
metadata package dan hash file; aplikasi tidak dibuka dan layar tidak diambil.

- APK: `android/app/build/outputs/apk/debug/app-debug.apk`
- Ukuran: 18.577.991 byte
- SHA-256: `8e9f48ab554fc76d772d46d4cadc15743ca75757857f4d04a56f2c966f3188dd`
