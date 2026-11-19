[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.7.4 - D.C.4 Progress Fixes and Experimental Dal Segno

This 19 November 2026 release fixes premature access to D.C. in D.C.4 and D.C.4 Plus Harmony. Arisa32 now validates progress and handles legacy profiles automatically. It also adds an early compatibility milestone for the MangaGamer edition of D.S. -Dal Segno- with its official English translation.

### D.C.4 and D.C.4 Plus Harmony progress

- The D.C. menu opens only after all eight required endings are confirmed and the game script awards its unlock flag. Flags copied from a save slot no longer count as proof of completion.
- Loading an earlier slot, including a prologue slot, preserves valid global progress. The story position in one slot does not determine whether the player has previously completed other routes.
- Arisa32 analyzes each legacy profile using the available history, preserves progress it can confirm, and saves the migrated profile automatically. Players no longer need to bring `system-profile.dat` to Codex for manual flag repair.
- Before replacing a legacy profile, Arisa32 creates `system-profile.dat.v1-backup`. Story slots, save companions, gallery/music/scene history, and settings are preserved.
- Older Arisa32 saves with legacy snapshot fingerprints can be loaded again.

Profiles affected by the old unlock bug may lack evidence for some endings. Arisa32 cannot recover information that was never recorded; unconfirmed endings need to be completed again before they count toward D.C. access. When sharing new saves, include the version-2 global profile so confirmed progress is carried with them.

The fix covers both D.C.4 editions. See [source change c3eb9b6](https://github.com/SakuraSymphonyReTranslation/arisa32/commit/c3eb9b625ced963c11d870173e6f7bb3d0b0b061), the [progress report](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/dc4-progression.md), and [legacy save compatibility](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/dc4ph-legacy-save-compatibility.md).

### MangaGamer Dal Segno: experimental

This milestone adds game detection and Dal Segno script handling, fixes startup into the title menu and advancement after script transfers, and adjusts the textbox and toolbar touch area. CONFIG sliders can be dragged, END returns to the library, and fresh profiles follow the script's unlock flag. Audio mapping also fixes voice/SE stop commands being interpreted as BGM indices.

**Dal Segno remains experimental and is not fully playable.** User testing reports missing character sprites, slow button responses, an unavailable EXTRA menu, and several buttons and menu functions that do not work yet. This is an early compatibility milestone for the MangaGamer edition with its official English translation.

Title tests check locked/unlocked behavior using the script flag. They do not establish full route completion, unlock awards at every ending, automatic Dal Segno profile repair, or complete native save read/write compatibility. See the [Dal Segno report and test limits](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/dalsegno-validation.md).

### Validation

Relevant desktop tests passed for opcodes, VM execution, save restoration, scenes, audio, and progress in both D.C.4 editions. Dal Segno checks cover six/seven title actions, 11 toolbar targets, and 11 sliders across five CONFIG tabs. The D.C.4 Plus Harmony progress fix was also checked on a phone, including profile migration, restart, and story-slot preservation. This does not cover complete playthroughs of all routes.

Universal release and debug APKs for v0.7.4 use versionCode 22. Signature, Android version, ABI, alignment, package-content, and updater checks are documented in the [v0.7.4 QA report](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.4/docs/release-qa-0.7.4.md).

### Downloads

Both APKs support ARM64 and ARMv7 and require Android 5.0 or later (API 21+).

| File | Purpose |
| --- | --- |
| [Arisa32-v0.7.4.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.4/Arisa32-v0.7.4.apk) | Release APK for normal use; selected by the in-app updater. |
| [Arisa32-v0.7.4-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.4/Arisa32-v0.7.4-debug.apk) | Debug APK for investigating problems. |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.4/SHA256SUMS.txt) | Checksums for both APKs. |

Install the update over the existing installation to preserve application data. Game files and patches are supplied separately. All downloads are available on the [v0.7.4 release page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.4).
