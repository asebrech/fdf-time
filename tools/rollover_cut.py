"""Cut minute-rollover recordings into tight GIFs:
1.2s steady old time -> morph -> ~2s steady new time.

Usage: rollover_cut.py <frames_root> <gifs_out_dir>
"""
import json
import os
import shutil
import subprocess
import sys

DIMS = {
    "aplite": (144, 168), "basalt": (144, 168), "diorite": (144, 168),
    "flint": (144, 168), "chalk": (180, 180), "emery": (200, 228),
    "gabbro": (260, 260),
}

frames_root, out_dir = sys.argv[1], sys.argv[2]
os.makedirs(out_dir, exist_ok=True)

for platform, (w, h) in DIMS.items():
    frames_dir = os.path.join(frames_root, platform)
    ev_path = os.path.join(frames_dir, "events.json")
    if not os.path.exists(ev_path):
        print("skip", platform)
        continue
    with open(ev_path) as f:
        ev = json.load(f)
    fps = ev["fps"]
    total = ev["frames"] - 1
    boundary = int(ev["boundary"] * fps)

    first = max(0, boundary - int(1.2 * fps))
    last = min(total, boundary + int(2.8 * fps))

    seq = os.path.join(frames_dir, "cut_seq")
    shutil.rmtree(seq, ignore_errors=True)
    os.makedirs(seq)
    n = 0
    for i in range(first, last + 1):
        src = os.path.join(frames_dir, "frame_{:05d}.ppm".format(i))
        if os.path.exists(src):
            os.link(src, os.path.join(seq, "frame_{:05d}.ppm".format(n)))
            n += 1
    vf = ("crop={w}:{h}:(iw-{w})/2:(ih-{h})/2,"
          "split[a][b];[a]palettegen=stats_mode=diff[p];"
          "[b][p]paletteuse=dither=none").format(w=w, h=h)
    out_path = os.path.join(out_dir, "{}_rollover.gif".format(platform))
    subprocess.run([
        "ffmpeg", "-v", "error", "-y", "-framerate", str(fps),
        "-i", os.path.join(seq, "frame_%05d.ppm"),
        "-vf", vf, "-loop", "0", out_path,
    ], check=True)
    shutil.rmtree(seq)
    print(os.path.basename(out_path), n, "frames")
