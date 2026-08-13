#include "fdf.h"
#include "digits.h"

// Layout of the heightmap grid (in cells): two digit pairs stacked
// vertically (HH above, MM below), strokes 2 cells thick so digits read as
// flat-top plateaus exactly like the original 42.fdf map. Borders and gaps
// are kept minimal: every cell of grid costs projected pixels.
#define BORDER 1
#define DIGIT_W (DIGIT_FONT_COLS * 2)   // 6
#define DIGIT_H (DIGIT_FONT_ROWS * 2)   // 10
#define DIGIT_GAP 2
#define ROW_GAP 3
// Inner region: 2*BORDER + 2*DIGIT_W + DIGIT_GAP = 16 cols,
//               2*BORDER + 2*DIGIT_H + ROW_GAP  = 25 rows.
#define INNER_COLS (FDF_COLS - 2 * FDF_BLEED)
#define INNER_ROWS (FDF_ROWS - 2 * FDF_BLEED)
// Extrusion height: FDF_Z_TOP maps to ~1.25 cell spacings — tall enough to
// pop, low enough that plateau tops don't occlude the row behind them.
#define Z_NUM 1
#define Z_DEN 8

static GPoint s_pts[FDF_ROWS][FDF_COLS];

static void prv_place_digit(uint8_t z[FDF_ROWS][FDF_COLS], int digit,
                            int col0, int row0) {
  for (int fr = 0; fr < DIGIT_FONT_ROWS; fr++) {
    for (int fc = 0; fc < DIGIT_FONT_COLS; fc++) {
      if (DIGIT_FONT[digit][fr] & (1 << (DIGIT_FONT_COLS - 1 - fc))) {
        for (int dy = 0; dy < 2; dy++) {
          for (int dx = 0; dx < 2; dx++) {
            z[row0 + fr * 2 + dy][col0 + fc * 2 + dx] = FDF_Z_TOP;
          }
        }
      }
    }
  }
}

static void prv_place_pair(uint8_t z[FDF_ROWS][FDF_COLS], int value, int row0) {
  int col0 = FDF_BLEED + BORDER;
  prv_place_digit(z, value / 10, col0, FDF_BLEED + row0);
  prv_place_digit(z, value % 10, col0 + DIGIT_W + DIGIT_GAP, FDF_BLEED + row0);
}

static uint16_t prv_z8_at(const FdfModel *m, int y, int x) {
  int from8 = m->z_from[y][x] << 8;
  int to8 = m->z_to[y][x] << 8;
  return from8 + (((to8 - from8) * (int32_t)m->morph) >> 16);
}

static void prv_snapshot_current(FdfModel *m) {
  for (int y = 0; y < FDF_ROWS; y++) {
    for (int x = 0; x < FDF_COLS; x++) {
      m->z_from[y][x] = prv_z8_at(m, y, x) >> 8;
    }
  }
  memset(m->z_to, 0, sizeof(m->z_to));
  m->morph = 0;
}

void fdf_model_init(FdfModel *m, GRect bounds) {
  memset(m, 0, sizeof(*m));

  int margin = PBL_IF_ROUND_ELSE(10, 2);
  int avail_x = bounds.size.w - 2 * margin;
  int avail_y = bounds.size.h - 2 * margin - 14;  // ~14 px extrusion room

  // Trimetric projection: the digit-row axis gets a gentle 22 deg slope
  // (readable baseline; the classic 30 deg iso tilts digits more than
  // needed), while the HH/MM stack axis is steeper so the model spends the
  // screen's height instead of letterboxing. The stack angle is chosen by
  // trying candidates and keeping whichever maximizes the fitted zoom.
  m->ax_cos = cos_lookup(DEG_TO_TRIGANGLE(22)) >> 6;  // 1024-scale
  m->ax_sin = sin_lookup(DEG_TO_TRIGANGLE(22)) >> 6;
  m->zoom8 = 0;
  for (int deg = 45; deg <= 70; deg += 5) {
    int32_t yc = cos_lookup(DEG_TO_TRIGANGLE(deg)) >> 6;
    int32_t ys = sin_lookup(DEG_TO_TRIGANGLE(deg)) >> 6;
    int32_t span_w = (INNER_COLS - 1) * m->ax_cos + (INNER_ROWS - 1) * yc;
    int32_t span_h = (INNER_COLS - 1) * m->ax_sin + (INNER_ROWS - 1) * ys;
    int32_t zx = ((int32_t)avail_x << 18) / span_w;
    int32_t zy = ((int32_t)avail_y << 18) / span_h;
    int32_t zoom = zx < zy ? zx : zy;
    if (zoom > m->zoom8) {
      m->zoom8 = zoom;
      m->ay_cos = yc;
      m->ay_sin = ys;
    }
  }

  m->center = GPoint(bounds.origin.x + bounds.size.w / 2,
                     bounds.origin.y + bounds.size.h / 2);
}

void fdf_model_set_time(FdfModel *m, int hours, int minutes) {
  prv_snapshot_current(m);
  prv_place_pair(m->z_to, hours, BORDER);
  prv_place_pair(m->z_to, minutes, BORDER + DIGIT_H + ROW_GAP);
}

void fdf_model_set_demo42(FdfModel *m) {
  prv_snapshot_current(m);
  prv_place_pair(m->z_to, 42, (INNER_ROWS - DIGIT_H) / 2);
}

// Edge classes drive the visual hierarchy. Plateau-top edges (both ends
// high) trace the digit outline and must dominate; slope walls and the flat
// base must recede so digit counters/holes stay readable.
// Color displays: recede via dark colors. 1-bit displays: no recessive
// color exists, so the base mesh is dropped except the plateau perimeter and
// tops get a bold stroke instead.

void fdf_draw(FdfModel *m, GContext *ctx) {
  int32_t sinv = sin_lookup(m->angle);
  int32_t cosv = cos_lookup(m->angle);
  int32_t cx8 = ((FDF_COLS - 1) << 8) / 2;
  int32_t cy8 = ((FDF_ROWS - 1) << 8) / 2;
  int32_t zheight8 = (m->zoom8 * Z_NUM) / Z_DEN;  // px<<8 per z<<8, scaled <<16

  // Transform every grid point: center, rotate around z, isometric-project.
  for (int y = 0; y < FDF_ROWS; y++) {
    for (int x = 0; x < FDF_COLS; x++) {
      int32_t fx8 = (x << 8) - cx8;
      int32_t fy8 = (y << 8) - cy8;
      int32_t rx8 = (fx8 * cosv - fy8 * sinv) >> 16;
      int32_t ry8 = (fx8 * sinv + fy8 * cosv) >> 16;
      int32_t z8 = prv_z8_at(m, y, x);

      int32_t sx8 = (rx8 * m->zoom8) >> 12;
      int32_t sy8 = (ry8 * m->zoom8) >> 12;
      int32_t px = (sx8 * m->ax_cos - sy8 * m->ay_cos) >> 14;
      int32_t py = (sx8 * m->ax_sin + sy8 * m->ay_sin) >> 14;
      py -= (z8 * zheight8) >> 16;

      s_pts[y][x] = GPoint(m->center.x + px, m->center.y + py);
    }
  }

  // Wireframe: connect each point to its right and bottom neighbor.
  GColor last = GColorClear;
  uint8_t last_w = 1;
  graphics_context_set_stroke_width(ctx, 1);
  for (int y = 0; y < FDF_ROWS; y++) {
    for (int x = 0; x < FDF_COLS; x++) {
      uint16_t z_here = prv_z8_at(m, y, x);
      for (int dir = 0; dir < 2; dir++) {
        int nx = dir == 0 ? x + 1 : x;
        int ny = dir == 0 ? y : y + 1;
        if (nx >= FDF_COLS || ny >= FDF_ROWS) {
          continue;
        }
        uint16_t z_n = prv_z8_at(m, ny, nx);
        uint16_t z_min = z_here < z_n ? z_here : z_n;
        uint16_t z_max = z_here > z_n ? z_here : z_n;
        bool is_top = z_min >= (7 << 8);
        bool is_ground = z_max < (2 << 8);

        GColor c;
        uint8_t w = 1;
#if defined(PBL_COLOR)
        c = is_top ? GColorWhite : GColorDarkGreen;  // walls recede with the base
        (void)is_ground;
#else
        if (is_top) {
          c = GColorWhite;
          w = 3;
        } else if (is_ground) {
          // Base mesh at half density: with full-screen cells it reads as a
          // recessive texture instead of noise.
          bool sparse_keep = (dir == 0 ? y : x) % 2 == 0;
          if (!sparse_keep) {
            continue;
          }
          c = GColorWhite;
        } else {
          continue;  // walls: even thin lines fill digit holes on 1-bit
        }
#endif
        if (w != last_w) {
          graphics_context_set_stroke_width(ctx, w);
          last_w = w;
        }
        if (!gcolor_equal(c, last)) {
          graphics_context_set_stroke_color(ctx, c);
          last = c;
        }
        graphics_draw_line(ctx, s_pts[y][x], s_pts[ny][nx]);
      }
    }
  }
}
