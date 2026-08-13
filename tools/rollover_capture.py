"""Minute-rollover recorder: boots a clean emulator, installs the app, waits
for the next real wall-clock minute boundary (the SiFli-based platforms
ignore emulator time injection) and records the morph animation around it.

Usage: rollover_capture.py <project_dir> <platform> <frames_out_dir>
"""
import json
import os
import shutil
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
PRE = 2.5      # recording starts this long before the minute boundary
DURATION = 6.5

project_dir, platform, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
os.chdir(project_dir)
pbw = os.path.join(project_dir, "build", "fdf-time.pbw")

frames_dir = os.path.join(out_dir, platform)
shutil.rmtree(frames_dir, ignore_errors=True)
os.makedirs(frames_dir)

cmd = ScreenshotCommand()
cmd._set_debugging(0)

persist = get_sdk_persist_dir(platform, sdk_manager.get_current_sdk())
if os.path.exists(persist):
    shutil.rmtree(persist)

pebble = cmd._connect_emulator(platform, None, vnc_enabled=False)
cmd.pebble = pebble
time.sleep(5)
ToolAppInstaller(pebble, pbw, quiet=True).install()
time.sleep(3)  # let the boot splash finish

monitor_port = pebble.transport.qemu_monitor_port

stop = threading.Event()
def keepalive():
    while not stop.wait(1.0):
        cmd._qemu_monitor_command(monitor_port, "sendkey left")
cmd._qemu_monitor_command(monitor_port, "sendkey left")
threading.Thread(target=keepalive, daemon=True).start()

# Wait so recording starts PRE seconds before the next minute boundary.
now = time.time()
wait = (60 - (now % 60)) - PRE
if wait < 1.0:  # too close: take the following boundary
    wait += 60
print(platform, "waiting {:.1f}s for minute boundary...".format(wait))
time.sleep(wait)
cmd._qemu_monitor_command(monitor_port, "sendkey left")
time.sleep(0.2)

t0 = time.perf_counter()
boundary_at = PRE - 0.2  # seconds into the recording

interval = 1.0 / FPS
next_t = 0.0
i = 0
while True:
    now = time.perf_counter() - t0
    if now >= DURATION:
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
    json.dump({"fps": FPS, "frames": i, "boundary": boundary_at}, f)
print(platform, "captured", i, "frames, boundary at {:.1f}s".format(boundary_at))

cmd._close_pebble_connection(pebble)
cmd._shutdown_platform_emulator(platform, None)
