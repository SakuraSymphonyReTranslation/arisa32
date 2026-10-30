# Catatan rilis per bahasa

Setiap rilis memiliki satu dokumen Bahasa Indonesia dan satu dokumen English. Pemilih bahasa membuka dokumen lain di tab yang sama; hanya satu bahasa tampil pada setiap halaman. GitHub tidak menjalankan JavaScript untuk mengganti isi README atau deskripsi rilis secara dinamis.

| Versi | Bahasa Indonesia | English |
| --- | --- | --- |
| v0.6.2 | [Catatan rilis](v0.6.2/id.md) | [Release notes](v0.6.2/en.md) |
| v0.6.1 | [Catatan rilis](v0.6.1/id.md) | [Release notes](v0.6.1/en.md) |
| v0.6.0 | [Catatan rilis](v0.6.0/id.md) | [Release notes](v0.6.0/en.md) |
| v0.5.5 | [Catatan rilis](v0.5.5/id.md) | [Release notes](v0.5.5/en.md) |
| v0.5.0 | [Catatan rilis](v0.5.0/id.md) | [Release notes](v0.5.0/en.md) |
| v0.4.0 | [Catatan rilis](v0.4.0/id.md) | [Release notes](v0.4.0/en.md) |
| v0.3.0 | [Catatan rilis](v0.3.0/id.md) | [Release notes](v0.3.0/en.md) |
| v0.2.0 | [Catatan rilis](v0.2.0/id.md) | [Release notes](v0.2.0/en.md) |

## Menyiapkan rilis berikutnya

1. Buat `docs/releases/<tag>/id.md` dan `docs/releases/<tag>/en.md`, menggunakan tag yang sama persis dengan rilis GitHub, termasuk awalan `v`.
2. Awali `id.md` dengan `**Bahasa Indonesia** | [English](en.md)` dan `en.md` dengan `[Bahasa Indonesia](id.md) | **English**`. Gunakan tautan Markdown biasa tanpa target tab baru.
3. Setelah pemilih bahasa, tambahkan satu judul tingkat pertama (`#`) yang menyebut Arisa32, versi, dan ringkasan dalam bahasa dokumen. Judul ini juga digunakan pada dialog pembaruan aplikasi. Tulis catatan rilis lengkap dalam satu bahasa per dokumen. Pertahankan nama aset, angka validasi, status beta, dan batasan yang sama pada kedua bahasa.
4. Gunakan tautan absolut untuk laporan, unduhan, dan halaman lain yang perlu dibuka dari aplikasi. Tautan pemilih bahasa tetap relatif agar navigasi antardokumen bekerja di GitHub.
5. Commit dan push kedua dokumen ke `main` sebelum menerbitkan rilis. Aplikasi mengambil dokumen berdasarkan bahasa aplikasi dari `https://raw.githubusercontent.com/SakuraSymphonyReTranslation/arisa32/main/docs/releases/<tag>/<id|en>.md`. Sesuaikan pengaturan Bahasa Aplikasi untuk menguji kedua versi.
6. Untuk body rilis GitHub, gunakan isi lengkap `id.md` dengan baris pertama diganti menjadi `**Bahasa Indonesia** | [English](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/main/docs/releases/<tag>/en.md)`. Ubah tautan relatif lain menjadi URL absolut jika ada. Catatan Inggris tetap berada pada dokumen tersendiri.
7. Periksa kedua dokumen, tautan bahasa, nama APK/checksum, dan tampilan catatan panjang pada dialog pembaruan. Tambahkan versi baru pada tabel di atas.

Dokumen lama mencatat keadaan fitur pada versi tersebut. Contohnya, catatan v0.5.0 menyebut LibVLC, sementara v0.6.1 mencatat perpindahan ke FFmpeg. Batasan dari rilis lama tidak otomatis menggambarkan versi terbaru.
