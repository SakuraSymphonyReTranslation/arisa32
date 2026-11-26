**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.7.5 - Pembaruan profil Dal Segno MangaGamer

Rilis 26 November 2026 ini menggabungkan dua kelanjutan pengembangan Dal Segno dengan perbaikan terbaru pada EXTRA, CONFIG, audio, dan tampilan adegan. Dukungan untuk **D.S. -Dal Segno- edisi MangaGamer dengan terjemahan Inggris resmi** masih eksperimental.

### Mulai permainan dan navigasi

- Arisa32 mengenali edisi MangaGamer dari skrip game walaupun folder instalasi sudah diganti nama. D.S. i.F. -Dal Segno- in Future memerlukan profilnya sendiri.
- New Game membuka layar nama bawaan game. Di Android, nama depan dan nama keluarga dapat diisi dengan keyboard, dikembalikan ke Takamura Atsuya, atau diperiksa lewat konfirmasi Yes/No. Nama yang disetujui dipakai dalam dialog dan tersimpan untuk sesi berikutnya.
- Toolbar dialog kembali aktif pada New Game dan setelah Load. Tombol tiga titik dan tombol melayang mengikuti keadaan cerita, layar menu, dan textbox yang disembunyikan. Upper Menu membuka Save, Load, CONFIG, Hide Textbox, dan Return to Title.
- Load dari judul membuka layar slot. Back mengembalikan layar judul, dan Return to Title dari cerita membersihkan tampilan adegan lama. Pada layar tanggal, toolbar tersembunyi dan ketukan di latar melanjutkan cerita, termasuk setelah Auto Load.
- Backlog memakai tampilan asli dengan lima baris per layar, scrollbar, pemutaran ulang suara per baris, dan konfirmasi sebelum kembali ke dialog yang dipilih.

### Save, Load, dan progres

- Layar bertema Dal Segno menyediakan 10 halaman dengan Latest, Auto, Quick, dan 97 slot biasa. Pratinjau menampilkan metadata dan judul berdasarkan tabel 280 skenario. Latest dan Auto dikelola oleh game.
- Impor dan ekspor mendukung slot PC `DSAO##.dat` pada checkpoint yang didukung. Ekspor Arisa32 menyertakan companion untuk menyimpan state tambahan. Format sekarang memakai nama seperti `DSAO03.arisa.dat`, dan companion lama berakhiran `.dat.arisa` tetap dapat dibaca.
- Pemulihan save mencakup posisi sprite, aksesori, kamera, dan audio pada adegan yang diuji. Companion Dal Segno kini memakai snapshot v15 dengan pembacaan snapshot v13 dan v14.
- Profil menyimpan nama, pengaturan, progres, dan pilihan Music Mode. Arisa32 dapat memakai progres dari `system.dat` PC dan memulihkan unlock Scenario serta CG yang didukung berdasarkan riwayat baca dan slot pada profil Arisa32 lama.

### EXTRA, Music Mode, dan CONFIG

- EXTRA membuka **Scenario Replay**, **CG Mode**, dan **Music Mode**. Tile Scenario dan CG mengikuti progres profil. Scenario Replay bisa dibatalkan atau diakhiri untuk kembali ke daftar. Save, Load, dan Upper Menu dibatasi selama replay agar progres cerita tetap terpisah.
- CG yang terbuka dapat diperbesar sesuai rasio gambar. Ketuk untuk melihat varian berikutnya dan gunakan Back untuk kembali ke galeri.
- Music Mode menyediakan 42 lagu asli, play/stop, mulai ulang, lagu sebelumnya/berikutnya, scrollbar, penanda lagu aktif, serta pilihan centang untuk playlist. Tersedia All On/Off, pengulangan satu lagu atau playlist, dan putar acak.
- CONFIG memakai lima tab dan 11 slider game. Pembaruan ini menyambungkan kecepatan teks dan Auto, opsi skip serta efek, transparansi textbox, dan kontrol volume/mute untuk master, BGM, SE, voice, karakter, dan movie. Keluar dari CONFIG mengembalikan adegan cerita.

### Sprite, dialog, dan efek

- Sprite tubuh dan aksesori mengikuti penempatan dan pivot dari game PC, termasuk pan dan zoom. Potret dialog seperti Ame kembali tampil, dan angka animasi pada adegan Himari dan Noeri mengikuti sprite karakter. Angka muncul pada adegan yang memicunya.
- Start Game dan Return to Title memakai transisi lingkaran menyusut. Fade pembuka, partikel cahaya, dan lapisan efek latar kini dirender pada adegan yang mendukungnya dan mengikuti pengaturan efek game.
- Layar tanggal memakai palet transparan dan posisi asli. Teks Inggris memakai font bawaan game bila tersedia. Penanda inline untuk garis dan not musik dirender sebagai gambar di dialog dan backlog.
- Pemetaan perintah berhenti BGM dan pemisahan potret dari perintah voice diperbaiki. Backlog dan tombol replay memakai suara baris yang bersangkutan.

### Cakupan pengujian

Pemeriksaan desktop dan Android mencakup menu, nama, save, backlog, layar tanggal, kamera, dan frame efek terpilih. Seluruh rute, ending, rangkaian unlock, dan kesamaan visual frame demi frame dengan PC masih perlu divalidasi. Impor save PC di titik pilihan dan ekspor saat video atau timed wait aktif masih terbatas. [Handoff pertama](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.5/docs/dalsegno-progress-2026-11-21.md) · [Handoff kedua](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.5/docs/dalsegno-handoff-2026-11-24.md) · [Perbandingan efek](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.5/docs/dalsegno-effects-validation-2026-11-26.md)

### Unduhan Android

Kedua APK memakai versionCode 23 dan mendukung ARM64 serta ARMv7 pada Android 5.0 atau lebih baru (API 21+).

| Berkas | Kegunaan |
| --- | --- |
| [Arisa32-v0.7.5.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.5/Arisa32-v0.7.5.apk) | APK untuk pemakaian biasa dan pembaruan dari dalam aplikasi |
| [Arisa32-v0.7.5-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.5/Arisa32-v0.7.5-debug.apk) | APK debug untuk pemeriksaan masalah |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.5/SHA256SUMS.txt) | Checksum kedua APK |

Pasang pembaruan di atas instalasi Arisa32 yang ada untuk mempertahankan data aplikasi. Arisa32 membaca aset dari instalasi game. [Halaman rilis v0.7.5](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.5)
