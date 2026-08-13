"""Cut the recorded frame sequences into two well-paced GIFs per platform:
  <platform>_boot.gif  — opens ON the 42 splash, morphs into the time, holds
  <platform>_orbit.gif — steady time -> full orbit spin -> steady time
The reinstall that replays the splash shows the OS loading screen (bright,
mostly uniform) before the app relaunches; the boot clip is cut to start on
the first dark frame AFTER that bright loader so no system UI leaks in.

Usage: story_cut.py <frames_root> <gifs_out_dir>
"""
import json
import os
import shutil
import subprocess
import sys

from PIL import Image

DIMS = {
    "aplite": (144, 168), "basalt": (144, 168), "diorite": (144, 168),
    "flint": (144, 168), "chalk": (180, 180), "emery": (200, 228),
    "gabbro": (260, 260),
}

frames_root, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)


def frame_path(d, i):
    return os.path.join(d, "frame_{:05d}.ppm".format(i))


def brightness(path):
    im = Image.open(path).convert("L")
    im.thumbnail((16, 16))
    px = list(im.getdata())
    return sum(px) / len(px)


def make_gif(frames_dir, first, last, w, h, out_path, fps):
    seq = os.path.join(frames_dir, "cut_seq")
    shutil.rmtree(seq, ignore_errors=True)
    os.makedirs(seq)
    n = 0
    for i in range(first, last + 1):
        src = frame_path(frames_dir, i)
        if os.path.exists(src):
            os.link(src, os.path.join(seq, "frame_{:05d}.ppm".format(n)))
            n += 1
    vf = ("crop={w}:{h}:(iw-{w})/2:(ih-{h})/2,"
          "split[a][b];[a]palettegen=stats_mode=diff[p];"
          "[b][p]paletteuse=dither=none").format(w=w, h=h)
    subprocess.run([
        "ffmpeg", "-v", "error", "-y", "-framerate", str(fps),
        "-i", os.path.join(seq, "frame_%05d.ppm"),
        "-vf", vf, "-loop", "0", out_path,
    ], check=True)
    shutil.rmtree(seq)
    print(os.path.basename(out_path), n, "frames @", fps, "fps")


for platform, (w, h) in DIMS.items():
    frames_dir = os.path.join(frames_root, platform)
    ev_path = os.path.join(frames_dir, "events.json")
    if not os.path.exists(ev_path):
        print("skip", platform, "(no capture)")
        continue
    with open(ev_path) as f:
        ev = json.load(f)
    fps = ev["fps"]
    total = ev["frames"] - 1
    splash = int(ev["splash"] * fps)
    tap = int(ev["tap"] * fps)

    # Find the bright OS loading screen around the reinstall, then start the
    # boot clip on the first dark frame after it (the 42 splash).
    search = range(max(0, splash - fps), min(total, splash + 4 * fps))
    bright = [i for i in search
              if os.path.exists(frame_path(frames_dir, i))
              and brightness(frame_path(frames_dir, i)) > 90]
    if bright:
        boot_first = bright[-1] + 2
    else:
        boot_first = splash + fps  # fallback: 1s after install returned
    # 42 holds 1.5s, morph 0.7s, then ~1.3s steady time before the loop.
    boot_last = min(total, boot_first + int(3.4 * fps), tap - 1)
    make_gif(frames_dir, boot_first, boot_last, w, h,
             os.path.join(out_dir, "{}_boot.gif".format(platform)), fps)

    # Orbit: 0.4s steady, 1.4s spin, ~0.5s steady -> seamless loop.
    orbit_first = max(0, tap - int(0.4 * fps))
    orbit_last = min(total, tap + int(1.9 * fps))
    make_gif(frames_dir, orbit_first, orbit_last, w, h,
             os.path.join(out_dir, "{}_orbit.gif".format(platform)), fps)
