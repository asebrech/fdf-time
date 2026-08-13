# CLAUDE.md

"FdF Time" — a Pebble watchface rendering the time as a 3D wireframe
heightmap in isometric projection, in the style of École 42's FdF project.
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

There are no unit tests; verification = `pebble build` succeeds + visual check
via emulator screenshot.

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

- Antialiasing is OFF on purpose: at ~4 px/cell it smears 1 px lines into
  noise.
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
- Strings passed to `text_layer_set_text` must be `static` (the layer does
  not copy them).

## Claude Code skill

`.claude/skills/pebble-watchface/` vendors Core Devices' official watchface
skill (templates, API reference, drawing/animation guides, QEMU workflow).
Use it when designing or substantially reworking a watchface.
