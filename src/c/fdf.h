#pragma once
#include <pebble.h>

// FdF-style wireframe heightmap model: a grid of altitudes rendered in
// isometric projection, exactly like the 42 school project, with integer-only
// math (sin 30 = 1/2 exactly; cos 30 ~= 887/1024).

#define FDF_COLS 16
#define FDF_ROWS 25
#define FDF_Z_TOP 10  // altitude of extruded digit cells, as in 42.fdf

typedef struct {
  uint8_t z_from[FDF_ROWS][FDF_COLS];  // altitudes before the current morph
  uint8_t z_to[FDF_ROWS][FDF_COLS];    // altitudes being morphed towards
  uint16_t morph;                      // 0..65535 progress from z_from to z_to
  int32_t angle;                       // TRIG_MAX_ANGLE units; 0 = canonical iso
  int32_t zoom8;                       // pixels per cell, 8-bit fraction
  GPoint center;                       // screen-space center of the model
} FdfModel;

void fdf_model_init(FdfModel *m, GRect bounds);
// Snapshot current (interpolated) altitudes into z_from, write the new time
// into z_to and reset morph so an animation can play.
void fdf_model_set_time(FdfModel *m, int hours, int minutes);
void fdf_model_set_demo42(FdfModel *m);
void fdf_draw(FdfModel *m, GContext *ctx);
