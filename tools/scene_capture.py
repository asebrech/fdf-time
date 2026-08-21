"""Plain scene recorder: boot a clean emulator, install, hold the backlight
on and record N seconds of frames. No scripted events — the scene is driven
by the build itself (a long splash, a theme rotation), which is how the
heart-rate and theme-cycle store assets are made.

Unlike story_capture this wipes the emulator's persisted settings, so the
build's DEFAULTS are what gets recorded — that is the whole point when the
capture build forces a scene or cycles themes.

Usage: scene_capture.py <project_dir> <platform> <frames_out_dir> [seconds]
"""
import json
import os
import shutil
import subprocess
import sys
import threading
import time

from pebble_tool.commands.screenshot import (
    ScreenshotCommand,
    ToolAppInstaller,
    sdk_manager,
    get_sdk_persist_dir,
)
from pebble_tool.sdk import add_tools_to_path, get_persist_dir, sdk_version

add_tools_to_path()
os.environ["PATH"] = "{}:{}".format(
    os.path.join(get_persist_dir(), "SDKs", sdk_version(), "toolchain", "bin"),
    os.environ["PATH"])

FPS = 15

project_dir, platform, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
duration = float(sys.argv[4]) if len(sys.argv) > 4 else 12.0
os.chdir(project_dir)
pbw = os.path.join(project_dir, "build", "fdf-time.pbw")

frames_dir = os.path.join(out_dir, platform)
shutil.rmtree(frames_dir, ignore_errors=True)
os.makedirs(frames_dir)

cmd = ScreenshotCommand()
cmd._set_debugging(0)

# Wipe persisted state: a settings blob from a previous run would override
# the capture build's defaults and silently record the wrong scene.
persist = get_sdk_persist_dir(platform, sdk_manager.get_current_sdk())
if os.path.exists(persist):
    shutil.rmtree(persist)

pebble = cmd._connect_emulator(platform, None, vnc_enabled=False)
cmd.pebble = pebble
time.sleep(5)  # pypkjs settle
try:
    ToolAppInstaller(pebble, pbw, quiet=True).install()
except Exception as e:
    print("initial install failed (", e, ") — the CLI install below is the"
          " one that matters")
time.sleep(2)

monitor_port = pebble.transport.qemu_monitor_port

# Backlight keepalive: on CAN_REST_WAVES boards the swell freezes when the
# light goes out, which would record a dead ocean.
stop = threading.Event()


def keepalive():
    while not stop.wait(1.0):
        cmd._qemu_monitor_command(monitor_port, "sendkey left")


cmd._qemu_monitor_command(monitor_port, "sendkey left")
threading.Thread(target=keepalive, daemon=True).start()
time.sleep(0.3)

# The CLI install is what actually LAUNCHES the watchface — installing
# through libpebble2 alone can leave the emulator on "Install an app to
# continue", which is how the first heart capture recorded the system UI.
# Doing it while recording also means the clip opens on the launch splash,
# which is exactly the scene these assets are after.
def relaunch():
    time.sleep(0.3)
    subprocess.run(["pebble", "install", "--emulator", platform],
                   cwd=project_dir, capture_output=True)


threading.Thread(target=relaunch, daemon=True).start()

t0 = time.perf_counter()
interval = 1.0 / FPS
next_t = 0.0
i = 0
while True:
    now = time.perf_counter() - t0
    if now >= duration:
        break
    if now < next_t:
        time.sleep(next_t - now)
    path = os.path.join(frames_dir, "frame_{:05d}.ppm".format(i))
    try:
        cmd._qemu_monitor_command(monitor_port, "screendump {}".format(path))
    except Exception:
        pass
    deadline = time.time() + 0.5
    while time.time() < deadline:
        if os.path.exists(path) and os.path.getsize(path) > 0:
            break
        time.sleep(0.005)
    next_t += interval
    i += 1

stop.set()
with open(os.path.join(frames_dir, "events.json"), "w") as f:
    json.dump({"fps": FPS, "frames": i}, f)
print(platform, "captured", i, "frames over", duration, "s")

cmd._close_pebble_connection(pebble)
cmd._shutdown_platform_emulator(platform, None)
