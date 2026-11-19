**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.7.4 - Perbaikan Progres D.C.4 dan Dal Segno Experimental

Rilis 19 November 2026 ini memperbaiki akses D.C. yang terbuka terlalu dini pada D.C.4 dan D.C.4 Plus Harmony. Arisa32 kini memvalidasi progres dan menangani profil lama secara otomatis. Rilis ini juga membawa tahap awal kompatibilitas D.S. -Dal Segno- versi MangaGamer dengan terjemahan English resmi.

### Progres D.C.4 dan D.C.4 Plus Harmony

- Menu D.C. baru terbuka setelah delapan ending yang diwajibkan terkonfirmasi dan skrip game memberikan flag pembukanya. Flag dari slot save yang disalin tidak lagi dianggap sebagai bukti penyelesaian.
- Memuat slot lama, termasuk slot prolog, mempertahankan progres global yang sudah sah. Posisi cerita di satu slot tidak menentukan apakah pemain pernah menyelesaikan rute lain.
- Arisa32 menganalisis setiap profil lama berdasarkan riwayat yang tersedia, mempertahankan progres yang dapat dikonfirmasi, dan menyimpan profil hasil migrasi secara otomatis. Pengguna tidak perlu membawa `system-profile.dat` ke Codex untuk memperbaiki flag secara manual.
- Sebelum mengganti profil versi lama, Arisa32 membuat `system-profile.dat.v1-backup`. Slot cerita, companion save, riwayat galeri/musik/scene, dan pengaturan tetap dipertahankan.
- Save Arisa32 lama dengan fingerprint snapshot terdahulu dapat dimuat kembali.

Profil yang terkena bug unlock lama mungkin tidak memiliki bukti untuk semua ending. Arisa32 tidak dapat memulihkan informasi yang belum pernah tercatat; ending yang belum dapat dikonfirmasi perlu diselesaikan lagi agar dihitung untuk akses D.C. Saat membagikan save baru, sertakan profil global versi 2 agar progres yang terkonfirmasi ikut terbawa.

Perbaikan ini mencakup kedua versi D.C.4. Lihat [perubahan sumber c3eb9b6](https://github.com/SakuraSymphonyReTranslation/arisa32/commit/c3eb9b625ced963c11d870173e6f7bb3d0b0b061), [laporan progres](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/dc4-progression.md), dan [kompatibilitas save lama](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/dc4ph-legacy-save-compatibility.md).

### Dal Segno MangaGamer: experimental

Tahap ini menambahkan deteksi game dan pembacaan skrip khusus Dal Segno, memperbaiki startup menuju menu utama dan kelanjutan setelah perpindahan skrip, serta menyesuaikan textbox dan area sentuh toolbar. Slider CONFIG dapat diseret, tombol END kembali ke library, dan profil baru mengikuti flag unlock dari skrip. Pemetaan audio juga diperbaiki agar perintah berhenti voice/SE tidak dibaca sebagai indeks BGM.

**Dal Segno masih experimental dan belum fully playable.** Menurut pengujian pengguna, sprite karakter belum muncul, respons tombol lambat, EXTRA belum dapat digunakan, serta sejumlah tombol dan fungsi menu belum bekerja. Dukungan ini merupakan milestone kompatibilitas awal untuk versi MangaGamer dengan terjemahan English resmi.

Tes title memeriksa keadaan terkunci/terbuka berdasarkan flag skrip. Tes tersebut belum membuktikan penyelesaian semua rute, pemberian unlock pada setiap ending, perbaikan otomatis profil Dal Segno, atau kompatibilitas save native melalui baca/tulis penuh. Lihat [laporan Dal Segno dan batas pengujian](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/dalsegno-validation.md).

### Pengujian

Tes desktop terkait opcode, VM, pemulihan save, scene, audio, serta progres kedua versi D.C.4 lulus. Pemeriksaan Dal Segno mencakup enam/tujuh aksi title, 11 target toolbar, dan 11 slider pada lima tab CONFIG. Perbaikan progres D.C.4 Plus Harmony juga diperiksa pada HP, termasuk migrasi profil, restart, dan pemeliharaan slot cerita. Ini belum mencakup playthrough lengkap semua rute.

APK rilis dan debug universal v0.7.4 memakai versionCode 22. Rincian tanda tangan, versi Android, ABI, alignment, isi paket, dan pemeriksaan updater tersedia pada [laporan QA v0.7.4](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/release-qa-0.7.4.md).

### Unduhan

Kedua APK mendukung ARM64 dan ARMv7, dengan Android 5.0 atau lebih baru (API 21+).

| Berkas | Kegunaan |
| --- | --- |
| [Arisa32-v0.7.4.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.4/Arisa32-v0.7.4.apk) | APK rilis untuk pemakaian biasa; dipilih oleh pembaruan dalam aplikasi. |
| [Arisa32-v0.7.4-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.4/Arisa32-v0.7.4-debug.apk) | APK debug untuk pemeriksaan masalah. |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.4/SHA256SUMS.txt) | Checksum kedua APK. |

Pasang pembaruan di atas instalasi yang ada untuk mempertahankan data aplikasi. Berkas game dan patch disediakan terpisah. Semua unduhan tersedia pada [halaman rilis v0.7.4](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.4).
