# DC2PC gallery JNI probe

This app_process harness runs the APK's ARM64 JNI library with the production `ScreenViewport.java`. It checks route IDs0 through6, the bottom-to-top pan on ID5, clicks during motion, the final WAIT, Back, and Fit/Stretch input mapping. Synthetic captures show the initial, middle and final crops. The original route emits numeric source comparisons only; it never exports original artwork as an image.

Run from the repository root:

```powershell
python tests/android/run_dc2pc_gallery_probe.py --apk <APK> --serial <DEVICE> --data /storage/emulated/0/DC2PC/AdvData --expect fixed
```

`--expect baseline` records expected old-build differences and continues. The frozen baseline for this change is `build/dc2dm-pretendard-layout-20261108/Arisa32-v0.7.0-dc2dm-pretendard-debug.apk`.

`--prepare-only` compiles Java/DEX and prepares local fixtures without running adb. `--kinds synthetic` runs only generated patterns. Surface size defaults to2400×1080; use `--width` and `--height` to match the device. `--output` must name a new directory. The default evidence directory is under `build/dc2pc-cgmode-20261108/device`.

The runner creates a unique `/data/local/tmp/arisa32-dc2pc-gallery-*` tree. Original fixtures link only GRP, MES and DAT, and the harness writes its own profile into an isolated saves directory. It does not install an APK or access the player's SaveData. Local CRX sources are decoded in memory to192 sampled pixel values per crop for numeric comparison. SHA256 values identify source assets and packaged libraries. Synthetic performance cases time up to20 changing pan frames at1920×1080 and2416×1359 with17ms spacing; the output reports mean, median,95th percentile and maximum render time.

This harness checks JNI rendering and viewport math. It does not replace an activity-level check of navigation buttons, surface lifecycle or accessibility.
