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
// Extrusion height: FDF_Z_TOP maps to ~1.9 cell spacings — just under the
// ROW_GAP's projected height, so MM plateau tops still clear HH's bottom row.
#define Z_NUM 3
#define Z_DEN 16

static GPoint s_pts[FDF_ROWS][FDF_COLS];
static uint16_t s_z8[FDF_ROWS][FDF_COLS];
#if defined(PBL_COLOR)
// Color-key altitudes: equal to s_z8 except on cells morphing DOWN, where
// they drop to the destination immediately — old digits melt away in base
// blues instead of flashing back through the warm end of the ramp.
static uint16_t s_ck8[FDF_ROWS][FDF_COLS];
#endif

#if defined(PBL_COLOR)
// Altitude palettes, indexed by z (0..FDF_Z_TOP): dark lowlands rising to
// bright peaks, one ramp per user-selectable theme. Every ramp must keep
// its low indices recessive (walls/floors) and save its brightest steps
// for the top — the legibility hierarchy depends on it.
// All are hand-quantized ports of popular editor color schemes. Pebble has
// 4 levels per channel, so each entry picks the nearest VIVID Pebble color
// that keeps the scheme's hue identity (naive rounding turns pastel schemes
// into washed-out gray).
static const GColor8 PALETTES[][FDF_Z_TOP + 1] = {
  { // 0: Catppuccin Mocha — night-lavender base, pastel accents. Wall
    // body stays on the dark overlays (#6c7086 -> Liberty) so the mauve
    // tips and the tops keep their pop; pastels are for accents, not
    // large areas.
    {.argb = GColorOxfordBlueARGB8},     {.argb = GColorDarkGrayARGB8},
    {.argb = GColorLibertyARGB8},        {.argb = GColorLibertyARGB8},
    {.argb = GColorRichBrilliantLavenderARGB8}, {.argb = GColorPictonBlueARGB8},
    {.argb = GColorCelesteARGB8},        {.argb = GColorMintGreenARGB8},
    {.argb = GColorPastelYellowARGB8},   {.argb = GColorMelonARGB8},
    {.argb = GColorBrilliantRoseARGB8},
  },
  { // 1: Gruvbox — retro warmth: olive-brown ground, amber walls (red
    // read as pure fire; plain orange read salmon on thin AA lines)
    {.argb = GColorDarkGrayARGB8},       {.argb = GColorArmyGreenARGB8},
    {.argb = GColorArmyGreenARGB8},      {.argb = GColorWindsorTanARGB8},
    {.argb = GColorChromeYellowARGB8},   {.argb = GColorBrassARGB8},
    {.argb = GColorLimerickARGB8},       {.argb = GColorRajahARGB8},
    {.argb = GColorPastelYellowARGB8},   {.argb = GColorPastelYellowARGB8},
    {.argb = GColorPastelYellowARGB8},
  },
  { // 2: Nord — polar night floor, frost mids, aurora morph
    {.argb = GColorOxfordBlueARGB8},     {.argb = GColorMidnightGreenARGB8},
    {.argb = GColorDarkGrayARGB8},       {.argb = GColorCadetBlueARGB8},
    {.argb = GColorBabyBlueEyesARGB8},   {.argb = GColorCelesteARGB8},
    {.argb = GColorCelesteARGB8},        {.argb = GColorMintGreenARGB8},
    {.argb = GColorPastelYellowARGB8},   {.argb = GColorMelonARGB8},
    {.argb = GColorSunsetOrangeARGB8},
  },
  { // 3: Tokyo Night — deep navy, neon blue walls, city-light morph.
    // Large areas (wall body 2-3 = blue0 #3d59a1, crests 5-6 = cyan
    // #7dcfff / teal #73daca) use the scheme's SATURATED blues; the pastel
    // purple lives in the morph sweep — pastels on big areas read white
    // and kill the tops' contrast.
    {.argb = GColorOxfordBlueARGB8},     {.argb = GColorOxfordBlueARGB8},
    {.argb = GColorCobaltBlueARGB8},     {.argb = GColorCobaltBlueARGB8},
    {.argb = GColorPictonBlueARGB8},     {.argb = GColorElectricBlueARGB8},
    {.argb = GColorMediumSpringGreenARGB8}, {.argb = GColorSpringBudARGB8},
    {.argb = GColorRajahARGB8},          {.argb = GColorBrilliantRoseARGB8},
    {.argb = GColorBabyBlueEyesARGB8},
  },
  { // 4: Kanagawa — Hokusai wave: deep teal sea, autumn-gold crests
    {.argb = GColorMidnightGreenARGB8},  {.argb = GColorMidnightGreenARGB8},
    {.argb = GColorCadetBlueARGB8},      {.argb = GColorPurpureusARGB8},
    {.argb = GColorPictonBlueARGB8},     {.argb = GColorBrassARGB8},
    {.argb = GColorRajahARGB8},          {.argb = GColorSunsetOrangeARGB8},
    {.argb = GColorChromeYellowARGB8},   {.argb = GColorPastelYellowARGB8},
    {.argb = GColorPastelYellowARGB8},
  },
  { // 5: Dracula — purple night, hot pink walls, green/cyan crests.
    // Wall body stays on the dark comment-blue; the pastel purple closes
    // the morph sweep, and the pale cyan sits at index 6 (only the rare
    // highest crest peaks) so large areas stay saturated.
    {.argb = GColorOxfordBlueARGB8},     {.argb = GColorDarkGrayARGB8},
    {.argb = GColorLibertyARGB8},        {.argb = GColorLibertyARGB8},
    {.argb = GColorBrilliantRoseARGB8},  {.argb = GColorScreaminGreenARGB8},
    {.argb = GColorCelesteARGB8},        {.argb = GColorPastelYellowARGB8},
    {.argb = GColorRajahARGB8},          {.argb = GColorSunsetOrangeARGB8},
    {.argb = GColorBabyBlueEyesARGB8},
  },
};
#define PALETTE_COUNT (sizeof(PALETTES) / sizeof(PALETTES[0]))

// Finished plateau tops take the theme's foreground ("text") color — the
// warm schemes' creams quantize to PastelYellow, the cool ones to White.
static const GColor8 TOPS[PALETTE_COUNT] = {
  {.argb = GColorWhiteARGB8},         // Catppuccin text #cdd6f4
  {.argb = GColorPastelYellowARGB8},  // Gruvbox fg #ebdbb2 (cream)
  {.argb = GColorWhiteARGB8},         // Nord snow storm #d8dee9
  {.argb = GColorWhiteARGB8},         // Tokyo Night fg #c0caf5
  {.argb = GColorPastelYellowARGB8},  // Kanagawa fujiWhite #dcd7ba
  {.argb = GColorWhiteARGB8},         // Dracula fg #f8f8f2
};

static const GColor8 *s_palette = PALETTES[0];
static GColor8 s_top = {.argb = GColorWhiteARGB8};
// Slope-gradient brightness cap (palette index): the knob behind the
// "relief intensity" setting. See the cap comment in fdf_draw.
static int s_grad_cap = 4;

void fdf_set_style(int theme, int relief) {
  unsigned t = (unsigned)theme < PALETTE_COUNT ? theme : 0;
  s_palette = PALETTES[t];
  s_top = TOPS[t];
  s_grad_cap = relief <= 0 ? 2 : relief >= 2 ? 6 : 4;
}

// Rolling terrain in the bleed ring: overlapping sine swells, capped low
// enough (z<=6, cool palette colors only) that the white digit plateaus
// stay the unambiguous foreground. Heights are computed at draw time with a
// fractional part and a phase, so the swell rolls smoothly as the phase
// advances with the seconds. 1-bit displays keep a flat base — the sparse
// ground mesh there can't afford wall edges (see 1-bit notes below).
#define TERRAIN_Z_MAX 6

static bool prv_in_ring(int y, int x) {
  return x < FDF_BLEED || x >= FDF_COLS - FDF_BLEED ||
         y < FDF_BLEED || y >= FDF_ROWS - FDF_BLEED;
}

static uint16_t prv_terrain8(int x, int y, int32_t phase) {
  int32_t v = sin_lookup((x * TRIG_MAX_ANGLE) / 7 + phase) +
              sin_lookup((y * TRIG_MAX_ANGLE) / 9 - phase) +
              sin_lookup(((2 * x + y) * TRIG_MAX_ANGLE) / 13 + 2 * phase);
  return ((v + 3 * TRIG_MAX_RATIO) * (TERRAIN_Z_MAX << 8)) /
         (6 * TRIG_MAX_RATIO);
}
#else
void fdf_set_style(int theme, int relief) {
  (void)theme;
  (void)relief;
}
#endif

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

static void prv_place_pair(uint8_t z[FDF_ROWS][FDF_COLS], int value, int col0,
                           int row0) {
  col0 += FDF_BLEED + BORDER;
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

  int margin = PBL_IF_ROUND_ELSE(12, 0);
  int avail_x = bounds.size.w - 2 * margin;
  int avail_y = bounds.size.h - 2 * margin;

  // The fit only guarantees the DIGIT plateaus on screen — border and bleed
  // ring are allowed to clip past the edges (that is what fills the screen
  // with mesh). The fitted shape is the union of the two staggered pair
  // boxes, tracked by their 8 corners, plus extrusion headroom on top.
  const int hc0 = FDF_BLEED + BORDER;
  const int hc1 = hc0 + 2 * DIGIT_W + DIGIT_GAP - 1;
  const int hr0 = FDF_BLEED + BORDER;
  const int hr1 = hr0 + DIGIT_H - 1;
  const int mr0 = hr1 + ROW_GAP + 1;
  const int corners[8][2] = {
    {hc0, hr0}, {hc1, hr0}, {hc0, hr1}, {hc1, hr1},
    {hc0 + FDF_STAGGER, mr0}, {hc1 + FDF_STAGGER, mr0},
    {hc0 + FDF_STAGGER, mr0 + DIGIT_H - 1},
    {hc1 + FDF_STAGGER, mr0 + DIGIT_H - 1},
  };

  // Trimetric projection: the digit-row axis gets a gentle 22 deg slope
  // (readable baseline; the classic 30 deg iso tilts digits more than
  // needed), while the HH/MM stack axis is steeper so the model spends the
  // screen's height instead of letterboxing. The stack angle is chosen by
  // trying candidates and keeping whichever maximizes the fitted zoom.
  m->ax_cos = cos_lookup(DEG_TO_TRIGANGLE(22)) >> 6;  // 1024-scale
  m->ax_sin = sin_lookup(DEG_TO_TRIGANGLE(22)) >> 6;
  m->zoom8 = 0;
  for (int deg = 35; deg <= 70; deg += 5) {
    int32_t yc = cos_lookup(DEG_TO_TRIGANGLE(deg)) >> 6;
    int32_t ys = sin_lookup(DEG_TO_TRIGANGLE(deg)) >> 6;
    int32_t minx = INT32_MAX, maxx = INT32_MIN;
    int32_t miny = INT32_MAX, maxy = INT32_MIN;
    for (int i = 0; i < 8; i++) {
      int32_t px = corners[i][0] * m->ax_cos - corners[i][1] * yc;
      int32_t py = corners[i][0] * m->ax_sin + corners[i][1] * ys;
      if (px < minx) minx = px;
      if (px > maxx) maxx = px;
      if (py < miny) miny = py;
      if (py > maxy) maxy = py;
    }
    int32_t span_w = maxx - minx;
    int32_t span_h = maxy - miny + (FDF_Z_TOP * 1024 * Z_NUM) / Z_DEN;
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
  // The extrusion headroom sits entirely above the grid plane; nudge the
  // model down by half of it so the lifted digits read centered.
  m->center.y += (m->zoom8 * FDF_Z_TOP * Z_NUM / Z_DEN) >> 9;
}

void fdf_model_set_time(FdfModel *m, int hours, int minutes) {
  prv_snapshot_current(m);
  prv_place_pair(m->z_to, hours, 0, BORDER);
  prv_place_pair(m->z_to, minutes, FDF_STAGGER, BORDER + DIGIT_H + ROW_GAP);
}

void fdf_model_set_demo42(FdfModel *m) {
  prv_snapshot_current(m);
  prv_place_pair(m->z_to, 42, FDF_STAGGER / 2, (INNER_ROWS - DIGIT_H) / 2);
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

  // Altitudes first: digit morph, then the rolling swell in the bleed ring
  // (color only — recomputed every frame so the phase animates it).
  for (int y = 0; y < FDF_ROWS; y++) {
    for (int x = 0; x < FDF_COLS; x++) {
      uint16_t z8 = prv_z8_at(m, y, x);
#if defined(PBL_COLOR)
      uint16_t ck8 = z8;
      uint16_t to8 = m->z_to[y][x] << 8;
      if (to8 < ck8) {
        ck8 = to8;
      }
      if (prv_in_ring(y, x)) {
        uint16_t t8 = prv_terrain8(x, y, m->wave_phase);
        if (t8 > z8) {
          z8 = t8;
        }
        if (t8 > ck8) {
          ck8 = t8;
        }
      }
      s_ck8[y][x] = ck8;
#endif
      s_z8[y][x] = z8;
    }
  }

  // Transform every grid point: center, rotate around z, isometric-project.
  for (int y = 0; y < FDF_ROWS; y++) {
    for (int x = 0; x < FDF_COLS; x++) {
      int32_t fx8 = (x << 8) - cx8;
      int32_t fy8 = (y << 8) - cy8;
      int32_t rx8 = (fx8 * cosv - fy8 * sinv) >> 16;
      int32_t ry8 = (fx8 * sinv + fy8 * cosv) >> 16;
      int32_t z8 = s_z8[y][x];

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
      for (int dir = 0; dir < 2; dir++) {
        int nx = dir == 0 ? x + 1 : x;
        int ny = dir == 0 ? y : y + 1;
        if (nx >= FDF_COLS || ny >= FDF_ROWS) {
          continue;
        }
        GColor c;
        uint8_t w = 1;
#if defined(PBL_COLOR)
        // FdF-style per-vertex coloring: each endpoint takes its own
        // altitude color and the line blends between them in short
        // segments — the wireframe equivalent of the original's per-pixel
        // interpolation. Finished tops take the theme's foreground color;
        // a wall is a small dark->accent gradient climbing to the rim, and
        // hole floors stay readable because the gradient is anchored dark
        // at the floor. Everything moves by one palette step at a time, so
        // no snaps.
        uint16_t ck_a = s_ck8[y][x];
        uint16_t ck_b = s_ck8[ny][nx];
        if (ck_a >= (9 << 8) && ck_b >= (9 << 8)) {
          c = (GColor)s_top;
        } else {
          int ia = ck_a >> 8;
          int ib = ck_b >> 8;
          if (ia != ib) {
            int di = ib - ia;
            int steps = di < 0 ? -di : di;
            if (steps > 4) {
              steps = 4;
            }
            GPoint pa = s_pts[y][x];
            GPoint pb = s_pts[ny][nx];
            for (int s = 0; s < steps; s++) {
              GPoint q0 = GPoint(pa.x + (pb.x - pa.x) * s / steps,
                                 pa.y + (pb.y - pa.y) * s / steps);
              GPoint q1 = GPoint(pa.x + (pb.x - pa.x) * (s + 1) / steps,
                                 pa.y + (pb.y - pa.y) * (s + 1) / steps);
              int si = ia + di * (2 * s + 1) / (2 * steps);
              // Cap slope gradients partway up the ramp (user-tunable via
              // the relief setting; default index 4): brighter segments
              // right under the white rims read as fuzzy thick strokes and
              // wreck glanceability (tried Malachite: green tips doubled
              // the rims; warm colors were worse still). The warm ramp
              // stays reserved for the morph sweep (equal-endpoint edges).
              if (si > s_grad_cap) {
                si = s_grad_cap;
              }
              GColor sc = (GColor)s_palette[si];
              if (!gcolor_equal(sc, last)) {
                graphics_context_set_stroke_color(ctx, sc);
                last = sc;
              }
              graphics_draw_line(ctx, q0, q1);
            }
            continue;
          }
          c = (GColor)s_palette[ia];
        }
#else
        uint16_t z_here = s_z8[y][x];
        uint16_t z_n = s_z8[ny][nx];
        uint16_t z_min = z_here < z_n ? z_here : z_n;
        uint16_t z_max = z_here > z_n ? z_here : z_n;
        if (z_min >= (7 << 8)) {
          c = GColorWhite;
          w = 3;
        } else if (z_max < (2 << 8)) {
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
