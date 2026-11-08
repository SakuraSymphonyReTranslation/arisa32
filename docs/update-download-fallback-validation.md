# v0.7.1 updater rebuild verification

Verified on 8 November 2026. The rebuilt APKs retain version name `0.7.1` and
package `org.dc4engine`, with version code `19` instead of `18`.

## Download checks

- 249 assertions pass across 48 downloader scenarios, including HTTP 504 to
  API fallback, API redirects, bounded retries, cancellation during transfer
  and retry delay, partial-file cleanup, and checksum/size failures.
- 143 asset-selection checks and 171 release-note parser checks pass.
- The downloader also compiles with Java 8 language/API compatibility.
- A live, unauthenticated request first used the direct GitHub release URL,
  then the official asset API and GitHub's asset CDN. It downloaded and
  verified the existing 16,440,457-byte APK in 17.4 seconds on the connection
  where the legacy direct request returned HTTP 504. No VPN was enabled by
  the test.

The fallback uses public GitHub API access. No access token is packaged.
Network or temporary server errors can switch routes; integrity failures
remain fatal. Every successful transport still goes through the existing
Android package, version and signing-certificate checks before installation.

## APKs

| File | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.7.1.apk | 16,441,699 | `b08a02a75052c852709cbd04ce88502985fb6b6c2ae4ffff928ac6e95801c352` |
| Arisa32-v0.7.1-debug.apk | 17,430,017 | `b18edf67cee4bdca7d12a4d6673fe46123303864dacf7de8bfb4357b85397c85` |

Both are universal ARM64/ARMv7 APKs for API 21+, targeting API 36. They pass
ZIP integrity, v1/v2 signature verification, and `zipalign -c -P 16 -v 4`.
The signing certificate matches the official v0.6.6, v0.6.7 and original
v0.7.1 APKs:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

All 22 native-library and asset entries in each APK match the corresponding
original v0.7.1 package byte for byte. The release grows by 1,242 bytes and
debug by 2,149 bytes. Only debug is debuggable; neither contains QA classes
or game saves. The APKs retain the existing fonts without `default.ttf`.

Builds use the tracked AGP 8.5.2 through the existing local init script,
Android Studio's JDK, and the Gradle wrapper. The unrelated local AGP edit
is excluded. Evidence is in `build/release-0.7.1-updater/`.

## Scope

This verification used host tests, a live download, and APK inspection.
No phone installation or gameplay test was performed for this rebuild.
Native libraries and game assets did not change.

Older installed downloaders still need a working direct GitHub connection,
possibly through the user's VPN, to obtain this fix. Already-installed
v0.7.1 clients do not offer a same-name update; install the revised APK over
the existing app manually. Subsequent newer-version downloads use the new
fallback, provided GitHub's API and asset CDN are reachable.

The published v0.7.1 tag is retained. The release description identifies the
source commit used for this updater rebuild.
