**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.6.9 - Animasi Sprite, Progres D.C., dan Galeri CG

Versi 0.6.9 memperbaiki animasi sprite dan syarat membuka D.C. pada D.C.4 serta D.C.4 Plus Harmony. Rilis ini juga memulihkan portrait dialog, memperbaiki pratinjau CG, dan menambahkan mode kursor Android.

### Animasi sprite DC4 dan DC4 Plus Harmony

- Gerakan memakai tujuan, arah, durasi, dan urutan perintah skrip asli. Gerakan bergelombang, jeda, pergantian sprite, serta kemunculan dan penghilangan sprite mengikuti waktu adegan dan kecepatan teks.
- Perbaikan berlaku pada sistem sprite bersama yang digunakan karakter lain, termasuk Nino dan Sorane.
- Save baru menyimpan gerakan yang sedang berjalan, antrean gerakan, serta kemunculan dan penghilangan yang belum selesai. Save lama tetap dapat dibaca, tetapi tidak memiliki seluruh informasi animasi yang baru mulai disimpan pada versi ini.

### Syarat membuka segmen D.C.

- Pada kedua game, menu D.C. memerlukan ending Nino, Sorane, Hiyori, Shiina, Miu, Chiyoko, Arisa, dan Arisu, bersama penanda pembukaan asli game.
- Profil dengan penanda D.C. yang telanjur aktif tetap terkunci jika salah satu dari delapan ending belum selesai. Menamatkan Nino dan Hiyori saja, sementara Arisu masih berjalan, tidak membuka D.C.
- Pemeriksaan berlaku saat membaca profil, mengimpor progres PC, memuat save lama, dan mengonfirmasi masuk ke D.C. Posisi cerita dan pilihan lokal tetap mengikuti slot yang dimuat.
- Profil lama yang sudah salah menandai seluruh delapan ending tidak direset otomatis karena penandanya tidak dapat dibedakan dari progres tamat asli.

### Portrait dialog

- Memulihkan portrait di kotak dialog DC4/DC4PH, termasuk Kasumi, dengan potongan gambar, posisi, dan aksesori dari aset FACE asli.
- Portrait mengikuti pergantian dialog dan perintah background, berada pada lapisan kotak dialog, serta mendukung penyimpanan keadaan pada save baru. Pemulihan dari save lama terbatas pada informasi yang masih tersedia.

### Galeri CG

- Pratinjau menampilkan gambar utuh tanpa memotongnya; gambar portrait memiliki ruang kosong di sisi kiri dan kanan.
- Android merender pratinjau dari gambar sumber ke kanvas sesuai ukuran layar, sehingga detail tidak lebih dahulu hilang akibat penurunan ke 720p. Pengaturan Fit to Screen dan Stretch tetap berlaku.
- Urutan gambar mengikuti skrip galeri, melewati entri terkunci, kembali ke awal setelah gambar terakhir, dan memakai mask transisi asli. Ketukan saat transisi menyelesaikannya sebelum berpindah gambar.
- Tombol Kembali ke galeri dan tombol Back Android kembali ke daftar CG. Kontrol pratinjau mengikuti sembunyi otomatis setelah empat detik dan dapat digunakan dengan keyboard.

### Mode kursor Android

- Mode kursor tersedia per game di pengaturan dan menu dalam game, dengan keadaan awal Off. Geser untuk menggerakkan kursor, ketuk tanpa menggeser untuk klik, atau tahan lalu geser untuk menyeret.
- Kursor tetap di dalam kanvas game, mengikuti Fit/Stretch, dan tersembunyi setelah empat detik tanpa aktivitas. Dialog dan kontrol Android tetap menerima sentuhan langsung.
- Pengaturan tersedia dalam Bahasa Indonesia, English, dan bahasa Jepang.

### Folder game dan daftar musik

- Pemeriksaan folder dasar DC4/DC4PH mengenali NAMES.DAT yang hilang atau tidak valid. Pemindaian melewati folder tidak lengkap; pendaftaran mencegah folder patch parsial menggantikan data dasar lengkap, dan pesan peluncuran menjelaskan berkas yang diperlukan. Folder overlay terjemahan tetap boleh berisi sebagian data.
- Baris musik terkunci menampilkan `??????` agar judulnya tidak tampak kosong. Lagu tetap terkunci sampai syarat game terpenuhi.

### Pengujian dan batasan

Pengujian fitur mencakup regresi sprite, efek, portrait, menu, progres, dan pemulihan save, serta skrip dan aset kedua game. Pemeriksaan perangkat sebelumnya mencakup 209 pemeriksaan mode kursor, 810 pemeriksaan galeri CG, dan 64 pemeriksaan folder game pada Android 16/API 36. Rincian pengujian paket akhir tercatat dalam [laporan verifikasi 0.6.9](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.9/docs/release-qa-0.6.9.md).

Perbandingan animasi langsung dengan rekaman mencakup Nino dan Sorane; ini bukan verifikasi seluruh animasi atau playthrough semua rute. Sebagian fade sprite dengan mask per piksel masih berupa pendekatan. Kesamaan seluruh waktu transisi galeri dan teks penanda musik dengan PC belum dipastikan. Dukungan DC2 Dearest Marriage tetap beta. Berkas game, patch terjemahan, dan font kustom disediakan terpisah.

Seluruh perbaikan 0.6.8 tetap disertakan, termasuk layar penuh, pembersihan COMU, dan transisi gear. Lihat [catatan 0.6.8](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.8/docs/releases/v0.6.8/id.md).

### Unduhan dan pemasangan

[Buka halaman unduhan v0.6.9](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.9)

- `Arisa32-v0.6.9.apk`: paket rilis untuk penggunaan sehari-hari.
- `Arisa32-v0.6.9-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

Pasang pembaruan tanpa menghapus aplikasi agar data tetap tersimpan. Pembaruan dari dalam aplikasi tersedia sejak 0.6.4; Android tetap meminta konfirmasi pemasangan. Android 5.0+; arm64-v8a dan armeabi-v7a.
