[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.7.7 - Cross-language saves in Dal Segno, CG gallery controls, and faster game startup

The November 28, 2026 release fixes loading a Dal Segno save after switching between the official English script and the Indonesian patch. In D.C.4 and D.C.4 Plus Harmony, saves from the D.C. segment now show its scenario title. All six games tested also reached their first frame sooner after pressing **Launch**.

### Dal Segno MangaGamer · Saving and loading across languages

- At supported dialogue checkpoints, a save made with **OFFICIAL ENG** can be loaded with **ID PATCH**, and vice versa. Previously, a longer or shorter translation could shift the dialogue position in the MES file and cause Load to reject the checkpoint.
- When loading a slot, Arisa32 matches the saved position to the active script. The speaker’s name and dialogue appear in the selected language. You can save again in the second language and continue in the first.
- When Arisa32 can write to the game’s save folder, both languages share that folder. If Arisa32 uses internal app storage, each language has its own folder. To transfer a slot, use **Export saves** in the original language, switch to the other language, then use **Import saves**. Import replaces files with matching names in the destination folder.
- When a translator revises the wording and keeps the script commands in the same order, Load displays the revised line. If the patch omits an entire MES file, that scenario uses the official English data. Compatibility covers text edits at supported checkpoints. Patches that change story flow or choices need separate testing.

### Dal Segno MangaGamer · Long dialogue in the text box

- One- and two-line dialogue keeps its original position. When English or Indonesian dialogue wraps to three or more lines, the first line can move up to keep the last line inside the text box while leaving room for the speaker’s name.
- Arisa32 positions the complete line of dialogue before revealing the first character. Text stays in place as the third line appears. The same rule applies to scripted line breaks and automatic wrapping.

### CG gallery controls and D.C.4 slot titles

- When you open a CG from the gallery, the floating controls show one Back button. It returns you to the gallery. The profile’s **Back / Side Menu** or **Back / Upper Menu** control returns after you leave the CG viewer. Hiding and revealing the controls while viewing an image follows the same rule.
- On the Save and Load screens in **D.C.4** and **D.C.4 Plus Harmony**, slots from the D.C. segment show the Japanese scenario title **Ｄ．Ｃ．** for `DC4_DC99990101A`. If the Indonesian lookup table contains the duplicate `D.C. / D.C.`, the slot displays **D.C.** once.

### Game library and KiriKiri detection

- Automatic discovery and full-storage scans skip **D.C.4 Sweet Harmony** (`DC4_SH`) and **D.C.4 Fortunate Departures** (`DC4_FD`). Both use KiriKiri. When you select their folders manually or open an old library entry, Arisa32 explains that the game uses KiriKiri and the app supports ADV32.
- The scanner also checks the signature at the start of XP3 archives and looks for the `startup.tjs` script. In testing, it skipped a renamed KiriKiri folder even when it contained `MES/start.mes`, and found the ADV32 game in the neighboring folder.

### Android game startup times

Arisa32 reduces repeated MES directory checks, reuses the active game profile, and loads Backlog artwork the first time you open the Backlog. The D.C. II Plus Communication test installation had 1,560 MES entries. Its median launch time fell from **2.66 to 1.99 seconds**.

| Game | Before | After |
| --- | ---: | ---: |
| D.C. II Plus Communication | 2.66 s | 1.99 s |
| D.C.4 | 1.26 s | 1.03 s |
| D.C.4 Plus Harmony | 1.32 s | 0.98 s |
| D.C. II To You | 0.53 s | 0.45 s |
| D.C. II Dearest Marriage | 1.26 s | 1.11 s |
| Dal Segno (official English) | 1.15 s | 1.09 s |

### Test coverage

- Each figure is the median of six launches per game per build on an iQOO Z9x 5G. The older and newer builds took turns running first. Timing started in an open library and ended at the first frame, with the storage cache enabled. For **D.C. II To You**, the first frame is from the opening movie. [Measurement report in Indonesian](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.7/docs/startup-iqoo-2026-11-28.md)
- Dal Segno save tests covered English → Indonesian → English, a revision with longer text, and a scenario available only in English. Layout tests covered three-line dialogue while its characters were still appearing.
- Nine on-device checks covered the folder picker, Arisa32’s folder browser, and old library entries for both D.C.4 KiriKiri aliases and a renamed test folder. A mixed-folder scan also confirmed that the ADV32 game was found.

### Android downloads

Both APKs use **versionCode 25** and support ARM64 and ARMv7 on Android 5.0 or later (API 21+).

| File | Use |
| --- | --- |
| [Arisa32-v0.7.7.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.7/Arisa32-v0.7.7.apk) | Regular play and in-app updates |
| [Arisa32-v0.7.7-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.7/Arisa32-v0.7.7-debug.apk) | Debug build for testing and troubleshooting |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.7/SHA256SUMS.txt) | SHA-256 checksums for both APKs |

Install the release APK over your existing Arisa32 installation to keep your app data. [v0.7.7 release page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.7)
