"""Scripted GIF session recorder for the FdF Time watchface.

Boots a clean emulator for one platform, installs the app, then records a
QEMU screendump frame sequence while triggering scripted events:
  - a reinstall (replays the 42 boot splash morphing into the time)
  - an accel tap (plays the full-orbit spin)
Frame timestamps of both events are written to events.json so a second pass
can cut clean, well-paced GIFs deterministically.

Usage: story_capture.py <project_dir> <platform> <frames_out_dir>
"""
import json
import os
import subprocess
import sys
import shutil
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
# The CLI entrypoint (run_tool) also puts QEMU on PATH; replicate it here.
os.environ["PATH"] = "{}:{}".format(
    os.path.join(get_persist_dir(), "SDKs", sdk_version(), "toolchain", "bin"),
    os.environ["PATH"])

FPS = 15
DURATION = 14.0

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
time.sleep(5)  # pypkjs settle (see pebble-tool's own per-platform loop)
ToolAppInstaller(pebble, pbw, quiet=True).install()
time.sleep(3)  # let the initial splash play out; we re-trigger it on record

monitor_port = pebble.transport.qemu_monitor_port

# Backlight keepalive, same trick as pebble-tool's rollover capture.
stop = threading.Event()
def keepalive():
    while not stop.wait(1.0):
        cmd._qemu_monitor_command(monitor_port, "sendkey left")
cmd._qemu_monitor_command(monitor_port, "sendkey left")
threading.Thread(target=keepalive, daemon=True).start()
time.sleep(0.3)

events = {}
def run_events():
    time.sleep(0.3)
    subprocess.run(
        ["pebble", "install", "--emulator", platform],
        cwd=project_dir, capture_output=True)
    events["splash"] = time.perf_counter() - t0  # app relaunches ~now
    time.sleep(4.0)  # splash (1.5s) + morph (0.7s) + hold
    subprocess.run(
        ["pebble", "emu-tap", "--emulator", platform],
        cwd=project_dir, capture_output=True)
    events["tap"] = time.perf_counter() - t0

t0 = time.perf_counter()
threading.Thread(target=run_events, daemon=True).start()

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
events["fps"] = FPS
events["frames"] = i
with open(os.path.join(frames_dir, "events.json"), "w") as f:
    json.dump(events, f)
print(platform, "captured", i, "frames, events:", events)

cmd._close_pebble_connection(pebble)
cmd._shutdown_platform_emulator(platform, None)
