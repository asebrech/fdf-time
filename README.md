# FdF Time

A [Pebble](https://repebble.com/) watchface that renders the time as a 3D
wireframe heightmap in isometric projection — a tribute to École 42's **FdF**
("fil de fer") project, whose demo map extrudes "42" from a flat terrain.
Here, the terrain grows the current time instead, refreshed every minute.

| basalt (color) | emery (Pebble Time 2) | diorite (1-bit) | chalk (round) |
|---|---|---|---|
| ![basalt](docs/screenshot-basalt.png) | ![emery](docs/screenshot-emery.png) | ![diorite](docs/screenshot-diorite.png) | ![chalk](docs/screenshot-chalk.png) |

Features:

- **HH / MM stacked** as flat-top plateaus (2-cell-thick strokes, exactly the
  style of the original `42.fdf` map), on a wireframe terrain grid.
- **Startup homage**: the face boots showing "42", then morphs into the time.
- **Morph animation**: on each minute change, altitudes interpolate from the
  old digits to the new ones — the terrain grows and recedes (~700 ms,
  ease-out).
- **Wrist-flick orbit**: a tap/flick spins the model through a full turn
  (the FdF rotation bonus), then settles back to the canonical isometric view.
- Runs on all 7 platforms: `aplite`, `basalt`, `chalk`, `diorite`, `emery`,
  `flint`, `gabbro`.

## How it works

Pure integer math, no floats — friendly to the FPU-less Cortex-M3:

- Trimetric projection tuned for a portrait watch screen: the digit-row axis
  slopes a gentle 22° (readable baseline), the HH/MM stack axis is picked at
  init (~60-70°) to fill the screen height, and a terrain "bleed" ring
  overflows the edges so the mesh reaches the corners. All axes are
  1024-scale fixed-point vectors.
- Rotation uses the SDK's `sin_lookup`/`cos_lookup` fixed-point trig.
- Time digits come from a 3×5 bitmap font scaled ×2 into a 16×25 heightmap;
  the whole transform chain (center → rotate → project) recomputes from the
  pristine grid every frame, FdF-style.
- Legibility on a 144 px screen comes from visual hierarchy: plateau-top
  edges (the digit outlines) are bright/bold, walls and the base mesh recede
  (dark green on color displays; dropped or thinned on 1-bit displays).

## Dev environment (NixOS / Nix + devenv)

The toolchain is reproducible via [devenv](https://devenv.sh/) +
[direnv](https://direnv.net/). The Pebble SDK ships prebuilt FHS binaries
(ARM toolchain, QEMU emulator), so on NixOS every `pebble` command runs
transparently inside a `buildFHSEnv` bubblewrap wrapper — no `nix-ld` or
system configuration needed.

### First-time setup

```sh
direnv allow        # or: devenv shell
pebble-setup        # installs pebble-tool (via uv) + SDK core + ICU fix
```

### Daily workflow

```sh
pebble build                        # build the .pbw for all platforms
pebble install --emulator basalt    # run in the QEMU emulator
pebble emu-tap --emulator basalt    # trigger the orbit animation
pebble screenshot --emulator basalt # grab a screenshot
pebble kill                         # stop emulators
```

To install on a real watch through the phone app's developer connection:

```sh
pebble install --phone <PHONE_IP>
```

## Project layout

```
src/c/main.c     app lifecycle, tick handling, morph/spin animations
src/c/fdf.c/.h   heightmap model, integer isometric pipeline, wireframe render
src/c/digits.h   3x5 digit font
package.json     Pebble app manifest (uuid, platforms)
wscript          waf build script (standard Pebble template)
devenv.nix       reproducible dev environment + FHS wrapper + pebble scripts
.claude/skills/  Core Devices' pebble-watchface skill for Claude Code
```

## Resources

- [FdF subject](https://cdn.intra.42.fr/) — École 42 wireframe project (the inspiration)
- [Pebble developer docs](https://developer.repebble.com/) — API reference, tutorials
- [Framebuffer & drawing guide](https://developer.repebble.com/guides/graphics-and-animations/framebuffer-graphics/)
- [kurval/42-fdf](https://github.com/kurval/42-fdf) — reference FdF implementation studied for the pipeline
- [pebble-watchface-agent-skill](https://github.com/coredevices/pebble-watchface-agent-skill)
  (© Core Devices, vendored under `.claude/skills/`)

## Notes / gotchas

- **Do not export `CC`/`AR`/`LD` into pebble builds**: waf gives ambient `CC`
  priority over the SDK's `arm-none-eabi-gcc`. The FHS wrapper unsets them.
- The STPyV8 ICU seed (done by `pebble-setup`) works around a read-only
  `/usr/share` inside the FHS environment.
- Antialiasing is deliberately disabled: at this wireframe density, AA smears
  1 px lines into noise. Crisp beats smooth.
