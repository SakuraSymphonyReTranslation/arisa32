# Waktu startup enam game di iQOO Z9x 5G

Pengukuran 28 November 2026 menggunakan data game dari penyimpanan bersama HP dan direktori save khusus pengujian. D.C. II Plus Communication memiliki 1.560 entri di folder MES.

Timer dimulai saat aksi **Jalankan** dipanggil dari koleksi yang sudah terbuka. Timer berhenti setelah renderer menggambar frame pertama dan memberi tahu UI. Untuk D.C. II To You, frame tersebut berasal dari video pembuka. Dal Segno menggunakan data Inggris resmi. Lima judul lainnya menggunakan data Jepang.

Setiap angka adalah median enam peluncuran per game untuk masing-masing build. Pada setiap pasangan, kedua build menjalankan urutan judul yang sama. Build yang dijalankan lebih dahulu bergantian, dan setiap judul menempati posisi pertama satu kali. Aplikasi uji memakai layar virtual 1920 × 1080 di HP fisik. Cache penyimpanan tetap aktif. Pengukuran ini tidak mencakup membuka Arisa32 dari ikon Android atau memulihkan sesi dari save.

| Game | Sebelum (detik) | Sesudah (detik) | Persiapan launcher sebelum / sesudah (ms) |
| --- | ---: | ---: | ---: |
| D.C. II Plus Communication | 2,66 | 1,99 | 427 / 291 |
| D.C.4 | 1,26 | 1,03 | 123 / 54 |
| D.C.4 Plus Harmony | 1,32 | 0,98 | 137 / 63 |
| D.C. II To You | 0,53 | 0,45 | 46 / 26 |
| D.C. II Dearest Marriage | 1,26 | 1,11 | 77 / 35 |
| Dal Segno (Inggris resmi) | 1,15 | 1,09 | 75 / 30 |

Launcher memvalidasi folder sekali pada jalur peluncuran yang berhasil, memakai ulang profil aktif, dan hanya meminta metadata berkas untuk nama yang cocok saat memeriksa penanda skenario. Engine membaca aset menu dan audio dalam blok dan berhenti mencari katalog setelah pembacaan pertama yang berhasil.

Pengukuran pendahuluan menjalankan keenam putaran build lama dan baru secara terpisah. Pada rangkaian tersebut, median Dearest Marriage berubah dari 1,29 ke 1,58 detik dan Dal Segno dari 1,23 ke 1,41 detik. Tabel di atas memakai perbandingan bergantian untuk mengurangi pengaruh kondisi HP antarputaran.
