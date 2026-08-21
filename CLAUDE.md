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
- Store web UIs: PREFER the Claude-in-Chrome browser tools when available
  (the user's own Chrome, already logged in to both portals — used
  successfully 2026-08-15 for the Rebble release + both store descriptions;
  form_input + file_upload by element ref, no coordinate fragility). The
  release-publish click still belongs to the user on Rebble; rePebble
  code-only releases can skip the UI entirely via the release API with
  just pbwFile+version+notes (NO replaceScreenshots — listing stays).
- `cdp.py` — fallback: minimal Chrome DevTools Protocol driver to operate
  store web UIs when Claude-in-Chrome isn't connected (Rebble dev-portal,
  rePebble dashboard). Launch
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

## Versioning policy (agreed with the user, 2026-08-13; loosened 2026-08-21)

- COMMITS ARE FREE AND DECOUPLED FROM VERSIONS (user's call, 2026-08-21:
  "c'est un peu chiant qu'à chaque fois qu'on veut commit on fait une poussée
  de version"). Commit whenever work is worth recording — mid-feature,
  unvalidated, debug logs still in — with a plain descriptive subject and NO
  version prefix. Several commits per release is the normal shape. Do not
  touch package.json's version, do not tag, and do not push to any store
  just because something was committed.
- A RELEASE is a separate, deliberate act, and only then: bump
  package.json + tag vX.Y.Z, and push that same number to BOTH stores
  (rePebble release API + Rebble dev-portal release). Only watch-code
  changes earn a release. The user says when.
- Release commits keep the historical "vX.Y.Z: summary" subject form; that
  prefix is what marks a commit AS the release.
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
  Tokyo Night (index 0, the original default), Catppuccin Mocha (index 1,
  the default since 2026-08-17), Dracula, Gruvbox, Kanagawa, Nord, each with
  a themed foreground color for the plateau tops. Theme indices are
  PERSISTED on watches — the C array can never be reordered after a store
  release. The Clay select lists them ALPHABETICALLY (Catppuccin, Dracula,
  Gruvbox, Kanagawa, Nord, Tokyo Night) with those same frozen values:
  display order and storage order are deliberately different.
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
  Battery scene (`fdf_model_set_battery`, 2026-08-17), all terrain, NO
  system-font overlay — the number is FdF like the time. Inner-coord layout
  (user's order, 2026-08-17): rows 3-10 a drawn battery at inner col 0 (case
  15x8, 2-cell frame, a 2-cell terminal nub on the middle rows, and 11 charge
  cells filling the case's WHOLE inner height — a thin bar floating inside
  with gaps reads as a lonely stroke in an empty box), rows 14-23 the two
  percentage digits under it, in the MM slot. That is the longest case that
  fits at that height: the band leans, so the higher the case the sooner it is
  cut on the right, and its top-right corner already lands 10 px from the edge
  on basalt (verified by replaying the projection, not by eye — the lean makes
  everything look clipped in a screenshot). One more column puts it at 4 px.
  The 1-cell nub was widened to 2 because at 100% the case is entirely solid
  and the nub is the only thing left saying "battery".
  EVERYTHING STANDS AT ONE ALTITUDE, the level's palette index: the digits,
  the case, the nub and the charge. Giving the charge its own (lower) colour
  was tried and turns the pictogram to mush — it puts a wall around the bar
  INSIDE the case, and at ~6 px per cell that doubles the line count in a
  small area. One altitude = one outline against the floor, as clean as the
  digits. `prv_place_glyph` / `prv_place_pair` take a `top` parameter for
  exactly this. Level → index runs 6 (full) to 9 (nearly empty), staying in
  the ramp's vivid half (the low indices are recessive darks by design — an
  early "track" drawn there was invisible), which reads green → yellow →
  orange → hot on most themes. At 100% there is no number (three digits never
  fit a pair box, and "10"/"00" would misread as 10%/0%): the full battery
  stands centred, alone. While charging, the 10% currently filling lights in
  the theme's foreground. THE USER REJECTED, IN ORDER, do not retry: a
  rising-step staircase; a bare 10-cell block under the number ("un chiffre
  et un carré"); a long thin 20-cell bar with a track; that bar plus a 3x5
  "%" glyph. What passed is the number plus a drawn battery — the pictogram
  is what makes the scene self-explanatory.
  Heart-rate scene (2026-08-19, splash 9 / shake 10, Clay label "Heart
  rate"): a medical-monitor ECG strip (2-cell baseline rows 8-9, QRS spike
  up to row 3, S-dip to row 11, T-bump) scrolling LEFT one full period per
  heartbeat via its own 66 ms AppTimer, BPM digits in the MM slot, whole
  scene tinted by rate (palette 6 calm → 9 racing, same vivid half as the
  battery). Strip is inner cols 1-14 because the probe intersection
  (tools-style projection replay over all 7 platforms) caps row 3 at col 14
  — every scroll position of the spike stays on screen. ≥100 BPM: hundreds
  digit in the HH tens slot (cols 1-6 rows 1-10), strip keeps its right
  half (cols 8-14). No reading: flatline + "--" (monitor idiom). Health
  gotchas: the sensor is duty-cycled (peek can be minutes old) so the scene
  requests health_service_set_heart_rate_sample_period(1) + subscribes
  HealthEventHeartRateUpdate for live digits, and the request MUST be
  cancelled on EVERY exit path (it outlives the app otherwise — battery
  leak); prv_heart_stop() is wired into view revert/exit, apply_settings,
  splash_done, show_view_now entry and window_unload. prv_heart_ok()
  (health_service_metric_accessible & Available) gates entry: shake falls
  back to orbit, splash to "42" — aplite has no PBL_HEALTH at all (stubs),
  basalt/chalk have PBL_HEALTH but no HR hardware, flint (Pebble 2 Duo) has
  no HR sensor either. QEMU surprise: basalt QEMU DELIVERS synthetic HR
  (an update event landed with 20 bpm), so the event→morph path is testable
  headless; visual tests still force the BPM in a scratch build (splash 9 +
  SPLASH_MS 90000). VERIFIED on the real Time 2 on 2026-08-19: the 2026
  firmware delivers HR readings and events to apps fine — unlike the accel
  data service, health is trustworthy ("ça marche super bien").
  BEAT LOCK (2026-08-21, user: "elle suit pas les battements ?"): the
  averaged HealthMetricHeartRateBPM only gives a RATE, so the trace ran at
  the right speed but free of the actual beats. `HealthEventHRVUpdate` +
  `health_service_peek_hrv_ppi_ms()` carry the real peak-to-peak interval:
  the frame timer now takes its period from a fresh PPI (falling back to
  60000/bpm when none, so nothing is ever worse than before), and each
  accepted beat pulls the sweep a QUARTER of the way onto
  `fdf_heart_beat_phase(bpm)` — the phase where the spike enters the strip's
  RIGHT edge (paper feeds left). Quarter, not a snap: a snap yanks the trace
  on every late/jittery event. PPI outside 300-2000 ms is dropped as a
  missed beat. `health_service_set_hrv_sample_period(1)` SHARES the scene's
  existing 1 s sensor subscription (the header: driven at the shorter of the
  two periods) so it is nearly free — but it needs the same cancel-on-every-
  exit discipline, and 0 clears only its own request. The lock target math
  must aim at the MIDDLE of an offset's phase bucket: the stamp truncates
  `(sweep16 * len) >> 16`, so the bucket's lower edge rounds back one cell
  early (verified by round-tripping both layouts offline). Displayed number
  deliberately stays on the FILTERED metric while the animation uses PPI: a
  number flickering beat to beat is unreadable, an animation that does is
  exactly the point. A temporary `APP_LOG("HRV ppi=...")` is in
  prv_health_handler to check on-watch whether events actually arrive
  per-beat — REMOVE IT once validated.
- `src/c/digits.h` — 3×5 bitmap digit font, scaled ×2 when composed so
  strokes are 2-cell plateaus like the original 42.fdf map. Plus a `%` glyph
  (battery scene). Glyph rule: strokes move between columns ORTHOGONALLY,
  never diagonally — corner-touching cells share no edge in the plateau
  wireframe and render as floating blocks (hence the straight-legged 7, and
  the percent sign's slash drawn as a staircase).
- User drawing (2026-08-15, from a store user's request): a 22×25 1-bit
  grid (`FDF_CUSTOM_COLS/ROWS` — exactly the inner region, same on all
  platforms) drawn in a custom Clay component (`src/pkjs/pixel-grid.js`,
  its own always-visible "Your drawing" section) with TWO consumers: splash
  value 5 ("My drawing") and shake_action 5 ("Show my drawing" — 6 s
  auto-revert peek like the date peek, falls back to orbit when no drawing
  is saved so the gesture is never dead). Serialized as 6 lowercase hex
  chars per row (bit 1<<col = filled, col 0 leftmost — same convention as
  the C splash tables), 150 chars in messageKey `CustomMap`, persisted raw
  (100 B) under `CUSTOM_KEY 3`; `app_message_open` inbox is 512 for this.
  On save with either consumer selected the watch replays the drawing like
  a launch splash (morph in, hold, melt back) so the user sees it land.
  FRAMING RULE: the grid spans the full inner region, so it must render in
  the CLASSIC framing — in the seconds display mode the pair framing would
  overflow the screen. Every full-region stamp (custom grid, NixOS, Pebble
  splashes; NOT the "42", which is a pair) forces
  `fdf_model_set_mode(false)` before stamping, and `prv_splash_done` /
  the peek revert restore the mode's own framing (verified: heart fits in
  seconds mode, then the SS pair returns). The editor is WYSIWYG at native
  cell resolution — this deliberately sidesteps the whole
  downsample-fine-art problem that made the logo splashes painful — plus a
  "Stamp" field that rasterizes any emoji/short text the phone can render
  — REWRITTEN 2026-08-21 ("améliorer au maximum cette partie"), pipeline now:
  (1) 8× supersample → per-cell coverage+luminance; (2) OTSU threshold over
  the INKED cells only (background would drag the split down and bloat every
  silhouette), clamped to [0.30, 0.60] — replaces a hardcoded 0.45 that ate
  light glyphs and bloated heavy ones; (3) left-right symmetry snap;
  (4) detail carve BY REGION — dark-on-bright (eyes/mouths, body median
  L>90 and cell <0.55×) and the mirror bright-on-dark case (>median+80,
  self-guarding: unreachable on a bright body, so 🆘/boxed arrows/🎱 come
  out in relief), but a carve only applies if it is a connected blob of ≥3
  cells: per-cell carving shredded glyphs into pepper noise; (5) fill
  enclosed ≤2-cell pinholes, prune components <4 cells; (6) repairThin —
  a cell in no 2×2 ink block is thin, and is THICKENED if it has ≥2 filled
  neighbours (a stroke: this is what saves text) or DROPPED if ≤1 (a stub
  left by a carve; growing stubs grows noise); (7) weld corner-only links,
  iterating with prune to a fixed point. Also: the canvas font is now BOLD
  — free for emoji (colour bitmaps ignore weight), decisive for text, whose
  regular-weight stems land 1 cell wide and cannot render as plateaus.
  WHY these rules: the two hard renderer constraints, corner-only links
  render as floating blocks and sub-2-cell features are illegible, are now
  ENFORCED by the rasterizer instead of hoped for. Measured on a 16-glyph
  bench offline (real Noto Color Emoji through PIL, tools-style replay):
  before, 6 of 16 glyphs produced floating blocks (☕ alone had 5) and "42"
  came out as 26 thin cells; after, floating blocks and thin cells are ZERO
  across the whole battery. Bench + the JS-vs-Python differential test live
  in the session scratchpad, not the repo — rebuild them from this note if
  the pipeline is touched again. Two symmetry fixes (user: faces came out
  lopsided): center the INK box, not the advance width (emoji ink is often
  off-center in its advance — a sub-cell offset flips edge columns), and a
  left-right symmetry snap (mirror-averaged coverage) that engages only
  when <15% of mirrored silhouette pairs mismatch — asymmetric glyphs
  (letters, 🌙, hands) are untouched, and only the silhouette is snapped,
  never the carve (a 😉 keeps its wink). Validated on a glyph battery:
  bold shapes (🙂❤️⭐, letters) come out great, detailed ones (🌍👌)
  blob — acceptable because the user sees and hand-edits.
  Headless JS verification (waf does NOT parse JS): `node --check` for
  syntax, plus a differential test that slices the SHIPPED rasterizer text
  out of pixel-grid.js, runs it on the same pixel buffer as the Python
  reference through a stub canvas, and diffs the grids — that is what
  catches a transcription slip in a port. And check the Clay closure trap
  mechanically: `String(component.initialize)` must contain every helper's
  definition, since toSource() drops the module scope.
  Editor UX: the component folds behind a settings-row header (label +
  live pixel thumbnail + chevron), everything inside stacked full-width
  (side-by-side controls got crushed on phones). Undo/redo buttons keep a
  60-entry history, one entry per finished gesture (a whole drag stroke,
  a stamp, a clear — not per cell). With no saved drawing yet the grid
  opens prefilled with a stamped 😎 (reaches the watch only on Save).
  Headless JS testing without Chrome: pypkjs's STPyV8 runs the component
  fine — eval the module with a stub DOM/canvas (see the session's
  test_raster.py pattern: fake measureText/fillText/getImageData drawing
  synthetic shapes) to unit-test rasterizer changes AND syntax-check the
  file before shipping; waf does NOT parse JS, a syntax error only
  explodes at runtime on the phone.
  CRITICAL Clay-component lesson: Clay embeds custom components into the
  config page with toSource() — functions are serialized WITHOUT module
  closure, so a component must be fully self-contained (all helpers inside
  initialize(); manipulator get/set called before initialize can only stash
  the raw value). Referencing a module-level helper compiles fine and dies
  on-page with ReferenceError, killing every item after it in the config.
- `src/c/main.c` — lifecycle: splash (key stays `Splash42`, old bool 0/1
  values keep meaning) morphs into the current view. Splash styles
  (persisted values — never renumber): 1 "42", 2 NixOS snowflake, 4
  Pebble slashed-e (those three in fdf.c `fdf_model_set_splash`,
  rasterized from official vector/logo art), 5 user drawing, 6 today's
  date (day terrain + weekday-month overlay via `s_splash_overlay`),
  7 orbit (no scene: the time rises during one full camera turn —
  verified by log; QEMU screenshots are too slow to sample a 1.4 s
  animation), 8 battery gauge. Value
  3 was Arch Linux — shipped briefly, user rejected it (a filled A is a
  dense mesh blob, and the crossbar that makes it read "A" is sub-cell);
  the number stays reserved, unknown values fall back to "42".
  Two hard-won rules: (a) TOPOLOGY-FIRST — the NixOS lambdas' separating
  channels are sub-cell, so cells are labeled per-lambda and a 1-cell
  channel carved where two arms touch, C2 symmetry enforced (plain
  thresholding welds a gear; the user rejected that only mildly, but
  approved the carved version highly); (b) camera pre-shear (sampling
  through the ax22/ay45 projection so the shape stands upright on
  screen) WORKS for solid silhouettes (Arch — verified great) and FAILS
  for thin-armed shapes (NixOS v2 — 1-cell stair spikes, user:
  "dégueulasse"); round shapes (Pebble e) don't need it. Tux and Ubuntu
  CoF were tried and rejected: a filled Tux silhouette is an anonymous
  blob, the CoF's heads/moats collapse at 20 cells. Minute boundaries trigger a morph
  `Animation`. Display modes: classic HH/MM terrain, or seconds (a
  single centered SS pair at a 1.5x-fitted "pair" framing, morphing
  every second, HH:MM as small text over the ocean; briefly removed
  2026-08-14 then restored on user request, as was the sticky toggle —
  don't remove them again). The shake gesture is a 10-way setting: orbit
  spin (1) / peek at seconds (2: SS terrain + HH:MM overlay in the
  CLASSIC framing so there is no camera jump, reverts where the minute
  morph begins or on a second shake) / sticky seconds toggle (3:
  persisted under PEEK_KEY, survives relaunches; 2 and 3 exist in
  classic display mode only, guarded on both Clay and watch sides) /
  view peeks 4-9 (`s_view_peek` + `prv_show_view_now`, one generic
  machinery: 4 date — DD terrain + weekday-month overlay, 5 the user's
  drawing — falls back to orbit when none saved, 6 "42", 7 NixOS,
  8 Pebble, 9 the battery scene (number + bar, no overlay text);
  6 s auto-revert, allowed in every display mode; full-region
  scenes force classic framing, the revert restores the mode's own;
  overlays the underlying view — s_peeking stays untouched so the
  revert lands back on it) / off (0). The scene catalog is deliberately
  SHARED with the splash select (2026-08-16 user request): both sides
  offer 42/drawing/date and orbit; only the seconds views stay
  shake-only (they already are a display mode). The Pebble logo entries
  (splash 4, shake 8) were REMOVED from the Clay UI on 2026-08-16 —
  superseded by the user drawing — but the watch still renders those
  persisted values so store users who had picked it keep it; don't
  reuse the numbers. NixOS was pulled the same day and immediately
  reinstated on user request ("laisse nixos") — it stays. The sticky
  seconds toggle (shake 3) was also dropped from the UI on 2026-08-16
  ("on simplifie" — this supersedes the earlier "don't remove" note,
  which was about removing the FEATURE; the watch code and persisted
  value still work). LABELS ARE SHARED BY BOTH SELECTS (2026-08-17 user
  request, supersedes the earlier "Show the seconds" wording): the same
  scene has the same name and the same position in the splash select and
  the shake select — "42" / My drawing / Today's date / Battery / NixOS /
  Orbit spin / Off, no "Show…" prefixes; the shake list is that list with
  "Seconds" inserted after the date (it is the one scene the splash cannot
  offer). Only the persisted VALUES differ between the two sides, and they
  must never be renumbered.
  Low-battery alert (2026-08-17, `low_batt` / Clay `LowBatt`, default on):
  the SAME scene 9 raises itself when the charge first crosses under 20%,
  then under 10% — whatever the shake is set to. Only the CROSSING alerts,
  once per step: the lowest threshold announced is persisted under
  `BATT_KEY 4` (a watchface relaunches every time the user leaves an app,
  so an unpersisted latch would replay the alert constantly) and rearms
  when the charge recovers or the watch is plugged in. An alert fired at a
  dark screen would be wasted, so it waits in `s_batt_pending` for the
  backlight (CAN_REST_WAVES boards) or for a running splash to end; when
  the watch is already low at launch, the gauge takes the splash slot
  instead of interrupting a moment later. Deliberately NO vibration: the
  firmware pushes its own low-battery notification, a second buzz would
  double up. Buttons are as impossible as touch on watchfaces:
  the kernel's shell/normal/watchface.c owns the ClickManager
  (launcher, timeline, quick-launch) — a watchface never sees clicks.
  Tap handling: events within 1.2 s are one physical shake (burst
  grouping, `s_burst_ms`). Wake-first gate (`wake_first`, Clay
  `WakeFirst`, default OFF — the default orbit action is harmless, even
  pleasant, on the waking shake itself): a shake only acts if the screen is lit for
  >1 s (backlight hardware; the backlight service is now ALWAYS
  subscribed on CAN_REST_WAVES hardware — s_lit feeds both the wave
  rest and this gate) or a previous shake landed <6 s ago — so the
  first jolt wakes/arms instead of triggering, same model as the
  firmware's touch session. The arm-window path is what keeps gestures
  usable in daylight (ALS keeps the backlight dark) and on old watches. TOUCH IS
  IMPOSSIBLE ON WATCHFACES, don't retry: PebbleOS reserves touch for
  watchapps — applib's prv_get_state() returns NULL under
  sys_app_is_watchface() ("Touch is reserved for watchapps; watchfaces
  must not consume it", firmware touch_service.c), so touch_service
  subscriptions AND the recognizer/touch-bridge route
  (window_attach_recognizer + window_set_touch_bridge_disabled +
  app_touch_navigation_enable) all silently no-op. Both were implemented
  on 2026-08-14 and removed after failing on the real Time 2; the SDK
  compiles them without complaint. Revisit only if the firmware grows a
  watchface opt-in. Guard new-HW APIs by testing
  the `_PBL_API_EXISTS_<fn>` marker macro directly — `PBL_API_EXISTS()`
  inside `#if` trips -Wexpansion-to-defined. Wave modes: silk
  (66 ms AppTimer interpolates the phase continuously), fluid (second
  ticks), eco (minute drift), frozen, pulse (2026-08-19, value 4, Clay
  "Pulse (beats with your heart)": silk's timer but the phase INTEGRATES at
  one wavelength per 30 beats — at 60 BPM exactly silk's pace — from the
  firmware's duty-cycled HR reading, peeked once a minute plus whatever the
  heart scene receives; NO sensor request, zero battery cost; integration
  means wave rest pauses it for free, just a 500 ms dt clamp on resume; no
  reading/no sensor paces like a calm 60); 1-bit has no terrain and stays
  on MINUTE_UNIT unless the seconds view needs SECOND_UNIT. Wave rest
  (`wave_rest`, Clay `WaveRest`, default on): the swell pauses while the
  backlight is off — silk timer stopped, fluid dropped to minute ticks —
  and on wake the phase is REBASED (`s_wave_offset`) so it resumes from
  the frozen spot instead of teleporting to the wall clock. Gated by
  `PBL_API_EXISTS(backlight_service_subscribe)` (`CAN_REST_WAVES`): the
  BacklightService only exists on Core Devices boards (emery/flint/
  gabbro); OG Pebbles ship stubs where `light_is_on()` is a literal 0,
  so nothing outside the guard may touch it (waves would rest forever).
  Daylight caveat: the ALS keeps the backlight off when it's bright, so
  on-wrist in daylight the ocean stays frozen. QEMU: `light_is_on()` is
  false at boot (waves rest immediately on emery/gabbro) and
  `sendkey left` on the QEMU monitor is the backlight trigger — the
  story/rollover capture keepalive already holds it lit during recording.
  Tilt sway (accelerometer parallax shear of the extrusion) was FULLY
  BUILT on 2026-08-15 and REMOVED the same day — blocked by THREE
  firmware defects on the Time 2, all verified in PebbleOS source and
  on-watch; do not re-attempt until they are fixed upstream:
  (1) accel_data unsubscribe with an in-flight event kernel_free()s the
  app's STATIC accel session (applib accel_service.c deferred_free +
  prv_is_session_task wrongly including PebbleTask_App) — kernel heap
  corruption, deterministic crashloop at a firmware PC;
  (2) reconfiguring a live subscription (set_sampling_rate /
  set_samples_per_update) permanently wedges delivery — zero callbacks
  ever after, measured;
  (3) a continuous data subscription raises the LSM6DSO ODR, which
  detunes the hardware wake-up so badly that SYSTEM MOTION WAKE stops
  working while the stream runs (driver's own comments warn wake-up
  duration is not rescaled on ODR change).
  Also: accel_service_peek without a data subscriber returns stale
  burst values (kernel samples the IMU slowly) — useless for motion.
  The FdfModel.sway_x8/y8 shear plumbing in fdf.c is kept (zeroed) for
  a future return; settings byte reserved1 was tilt_sway.
  Crash-debugging recipe: `pebble logs --phone` catches "App fault ...
  PC: 0x..."; app code runs from RAM ~0x2005xxxx (log a function
  pointer to get the slide — PCs in 0x12xxxxxx are FIRMWARE, don't
  addr2line them against the app ELF, a wrong base gives convincing
  nonsense). Only ONE Dev Connect session at a time — a screenshot
  kills a running logs tail. Scale is
  zoom8/8000 per mg, inputs clamped to ±1100 mg so motion spikes can't
  kick the terrain. Test in QEMU: `pebble emu-accel gravity+x` while a
  sendkey-left keepalive holds the backlight (and thus the timer) alive.
- `src/pkjs/` — phone-side JS: `config.js` is the Clay settings page
  (theme, wave mode, gradient on/off, splash, shake orbit, BT vibe),
  `index.js` just instantiates Clay. Settings arrive in `main.c` via
  AppMessage (`prv_inbox_received` — Clay sends select values as STRINGS,
  toggles as ints; `prv_tuple_int` handles both), persist under
  `SETTINGS_KEY`, and apply live via `prv_apply_settings` (re-subscribes
  tick/tap/connection services; safe to call repeatedly).
- Headless settings test: run `pebble emu-app-config --emulator basalt` in
  the harness background (sandboxed `setsid ... &` children are killed when
  the Bash call ends — servers silently die), find its port with
  `ss -tlnp | grep '"pebble"'` (the process is named "pebble", NOT python —
  the python ports belong to pypkjs, whose server answers /close with a
  misleading 500), then
  `curl "http://localhost:<port>/close?<urlencoded settings JSON>"` — this
  is exactly what Clay's Save button does. The /close server is ONE-SHOT:
  each save needs a fresh emu-app-config. Payload can be flat
  {"Key":"val"} or Clay's native {"Key":{"value":...}}.
- pypkjs localStorage corruption: its dumbdbm-backed store
  (`~/.local/share/pebble-sdk/4.33/<platform>/localstorage/<uuid>.*` inside
  the FHS env) reuses blocks without truncating, so a SHRINKING
  clay-settings value leaves trailing junk that crashes Clay's JSON.parse
  ("Unexpected non-whitespace character after JSON at position N") — Clay
  then never answers showConfiguration and emu-app-config times out or
  500s. Fix: rm those files and restart the emulator.
- `pebble emu-battery --emulator <plat> --percent N` needs the explicit
  `--emulator` flag and lands ASYNCHRONOUSLY: the app often still peeks the
  old charge on the very next `pebble install`, so a level test takes an
  extra install cycle (verify with the value the scene itself prints). A
  scene that only shows for 1.5 s cannot be screenshotted either — bump
  SPLASH_MS to 90000 in a scratch build to inspect it, and remember to
  revert.
- `pebble emu-tap` stopped delivering tap events to the app in the
  2026-08-15 session (no accel_tap callback fires at all — verified with an
  APP_LOG first thing in the handler, on fresh emulators, with the orbit
  action too; story_capture used it successfully at v1.2.0). Don't burn
  time re-debugging app code when a tap-driven feature "doesn't work" in
  QEMU — verify the settings landed (the save replay proves that path) and
  test the gesture on the real watch.
- Testing the Clay page in Chrome: decode the page from the newest
  `~/pebble-tool-emu-app-config-*.html` (URL fragment after '#',
  urldecode), serve it over local HTTP (harness background), and drive it
  with Claude-in-Chrome. The Save button navigates to a literal
  `$$RETURN_TO$$` placeholder when served this way (the phone app/S3 host
  substitutes it) — grab the payload from the resulting 404 URL and curl it
  to a fresh emu-app-config /close to complete the loop. Chrome screenshot
  coords are scaled vs CSS px (screenshot_width / window.innerWidth); the
  screenshot tool intermittently times out — read state via javascript_tool
  instead.
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
- ALTITUDE CANNOT ENCODE A VALUE. `Z_NUM/Z_DEN = 3/16` is sized so digit
  plateaus just clear the ROW_GAP, which makes one altitude unit ≈ 1 px:
  the full 0..10 range is ~11 px. Every "gauge by height" idea (staircase
  of rising steps, ramp, sinking slab) renders as a tilted plate lost in
  the swell — four geometries tried on 2026-08-17, all rejected. Only two
  things read at this density: the FOOTPRINT of a full-height plateau, and
  the palette color. Design new scenes as shapes, not as reliefs.
- The flip side: ALTITUDE IS THE COLOUR CHANNEL, and it is nearly free.
  Stamping a shape one or two steps below FDF_Z_TOP changes its colour to
  that palette entry while moving it ~1 px — that is how the battery scene
  tints its digits by charge level. Two rules: stay in the ramp's vivid
  half (6-9; the low indices are recessive darks by design and vanish
  against the mesh), and on 1-bit stamp FDF_Z_TOP instead — mid altitudes
  draw NOTHING there (walls dropped, tops only above z=7).
- LINE COUNT IS THE REAL BUDGET at ~6 px/cell. Every altitude CHANGE draws
  a wall, so a shape with an inner element at a different altitude costs
  twice the strokes of the same shape drawn flat — enough to turn a small
  pictogram into mush. Stamp composite scenes at ONE altitude and let the
  colour (that same altitude) carry the meaning; reserve a second altitude
  for one small accent (the charging cell).
- KEEP SCENES INSIDE THE BORDER: never stamp on inner col 0 / col 21 /
  row 0 / row 24. Those cells are adjacent to the bleed ring, and since the
  wireframe draws an edge to every neighbour, the edge on that side has one
  endpoint IN THE SWELL and wobbles with the waves at 15 fps. That is what
  BORDER=1 buys the digits. Shipped broken in v1.4.0 (the battery case
  started at col 0 and its back edge rippled — "l'arrière de la batterie
  bouge comme l'océan"), fixed in v1.4.1. Note the user-drawn grid spans
  the whole inner region by design and does touch the ring on all four
  sides — that is a known, accepted exception.
- THERE ARE NO VERTICAL EDGES in this renderer: every line joins two
  ADJACENT GRID VERTICES, so a "wall" is the diagonal between a high cell
  and its lower neighbour — one grid step sideways (~6 px) plus the whole
  height drop (~11 px at FDF_Z_TOP). That lands ~24° off vertical on the
  digit-row axis and ~11° on the stack axis. It is the house style (the
  digits' flanks do it too) but the eye rejects it on any edge where it
  expects a clean vertical, e.g. a gauge's fill line (user report,
  2026-08-17). It cannot be straightened, only SHORTENED, by raising the
  surface the edge drops onto. That was tried on the battery (uncharged
  cells 3 palette steps under the charge instead of at ocean level) and the
  user chose to keep the deep version once the geometry was explained — the
  short step weakens the case outline. Explain the lean before "fixing" it.
- Inside an enclosing frame, an inner bar needs a 1-cell gap from the frame
  above and below, or the two fuse into a single blob and the level
  disappears. That is why the battery case is 8 rows for a 2-row bar.
- NEW SCENES MUST FIT THE VISIBLE BAND, which is much smaller than the
  inner region. The fitted zoom only guarantees the two DIGIT boxes on
  screen (that is deliberate — the bleed ring is meant to clip), so the
  inner region's own corners fall off the edges. Measured with a probe
  grid and by replaying the projection offline: an inner cell (col, row)
  is on screen iff `col - row` is within [-13, 10] — a diagonal band. It
  is the same on every platform (the two aspect ratios differ by a few
  percent, round chalk is the most generous, and the 1-bit boards'
  narrower bleed ring cancels out, the fit being relative to the inner
  region either way). Consequences: a shape spends that budget on its
  width AND its height, so anything bigger than a digit-pair box (14×10,
  span 22) gets clipped; the failure mode is silent and asymmetric — the
  first battery staircase was 20 cols wide and simply vanished at 10%
  charge, its short steps having fallen off the bottom-left corner.
  Logos and drawings survive because they are centered and roughly
  circular; a wide rectangle is not.

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
- `clock_is_24h_style()` must be respected for time formatting. The Clay
  `TimeFormat` select (0 auto/1 12h/2 24h, `prv_use_24h`) can override it;
  in 12h the seconds/date overlay text carries AM/PM and the classic
  terrain draws a small AM/PM tag (top-right on rect — the HH pair leans
  left so the ocean is free there; MID-RIGHT on round, moved there
  2026-08-17 on user report: top-center sat right on the HH pair, since the
  round fit centers the model and the digits reach the top of the disc.
  Mid-right is the free pocket the trimetric lean leaves between HH's
  bottom-right and MM's top-right — checked against the maximal footprint
  by stamping 88:88, not just the current time). 24h shows no
  tag, and the Clay `ShowAmPm` toggle (default on) can hide it in 12h.

## Claude Code skill

`.claude/skills/pebble-watchface/` vendors Core Devices' official watchface
skill (templates, API reference, drawing/animation guides, QEMU workflow).
Use it when designing or substantially reworking a watchface.
