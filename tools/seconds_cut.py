"""Cut a seconds-mode loop from story_capture recordings made with a build
whose display_mode defaults to seconds: ~3 s of the terrain morphing a new
SS value every second, taken from the calm region after the orbit.

Usage: seconds_cut.py <frames_root> <gifs_out_dir>
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


def frame_path(d, i):
    return os.path.join(d, "frame_{:05d}.ppm".format(i))


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
    tap = int(ev["tap"] * fps)
    # The orbit ends ~1.9s after the tap; start a little later so the loop
    # is purely second-morphs, and take 3 of them.
    first = min(total - 1, tap + int(2 * 2.5 * fps) // 2)
    last = min(total, first + 3 * fps)
    make_gif(frames_dir, first, last, w, h,
             os.path.join(out_dir, "{}_seconds.gif".format(platform)), fps)
