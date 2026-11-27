# Pilihan alarm pada D.C. II P.C. ~Da Capo II Plus Communication~

Ketiga pilihan di `fst_1219_f1_cm1.mes` menjalankan cabang malam masing-masing lalu masuk ke `meza_nin_20.mes`. Skrip alarm memasang tiga tombol dan mendaftarkan `meza_menu` sebagai tujuan tombol Kembali. Arisa32 sebelumnya meneruskan label itu ke skrip pagi. Skrip pagi kemudian berhenti di awal dengan pesan `unknown MES label 'meza_menu'`.

| Perintah DC2PC | Fungsi dalam alur ini |
| --- | --- |
| `0x4F` diikuti `0x3A` | Memasang area tombol dan tujuan lokalnya. |
| `0x3B` | Mendaftarkan label lokal untuk tombol Kembali. Label kosong menghapusnya. |
| `0x0A` | Menunggu pilihan selama tombol skenario terpasang. |
| `0x37` diikuti `0x2E` | Masuk ke label yang ditentukan di file MES tujuan. |

Di alarm, Kembali membuka sidebar melalui `meza_menu`. Menutup sidebar mengembalikan pemain ke pilihan waktu. Pilihan `fast`, `normal`, dan `slow` pada tanggal 20 masing-masing menuju `fst_1220_a1_otm.mes`, `fst_1220_a1_kko.mes`, dan `fst_1220_a1_anz.mes`.

Perilaku ini memerlukan MES ClassicDC2 versi `0x9969` serta dua berkas skenario pada data dasar gim, `MES/ed_ais.mes` dan `MES/ed_erk.mes`. Instalasi D.C.P.C. ~Da Capo Plus Communication~ yang diperiksa tidak memiliki kedua berkas itu. Uji regresi `dc2pc_flow_test` juga menjalankan skrip berheader sama tanpa penanda gim dan memeriksa perilaku perpindahan label yang sudah ada.

Audit 1.558 MES dengan overlay bahasa Indonesia menemukan seluruh 255 tujuan Kembali yang berisi nama di file aktif dan seluruh 136 tujuan masuk bernama di file tujuan. Pemeriksaan ini memvalidasi referensi label. Uji alur memeriksa secara terpisah apakah pemain dapat menekan tombol dan mencapai dialog berikutnya.

Pengujian melalui `SceneRunner` mencapai dialog pagi dari ketiga cabang quick save tanggal 19. Ketiga tombol diuji di setiap 16 skrip alarm tanggal 20–23, dengan total 48 perpindahan berhasil. Pada peta `kimo_kko` dan `kimo_nnk`, pengujian memilih lokasi, menekan Kembali, memilih ulang, lalu mencapai dialog tujuan masing-masing.

Semua 16 skrip alarm meminta tombol pertama memulihkan area layar pada posisi yang sama dari sheet 0. Renderer sudah menggambar panel statis saat menyiapkan setiap frame. Menyalin ulang latar meja pada langkah itu menghapus sudut kiri atas panel. Pengujian di ponsel I2219 memakai gambar Jepang dari `id_Data/.original_cache` dan gambar Indonesia dari `id_Data`. Kedua panel tetap utuh setelah sorotan dan tekanan pada masing-masing tombol dilepas. [Perbandingan render pada perangkat](dc2pc-alarm-render.jpg)
