# Changelog

## 0.7.2 - 2026-11-11

### Bahasa Indonesia

- Menambahkan dukungan awal DC2TY: opening, Title sesuai progres, pemilih empat cerita, menu asli, galeri CG, dan impor progres serta konfigurasi awal dari `system.dat`.
- Menambahkan decoder voice DC2TY untuk dialog dan pengumuman menu, dengan volume serta mute per karakter.
- Memperbaiki judul dan kontrol Music Mode, tanggal serta bingkai Scene Replay, teks dan keadaan tekan pilihan, serta konfirmasi reset Config DC2TY.
- Memperbaiki posisi sprite close-up, mask/dissolve/efek pecahan layar, dan gambar pergantian tanggal DC2TY dengan keempat variasi artwork asli.
- Menambahkan baca/tulis `dc2tyxx.dat` dengan companion `.dat.arisa`, judul skenario, tanggal, ikon, quicksave `00`, salinan save manual terakhir `01`, dan 48 slot manual sesuai penomoran PC. Save `.dc4sav` lama tetap dapat dimuat; backup ZIP menyertakan companion.
- Memverifikasi penerimaan struktur save ekspor oleh reader dan loader executable PC asli. Pemulihan adegan penuh di PC belum diverifikasi; companion memulihkan posisi BGM/SE berulang, tetapi voice dialog diputar ulang dari awal.
- Memperjelas MS Gothic dan memperbaiki ukuran serta lebar proporsional Pretendard JP Bold pada preview CONFIG dan backlog DC2DM.
- Memperbaiki pemilihan baris backlog DC2DM melalui ketukan cepat, gerakan jari, scroll, dan pergantian mode cursor.

### English

- Adds initial DC2TY support: opening, progress-dependent Title, four-story selection, original menus, CG gallery, and progress and initial-settings import from `system.dat`.
- Adds DC2TY voice decoding for dialogue and menu announcements, with per-character volume and mute.
- Fixes DC2TY Music Mode titles and controls, Scene Replay dates and borders, choice text and pressed states, and the Config reset confirmation.
- Fixes close-up sprite positioning, masks/dissolves/screen-breakup effects, and DC2TY date-change cards with all four original artwork variants.
- Adds `dc2tyxx.dat` reading and writing with `.dat.arisa` companions, scenario titles, dates, icons, quicksave `00`, latest manual-save copy `01`, and 48 manual slots following PC numbering. Existing `.dc4sav` saves remain loadable; ZIP backups include companions.
- Verifies that the original PC executable's reader and loader accept the exported save structure. Full scene restoration on PC remains unverified; companions restore BGM and looping sound-effect positions, while dialogue voice replays from the beginning.
- Improves MS Gothic clarity and Pretendard JP Bold sizes and proportional widths in the DC2DM CONFIG preview and backlog.
- Fixes DC2DM backlog row selection for quick taps, finger movement, scrolling, and cursor-mode changes.

Catatan rilis lengkap / Full release notes: [Bahasa Indonesia](docs/releases/v0.7.2/id.md) | [English](docs/releases/v0.7.2/en.md).

## 0.7.1 - 2026-11-08

### Bahasa Indonesia

- Menetapkan D.C. II Dearest Marriage sebagai fully playable setelah konfirmasi permainan sampai selesai oleh pengguna.
- Memperbaiki perpindahan Title dan pilihan cerita DC2DM tanpa jeda hitam tambahan, mempertahankan BGM, meredupkan seluruh label bersama latar, serta membersihkan kontrol saat opening dimulai atau dibatalkan.
- Menghapus jeda aplikasi saat masuk ke minigame DC2DM dan mempertahankan transisi dari skrip asli.
- Memuat riwayat baca DC2DM sebelum impor progres PC agar startup tidak menimpanya dengan riwayat kosong.
- Menambahkan Pretendard JP Bold ke pemilih font Android, beserta lisensinya, dan memperbaiki ukuran serta jarak hurufnya pada dialog, nama pembicara, dan pilihan DC2DM.
- Memperbaiki lebar huruf Pretendard pada pesan COMU DC4/DC4PH yang memakai spasi padding, dengan ukuran teks dan susunan dua baris tetap. Tata letak Sakuramusubi dan font lain dipertahankan.
- Memperbaiki urutan CG DC2PC antarkelompok thumbnail, potongan dan gerakan gambar tinggi, serta pratinjau dari gambar sumber. Cache sampel mengurangi pekerjaan render saat gambar bergerak.
- Memperbarui README Indonesia dan Inggris dengan petunjuk folder data asli, patch terjemahan, dan pemilihan bahasa.
- Menyediakan APK rilis dan debug universal, masing-masing berisi ARM64 dan ARMv7 untuk Android 5.0+. Pembaru di dalam aplikasi mengunduh paket rilis universal.

### English

- Marks D.C. II Dearest Marriage as fully playable following the user's confirmation of completed play.
- Fixes DC2DM Title and story-selection transitions without an extra black gap, preserves BGM, fades labels with the background, and clears controls when an opening begins or is canceled.
- Removes the app delay when entering DC2DM minigames and retains the original scripted transitions.
- Loads DC2DM read history before importing PC progress so startup cannot overwrite it with an empty history.
- Adds Pretendard JP Bold and its license to Android's font picker, and corrects its glyph size and spacing in DC2DM dialogue, speaker names, and choices.
- Corrects Pretendard letter widths in DC4/DC4PH COMU messages that use padding spaces while preserving text size and the two-line layout. Sakuramusubi layouts and other fonts remain unchanged.
- Fixes DC2PC CG order between thumbnail groups, tall-image crops and movement, and previews rendered from source images. Cached samples reduce rendering work during movement.
- Updates the Indonesian and English READMEs with original-data and translation-patch folder instructions and language selection.
- Provides universal release and debug APKs, each containing ARM64 and ARMv7 for Android 5.0+. The in-app updater downloads the universal release package.

Catatan rilis lengkap / Full release notes: [Bahasa Indonesia](docs/releases/v0.7.1/id.md) | [English](docs/releases/v0.7.1/en.md).

## 0.7.0 - 2026-11-08

### Bahasa Indonesia

- Menambahkan pembacaan dan penulisan slot PC DC2DM dalam pasangan `.dat` dan `.dat.arisa`, termasuk pemulihan dialog, pilihan, gambar, dan audio. Save portabel lama tetap dapat dimuat.
- Mengimpor progres DC2DM dari `system.dat` agar latar judul, segmen cerita, dan galeri mengikuti progres game serta bertahan setelah aplikasi dimulai ulang atau slot lama dimuat.
- Memperbaiki penomoran quicksave, autosave, save terakhir, dan slot biasa DC2DM. Autosave dibuat pada checkpoint skrip; replay galeri tidak menulis autosave.
- Menampilkan judul skenario pada baris Save/Load, termasuk 279 judul MAR/SIN/SUC dan judul executable terjemahan. Tanggal dibaca dari isi save PC.
- Memperbaiki pengumuman suara menu dan tiga gaya suara Otome, termasuk suara setelah Save/Load dikonfirmasi. CONFIG tidak lagi memutar seluruh suara tombol saat digambar ulang.
- Memperbaiki pratinjau CG dari gambar sumber, urutan varian dalam setiap kategori, pergantian dengan fade, serta halaman thumbnail saat kembali dari pratinjau.
- Memulihkan tampilan pilihan cerita dari aset PC, termasuk posisi, teks, latar redup, dan keadaan normal/hover/pressed. Replay suara hanya aktif untuk rekaman milik dialog saat ini; narasi dan dialog Yoshiyuki tanpa suara tidak memutar rekaman sebelumnya.
- Menambahkan minigame suit dan Reversi dari menu asli, dengan aturan menang/kalah/seri, tingkat kesulitan Reversi, giliran, reaksi Otome, serta jumlah kemenangan dan kekalahan dari skrip game.
- Memperbaiki kembali paksa ke judul dari minigame: sesi dibatalkan tanpa menyimpan hasilnya, skor sesi terdahulu tetap ada, dan menu Load dapat digunakan kembali.
- Menghapus konfirmasi Start jika pilihan cerita sudah terbuka: dua cerita setelah Graduation Days selesai, tiga setelah Married Life selesai. Konfirmasi tetap tersedia sebelum Graduation Days selesai.
- Menambahkan transisi lingkaran saat kembali ke judul dan memperbaiki dissolve, fade, serta mask pergantian adegan, termasuk transisi putih kelas, pause, penantian skrip, dan pembersihan saat kembali paksa ke judul.

### English

- Adds DC2DM PC slot reading and writing with paired `.dat` and `.dat.arisa` files, including dialogue, choices, artwork, and audio restoration. Existing portable saves remain loadable.
- Imports DC2DM progress from `system.dat` so title backgrounds, story segments, and galleries follow game progress and survive restarts or loading an older slot.
- Corrects DC2DM quicksave, autosave, latest-save, and regular-slot numbering. Script checkpoints create autosaves; gallery replay does not overwrite them.
- Shows scenario titles in Save/Load rows, including 279 MAR/SIN/SUC titles and titles from translated executables. Dates are read from the PC save contents.
- Corrects menu announcements and Otome's three voice styles, including accepted Save/Load voices. Redrawing CONFIG no longer plays every button's attached voice.
- Improves CG previews from the source images, variant order within each category, fade transitions, and the thumbnail page restored when leaving a preview.
- Restores story-choice presentation from PC assets, including placement, captions, dimmed backgrounds, and normal/hover/pressed states. Voice replay is available only for the current line's recording; unvoiced narration and Yoshiyuki lines do not replay a previous voice.
- Adds rock-paper-scissors and Reversi through the original menu, with win/loss/draw rules, Reversi difficulty levels, turns, Otome reactions, and script-driven win/loss totals.
- Fixes forced returns to Title from a minigame: the active session is discarded without saving its results, earlier completed-session totals remain, and Load becomes available again.
- Removes the Start confirmation once story selection is unlocked: two stories after Graduation Days, three after Married Life. Confirmation remains before Graduation Days is completed.
- Adds the circular return-to-Title transition and improves scene dissolves, fades, and masks, including the classroom's white transition, pause, script waits, and cleanup after a forced return to Title.

Perubahan perilaku game di atas khusus profil DC2DM. Dukungan tetap beta; sebagian efek native dan crossfade langsung dari Title ke pilihan cerita belum sesuai PC. / The game-specific behavior above applies only to DC2DM. Support remains beta; some native effects and the direct Title-to-selector crossfade still differ from PC.

Catatan rilis lengkap / Full release notes: [Bahasa Indonesia](docs/releases/v0.7.0/id.md) | [English](docs/releases/v0.7.0/en.md).

## 0.6.9 - 2026-11-06

### Bahasa Indonesia

- Memperbaiki gerakan, antrean animasi, waktu kemunculan/penghilangan, dan pemulihan sprite DC4/DC4PH melalui sistem bersama yang berlaku untuk karakter lain, termasuk Nino dan Sorane.
- Memperketat pembukaan menu D.C. pada kedua game: penanda D.C. harus disertai delapan ending heroine. Pemeriksaan juga berlaku pada profil lama, impor PC, pemuatan slot, dan konfirmasi masuk.
- Memulihkan portrait kotak dialog dari aset FACE asli, termasuk posisi, aksesori, pembersihan saat pergantian adegan, dan save baru.
- Memperbaiki galeri CG dengan gambar utuh, pratinjau sesuai resolusi layar, urutan gambar terbuka, mask transisi asli, dan kontrol kembali ke galeri.
- Menambahkan mode kursor Android per game dengan geser relatif, ketuk untuk klik, tahan lalu geser, serta sembunyi otomatis setelah empat detik.
- Mendeteksi folder dasar DC4/DC4PH yang tidak lengkap tanpa menolak overlay terjemahan parsial, dan menampilkan `??????` untuk judul musik terkunci.
- Memperluas regresi sprite, portrait, galeri, progres, dan kompatibilitas save. Seluruh perbaikan 0.6.8 tetap disertakan; dukungan DC2DM tetap beta.

### English

- Fixes DC4/DC4PH movement, animation queues, entrance/exit timing, and sprite restoration through the shared system used by other characters, including Nino and Sorane.
- Tightens D.C. menu requirements in both games: the D.C. marker must be accompanied by all eight heroine endings. Checks also cover older profiles, PC imports, slot loading, and entry confirmation.
- Restores textbox portraits from the original FACE assets, including positioning, accessories, scene-change cleanup, and new saves.
- Improves the CG gallery with whole-image fitting, previews sized for the display, unlocked image order, original transition masks, and controls to return to the gallery.
- Adds a per-game Android cursor mode with relative movement, tap to click, hold then drag, and four-second auto-hide.
- Detects incomplete DC4/DC4PH base folders while allowing partial translation overlays, and shows `??????` for locked music titles.
- Extends sprite, portrait, gallery, progression, and save compatibility regressions. All 0.6.8 fixes remain included; DC2DM support remains beta.

Catatan rilis lengkap / Full release notes: [Bahasa Indonesia](docs/releases/v0.6.9/id.md) | [English](docs/releases/v0.6.9/en.md).

## 0.6.8 - 2026-11-05

### Bahasa Indonesia

- Menambahkan toggle Layar penuh per game untuk menyembunyikan navigation bar Android, dengan Fit to Screen / Stretch, koordinat sentuhan, kontrol mengambang, dan video mengikuti area layar.
- Memperbaiki kotak HP dan riwayat COMU DC4/DC4PH yang tertinggal setelah background berganti. Gambar transisi keluar mempertahankan HP hingga adegan berikutnya terlihat; preload dan narasi biasa tidak menutup COMU lebih awal.
- Memulihkan transisi gear membesar dan mengecil sesuai skrip dan aset asli DC4/DC4PH, termasuk durasi, skip, penantian skrip, dan pemulihan save.
- Memperluas pengujian dengan 440 pemeriksaan layar pada Android 16, 20 pemeriksaan preferensi host, dan regresi COMU, efek, serta scene. Seluruh perbaikan 0.6.7 tetap disertakan.

### English

- Adds a per-game Full screen toggle to hide Android navigation, with Fit to Screen / Stretch, touch coordinates, floating controls, and video following the display area.
- Fixes the DC4/DC4PH phone frame and COMU history remaining after background changes. The outgoing transition image retains the phone until the next scene appears; preloading and ordinary narration do not dismiss COMU early.
- Restores expanding and shrinking gear transitions from DC4/DC4PH scripts and original assets, including timing, skipping, script waits, and save restoration.
- Extends testing with 440 screen checks on Android 16, 20 host preference checks, and COMU, effect, and scene regressions. All 0.6.7 fixes remain included.

Catatan rilis lengkap / Full release notes: [Bahasa Indonesia](docs/releases/v0.6.8/id.md) | [English](docs/releases/v0.6.8/en.md).

## 0.6.7 - 2026-11-05

### Bahasa Indonesia

- Memperbaiki COMU DC4/DC4PH: spasi manual patch dipertahankan, format Sakuramusubi dan nama pengirim Latin dikenali, serta font proporsional memakai teks lebih besar dan maksimal tiga baris.
- Menyediakan FXAA Android 5 sampai 12 melalui OpenGL ES 2.0, dengan Bilinear sebagai cadangan; Android 13+ tetap memakai RuntimeShader.
- Menambahkan Fit to Screen / Stretch per game dan ukuran dialog DC4/DC4PH 18 sampai 22 untuk MS Gothic, font sistem, dan font kustom.
- Memulihkan sakura dan cahaya melayang sesuai skrip/aset asli, menambahkan Visual effect On/Off, serta mempertahankan keadaan efek dalam save yang didukung.
- Memperbaiki referensi replay suara pada narasi/dialog tanpa suara dan prioritas sprite, termasuk Gen di belakang Mitsumi.
- Menambah pemeriksaan progres ending dan kompatibilitas save. Syarat Sorane Ending 2 dan menu D.C. tetap mengikuti progres asli; data tamat tidak direset otomatis.

### English

- Fixes DC4/DC4PH COMU: preserves manual patch spacing, recognizes Sakuramusubi formatting and romanized senders, and uses larger proportional-font text with up to three lines.
- Adds FXAA on Android 5 through 12 using OpenGL ES 2.0 with Bilinear fallback; Android 13+ retains RuntimeShader.
- Adds per-game Fit to Screen / Stretch and DC4/DC4PH dialogue sizes from 18 to 22 for MS Gothic, system fonts, and custom fonts.
- Restores script-driven sakura and floating lights using original assets, adds Visual effect On/Off, and retains supported effect state in saves.
- Fixes voice replay references on narration/unvoiced dialogue and sprite priority, including Gen behind Mitsumi.
- Extends ending progression and save compatibility checks. Sorane Ending 2 and D.C. menu requirements retain original progression; completion data is not automatically reset.

Catatan rilis lengkap / Full release notes: [Bahasa Indonesia](docs/releases/v0.6.7/id.md) | [English](docs/releases/v0.6.7/en.md).

## 0.6.6 - 2026-11-03

- Memperbesar ukuran bawaan dialog Latin DC4/DC4PH dari 20 menjadi 22 piksel dan nama karakter dari 18 menjadi 22 piksel pada kanvas game. Jendela dialog Android memakai MS Gothic agar bentuk dan jarak huruf lebih mendekati versi PC.
- Memisahkan font jendela dialog dari font menu. Pemenggalan baris, tampilan teks bertahap, dan pemulihan teks dari save tetap menggunakan ukuran dan jarak huruf yang sesuai.
- Mengurangi opasitas penanda biru pembuka kontrol menjadi 40% saat tidak digunakan. Penanda kembali penuh saat disentuh atau mendapat fokus keyboard, dengan area sentuh dan gerakan navigasi yang sama.

Perbaikan progres rute dan peta dari 0.6.5 tetap tersedia. Dukungan DC2DM tetap beta.

Catatan rilis lengkap: [Bahasa Indonesia](docs/releases/v0.6.6/id.md) | [English](docs/releases/v0.6.6/en.md).

## 0.6.5 - 2026-11-02

- Memperbaiki profil baru DC4/DC4PH yang sebelumnya dianggap sudah menamatkan rute. Pembukaan rute Arisu/Arisa, pilihan tambahan Sorane, dan menu D.C. kini mengikuti syarat skrip asli.
- Menyembunyikan karakter, penanda lokasi, dan pilihan tambahan pada empat peta sampai syaratnya terpenuhi. Menu D.C. yang terkunci juga menolak aktivasi langsung.
- Mempertahankan progres tamat setelah memuat save lama, mengulang dari backlog, atau mereset konfigurasi. Skor pilihan peta tetap mengikuti save yang dimuat.
- Mempertahankan progres impor PC saat profil Android yang lebih lama dimuat. Profil 100% tetap terbuka seluruhnya.

Flag profil lama yang telanjur terbuka akibat bug tidak direset otomatis karena tidak dapat dibedakan dengan progres tamat asli. Perbaikan ini khusus DC4/DC4PH; perbaikan sebelumnya tetap tersedia.

Catatan rilis lengkap: [Bahasa Indonesia](docs/releases/v0.6.5/id.md) | [English](docs/releases/v0.6.5/en.md).

## 0.6.4 - 2026-11-02

- Memperbaiki pemilihan kelompok rute DC4/DC4PH berdasarkan skor pilihan peta, termasuk aturan skor seri dan pemulihan dari save sebelum percabangan.
- Memulihkan kelopak sakura dan transisi dari aset asli, termasuk bagian putih, mask, durasi skrip, serta penyimpanan keadaan efek pada save baru.
- Memperbaiki dua zoom Nino dan kemunculan sprite jam tangan DC4/DC4PH tanpa bergantung pada teks terjemahan. Delapan frame kontrol zoom lain tetap identik dalam pengujian.
- Menambahkan unduhan pembaruan di dalam aplikasi dengan progress bar, pemeriksaan APK, dan pemasangan melalui konfirmasi Android. APK sementara dibersihkan; sesi siap pasang dapat dilanjutkan setelah proses aplikasi berhenti.
- Mempertahankan perbaikan DC2DM, konfirmasi, penghalusan gambar, dan navigasi dari 0.6.3.

Untuk rute yang telanjur salah, muat save sebelum percabangan kelompok rute. Izin instal aplikasi tidak dikenal dan konfirmasi pemasangan tetap dikelola Android.

Catatan rilis lengkap: [Bahasa Indonesia](docs/releases/v0.6.4/id.md) | [English](docs/releases/v0.6.4/en.md).

## 0.6.3 - 2026-11-02

Versi 0.6.3 membawa perbaikan D.C. II Dearest Marriage, konfirmasi D.C.4 dan Plus Harmony, penghalusan gambar, serta kontrol game yang bisa disembunyikan dan dipindahkan.

### D.C. II Dearest Marriage

- Popup konfirmasi memakai tulisan dari gambar asli game, sehingga bentuk dan warna caption mengikuti aset PC, termasuk saat bahasa aplikasi diganti.
- Pratinjau teks CONFIG memakai ukuran dan jarak teks yang sesuai, tampil bertahap mengikuti pengaturan kecepatan teks, dan menampilkan spasi patch terjemahan dengan benar tanpa underscore pengganti spasi. Arah pengaturan transparansi pratinjau dan jendela dialog juga diperbaiki.
- Perbaikan ukuran glyph, jarak huruf dialog, penempatan nama pembicara, serta posisi dan reset gambar tanggal.
- Backlog mengikuti empat baris, warna, area tombol, dan atlas portrait asli game. Facelog Yuto kini memakai portrait yang sesuai; lingkaran merah muda kosong adalah Yoshiyuki.
- Portrait pada jendela dialog dimuat dari arsip FACE yang benar, termasuk Yuto dan ekspresi Maya berkacamata. Posisi, pemotongan gambar, pembersihan portrait, serta pemulihannya pada save baru diperbaiki.
- Perintah preload suara tidak lagi memutar suara lebih awal. Dialog tanpa suara juga tidak mewarisi rekaman suara baris sebelumnya untuk diputar ulang dari backlog.
- Back / Sidemenu tersedia saat cerita normal. Pembatasan sidemenu dan Save/Load pada Scene Replay tetap berlaku.

Perbaikan kompatibilitas di atas khusus untuk profil DC2DM. Status dukungannya tetap beta.

### Konfirmasi D.C.4 dan D.C.4 Plus Harmony

- Pengaturan Confirmations On/Off di CONFIG kini digunakan oleh dialog terkait, termasuk Save/Load, quick save/load, kembali ke judul, replay, dan keluar.
- Pilihan untuk tidak menanyakan lagi tersimpan langsung. Mengaktifkan kembali konfirmasi dari CONFIG membuat dialog muncul lagi; All On, All Off, dan reset tetap mengikuti pengaturan asli game.

### Kontrol mengambang dan auto-hide

1. Titik tiga dan Back / Sidemenu mulai di pojok kiri atas saat game dibuka, mengikuti batas aman layar dan kamera/cutout. Posisi lama yang terlalu ke tengah tidak dipulihkan lagi.
2. Opsi **Sembunyikan otomatis tombol game** menyembunyikan kontrol setelah empat detik. Geser titik tiga atau Back / Sidemenu ke kiri untuk menyembunyikannya secara manual.
3. Penanda di tepi kiri saat kontrol tersembunyi bisa ditarik ke atas atau bawah, misalnya untuk menjauhkannya dari tombol Game Booster. Posisi penanda tersimpan dan menyesuaikan tinggi layar.
4. Ketuk atau geser penanda ke kanan untuk memunculkan kontrol kembali. Tarikan vertikal hanya memindahkan penanda, tanpa membuka kontrol atau meneruskan sentuhan ke game.
5. Pada DC4/DC4PH, titik tiga ikut bersembunyi dan muncul kembali. Pada DC2 Plus Communication/DC2 Dearest Marriage, titik tiga dan Back / Sidemenu bersembunyi dan muncul bersama. Tombol yang terlihat masih bisa dipindahkan selama sesi bermain.

### Penghalusan gambar dan FPS Meter

- Pengaturan penghalusan gambar berlaku untuk semua profil game: **Off**, **Bilinear** sebagai bawaan, dan **FXAA** pada Android 13 atau lebih baru. Pilihan tersimpan dan dapat diubah saat bermain.
- Filter diterapkan ke hasil render game, termasuk backlog, popup, menu, dan cerita. FXAA kembali ke Bilinear jika dukungan GPU yang dibutuhkan tidak tersedia. Video MediaPlayer dan kontrol antarmuka Android memakai jalur render masing-masing.
- FXAA menghaluskan sebagian tepi gambar; filter ini tidak mengubah file tekstur, menambah detail, atau menjamin hasil yang sama pada setiap gambar. Teks tertentu dapat terlihat lebih lembut. Dampak FPS dan baterai belum diukur.
- Duplikasi FPS Meter di pengaturan dihapus sehingga hanya satu entri yang tampil.

### Batasan

- DC2DM tetap beta. Pengujian mencakup adegan dan kasus yang tercatat, bukan seluruh rute, perangkat, atau patch terjemahan. Raster font Android masih dapat berbeda dari PC.
- Save lama yang belum menyimpan portrait tidak dapat memulihkan gambar yang tidak tercatat; masuk kembali ke adegan untuk memuatnya. Save baru menyimpan portrait tersebut.
- Impor save native PC DC2DM belum didukung; gunakan format portable Arisa32. Backend video desktop masih MPEG-1, sedangkan patch berisi MP4/H.264 memerlukan FFmpeg Android.
- Pemindahan penanda dilakukan manual; Arisa32 tidak mengatur posisi overlay Game Booster.

Catatan rilis lengkap: [Bahasa Indonesia](docs/releases/v0.6.3/id.md) | [English](docs/releases/v0.6.3/en.md).

## 0.6.2 - 2026-10-30

### D.C. II Dearest Marriage: compatibility beta

Dukungan D.C. II Dearest Marriage (DC2DM) masih dalam tahap penambahan kompatibilitas dan beta test. Menurut pengujian pengguna, game sudah playable penuh, tetapi dukungannya belum sepenuhnya stabil.

- Arisa32 kini mengenali profil dan skrip DC2DM, dengan dukungan menu asli, dialog, pilihan, backlog, serta Save/Load portable Arisa32.
- Perbaikan posisi sprite, pergantian pose, urutan layer, dan trigger muncul/hilang. Aksesori seperti topi mengikuti posisi dan zoom karakter; penghapusan karakter ikut membersihkan aksesori.
- Scenario Mode / Scene Replay memblokir sidemenu, Save/Load, dan Quick Save/Load, termasuk pintasan dan dialog konfirmasi. Tombol QS/QL memakai tampilan nonaktif; cerita normal tetap dapat mengakses fitur tersebut.
- Opening memprioritaskan `.mpg` di folder MOVIE, dengan `.mp5` sebagai fallback. Backend FFmpeg Android mengenali isi file, termasuk patch `.mpg` yang sebenarnya berisi MP4/H.264 dan AAC.
- Ending native DC2DM menampilkan credits dan montase dari aset PCK, termasuk alur selesai normal dan replay. Sidemenu tidak dapat dibuka selama ending.

### Bug DC2DM yang masih diketahui

- Ukuran font pada menu masih terlalu besar atau tampak ter-zoom.
- Warna teks pada popup notifikasi masih belum sesuai.
- Suara karakter sering berulang.
- Masih ada bug UI dan kompatibilitas lain selama beta test. Status playable penuh tidak berarti seluruh bug telah selesai diperbaiki.

### D.C.4, Plus Harmony, dan pemutaran video

- Patch Inggris Sakuramusubi untuk D.C.4 Plus Harmony tetap memakai CP932. Arisa32 kini menangani penanda spasi khususnya agar tidak tampil sebagai kotak dan agar tanda kutip pembuka Jepang tetap bersama awal kalimat. Tidak perlu memilih encoding baru.
- Ending setelah segmen D.C. pada D.C.4 Plus Harmony kini menerima halaman credits `GRP/ED/ED.PCK` setinggi 721 piksel, lalu melanjutkan scrolling dan kembali ke menu utama.
- Perbaikan penyelesaian video Android agar decoder memproses frame dan audio yang masih tersisa sebelum melaporkan selesai, termasuk setelah pause mendekati akhir.
- `KotoRI_uta02.mpg` dan `dc4_mv001.mpg` dari D.C.4 / Plus Harmony telah diputar sampai selesai dengan audio pada pengujian Android.

### Validasi dan batasan

- Pemeriksaan ulang untuk 0.6.2: 22/22 suite CTest desktop, 317 pemeriksaan profil game Android, dan 253 pemeriksaan ekspor save lulus.
- Library APK release 0.6.2 juga lulus sembilan kasus replay, pemeriksaan spasi CP932, serta probe video dan audio di Android. Kedua APK lulus pemeriksaan versi, arsitektur, dan tanda tangan. Lihat [verifikasi rilis 0.6.2](docs/release-qa-0.6.2.md).
- Pengujian sprite, credits, ending, dan kompatibilitas sebelumnya tercatat di [laporan kompatibilitas](docs/dc2dm-dc4-compatibility-validation.md), beserta batas pengujiannya.
- Probe Android tersebut tidak mencakup seluruh siklus Activity atau seluruh rute permainan. Impor save native PC DC2DM belum didukung; Save/Load memakai format portable Arisa32. Pemetaan seluruh portrait backlog belum terverifikasi.
- Backend video desktop masih mendukung MPEG-1. Patch DC2DM berisi MP4/H.264 memerlukan backend FFmpeg Android.
- APK mendukung Android 5.0+ dengan `arm64-v8a` dan `armeabi-v7a`. File game dan patch terjemahan disediakan terpisah.

### Unduhan

- `Arisa32-v0.6.2.apk`: paket release untuk penggunaan biasa.
- `Arisa32-v0.6.2-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

## 0.6.1 — 2026-10-28

### Fitur baru dan peningkatan UI

- **Pemeriksaan pembaruan otomatis (Auto Check Update)**:
  - Arisa32 kini secara otomatis memeriksa ketersediaan rilis terbaru dari repositori GitHub saat aplikasi diluncurkan (apabila opsi aktif).
  - Notifikasi rilis baru disajikan melalui status bar Android (system notification), notifikasi toast, dan dialog interaktif lengkap dengan ringkasan catatan rilis (changelog) serta tombol langsung untuk mengunduh APK rilis terbaru.
  - Opsi pengaturan On / Off tersedia pada menu Pengaturan (Settings), lengkap dengan tombol untuk memeriksa pembaruan secara manual sewaktu-waktu.
- **FPS Meter dinamis di pojok frame layar**:
  - Penambahan penghitung frame rate real-time yang diperbarui setiap 500 ms dengan pembacaan delta render yang akurat dan stabil.
  - Posisi FPS meter mengikuti batas kotak frame aktif visual novel secara dinamis:
    - Pada game berasio 4:3 seperti D.C. II P.C. (`dc2pc`), indikator menempel tepat di pojok kanan atas bidang permainan ($X = 1120$), tidak lagi keluar ke area pilar hitam.
    - Pada game berasio 16:9 seperti D.C.4 dan D.C.4 Plus Harmony (`dc4` / `dc4ph`), indikator menempel di pojok kanan atas layar penuh ($X = 1280$).
    - Otomatis beradaptasi secara mulus saat pemutaran video / movie cutscene dengan rasio khusus dan saat perubahan mode windowed/fullscreen.
  - Tombol sakelar On / Off tersedia di menu Pengaturan aplikasi dan menu gelembung (in-game Extra menu).

### Backend Video FFmpeg & Optimasi Ukuran File APK

- **Migrasi backend pemutaran video dari LibVLC ke FFmpeg**:
  - Menggantikan ketergantungan LibVLC dengan build FFmpeg kustom yang ramping, dikonfigurasi khusus hanya untuk menyertakan komponen, demuxer, dan codec video/audio yang benar-benar dibutuhkan oleh engine visual novel (MPEG-1/2, H.264, WMV, AAC, MP3, Vorbis, WMA).
  - Mengurangi ukuran file APK (filesize) secara signifikan karena membuang modul-modul eksternal yang tidak diperlukan.
  - Integrasi rendering langsung ke pipeline grafis native Arisa32 dengan sinkronisasi frame audio-video yang presisi serta pemakaian memori yang lebih efisien.

### Pembatasan Save / Load pada Scene Replay & Scenario Mode (Akurasi Native PC)

- **Penonaktifan Save, Load, QuickSave, dan QuickLoad saat Replay/Scenario Mode**:
  - Mengikuti perilaku asli engine Circus di versi PC (native Windows), fitur persistence (Save, Load, QuickSave, QuickLoad) kini otomatis dinonaktifkan (`disabled`) saat user berada dalam mode Scene Replay atau Scenario Replay.
  - Tombol QuickSave/QuickLoad dan tombol aksi terkait di taskbar/sidebar disembunyikan atau dinonaktifkan agar tidak dapat dieksekusi, mencegah rusaknya save data atau state flag cerita saat memutar ulang adegan/scene.
  - Menolak operasi `save_state` dan `load_state` ketika flag replay terdeteksi aktif, menjaga integritas alur permainan persis seperti game aslinya di PC.

### Penyempurnaan Engine & Stabilitas

- Pemeliharaan stabilitas rendering kamera, Event CG, background, dan penempatan sprite karakter pada seluruh profile game yang didukung.
- Seluruh 18 suite unit test desktop (`ctest`) lulus 100%.

### Unduhan

- `Arisa32-v0.6.1.apk`: paket instalasi release untuk penggunaan biasa.
- `Arisa32-v0.6.1-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.

## 0.6.0 — 2026-10-26

### Dukungan D.C. II P.C. (Plus Communication)

- **D.C. II P.C. \~Da Capo II Plus Communication\~ resmi didukung.** Dukungan mencakup skrip dan teks, dialog/pilihan, gambar karakter dan ekspresi, musik/suara, title screen, sidebar, Save/Load, backlog, config, dan menu Extra/replay.
- Perbaikan navigasi menu Extra, scrolling replay, pemulihan snapshot lama, dan tombol Back/sidebar Android.
- Progres serta flag rute yang tersimpan dipulihkan tanpa mengubah flag nol menjadi unlock.

### Save dan pemulihan

- DC2PC, D.C.4, dan D.C.4 Plus Harmony menyimpan slot dalam format PC `.dat`, dengan pendamping `.dat.arisa` untuk keadaan tambahan Arisa32.
- Penomoran save mengikuti format setiap game. DC2PC memakai penomoran halaman asli dan mempertahankan `DC2PC01.dat` sebagai salinan Latest, terpisah dari slot reguler.
- Folder SaveData yang sudah tersedia digunakan kembali; save `.dc4sav` lama tetap menjadi fallback dan dimigrasikan tanpa menimpa slot yang sudah ada.
- Judul skenario dan thumbnail ditampilkan pada slot. Tabel judul bawaan mempercepat pembukaan menu Load pertama.
- Perbaikan posisi, skala, pivot, cropping, dan kamera karakter saat memuat save native D.C.4/Plus Harmony.
- Perbaikan suara pengumuman Save/Load yang sebelumnya dapat dimulai dua kali, serta waktu transisi saat aplikasi dijeda/dilanjutkan.

### Ekspor simpanan

- Popup ekspor menyediakan dua pilihan: **Dengan .dat.arisa** untuk backup lengkap Arisa32 dan **Tanpa .dat.arisa** untuk arsip yang lebih kecil.
- Tanpa pendamping, save `.dat` pada game yang didukung tetap bisa di-Load. Posisi audio dan sebagian keadaan tambahan Arisa32 tidak dipulihkan; musik dapat mulai ulang. Game PC mengabaikan `.dat.arisa`.
- Kedua pilihan tetap menyertakan simpanan lain, preview, pengaturan, unlock, dan riwayat baca jika tersedia.
- Popup proses menampilkan mode ekspor, nama berkas, persentase/jumlah berkas, dan konfirmasi selesai. Nama ZIP membedakan mode `with_arisa` dan `without_arisa`.

### Tampilan aplikasi

- Ikon default aplikasi memakai artwork karakter baru, dengan ikon adaptive dan ukuran untuk berbagai kepadatan layar Android.
- Ikon di kiri judul Arisa32 pada Game Library memakai ikon yang sama, dengan ukuran dan posisi sejajar dengan judul.

### Validasi dan catatan

- Validasi: **15/15 tes core C++, 288 pemeriksaan profil Android, dan 217 pemeriksaan ekspor ZIP lulus**. APK debug dan release berhasil dibangun. Sampel save native DC2PC/D.C.4/Plus Harmony juga telah dimuat dan dilanjutkan pada pengujian perangkat/PC.
- Beberapa keadaan animasi/renderer dan riwayat native masih memiliki batasan; hasil sampel tidak menjamin semua checkpoint. Pemulihan film setelah proses aplikasi berakhir masih belum didukung.
- APK mendukung Android 5.0+ (`arm64-v8a` dan `armeabi-v7a`). File game dan paket terjemahan disediakan terpisah.

### Unduhan

- `Arisa32-v0.6.0.apk`: paket instalasi release untuk penggunaan biasa.
- `Arisa32-v0.6.0-debug.apk`: paket debug untuk pengujian dan diagnosis.
- `SHA256SUMS.txt`: checksum kedua APK.
