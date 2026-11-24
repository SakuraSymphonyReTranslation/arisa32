# Pembersihan penyimpanan proyek

Pemeriksaan 9 November 2026 menemukan bahwa folder `build/` memakai sekitar 21,47 GiB dari ukuran logis proyek 25,57 GiB. Ada 98 subfolder langsung di dalamnya, sebagian besar keluaran pengujian dan analisis. Rilis aplikasi saat pemeriksaan adalah 0.7.1.

Dokumen ini adalah daftar tinjauan untuk pembersihan manual. Dokumen ini tidak menjalankan penghapusan. Perintah penghapusan otomatis sebelumnya ditolak alat dengan pesan `blocked by policy`.

## Pembersihan pertama: sekitar 9,36 GiB

Tutup proses build dan aplikasi yang berjalan dari folder proyek. Di File Explorer, buka `E:\Games\Adv32roid` dan tinjau target berikut. Gunakan Recycle Bin terlebih dahulu bila tersedia. Ruang drive baru benar-benar bebas setelah isi Recycle Bin tersebut dibuang.

| Target relatif terhadap folder proyek | Ukuran | Dasar pemeriksaan |
| --- | ---: | --- |
| Berkas `*.apk` hasil build | 2,28 GiB | 146 APK; tidak ada yang dilacak Git. Pengguna mengonfirmasi rilis sudah tersedia di GitHub. Termasuk APK pengujian yang dapat dibangun ulang. |
| `build/dc2dm-pretendard-layout-20261108/frames` | 4,42 GiB | Hanya frame PNG hasil ekstraksi; video sumber masih ada dan SHA-256 cocok dengan laporan ekstraksi. |
| `build/dc2pc-cgmode-20261108/video/pcver/frames` | 0,91 GiB | Hanya frame PNG hasil ekstraksi; video sumber dan hash sudah diverifikasi. |
| `build/dc2pc-cgmode-20261108/video/arisa32/frames` | 0,74 GiB | Hanya frame PNG hasil ekstraksi; video sumber dan hash sudah diverifikasi. |
| `android/app/build` | 0,54 GiB | Keluaran Gradle yang dapat dibangun ulang. |
| `android/app/.cxx` | 0,45 GiB | Cache build native Android yang dapat dibuat ulang. |
| `android/.gradle`, `android/build`, `build_vs` | 0,04 GiB | Cache dan keluaran build; audit tidak menemukan save atau sumber unik di dalam target ini. |

Untuk APK, cari `*.apk` dari folder proyek melalui kotak pencarian File Explorer. Tinjau hasil sebelum menghapusnya. Untuk direktori pada tabel, pilih hanya direktori yang disebutkan, bukan seluruh direktori induknya. Folder metadata dan skrip ekstraksi di luar direktori `frames` dapat tetap disimpan.

Total 9,36 GiB sudah menghindari penghitungan ganda APK yang berada di direktori cache/build. Ukuran masing-masing baris tidak boleh langsung dijumlahkan.

## Pembersihan kedua: gambar hasil analisis yang selesai

Audit menemukan 3.355 PPM/BMP di `build/` dan `scratch/`, sebesar 6,46 GiB secara logis atau sekitar 4,51 GiB ruang fisik saat ini. Tidak ada yang dilacak Git, menjadi input wajib bagi build/tes yang dilacak Git, berada di direktori save, atau bernama `*.preview.bmp`. Berkas tersebut adalah keluaran capture dan bukti analisis; skrip analisis lokal perlu menghasilkan capture ulang apabila kelak dijalankan kembali.

Untuk pembersihan manual, tinjau pencarian `*.ppm` lalu `*.bmp` secara terpisah di `build/` dan `scratch/`. Cocokkan dengan daftar hasil audit, terutama jika ada pengujian baru setelah tanggal pemeriksaan. Jangan menghapus BMP di seluruh proyek secara umum: berkas `*.preview.bmp` dapat menjadi pendamping save pengguna.

Daftar lengkap kedua tahap tersedia di [manual-cleanup-files.txt](C:/Users/adamb/AppData/Local/Temp/adv32roid-cleanup-20261109-080411/manual-cleanup-files.txt). Daftar ini tidak melakukan penghapusan. Ada 14.101 target berkas unik, total 15,81 GiB logis. Penghematan ruang fisik saat ini diperkirakan sekitar **13,87 GiB** karena sebagian gambar telah terkompresi. Jangan menghapus seluruh folder induk hanya karena satu berkasnya tercantum.

Kompresi yang sempat dijalankan telah dihentikan. Sebanyak 1.625 gambar terkompresi telah diverifikasi memiliki SHA-256 yang sama dengan sebelumnya dan menghemat sekitar 1,95 GiB ruang fisik. Jumlah berkas dan ukuran logis tetap; belum ada file proyek yang dihapus oleh agen.

## Yang harus dilindungi

- Kode, aset, `third_party/`, serta `third_party/ffmpeg/android/` yang diperlukan build.
- Save dan berkas pendampingnya, termasuk kemungkinan lokasi fallback `build/desktop/saves/`.
- Keystore, `android/local.properties`, konfigurasi lokal, dan backup yang belum dinyatakan tidak diperlukan.
- Perubahan lokal `android/build.gradle` serta folder belum terlacak `save-screen-test-data/`.
- Skrip atau data penelitian yang hanya tersimpan di `build/`, sampai sudah dipisahkan dari keluaran sekali pakai.

Jangan menghapus seluruh `build/` sekaligus. Aturan keluaran sementara di `AGENTS.md` menetapkan agar pengujian berikutnya memakai direktori sementara dengan masa hidup terbatas dan hanya menyimpan hasil yang diperlukan.

## Pembersihan lanjutan DC4 dan DC4 Plus Harmony, 9 November 2026

Daftar gabungan berisi 5.962 file, 2.723.171.594 byte atau 2,54 GiB ukuran logis. Perkiraan ruang drive yang dapat dibebaskan adalah 1,99 GiB; 928 kandidat sudah memakai kompresi NTFS. Targetnya adalah keluaran berlebih dari pemeriksaan fitur yang sudah selesai: frame video, capture antara, gambar hasil decode, kelas Java/DEX, binary probe dan library hasil ekstraksi dari run yang lulus, serta satu salinan dependency analisis FontTools. Pemeriksaan memakai laporan validasi fitur dan rilis; ini tidak menyatakan seluruh rute kedua game sudah dimainkan.

COMU adalah fitur pesan DC4/DC4PH. Sakuramusubi di sini adalah versi patch DC4PH, sehingga hasil pengujiannya termasuk cakupan yang sama. `apaini-analysis` memeriksa gerakan sprite kedua game, dan `nino-validation` memeriksa perbaikan sprite/cut-in yang sudah lulus. Koreksi cakupan ini menambahkan 2.391 file, 1.284.968.675 byte atau 1,20 GiB logis, ke daftar DC4 sebelumnya.

| Kelompok | File | Ukuran |
| --- | ---: | ---: |
| Sepuluh direktori `dc4-*` / `dc4ph-*` | 1.714 | 819,42 MiB |
| Tujuh direktori terkait potret, galeri, musik, menu, FXAA dan gerakan sprite | 1.857 | 552,16 MiB |
| `apaini-analysis` | 11 | 3,21 MiB |
| `comu-custom-font` | 757 | 106,85 MiB |
| `comu-pretendard-20261108` | 946 | 664,42 MiB |
| `comu-sakuramusubi` | 30 | 45,20 MiB |
| `comu-video` | 23 | 17,72 MiB |
| `fullscreen-comu-gear` | 551 | 198,58 MiB |
| `nino-validation` | 59 | 165,73 MiB |
| `sakuramusubi-check` | 14 | 23,72 MiB |

Log pembersihan sebelumnya mencatat 616 library, 814.652.920 byte, sudah terhapus. Daftar gabungan ini tidak beririsan dengan daftar tersebut. Urutan frame di `apaini-analysis/frames`, `comu-video/pc`, `comu-video/arisa`, dan `comu-sakuramusubi/frames` sudah kosong saat diperiksa dan tidak dihitung sebagai penghematan baru.

Semua 5.962 kandidat masih ada saat audit, cocok dengan ukuran dan waktu modifikasi inventaris, tidak dilacak Git, dan berada di 25 direktori `build` yang diperiksa tanpa symlink/junction. Hash SHA256 setiap file dicatat agar skrip menolak hasil build atau capture yang berubah setelah audit. Fingerprint seluruh 3.571 kandidat lama tetap identik dengan audit pertama; daftar baru tidak mengizinkan ulang file lama yang berubah. Skrip juga memeriksa status tracked Git saat dijalankan.

Tidak ada target folder, save, preview save, keystore, library `third_party`, kode proyek, atau aset game. Run gagal/baseline, backup, skrip, laporan, dan gambar bukti terpilih dipertahankan. Termasuk EXE/DLL patch Sakuramusubi, corpus MES, font, baseline `comu-video/android/recording-messages.ppm`, dua baseline decoder CP932, serta executable standalone custom-font yang pernah gagal tanpa decoder Java.

Sebanyak 717 kandidat adalah salinan FontTools 4.68.0 dari pip di `build/comu-custom-font/python-deps`, termasuk sumber vendor dan cache. RECORD paket diverifikasi; tidak ditemukan file tambahan atau perubahan buatan pengguna. Paket ini dipakai untuk analisis font yang sudah selesai, bukan dependency build aplikasi. Metadata versi, RECORD dan lisensi tetap ada. Bila analisis tersebut diperlukan lagi, paket dapat dipasang ulang dari akar proyek dengan `python -m pip install --upgrade --target build/comu-custom-font/python-deps fonttools==4.68.0`.

Rincian path dan alasan tersedia dalam [CSV gabungan](C:/Users/adamb/AppData/Local/Temp/adv32roid-comu-cleanup-20261109-094131-1burct8b/dc4-analysis-cleanup-reviewed.csv). [Laporan audit gabungan](C:/Users/adamb/AppData/Local/Temp/adv32roid-comu-cleanup-20261109-094131-1burct8b/dc4-analysis-cleanup-review.json) merangkum kelompok dan pemeriksaan. Laporan tambahan beserta daftar kandidat/dipertahankan tersedia di folder audit yang sama: `comu-fonts-*`, `comu-media-*`, `nino-*`, dan `sakuramusubi-*`.

Audit sebelumnya tetap tersedia tanpa perubahan: [direktori utama](C:/Users/adamb/AppData/Local/Temp/adv32roid-dc4-cleanup-20261109-092655-3szbsdel/dc4-named-audit.json), [file yang dipertahankan](C:/Users/adamb/AppData/Local/Temp/adv32roid-dc4-cleanup-20261109-092655-3szbsdel/dc4-named-retained.csv), dan [bukti pilihan direktori terkait](C:/Users/adamb/AppData/Local/Temp/adv32roid-dc4-cleanup-20261109-092655-3szbsdel/dc4-related-retained-evidence.csv).

[Skrip DC4](E:/Games/Adv32roid/tools/cleanup_dc4_analysis.py) memakai pemeriksaan yang sama dengan skrip library. Jalankan ketika build dan probe berhenti. CSV beserta `dc4-analysis-cleanup-fingerprints.json` harus tetap berada di folder audit yang sama; keduanya diperiksa terhadap hash yang dikunci dalam skrip.

```powershell
python -B "E:\Games\Adv32roid\tools\cleanup_dc4_analysis.py" --dry-run
python -B "E:\Games\Adv32roid\tools\cleanup_dc4_analysis.py" --delete
```

Mode pertama hanya simulasi. `--delete` menghapus permanen file yang lolos validasi dan menulis log JSONL di folder audit. Penghapusan tidak dijalankan oleh agen pada tahap ini. Pemeriksaan ulang versi historis kelak mungkin perlu membangun probe atau menyiapkan APK yang sesuai lagi.

Verifikasi skrip: 20 pengujian lulus, termasuk file berubah, symlink/junction, preview save, kandidat yang kini dilacak Git, file kosong, dan penghentian ketika gagal. Penghapusan dalam pengujian dimock; fixture berada di Temp. Simulasi daftar gabungan lulus untuk seluruh 5.962 file (2.597,02 MiB), tanpa penghapusan. Simulasi daftar library lama mengonfirmasi 616 file sudah tidak ada dan tidak ada kandidat lama tersisa.

`scratch` dikecualikan: audit agen untuk bagian tersebut terhenti karena filter konten memblokir respons tanpa alasan rinci. Audit itu tidak dicoba ulang melalui alat atau agen lain.
