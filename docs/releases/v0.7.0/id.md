**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.7.0 - Save PC, Minigame, dan Perbaikan DC2DM

Rilis 8 November 2026 ini memperluas dukungan D.C. II Dearest Marriage dengan save PC, minigame suit dan Reversi, serta perbaikan suara, pilihan cerita, galeri, dan transisi. Aturan khusus game ini hanya aktif pada profil DC2DM. Dukungan DC2DM tetap beta; perbaikan dari 0.6.9 tetap disertakan.

### Save PC dan progres cerita

- Slot DC2DM kini dibaca dan ditulis sebagai pasangan `.dat` dan `.dat.arisa`. File pendamping menyimpan riwayat lengkap dan keadaan tambahan Arisa32, termasuk posisi pemutaran audio. Save PC tanpa pendamping dan save portabel `.dc4sav` lama tetap dapat dimuat.
- Arisa32 mengimpor `system.dat` agar latar judul, segmen cerita, musik, CG, dan Scenario Mode mengikuti progres game. Progres yang telah selesai tetap ada setelah aplikasi dimulai ulang atau slot lama dimuat.
- Penomoran mengikuti PC: `DC2DM01.dat` untuk quicksave, `DC2DM02.dat` untuk autosave, `DC2DM03.dat` sebagai salinan save terakhir PC, dan `DC2DM04.dat` sampai `DC2DM103.dat` untuk 100 slot biasa. Autosave dibuat pada checkpoint skrip; replay galeri tidak menimpanya.
- Baris Save/Load menampilkan tanggal dari isi save dan judul skenario, termasuk 279 judul MAR/SIN/SUC. Judul dari executable terjemahan dapat menggantikan judul Jepang bawaan.

Simpan `.dat` dan `.dat.arisa` bersama untuk mempertahankan seluruh keadaan Arisa32. Progres global Arisa32 disimpan dalam `system-profile.dat`; ekspor balik ke `system.dat` belum tersedia. Penulisan slot tidak membuat ulang salinan `DC2DM03.dat`. Pemuatan slot hasil ekspor langsung dengan executable PC belum diverifikasi.

### Start dan urutan cerita

| Progres | Perilaku Start |
| --- | --- |
| Graduation Days belum selesai | Menampilkan konfirmasi sebelum memulai Graduation Days |
| Graduation Days selesai | Langsung membuka Graduation Days dan Married Life, tanpa konfirmasi tambahan |
| Graduation Days dan Married Life selesai | Langsung membuka tiga cerita, termasuk Dear to, tanpa konfirmasi tambahan |

Alur normal berlanjut dari Graduation Days ke Married Life, lalu Dear to dan credits. Pemain tidak perlu membuka setiap thumbnail CG atau memainkan semua cabang Scenario Mode untuk mencapai credits. Tulisan asli pada tombol cerita kedua adalah `Marrid life`. Konfirmasi tahap awal dipertahankan sesuai aturan yang diminta; skrip asli memastikan syarat pembukaan dua atau tiga cerita.

### Suara menu dan replay dialog

- Pengumuman Start, Load, Extra, Scenario Mode, CG Mode, Music Mode, Mini Game, CONFIG, dan Quit memakai aset suara yang sesuai dengan tiga gaya Otome A/B/C. Pilihan `Nothing` mematikan pengumuman.
- Perubahan gaya Otome memainkan sampel yang benar. CONFIG tidak lagi memutar suara semua tombol saat digambar ulang. Save dan Load yang dikonfirmasi memakai suara masing-masing; membuka Save tetap tanpa pengumuman, mengikuti rekaman referensi.
- Replay hanya mengacu pada suara yang benar-benar dimainkan untuk baris saat ini. Preload suara berikutnya tidak menjadikannya milik baris tersebut. Tombol replay tidak aktif pada narasi dan dialog Yoshiyuki tanpa suara, termasuk setelah memuat save.

### Pilihan cerita dan galeri CG

- Semua pilihan cerita DC2DM memakai gambar pilihan PC untuk keadaan normal, hover, dan ditekan. Posisi kelompok pilihan, ukuran teks, area klik, serta latar yang diredupkan mengikuti referensi game. Pembatalan sentuhan tidak memilih baris.
- Setiap pilihan tetap menuju label dari skrip asli. Pemeriksaan target mencakup seluruh kelompok pilihan yang ditemukan; pengujian jalur langsung mencakup kedua pilihan dalam rekaman Anzu dan titik pertemuannya.
- Pratinjau CG memakai gambar sumber dan interpolasi pada ukuran tampilan yang diminta. Gambar asli 1024 x 576 tetap menjadi batas detail; perubahan ini tidak membuat aset HD baru.
- Pergantian gambar mengikuti urutan dan varian terbuka dalam kategori yang sedang dipilih, melewati entri terkunci, dan berulang ke awal kategori. Kembali ke thumbnail membuka halaman gambar terakhir.
- Fade mengikuti pengaturan kecepatan efek dan Effects Off. Ketukan saat fade menyelesaikan transisi lebih dahulu agar tidak melewati gambar berikutnya.

### Suit dan Reversi

- Kedua minigame dapat dibuka melalui menu MINI GAME asli, memakai gambar kontrol, papan, portrait, dan prompt game.
- Suit menangani ketiga tangan, seri, ulang permainan, dan keluar. Reversi menyediakan Easy/Normal/Hard, pilihan giliran, langkah legal, pembalikan batu, pass otomatis, dan hasil menang/kalah/seri dari sudut pemain.
- Dialog, rekaman suara, dan tujuan kembali tetap mengikuti skrip Otome. Teks jumlah kemenangan dan kekalahan memakai total permainan yang sesuai; seri tidak menambah salah satu total.
- Keluar melalui alur minigame menyimpan hasil sesi. Kembali paksa ke judul lewat Arisa32 atau pintasan membatalkan hasil sesi aktif, mempertahankan total sesi terdahulu, dan memulihkan akses Load. Pause dan CONFIG tidak menyimpan hasil sesi yang masih berjalan.

### Transisi adegan

- Kembali dari cerita ke Title memakai atlas lingkaran asli di atas gambar adegan sebelumnya.
- Dissolve, fade masuk/keluar, feather reveal, dan mask adegan memakai perilaku DC2DM. Transisi kelas menutup adegan dengan putih lalu membuka kelas berikutnya melalui mask asli.
- Pause menghentikan waktu efek. Skrip menunggu transisi selesai, dan kembali paksa ke Title membersihkan mask serta penantian skrip agar Load tidak tertinggal terkunci.

### Pengujian dan batasan

Laporan fitur mencatat analisis rekaman, skrip dan aset asli, pengujian desktop, serta pengujian library JNI dari APK pada Android dengan save sementara terpisah. Lihat [save dan progres](../../dc2dm-save-progress-validation.md), [suara dan CG](../../dc2dm-menu-audio-transition-validation.md), [pilihan dan replay](../../dc2dm-choice-voice-validation.md), [minigame](../../dc2dm-minigame-validation.md), serta [Start dan transisi](../../dc2dm-start-scene-transition-validation.md). Verifikasi paket rilis dicatat terpisah dalam [laporan 0.7.0](../../release-qa-0.7.0.md).

- Pengujian bukan playthrough seluruh rute, perbandingan setiap rekaman suara, atau verifikasi semua perangkat dan patch. Raster font masih dapat berbeda dari PC.
- Perpindahan Title ke pilihan cerita masih melewati hitam singkat; PC memakai crossfade langsung.
- Sebagian efek native, termasuk partikel ubin, flash/guncangan, dan efek sprite tertentu, belum direproduksi sepenuhnya. Urutan scene yang lebih kompleks masih memerlukan pembandingan.
- Reversi mengikuti jalur keputusan yang telah dianalisis, tetapi tidak meniru seluruh urutan acak executable. Pilihan CPU yang bernilai setara dapat berbeda dari rekaman.
- Pengujian runtime perangkat yang tercatat memakai ARM64. ARMv7 tersedia dalam build, tetapi belum diuji pada perangkat 32-bit.

### Unduhan dan pemasangan

[Buka halaman unduhan v0.7.0](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.0)

- `Arisa32-v0.7.0.apk`: paket rilis.
- `Arisa32-v0.7.0-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Pasang pembaruan tanpa menghapus aplikasi agar data tetap tersimpan. Android 5.0+; arm64-v8a dan armeabi-v7a. Berkas game dan patch terjemahan disediakan terpisah.
