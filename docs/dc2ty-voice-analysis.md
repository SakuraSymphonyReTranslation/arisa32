# Voice DC2TY: XPCM 0x0303

Voice yang tidak terdengar berasal dari decoder XPCM yang belum mendukung codec DC2TY. Pembaca arsip berhasil membuka `VOICE.DAT`, tetapi parser lama menganggap header nonzero sebagai format musik Vorbis. Nilai `0x0303` kemudian ditandai sebagai codec tidak dikenal dan pemutaran ditolak.

## Bukti dari arsip dan HP

`E:\Games\DC2TY\AdvData\VOICE.DAT` berukuran 596.152.320 byte dan memuat 7.800 entri, termasuk 288 voice sistem. Seluruh entri memakai XPCM `0x0303`, mono 44.100 Hz, PCM keluaran 16 bit. Semua offset valid dan unik.

Log aplikasi di HP mencatat `VOICE.DAT: 7800 entries`. SHA256 arsip PC dan `/sdcard/Gamehub/DC2TY/AdvData/VOICE.DAT` identik:

```text
4925b68fab9b0dcd2f55cf2abaa2efc115894eb38313277389295e4d0c51b856
```

Jadi, arsip di HP tersedia dan sama dengan sumber PC. Tidak perlu menyalin ulang data game untuk memperbaiki masalah codec ini.

| Offset blob | Isi |
| --- | --- |
| `0x00` | Magic `XPCM` |
| `0x04` | Ukuran PCM hasil decode |
| `0x08` | Mode kompresi `03`, flags `03`, dua byte nol |
| `0x0C` | Format PCM, jumlah kanal, sample rate, byte rate, alignment, bit depth |
| `0x1C` | Ukuran payload terkompresi |
| `0x20` | Stream zlib yang berisi data Circus VQ |

Membuka zlib saja belum menghasilkan PCM. Setiap blok VQ 8.192 byte membutuhkan rekonstruksi koefisien, transformasi fixed point, filter riwayat sampel, dan overlap 32 sampel. Satu blok menghasilkan 4.064 sampel baru.

## Pembandingan dengan engine asli

Rutin `0x406040` dari `DC2TY.exe` dijalankan secara terisolasi melalui emulator CPU Unicorn, dengan inisialisasi lookup `0x405F20` dan tabel asli. Ini tidak membuka game atau mengendalikan layar HP.

Keluaran native untuk 17 voice asli, terdiri dari pengumuman pertama 16 karakter dan satu dialog awal Hydrangea, identik per sampel dengan vgmstream-cli r2168. Fungsi native juga mengonfirmasi pembulatan signed, filter `(27*x + 4*h1 + h2)`, dan overflow 32 bit sebelum pembagian akhir. Pada fixture sintetis dengan koefisien besar, perilaku overflow native berbeda dari decoder referensi umum; golden test mengikuti executable DC2TY.

Implementasi portable memakai transformasi dari vgmstream dengan aritmetika sesuai DC2TY, tabel fixed point tetap, dan dekompresi miniz. Atribusi sumber disertakan dalam kode serta `assets/licenses/vgmstream-NOTICE.txt` di APK. Ukuran output, flags, panjang frame, checksum zlib, dan akhir stream divalidasi sebelum PCM diterima.

## Batas pemeriksaan sebelumnya

Tes menu sebelumnya memeriksa pemetaan nama voice. Tes audio karakter menggunakan PCM sintetis. Keduanya tidak membuktikan decode payload asli `0x0303`. Regresi baru menggunakan arsip asli, keluaran mixer, dialog/replay, serta pengaturan volume dan mute.

## Verifikasi akhir

| Pemeriksaan | Hasil |
| --- | --- |
| Seluruh arsip | PASS: 7.800 entri, 1.365.965.735 frame PCM, 0 gagal, 0 hasil sepenuhnya senyap |
| Voice menu | PASS: 240 aset berbeda yang dirujuk event menu untuk 16 karakter berhasil dicari dan didekode |
| Dialog dan mixer | PASS: dialog asli Hydrangea, replay identik, volume karakter/global/master, mute, serta pengumuman sistem setelah dialog dimute |
| Kecocokan native | PASS: keluaran portable identik per sampel dengan DC2TY.exe untuk 17 voice asli dan tiga fixture sintetis |
| Regresi | PASS: `xpcm_test`, `audio_test`, `media_test`; termasuk input rusak dan varian PCM/Vorbis lama |
| Build | PASS: desktop serta APK arm64-v8a dan armeabi-v7a |
| Decoder Android | PASS: probe tanpa UI memakai `libarisa32.so` dari APK yang terpasang di HP; `sys_otm_01` menghasilkan 44.988 sampel mono 44.100 Hz, identik dengan keluaran native |
| Instalasi | PASS: `adb install -r` pada iQOO Z9x, versi 0.7.1 / code 19; pembaruan tercatat 10 November 2026 pukul 14.41 Asia/Bangkok |
| Whitespace | PASS: `git diff --check` |

Probe Android melakukan decode ke data PCM tanpa membuka game atau memutar suara ke speaker. Pemutaran melalui menu game dan keluaran speaker belum diuji langsung pada revisi ini. Data/save aplikasi dipertahankan oleh instalasi pembaruan.

APK akhir berukuran 19.498.767 byte. Atribusi vgmstream telah diperiksa di dalam APK.

```text
APK SHA256
8cc3ffb771fc7c9e5a84c283ad48ed158ecfc2604b09da6dca7c4c29e4c632a1

arm64-v8a libarisa32.so SHA256 (APK dan perangkat identik)
7b33165f80b14316d8bc7e691cafec13b3f81b53ee8c948d7260aaca61f9ef08

armeabi-v7a libarisa32.so SHA256
4b25eb0513feff431b5b27bcfda1ab0d722b590a16f03270f6fc3754b43849b2

sys_otm_01 PCM SHA256 (native, portable desktop, Android identik)
faf2de2966ff00ab3550dda1ec2c6254fae8d6ec6ac2cfb9d360ebd253d7937d
```

## Antislop during: delivery gate

- Hard Gate PASS dalam cakupan perbaikan codec: build, decoder nyata, kontrol audio, dan input rusak telah diuji; tidak ada aset visual atau navigasi baru.
- Purpose Gate PASS: transformasi fixed point dan tabel tetap mereproduksi keluaran native, dibuktikan pembandingan sampel.
- Liveliness PASS untuk cakupan ini: tampilan dan animasi tetap mengikuti arah game; perubahan hanya pada decoding audio.
- Craftsmanship PASS: pengujian memakai seluruh arsip dan golden output native, lisensi dibundel, serta batas pengujian speaker dinyatakan eksplisit. Komentar baru menjelaskan format, aritmetika, atau batas data.
