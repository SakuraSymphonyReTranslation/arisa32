# Android Save/Load announcement restart

Investigated 2026-10-25 using the two supplied screen recordings and the
corresponding original voice archive entries. This is a host call-order bug.

`menu_action(Save/Load)` started the selected system voice before returning to
Java. `Arisa32Activity.openSlots()` then scanned slot metadata and thumbnails
through `refreshSlots()`, and `dc4SlotsOpen()` started the same voice again.
The second playback replaced the first on the voice channel. Switching between
Save and Load had the same duplicate start in `dc4SlotsTouch(Action::Mode)`.

The entry announcement now belongs to `dc4SlotsOpen()` after the slot scan.
Both entry paths return their action without starting the voice. Actor and
voice/master-volume selection still use `play_system_voice()` and the current
Config settings. Desktop already has a single entry announcement.

## Recording evidence

Audio was extracted as mono 22050 Hz WAV and compared with independently
decoded original `SYSTEM` assets. Ogg page version/checksum normalization was
needed for FFmpeg to decode the archive payload; this does not change audio.
Generated comparison assets are in `build/menu-voice/`.

| Recording | Actor / asset | Full asset duration | First start → restart |
|---|---|---:|---|
| `Screenrecording_20261025_141646.mp4` | Kanata / `SYSTEM_KAN013.PCM` | 1.566 s | 1.079 → 2.049 s; 7.429 → 8.299 s |
| `Screenrecording_20261025_141720.mp4` | Cheshi / `SYSTEM_CHE013.PCM` | 2.132 s | 1.120 → 2.240 s; 6.010 → 6.970 s; 9.670 → 10.480 s |

The repeated opening segments match the original assets (normalized
correlations approximately 0.962–0.998). Every second start above occurs
before the full utterance could finish. These are recording observations
from the old build, separate from verification of the changed build.

## Changed-build device sample

The installed build was checked on an I2219 running Android 16. Playback audio
capture recorded one DC4 Load entry with Kanata selected. Matching finds one
opening at 10.467 seconds and no second opening above the comparison threshold.
See `device-qa-20261025.md` for the recording, matching limits and other device
checks. This sample does not cover every actor or Save/Load mode transition.
