#!/usr/bin/env python3
"""Apply / revert the temporary source tweaks that make a store-asset
capture possible. These NEVER ship: main.c is backed up first and restored
by `revert`, and the release build is made from the restored source.

  capture_patch.py heart   -> long heart splash, realistic forced BPM
  capture_patch.py themes  -> rotate the palette every 1.6 s
  capture_patch.py battery -> long battery splash at a photogenic level
  capture_patch.py revert  -> restore the pristine source
"""
import io
import os
import shutil
import sys

MAIN = "/home/alo/Projects/fdf-time/src/c/main.c"
BACKUP = "/tmp/fdf-time-main.c.pristine"


def read():
    return io.open(MAIN, encoding="utf-8").read()


def write(s):
    io.open(MAIN, "w", encoding="utf-8").write(s)


def backup():
    if not os.path.exists(BACKUP):
        shutil.copy2(MAIN, BACKUP)


mode = sys.argv[1]

if mode == "revert":
    if not os.path.exists(BACKUP):
        sys.exit("no backup to restore")
    shutil.copy2(BACKUP, MAIN)
    os.remove(BACKUP)
    print("source restored")
    sys.exit(0)

backup()
s = read()

if mode == "heart":
    s = s.replace("#define SPLASH_MS 1500", "#define SPLASH_MS 9000", 1)
    s = s.replace(".splash_style = 5,", ".splash_style = 9,", 1)
    # QEMU serves a synthetic ~20 bpm, which would look broken in a store
    # shot; pin a resting rate and stop live updates from overwriting it.
    s = s.replace(
        "  s_heart_bpm = (int)health_service_peek_current_value("
        "HealthMetricHeartRateBPM);\n  s_heart_ppi_ms = 0;",
        "  s_heart_bpm = 72;\n  s_heart_ppi_ms = 0;", 1)
    s = s.replace("""static void prv_health_handler(HealthEventType event, void *context) {
  if (!s_heart_active) {
    return;
  }""",
"""static void prv_health_handler(HealthEventType event, void *context) {
  return;  /* capture build: hold the pinned BPM */
  if (!s_heart_active) {
    return;
  }""", 1)
    s = s.replace("""static bool prv_heart_ok(void) {
#if defined(PBL_HEALTH)
  time_t now = time(NULL);""",
"""static bool prv_heart_ok(void) {
#if defined(PBL_HEALTH)
  return true;  /* capture build: QEMU reports the metric unavailable */
  time_t now = time(NULL);""", 1)

elif mode == "battery":
    s = s.replace("#define SPLASH_MS 1500", "#define SPLASH_MS 9000", 1)
    s = s.replace(".splash_style = 5,", ".splash_style = 8,", 1)

elif mode == "themes":
    s = s.replace("""static void prv_wave_timer_cb(void *data) {
  s_wave_timer = app_timer_register(WAVE_FRAME_MS, prv_wave_timer_cb, NULL);""",
"""static uint64_t s_theme_t0;  /* capture build */
static void prv_wave_timer_cb(void *data) {
  s_wave_timer = app_timer_register(WAVE_FRAME_MS, prv_wave_timer_cb, NULL);
  /* Capture build: walk the palette so one clip shows every theme. */
  if (!s_theme_t0) { s_theme_t0 = prv_now_ms(); }
  fdf_set_style((int)(((prv_now_ms() - s_theme_t0) / 1600) % 7),
                s_settings.gradient != 0);""", 1)
else:
    sys.exit("unknown mode " + mode)

write(s)
print("patched for", mode)
