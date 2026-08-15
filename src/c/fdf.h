#pragma once
#include <pebble.h>

// FdF-style wireframe heightmap model: a grid of altitudes rendered in
// isometric projection, exactly like the 42 school project, with integer-only
// math (sin 30 = 1/2 exactly; cos 30 ~= 887/1024).

// Inner region (digits + 1-cell border) plus a "bleed" ring of terrain that
// intentionally projects past the screen edges so the mesh fills the corners.
// The MM pair is staggered right of HH: it cancels the trimetric lean (rows
// project down-LEFT), stacking the pairs vertically in screen space so the
// digit bounding box shrinks and the fitted zoom grows.
// Color displays get a wide ring (the swell needs room and the zoom now
// clips it at the edges); 1-bit keeps the narrow one — flat base, and
// aplite's RAM is tight.
#define FDF_BLEED PBL_IF_COLOR_ELSE(6, 3)
#define FDF_STAGGER 6
#define FDF_COLS (16 + FDF_STAGGER + 2 * FDF_BLEED)
#define FDF_ROWS (25 + 2 * FDF_BLEED)
#define FDF_Z_TOP 10  // altitude of extruded digit cells, as in 42.fdf

typedef struct {
  uint8_t z_from[FDF_ROWS][FDF_COLS];  // altitudes before the current morph
  uint8_t z_to[FDF_ROWS][FDF_COLS];    // altitudes being morphed towards
  uint16_t morph;                      // 0..65535 progress from z_from to z_to
  int32_t angle;                       // TRIG_MAX_ANGLE units; 0 = canonical view
  int32_t wave_phase;                  // terrain swell phase (color only)
  // Tilt sway: screen-space shear of the extrusion axis, px<<8 per z<<8
  // (same scale as the internal zheight8). Zero = straight extrusion; the
  // driver (main.c) feeds it smoothed accelerometer tilt so plateau tops
  // lean with the wrist while bases stay put.
  int32_t sway_x8, sway_y8;
  // Two fitted framings: classic (HH/MM staggered pairs) and pair (a single
  // centered pair — the seconds display mode, zoomed larger). zoom8/center
  // hold the active one, switched by fdf_model_set_mode.
  int32_t zoom8_classic, zoom8_pair;
  GPoint center_classic, center_pair;
  int32_t zoom8;                       // pixels per cell, 8-bit fraction
  // Trimetric axes, 1024-scale: the grid x axis (digit rows) projects along
  // (ax_cos, ax_sin), the y axis (HH/MM stack) along (-ay_cos, ay_sin).
  int32_t ax_cos, ax_sin;
  int32_t ay_cos, ay_sin;
  GPoint center;                       // screen-space center of the model
} FdfModel;

void fdf_model_init(FdfModel *m, GRect bounds);
// Select the altitude palette (0 tokyo night, 1 catppuccin, 2 dracula,
// 3 gruvbox, 4 kanagawa, 5 nord) and whether walls draw per-line
// gradients (else one solid color per edge). No-op on 1-bit displays.
void fdf_set_style(int theme, int gradient);
// Snapshot current (interpolated) altitudes into z_from, write the new time
// into z_to and reset morph so an animation can play.
void fdf_model_set_time(FdfModel *m, int hours, int minutes);
// A single centered pair in the classic framing (seconds/date peeks).
// Launch splash: 1 = the "42" homage, 2 = NixOS snowflake, 4 = Pebble
// slashed-e (3 was Arch, removed; unknown values fall back to "42").
// Values are persisted in settings — never renumber after a store release.
void fdf_model_set_splash(FdfModel *m, int style);
// Seconds mode: one centered pair morphing every second.
void fdf_model_set_seconds(FdfModel *m, int seconds);
void fdf_model_set_mode(FdfModel *m, bool seconds_mode);
// The active theme's plateau-top ("text") color; White on 1-bit.
GColor fdf_top_color(void);
void fdf_draw(FdfModel *m, GContext *ctx);
