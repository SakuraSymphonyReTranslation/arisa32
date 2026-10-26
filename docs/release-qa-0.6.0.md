# Release 0.6.0 validation

Validated on 2026-10-26.

- Android Gradle `:app:assembleDebug :app:assembleRelease` succeeded for version
  name `0.6.0`, version code `7`, Android API 21+ and both ARM ABIs.
- Desktop build is current. All 15 CTest targets passed.
- Java profile checks passed 288 assertions.
- Standalone ZIP export checks passed 217 assertions, covering companion
  inclusion/omission, mixed-case names, exact retained bytes, source preservation,
  fresh-directory import round trips, progress and destination/empty failures.
- The updated debug app was installed on an iQOO I2219 running Android 16.
  The supplied artwork appeared in the library header at the existing 40 dp
  size, vertically aligned with the wordmark. The Indonesian export dialog
  displayed both radio choices and descriptions, with full backup selected by
  default. Choosing Export opened the document picker with `with_arisa` in the
  suggested archive filename; the picker was cancelled without writing a save
  archive. Long explanatory text remains scrollable in landscape.
- Local screenshots are in ignored `build/export-qa/`. The archive checks do
  not establish full VM restoration; native scene/device/PC samples and known
  limitations are documented in `device-qa-20261025.md` and `opcode-profiles.md`.
