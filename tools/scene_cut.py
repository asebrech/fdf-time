"""Cut a plain scene recording (scene_capture.py) into one looping GIF.

Skips the bright OS loading screen at the front the same way story_cut does,
then keeps a fixed window. Frames are cropped to the platform's exact
resolution — the stores silently reject anything else.

Usage: scene_cut.py <frames_root> <gifs_out_dir> <suffix> [skip_s] [len_s]
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

frames_root, out_dir, suffix = sys.argv[1], sys.argv[2], sys.argv[3]
skip_s = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
len_s = float(sys.argv[5]) if len(sys.argv) > 5 else 0.0
os.makedirs(out_dir, exist_ok=True)


def frame_path(d, i):
    return os.path.join(d, "frame_{:05d}.ppm".format(i))


def brightness(path):
    im = Image.open(path).convert("L")
    im.thumbnail((16, 16))
    px = list(im.getdata())
    return sum(px) / len(px)


for platform, (w, h) in DIMS.items():
    frames_dir = os.path.join(frames_root, platform)
    ev_path = os.path.join(frames_dir, "events.json")
    if not os.path.exists(ev_path):
        print("skip", platform, "(no capture)")
        continue
    with open(ev_path) as f:
        ev = json.load(f)
    fps, total = ev["fps"], ev["frames"] - 1

    # Walk past the OS loader (bright, near-uniform) so no system UI leaks in.
    first = 0
    for i in range(0, min(total, 8 * fps)):
        p = frame_path(frames_dir, i)
        if os.path.exists(p) and brightness(p) > 90:
            first = i + 2
    first += int(skip_s * fps)
    last = total if len_s <= 0 else min(total, first + int(len_s * fps))
    if last <= first:
        print("skip", platform, "(window empty)")
        continue

    seq = os.path.join(frames_dir, "cut_seq")
    shutil.rmtree(seq, ignore_errors=True)
    os.makedirs(seq)
    n = 0
    for i in range(first, last + 1):
        src = frame_path(frames_dir, i)
        if os.path.exists(src):
            os.link(src, os.path.join(seq, "frame_{:05d}.ppm".format(n)))
            n += 1
    out_path = os.path.join(out_dir, "{}_{}.gif".format(platform, suffix))
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
