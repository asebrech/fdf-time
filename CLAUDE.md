# CLAUDE.md

Pebble smartwatch watchface written in C against the Pebble SDK (2026 Core
Devices SDK, currently 4.33). Built with waf via `pebble-tool`, tested in the
QEMU emulator. Dev environment is managed by devenv/direnv on NixOS.

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

- `src/c/main.c` — the entire watchface: one `Window`, `TextLayer`s for time
  and date, `tick_timer_service_subscribe(MINUTE_UNIT, ...)` to refresh.
- `package.json` — Pebble manifest under the `"pebble"` key: UUID,
  `targetPlatforms`, `watchapp.watchface: true`, `resources.media` (fonts,
  images). Adding a resource means declaring it here, then referencing the
  generated `RESOURCE_ID_*` in C.
- `wscript` — standard Pebble waf template; rarely needs editing.
- `devenv.nix` — defines the `pebble-fhs` FHS wrapper (the SDK's downloaded
  ARM toolchain/QEMU are FHS binaries that can't run bare on NixOS) and the
  `pebble` / `pebble-setup` wrapper scripts.

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
