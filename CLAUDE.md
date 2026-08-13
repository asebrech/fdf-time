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
- Zombie emulators cause `Connection refused`: clean with
  `pebble-fhs bash -c 'pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; rm -f /tmp/pb-emulator.json'`
  (the state file lives in the FHS env's private /tmp).

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

- `src/c/fdf.c` / `fdf.h` — the core: a heightmap (`FdfModel`, inner 16×25
  digit region + bleed ring) with `z_from`/`z_to` altitude grids and a
  `morph` progress for animation; integer-only trimetric pipeline (center →
  z-rotate via `sin_lookup`/`cos_lookup` → project along two independent
  1024-scale axis vectors chosen at init to fill the screen);
  wireframe drawn right+bottom-neighbor with a visual
  hierarchy (plateau-top edges bright/bold, walls and base mesh recede —
  different strategy per PBL_COLOR vs 1-bit, see `fdf_draw`).
- `src/c/digits.h` — 3×5 bitmap digit font, scaled ×2 when composed so
  strokes are 2-cell plateaus like the original 42.fdf map.
- `src/c/main.c` — lifecycle: splash shows "42" then morphs to the time;
  `MINUTE_UNIT` tick triggers a morph `Animation`; `accel_tap_service`
  triggers a full-orbit spin `Animation` (progress maps 1:1 to
  `TRIG_MAX_ANGLE`).
- `package.json` — Pebble manifest under the `"pebble"` key: UUID,
  `targetPlatforms`, `watchapp.watchface: true`, `resources.media`.
- `wscript` — standard Pebble waf template; rarely needs editing.
- `devenv.nix` — defines the `pebble-fhs` FHS wrapper (the SDK's downloaded
  ARM toolchain/QEMU are FHS binaries that can't run bare on NixOS) and the
  `pebble` / `pebble-setup` wrapper scripts.

## Rendering lessons already learned (don't regress these)

- Antialiasing is ON at the current ~6 px/cell density and looks good; it
  was OFF at the earlier ~4 px/cell density where it smeared 1 px lines into
  noise. If the grid ever gets denser, revisit.
- Edge styling keys off the LOWER endpoint altitude: that is what keeps
  digit holes/counters readable (walls recede instead of filling them).
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
round, sizes from 144x168 to 200x228); use `PBL_IF_ROUND_ELSE`,
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
