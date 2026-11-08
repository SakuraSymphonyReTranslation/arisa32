# In-app APK updates

Automatic checking still offers a release; downloading begins only after the
user chooses **Update now / Perbarui**. Manual checks use the same flow.
The update notification opens Arisa32. The optional **View Release** action
opens the release page, but downloading and installation do not use a browser
or external download manager.

`UpdateDownload` streams HTTPS into `cache/arisa-update.apk.part`, reports
received bytes and the known total, and rejects HTTP downgrades, excessive
redirects, incomplete files, files larger than 512 MiB, and incorrect GitHub
asset sizes or SHA-256 digests. A missing asset never falls back to downloading
the release's HTML page. When GitHub supplies no digest, package-signature
verification still applies.

The revised v0.7.1 updater uses the selected release asset's `url` as a fallback
when `browser_download_url` fails with a connection problem or a temporary HTTP
error. The fallback is restricted to the official repository's HTTPS asset API
endpoint and requests the binary with `Accept: application/octet-stream`.
It uses public API access without embedding a GitHub token. Retries are bounded,
and cancellation also stops the delay before another attempt. Each attempt
starts with a fresh partial file and applies the same size and checksum checks;
an integrity failure is not retried as a network error.

`AppUpdate` verifies the package name, release version name, strictly increasing
version code, and signing certificate before staging the APK in Android's
`PackageInstaller`. It supports verified certificate rotation on Android 9+
and exact signer matching on older devices. The cache file is deleted before
the installer session becomes ready. Cancellation or preparation failure
removes the partial download and abandons the installer session. No APK is
written to public Downloads or shared storage.

Android owns its staged installation data and the installed application itself.
On completion or cancellation, Arisa32 clears session bookkeeping; Android
removes the installation session. An interrupted download is cleaned on the
next process launch. Downloads survive Activity recreation while the process
is alive; they do not resume after process death. A completely staged APK can
continue after process death without downloading it again.

Android still requires its installation confirmation. Android 8+ may first
require **Allow installation from Arisa32** in system settings. Some phones
kill the application when this permission changes, so readiness and the session
ID are persisted before leaving Arisa32. The returning application commits the
recovered session and opens Android's confirmation. Installation can close
Arisa32 while its package is replaced. No silent-install or device-management
privileges are requested.

The **Install unknown apps** settings page may display Android's general warning
about allowing an application to install APKs. That wording belongs to Android;
it is not an Arisa32 scan result and cannot be removed by changing this app.
The settings page is only needed while permission is missing. With permission
already granted, subsequent updates proceed to the system confirmation.

Installer callbacks use an explicit receiver, a per-session random token, and
the Android session ID. The receiver is not exported. A stale or unrelated
callback cannot complete or cancel another update.

## Updating from older versions

Versions such as v0.6.6 and v0.6.7 can update directly to v0.7.1 without installing
intermediate releases. Their installed downloader still uses the direct GitHub
release URL. If that route is unreachable, a VPN or another working network may
be needed once to obtain the revised APK. The API fallback takes effect only
after that APK is installed, and still requires access to GitHub's servers.

This updater fix keeps the v0.7.1 version name. Clients that already report
v0.7.1 will not offer the revised APK because release discovery compares version
names. Those users can download the revised APK from the release page and
install it over the existing app without uninstalling it.

## Interface and verification

The dialog follows ArisaUi's existing dark surface, type hierarchy, and cyan
actions. Design read: ENERGY 1 / RHYTHM 1 / MOTION 1, with progress as the focal
point; motion is confined to conveying pending work. The numeric progress and
bar remain outside the scrollable status text, including at 200% text size.
Stage changes use a polite accessibility live region; frequent byte counts do
not trigger live announcements. Errors, retry, cancellation, permission and
installation states are translated into English, Indonesian and Japanese.

Text and accent contrast on `#131b26`, measured with the installed skill's
checker: `#e6edf5` 14.68:1; `#8899ac` 5.94:1; `#38bdf8` 8.08:1.

Validation uses the isolated `org.dc4engine.updateqa` application, never the
main application's games or saved preferences. The download transport is
injected with local fixture streams for repeatable device tests. Package
parsing, certificate checks, installer sessions, system confirmation and
package replacement use the real Android implementation. JVM tests separately
exercise HTTPS redirect policy and download failures. Device coverage was on
Android 16; minimum-SDK compatibility is compiled but older Android devices
were not physically tested.

Recorded results on 2026-11-02:

- 84 host assertions across the 15 download cases.
- 162 release-dialog assertions for English and Indonesian notes, scrolling,
  keyboard activation, usable actions, and in-app handling of a missing APK.
- 61 device assertions through staging, plus 8 for resuming after permission
  changes and cancelling in Android's actual confirmation dialog.
- A real installation from QA version code 10 to 11 succeeded. The installed
  package reported version 0.6.4; the private cache was empty and the persisted
  `app-update.xml` contained an empty map after replacement.

Evidence is stored in `build/updater-validation/` (ignored build output).

See `tests/android/README.md` for reproducible commands. The run above used
production version 0.6.4 / code 11. The isolated QA fixture keeps its baseline
at 0.6.3 / code 10 and its candidate at 0.6.4 / code 11, independently of
production. Updated APKs must increment the version code and match the release
tag. A rebuild under the same version name can be installed manually with a
higher version code, but only a newer version name triggers the update offer.
