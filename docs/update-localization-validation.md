# Update descriptions and bilingual documentation

Verified on 30 October 2026. The application changes are for the next APK update;
the existing v0.6.2 release assets were not replaced.

## Behavior

- The update dialog retains the full release description in a vertical scroll
  view. Its actions stay outside the scrolling content.
- Release titles and descriptions follow the app language: Indonesian uses
  `id.md`, English uses `en.md`, and Japanese uses the English notes. The game's
  Japanese default locale does not choose the release language.
- A missing translation shows a localized explanation while keeping the update
  actions available. Empty notes also have an explicit message.
- README and all eight existing release descriptions have reciprocal Indonesian
  and English links. Each page shows one language. GitHub opens the selected
  document through an ordinary link in the same tab.

## Validation

- PASS: `ReleaseNotesChecks` completed 99 checks, including safe tag URLs, both
  selectors, code/path preservation, locale independence, and the complete final
  line of all 16 localized release documents.
- PASS: `UpdateDialogChecks` completed 158 checks on the I2219 Android device,
  using the isolated `org.dc4engine.updateqa` package. The four cases were English
  and Indonesian at 540dp/100% text and 380dp/200% text.
- PASS: every device case accepted a touch scroll, reached the last line, kept
  all three actions fully visible with at least 44dp height, dismissed with Back,
  traversed all actions with Tab, and activated Later with Enter and Space.
- PASS: View Release and Download APK each dispatched an HTTPS view intent in
  every case. The harness intercepted these intents to avoid downloads or
  launching another application.
- PASS: six device network checks fetched both published v0.6.2 translations with
  the application's HTTP reader, matched their complete parsed title/body to the
  fixtures, and confirmed that a missing translation raises the handled HTTP
  error.
- PASS: debug and release APK builds completed with the repository's AGP 8.5.2.
  A temporary init script selected that version without overwriting the separate
  local AGP 8.13.2 edit. Device instrumentation was built with AGP 8.13.2.
- PASS: all 19 published Markdown files returned HTTP 200 with content matching
  the local documents. All eight GitHub release bodies matched their prepared
  notes files. IDs, tags, titles, publication dates, status, and asset metadata
  remained unchanged.
- PASS: 44 local links and six heading anchors resolved; `git diff --check`
  reported no whitespace errors. Example links inside code are not navigation.

The evidence is saved locally under `build/update-localization/`: device and
network logs, build logs, and eight screenshots. Browser click verification was
unavailable because the browser bridge could not initialize; GitHub navigation
was checked through Markdown targets and live HTTP/API responses.

## Scoped antislop delivery gate

The source screenshot and existing Arisa32 dialog establish the visual direction.
ENERGY 1 / RHYTHM 1 / MOTION 1: a reading dialog with the existing dark panel,
system sans-serif text, cyan actions, and normal platform scroll feedback.

- R-02 / R-15 / R-16 PASS: new prose uses direct release descriptions and concrete
  action labels; it adds no decorative emoji, marketing claims, or em dashes.
- R-03 / R-35 PASS: the four device cases and inspected screenshots retain the
  complete scrollable description and unclipped actions at both text sizes.
- R-17 / R-18 / R-36 / R-38 PASS: translations retain historical facts, asset names,
  DC2DM beta limitations, and the original verification references.
- R-23 / R-24 PASS: no new visual assets were generated; each language selector
  targets a published document.
- R-25 PASS: computed text contrast on the dialog panel is 5.94:1 for body text,
  14.68:1 for headings, and 8.08:1 for cyan actions. Body text also passes at
  4.93:1 against the lighter scroll focus fill used for the additional check.
- R-26 / R-32 PASS: recorded touch, Back, Tab, Enter/Space, and URL-intent checks
  cover the dialog's controls; the existing platform focus behavior is retained.
- R-27 PASS: the manual check announces loading, network failures report an error,
  unavailable translations have a localized fallback, and empty notes have a
  tested message.
- R-33 / R-34 PASS: changes are in the Java sources; the existing app theme is used.
- Purpose and identity PASS: the badge identifies the version, title/body sizes
  separate release information, cyan identifies actions, and the bounded scroll
  area reserves room for the actions. No unrelated visual patterns were added.
- Craftsmanship PASS: build, parser, device interaction, and published-content
  checks support the scoped behavior above. These checks do not establish full
  game-route coverage or physical keyboard behavior on every Android version.
