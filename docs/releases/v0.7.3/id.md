**Bahasa Indonesia** | [English](en.md)

# Arisa32 v0.7.3 - Perbaikan Music Mode dan Scene Replay DC2PC

Rilis 14 November 2026 ini memperbaiki judul lagu, scrollbar, serta tampilan saat menekan baris Music Mode dan kartu Scene Replay di D.C. II Plus Communication (DC2PC). README Bahasa Indonesia dan English juga menjelaskan cara membaca dialog sebelumnya pada DC2PC dan DC2TY.

### Music Mode DC2PC

- Kotak atas menampilkan judul lagu yang dipilih dari 74 judul asli. Judul tetap tampil saat pointer berpindah, daftar digulir, atau Stop ditekan.
- Baris lagu mempertahankan sorotan kuning dari artwork asli selama ditekan, sehingga latarnya tidak lagi menghilang.
- Pegangan scrollbar putih/krem kini terlihat dan dapat diseret sampai lagu terakhir dalam katalog.

Perubahan Music Mode ini khusus DC2PC dan memakai artwork game asli. Lihat [laporan dan bukti Music Mode](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.3/docs/dc2pc-music-mode-fix.md).

### Scene Replay dan bahasa konfirmasi

- Tanggal pada kartu DC2PC tetap terlihat dalam keadaan normal, disorot, dan ditekan.
- Kartu yang ditekan mempertahankan artwork sorotan hijau muda. Menekan kartu langsung lewat sentuhan tidak lagi menghilangkan latar kartu atau tanggalnya.
- Saat patch Indonesia dipilih sebagai konten game, dialog memakai **Putar Scenario ini?**. Konten Jepang tetap memakai **このシーンを再生しますか？**. Bahasa dialog mengikuti pilihan konten/patch game, terpisah dari bahasa antarmuka Arisa32.

Perbaikan artwork kartu khusus DC2PC. Kata-kata Indonesia pada konfirmasi Scene Replay juga berlaku untuk profil lain yang memakai dialog bersama tersebut. Lihat [laporan, perbandingan gambar, dan pengujian kedua bahasa](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.3/docs/dc2pc-scene-replay-fix.md).

### Koreksi dokumentasi backlog

DC2PC dan DC2TY memang tidak memiliki layar atau artwork backlog native tersendiri. Tekan panah atas di area textbox untuk membaca dialog sebelumnya; panah bawah bergerak maju menuju dialog terbaru yang sedang tampil. DC2DM memiliki antarmuka backlog tersendiri.

README kedua bahasa kini menjelaskan perilaku tersebut sebagai desain game asli. Penulisan judul game juga diperbaiki agar tanda tilde tidak membuat teks tampil dicoret di GitHub. Bagian ini merupakan koreksi dokumentasi, bukan penambahan menu backlog baru.

### Verifikasi dan batas dukungan

- Enam tes regresi desktop lulus, mencakup menu DC2PC, DC2TY, DC2DM, menu sistem, render teks, dan dialog konfirmasi. Music Mode diperiksa dengan 74 judul serta kedua font, MS Gothic dan Pretendard JP.
- Probe renderer/input native Android lulus 120 pemeriksaan Scene Replay dan 36 pemeriksaan tambahan dengan konten Jepang serta patch Indonesia asli. Lima capture 1280x720 cocok dengan renderer desktop tanpa piksel berbeda. Probe ini berjalan tanpa Activity Android, sehingga bukan pengujian gesture fisik pada aplikasi.
- Pengujian perbaikan di atas memakai build pengembangan 0.7.2/code20. Laporan mencatat batasnya: DC2DM memakai fixture dan pemeriksaan aset DC4 dilewati. Hasil ini tidak mencakup semua game atau perangkat. Dukungan DC2TY tetap awal; seluruh rute belum diverifikasi sampai selesai.
- Build APK rilis dan debug universal v0.7.3/code21 selesai. Tanda tangan, ABI ARM64/ARMv7, batas Android API 21+, alignment ARM64 16 KB, dan isi paket sudah diperiksa. Lihat [laporan QA paket v0.7.3](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.3/docs/release-qa-0.7.3.md).

### Unduhan

Kedua APK mendukung ARM64 dan ARMv7, dengan Android 5.0 atau lebih baru (API 21+).

| Berkas | Kegunaan |
| --- | --- |
| [Arisa32-v0.7.3.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.3/Arisa32-v0.7.3.apk) | APK rilis untuk pemakaian biasa; dipilih oleh pembaruan dalam aplikasi. |
| [Arisa32-v0.7.3-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.3/Arisa32-v0.7.3-debug.apk) | APK debug untuk pemeriksaan masalah. |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.3/SHA256SUMS.txt) | Checksum kedua APK. |

Pasang pembaruan di atas instalasi yang ada untuk mempertahankan data aplikasi. Berkas game dan patch disediakan terpisah. Semua aset unduhan tersedia pada [halaman rilis v0.7.3](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.3).
