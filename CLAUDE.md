# CLAUDE.md

"FdF Time" — a Pebble watchface rendering the time as a 3D wireframe
heightmap in a trimetric projection, in the style of École 42's FdF project.
Written in C against the Pebble SDK (2026 Core Devices SDK, currently 4.33).
Built with waf via `pebble-tool`, tested in the QEMU emulator. Dev
environment is managed by devenv/direnv on NixOS.

## Commands

All commands must run inside the devenv shell (direnv loads it automatically;
otherwise prefix with `devenv shell --`).

- `pebble-setup` — one-shot bootstrap: installs pebble-tool + SDK core +
  STPyV8 ICU seed. Only needed once per machine.
- `pebble build` — compile for all `targetPlatforms`, produces `build/*.pbw`.
- `pebble install --emulator basalt` — boot QEMU emulator and install.
- `pebble screenshot --emulator basalt out.png` — capture the screen.
- `pebble logs --emulator basalt` — tail `APP_LOG` output.
- `pebble kill` — stop all emulators.
- `pebble clean` — wipe the build directory.
- `pebble install --phone` / `pebble screenshot <out.png> --phone` — real
  watch via Dev Connect (requires `pebble login`; note: positional args go
  BEFORE the bare `--phone` flag or it swallows them as a phone IP).

There are no unit tests; verification = `pebble build` succeeds + visual check
via emulator screenshot.

## Store visual pipeline (tools/)

Run with the pebble-tool venv python inside the FHS env
(`pebble-fhs .devenv/state/uv/tools/pebble-tool/bin/python tools/<script>`):

- `story_capture.py` / `story_cut.py` — per platform, records a scripted
  session (reinstall replays the 42 splash, emu-tap plays the orbit) via QEMU
  monitor screendumps at 15 fps, then cuts `_boot.gif` and `_orbit.gif`. The
  cut auto-skips the bright OS loading screen.
- `rollover_capture.py` / `rollover_cut.py` — same, around the real
  wall-clock minute boundary (SiFli boards ignore time injection) for
  `_rollover.gif`.
- `upload_release.py <project> <version> <notes> <gifs_dir>` — posts a
  release with `replaceScreenshots=true`; asset order per platform: boot,
  rollover, orbit GIFs, then `_steady.png`.
- ALWAYS verify every GIF with a contact sheet before uploading
  (`ffmpeg -i x.gif -vf "select=not(mod(n\,5)),scale=60:-1,tile=13x1"`).
- `cdp.py` — minimal Chrome DevTools Protocol driver used to operate store
  web UIs when no API exists (Rebble dev-portal, rePebble dashboard). Launch
  Brave detached with `setsid nohup brave --remote-debugging-port=9222
  --remote-allow-origins='*' --user-data-dir=<scratch>/brave-profile <url> &`,
  let the user log in, then drive: `cdp.py screenshot/navigate/eval-file/
  upload/click`. Gotchas: pass JS via eval-file (shell quoting mangles inline
  JS); some portal buttons are CSS-rendered and invisible to DOM text search —
  use `click <x> <y> <screenshot_width>` with screenshot-pixel coords; the
  final publish-type click belongs to the user (Claude Code's classifier
  rightly blocks it). Wipe the profile dir when done (it holds a session).
- Zombie emulators cause `Connection refused`: clean with
  `pebble-fhs bash -c "pkill -9 -f 'qemu-[p]ebble'; pkill -9 -f 'py[p]kjs'; rm -f /tmp/pb-emulator.json"`
  (the state file lives in the FHS env's private /tmp; the `[p]` bracket
  keeps pkill from matching — and killing — its own enclosing shell).
- rollover_capture.py sends an explicit SetUTC after install: without it,
  aplite's QEMU sometimes never gets phone time sync and sits at 00:00,
  producing a static "rollover". Verify every steady PNG is exactly the
  platform's resolution before upload — one basalt screendump came out
  148x172 (2px border) and Rebble rejects wrong dimensions.
- Rebble dev-portal listing mechanics: 5 screenshots max / 1 min per
  platform, ONE file per upload (multi-file selections silently take the
  first), per-slot hidden file inputs — map them via the ORDERED
  tabpanels under the Screenshots panel, never by find's labels. Banner
  lives per-platform under the Banners tab. Recipe to replace a full set:
  delete 4 of 5 old, upload the 4 new one-by-one, delete the last old.
- rePebble (post-Aug-2026 migration): the release API applies
  screenshots ASYNCHRONOUSLY (they appeared hours later, after an
  outage) and silently drops files with wrong dimensions. Listing
  management (screenshots per platform, per-platform banners, app
  icons) lives in the "Pebble Developer Dashboard"
  (developer.repebble.com/dashboard, user's session) — its platform
  TABS only switch via real coordinate clicks (ref-clicks silently
  no-op, leaving you editing the wrong platform: verify the active tab
  in a screenshot before touching anything). Its own state API is
  GET developer.repebble.com/api/dashboard/apps/<id> — the ground truth
  for verifying edits landed.

## Versioning policy (agreed with the user, 2026-08-13)

- GitHub tags are the source of truth: bump package.json + tag vX.Y.Z ONLY
  for watch-code changes, and push the same number to BOTH stores (rePebble
  release API + Rebble dev-portal release).
- Store listing changes (screenshots, GIFs, banner, description) must NOT
  bump the version: use the store web UIs (rePebble dashboard, Rebble
  dev-portal edit) — drivable via Brave + tools-style CDP if needed.
- ALWAYS `rm -rf build && pebble build` after a version bump: waf does not
  regenerate the appinfo versionLabel from package.json alone (1.0.5/1.0.6
  shipped mislabeled 1.0.4 pbws this way).
- History note: store versions 1.0.5/1.0.6 were listing-only pushes from
  before this policy; everything converged at v1.0.7.

## Architecture

- `src/c/fdf.c` / `fdf.h` — the core: a heightmap (`FdfModel`, inner digit
  region + bleed ring; the MM pair is staggered `FDF_STAGGER` cells right of
  HH to cancel the trimetric lean) with `z_from`/`z_to` altitude grids and a
  `morph` progress for animation; integer-only trimetric pipeline (center →
  z-rotate via `sin_lookup`/`cos_lookup` → project along two independent
  1024-scale axis vectors chosen at init; zoom is fitted to the DIGIT
  bounding-box corners only — border and bleed ring intentionally clip past
  the screen edges); wireframe drawn right+bottom-neighbor with a visual
  hierarchy (plateau-top edges bright/bold, walls and base mesh recede —
  different strategy per PBL_COLOR vs 1-bit, see `fdf_draw`). On color, six
  user-selectable 11-step altitude palettes — hand-quantized ports of
  Tokyo Night (default — closest heir to the original FdF blues, listed
  first) then alphabetically Catppuccin Mocha / Dracula / Gruvbox /
  Kanagawa / Nord, each with a themed foreground color for the plateau
  tops. Theme indices are PERSISTED on watches — never reorder after a
  store release.
  Pebble is 4-levels-per-channel; ports pick the nearest VIVID hue, never
  naive rounding (pastels round to washed-out gray), with each theme's
  signature accent on the wall tips (index 4). Placement rules: theme
  identity lives in the LARGE areas — floor 0-1 must be a "visible dark"
  in the scheme's dominant hue (55-level channels; near-black floors made
  every theme look identical), wall body 2-3 and crests 5-6 take the
  scheme's saturated colors; pastels (#AAAAFF-class) go in the morph
  sweep (7-9, most vivid at 9) only. Tops stay at stroke width 1 like
  everything else — width-2 tops were tried (digit/wall separation) and
  rejected by the user as too heavy. Top threshold is 9.5 (not 9.0, which
  cut the sweep before its final color; not 10.0 — the morph
  interpolation never quite reaches 10<<8). Wall gradients are a user
  toggle (default on); gradient segments clamp endpoint indices to
  GRAD_CAP BEFORE splitting so the blend spans the full line (clamping
  per-segment left half of every tall wall a flat accent block). Plus a rolling terrain
  swell in the bleed ring,
  recomputed each frame from `wave_phase` (fractional heights, capped at
  z=6 so digits stay the foreground).
- `src/c/digits.h` — 3×5 bitmap digit font, scaled ×2 when composed so
  strokes are 2-cell plateaus like the original 42.fdf map.
- `src/c/main.c` — lifecycle: splash shows "42" then morphs to the time;
  minute boundaries trigger a morph `Animation`; `accel_tap_service`
  triggers a full-orbit spin `Animation` (progress maps 1:1 to
  `TRIG_MAX_ANGLE`). Color platforms tick `SECOND_UNIT` to advance
  `wave_phase` (one swell wavelength per 30 s); 1-bit stays on
  `MINUTE_UNIT` — no terrain there, and second-ticking wastes battery.
- `src/pkjs/` — phone-side JS: `config.js` is the Clay settings page
  (theme, wave mode, gradient on/off, splash, shake orbit, BT vibe),
  `index.js` just instantiates Clay. Settings arrive in `main.c` via
  AppMessage (`prv_inbox_received` — Clay sends select values as STRINGS,
  toggles as ints; `prv_tuple_int` handles both), persist under
  `SETTINGS_KEY`, and apply live via `prv_apply_settings` (re-subscribes
  tick/tap/connection services; safe to call repeatedly).
- Headless settings test: run `pebble emu-app-config --emulator basalt &`,
  find the pebble process's listening port (`ss -tlnp`), then
  `curl "http://localhost:<port>/close?<urlencoded settings JSON>"` — this
  is exactly what Clay's Save button does.
- `package.json` — Pebble manifest under the `"pebble"` key: UUID,
  `targetPlatforms`, `watchapp.watchface: true`, `resources.media`, plus
  `capabilities: ["configurable"]` and the Clay `messageKeys`. After
  editing `messageKeys`, `rm -rf build` — waf won't regenerate
  `MESSAGE_KEY_*` constants otherwise (same staleness as versionLabel).
- `wscript` — standard Pebble waf template; rarely needs editing.
- `devenv.nix` — defines the `pebble-fhs` FHS wrapper (the SDK's downloaded
  ARM toolchain/QEMU are FHS binaries that can't run bare on NixOS) and the
  `pebble` / `pebble-setup` wrapper scripts.

## Rendering lessons already learned (don't regress these)

- Antialiasing is ON at the current ~6 px/cell density and looks good; it
  was OFF at the earlier ~4 px/cell density where it smeared 1 px lines into
  noise. If the grid ever gets denser, revisit.
- Color edges use FdF-style per-vertex interpolation: mixed-altitude edges
  are split into up to 4 segments blending between the endpoints' palette
  colors, CAPPED at VividCerulean (index 4). The cap is what protects
  legibility: full-ramp gradients (and even a Malachite cap) put bright
  segments against the white rims, reading as fuzzy doubled strokes.
  Uniformly dark walls (the pre-gradient state) hid the 3D entirely. Warm
  ramp colors are reserved for the morph sweep (equal-endpoint edges). The crest-vs-wall decision must be STRUCTURAL (both endpoints
  in the bleed ring = terrain), never altitude-based: an altitude threshold
  made rising digits' walls crest-glow then snap dark mid-climb — a
  screen-wide flash at every minute change.
- Cells morphing DOWN take their destination color immediately (`s_ck8`):
  old digits melt away in base blues. Coloring them by current altitude
  made the whole old time flash back through yellow/green at morph start
  (ease-out makes the first 100 ms cover most of the fall).
- `wave_phase` must advance continuously from window load (ticks subscribed
  during the splash, phase seeded from `tm_sec`); subscribing only after
  the splash froze the swell and then teleported it at first tick.
- 1-cell-thick digit strokes (sharp ridges) were tried and are NOT legible;
  2-cell plateaus are the minimum.
- On 1-bit displays, walls are dropped entirely, the base mesh is halved in
  density, and tops drawn at stroke width 3 — thin walls refill the digit
  holes.
- Classic 30° iso letterboxes badly on a portrait screen (~25% of pixels
  used); a single aspect-matched camera angle fills it but tilts the
  reading baseline as steeply as the angle itself. The fix is trimetric:
  digit-row axis at a gentle 22°, stack axis steep and auto-picked, plus a
  bleed ring of terrain overflowing the screen edges.

## Platform handling

`targetPlatforms` covers all 7 platforms. Screen geometries differ (rect vs
round, sizes from 144x168 to 260x260); note flint (Pebble 2 Duo) is
`PBL_BW` per the SDK platform defines — the vendored skill's table
wrongly lists it as 64-color. Use `PBL_IF_ROUND_ELSE`,
`PBL_IF_COLOR_ELSE` and `layer_get_bounds()`-relative layout rather than
hardcoded coordinates. Test at minimum on `basalt` (color rect), `chalk`
(round) and `emery` (large).

## Constraints & gotchas

- The FHS wrapper unsets `CC`/`AR`/`LD`/... — devenv exports host toolchain
  vars and waf's `find_program` gives OS-env `CC` priority over the SDK's
  `arm-none-eabi-gcc`, silently miscompiling. Never re-export these into
  pebble commands.
- Watchface C code runs on a microcontroller: small heap (~64–128 KB
  depending on platform), no floats in hot paths on aplite, always destroy
  what you create in `window_unload`/`deinit`.
- `clock_is_24h_style()` must be respected for time formatting.

## Claude Code skill

`.claude/skills/pebble-watchface/` vendors Core Devices' official watchface
skill (templates, API reference, drawing/animation guides, QEMU workflow).
Use it when designing or substantially reworking a watchface.
