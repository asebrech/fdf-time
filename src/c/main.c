#include <pebble.h>
#include "fdf.h"

#define MORPH_DURATION_MS 700
#define SECONDS_MORPH_MS 500
#define SPIN_DURATION_MS 1400
#define SPLASH_MS 1500

// User settings, edited from the phone via Clay and persisted on the watch.
#define SETTINGS_KEY 1
// Sticky-toggle state (shake_action 3): persisted so leaving and returning
// to the watchface resumes on the seconds view the user toggled into.
#define PEEK_KEY 2
typedef struct {
  uint8_t theme;        // palette index, see fdf_set_style (0 = tokyo night)
  uint8_t wave_mode;    // 0 fluid (second ticks), 1 eco (minute drift),
                        // 2 frozen, 3 silk (continuous ~15 fps timer)
  uint8_t gradient;     // per-line wall gradients on/off
  uint8_t display_mode; // 0 classic HH/MM terrain, 1 seconds SS terrain
  bool splash42;        // play the "42" splash on launch
  uint8_t shake_action; // 0 off, 1 orbit spin, 2 peek at seconds (reverts
                        // at the minute), 3 toggle seconds (sticky until the
                        // next shake). 2/3 exist in classic mode only;
                        // values match the pre-select boolean so persisted
                        // settings keep meaning orbit.
  bool bt_vibe;         // double pulse when the phone connection drops
} Settings;

static Settings s_settings;

static Window *s_window;
static Layer *s_layer;
static FdfModel s_model;
static Animation *s_morph_anim;
static Animation *s_spin_anim;
static AppTimer *s_splash_timer;
static bool s_bt_connected;
static char s_hhmm[8];  // seconds mode: small HH:MM drawn above the scene
// Seconds peek (classic mode + shake_action 2): the terrain shows SS until
// the minute rolls over — the revert then IS the classic minute morph — or
// until a second flick. Entry shakes fire multiple taps, hence the debounce.
static bool s_peeking;
static uint64_t s_peek_entered_ms;
// Silk wave mode: a repeating timer interpolates the swell phase between
// seconds. ~15 fps is ample — the swell moves a fraction of a cell per
// second, so the per-frame delta is sub-pixel smooth.
static AppTimer *s_wave_timer;
#define WAVE_FRAME_MS 66
#define WAVE_PERIOD_MS 30000  // one wavelength / 30 s, same pace as Fluid

static void prv_update_proc(Layer *layer, GContext *ctx) {
  // AA is fine at the current cell size (~6 px); it smeared into noise at
  // the pre-trimetric density. No-op on 1-bit displays.
  graphics_context_set_antialiased(ctx, true);
  fdf_draw(&s_model, ctx);
  if (s_settings.display_mode == 1 || s_peeking) {
    // Seconds mode: the terrain shows SS; HH:MM floats small over the
    // ocean's top band in the theme's foreground color.
    GRect b = layer_get_bounds(layer);
    graphics_context_set_text_color(ctx, fdf_top_color());
    graphics_draw_text(ctx, s_hhmm,
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, PBL_IF_ROUND_ELSE(10, 2), b.size.w, 28),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

// --- morph animation (altitudes z_from -> z_to) ---

static void prv_morph_update(Animation *anim, AnimationProgress p) {
  s_model.morph = p > 65535 ? 65535 : p;
  layer_mark_dirty(s_layer);
}

static void prv_morph_stopped(Animation *anim, bool finished, void *ctx) {
  s_morph_anim = NULL;
  s_model.morph = 65535;
  layer_mark_dirty(s_layer);
}

static const AnimationImplementation MORPH_IMPL = {
  .update = prv_morph_update,
};

static void prv_start_morph_ms(uint32_t ms) {
  if (s_morph_anim) {
    animation_unschedule(s_morph_anim);
  }
  s_morph_anim = animation_create();
  animation_set_duration(s_morph_anim, ms);
  animation_set_curve(s_morph_anim, AnimationCurveEaseOut);
  animation_set_implementation(s_morph_anim, &MORPH_IMPL);
  animation_set_handlers(s_morph_anim, (AnimationHandlers) {
    .stopped = prv_morph_stopped,
  }, NULL);
  animation_schedule(s_morph_anim);
}

// --- spin animation (full orbit on wrist flick, the FdF rotation bonus) ---

static void prv_spin_update(Animation *anim, AnimationProgress p) {
  // TRIG_MAX_ANGLE == 0x10000, so progress maps directly to one full turn.
  s_model.angle = p;
  layer_mark_dirty(s_layer);
}

static void prv_spin_stopped(Animation *anim, bool finished, void *ctx) {
  s_spin_anim = NULL;
  s_model.angle = 0;
  layer_mark_dirty(s_layer);
}

static const AnimationImplementation SPIN_IMPL = {
  .update = prv_spin_update,
};

static void prv_show_time(void);
static void prv_show_seconds_now(void);
static void prv_subscribe_ticks(void);

static uint64_t prv_now_ms(void) {
  time_t s;
  uint16_t ms = time_ms(&s, NULL);
  return (uint64_t)s * 1000 + ms;
}

static void prv_start_spin(void) {
  if (s_spin_anim) {
    return;
  }
  s_spin_anim = animation_create();
  animation_set_duration(s_spin_anim, SPIN_DURATION_MS);
  animation_set_curve(s_spin_anim, AnimationCurveEaseInOut);
  animation_set_implementation(s_spin_anim, &SPIN_IMPL);
  animation_set_handlers(s_spin_anim, (AnimationHandlers) {
    .stopped = prv_spin_stopped,
  }, NULL);
  animation_schedule(s_spin_anim);
}

static void prv_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_splash_timer) {
    return;
  }
  // The peek/toggle gestures only exist in classic display mode; the
  // dedicated seconds mode keeps the orbit on shake.
  if ((s_settings.shake_action == 2 || s_settings.shake_action == 3) &&
      s_settings.display_mode == 0) {
    uint64_t now = prv_now_ms();
    if (now - s_peek_entered_ms < 1200) {
      return;  // one shake fires several taps — don't instantly exit
    }
    if (!s_peeking) {
      s_peeking = true;
      s_peek_entered_ms = now;
      prv_show_seconds_now();
      prv_subscribe_ticks();
    } else {
      s_peeking = false;
      prv_show_time();
      prv_subscribe_ticks();
    }
    if (s_settings.shake_action == 3) {
      persist_write_bool(PEEK_KEY, s_peeking);
    }
    return;
  }
  prv_start_spin();
}

// --- time handling ---

static int prv_display_hours(const struct tm *t) {
  int hours = t->tm_hour;
  if (!clock_is_24h_style()) {
    hours = hours % 12;
    if (hours == 0) {
      hours = 12;
    }
  }
  return hours;
}

static void prv_show_seconds_now(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  snprintf(s_hhmm, sizeof(s_hhmm), "%d:%02d", prv_display_hours(t),
           t->tm_min);
  fdf_model_set_seconds(&s_model, t->tm_sec);
  prv_start_morph_ms(SECONDS_MORPH_MS);
}

static void prv_show_time(void) {
  if (s_settings.display_mode == 1 || s_peeking) {
    prv_show_seconds_now();
    return;
  }
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  fdf_model_set_time(&s_model, prv_display_hours(t), t->tm_min);
  prv_start_morph_ms(MORPH_DURATION_MS);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // During the splash the 42 owns the stage: hold the time morph until
  // prv_splash_done plays it (the swell below still rolls).
  if (!s_splash_timer && s_peeking) {
    if ((units_changed & MINUTE_UNIT) && s_settings.shake_action == 2) {
      // Peek variant: it ends where the classic minute morph begins — the
      // seconds finish the minute and the terrain morphs back into the new
      // time. The sticky variant (3) stays until the next shake.
      s_peeking = false;
      prv_show_time();
      prv_subscribe_ticks();
    } else {
      prv_show_seconds_now();
    }
  } else if (!s_splash_timer &&
             (s_settings.display_mode == 1 || (units_changed & MINUTE_UNIT))) {
    prv_show_time();
  }
#if defined(PBL_COLOR)
  // The terrain swell rolls with the phase: one wavelength every 30 ticks,
  // continuous across the minute boundary. Fluid mode ticks every second;
  // eco mode drifts one step per minute; frozen keeps phase 0; silk is
  // driven by its own timer, not ticks. 1-bit has no terrain and always
  // stays on minute ticks.
  if (s_settings.wave_mode == 0) {
    s_model.wave_phase = tick_time->tm_sec * (TRIG_MAX_ANGLE / 30);
  } else if (s_settings.wave_mode == 1) {
    s_model.wave_phase = tick_time->tm_min * (TRIG_MAX_ANGLE / 30);
  }
  layer_mark_dirty(s_layer);
#endif
}

#if defined(PBL_COLOR)
static void prv_wave_timer_cb(void *data) {
  s_wave_timer = app_timer_register(WAVE_FRAME_MS, prv_wave_timer_cb, NULL);
  uint64_t ms = prv_now_ms();
  s_model.wave_phase =
      (int32_t)((ms % WAVE_PERIOD_MS) * TRIG_MAX_ANGLE / WAVE_PERIOD_MS);
  layer_mark_dirty(s_layer);
}
#endif

static void prv_splash_done(void *data) {
  s_splash_timer = NULL;
  prv_show_time();
}

// --- settings ---

static void prv_bt_handler(bool connected) {
  if (s_settings.bt_vibe && s_bt_connected && !connected) {
    vibes_double_pulse();
  }
  s_bt_connected = connected;
}

// Tick granularity depends on mode AND transient state (the seconds peek
// temporarily needs second ticks, even on 1-bit).
static void prv_subscribe_ticks(void) {
  TimeUnits unit = MINUTE_UNIT;
  if (s_settings.display_mode == 1 || s_peeking) {
    unit = SECOND_UNIT;
  }
#if defined(PBL_COLOR)
  if (s_settings.wave_mode == 0) {
    unit = SECOND_UNIT;
  }
#endif
  tick_timer_service_subscribe(unit, prv_tick_handler);
}

// (Re)wire every service the settings influence. Safe to call repeatedly:
// re-subscribing replaces the previous subscription.
static void prv_apply_settings(void) {
  fdf_set_style(s_settings.theme, s_settings.gradient != 0);
  fdf_model_set_mode(&s_model, s_settings.display_mode == 1);
  if (s_settings.display_mode == 1 ||
      (s_settings.shake_action != 2 && s_settings.shake_action != 3)) {
    s_peeking = false;  // the peek/toggle only lives in classic mode
    persist_write_bool(PEEK_KEY, false);
  }
#if defined(PBL_COLOR)
  if (s_settings.wave_mode == 2) {
    s_model.wave_phase = 0;
  }
  if (s_wave_timer) {
    app_timer_cancel(s_wave_timer);
    s_wave_timer = NULL;
  }
  if (s_settings.wave_mode == 3) {
    s_wave_timer = app_timer_register(WAVE_FRAME_MS, prv_wave_timer_cb, NULL);
  }
#endif
  prv_subscribe_ticks();

  if (s_settings.shake_action != 0) {
    accel_tap_service_subscribe(prv_tap_handler);
  } else {
    accel_tap_service_unsubscribe();
  }

  s_bt_connected = connection_service_peek_pebble_app_connection();
  if (s_settings.bt_vibe) {
    connection_service_subscribe((ConnectionHandlers) {
      .pebble_app_connection_handler = prv_bt_handler,
    });
  } else {
    connection_service_unsubscribe();
  }

  if (s_layer) {
    layer_mark_dirty(s_layer);
  }
}

// Clay sends select values as strings and toggles as ints; accept both.
static int prv_tuple_int(const Tuple *t) {
  return t->type == TUPLE_CSTRING ? atoi(t->value->cstring)
                                  : (int)t->value->int32;
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_Theme))) {
    s_settings.theme = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WaveMode))) {
    s_settings.wave_mode = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Gradient))) {
    s_settings.gradient = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Mode))) {
    s_settings.display_mode = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Splash42))) {
    s_settings.splash42 = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ShakeOrbit))) {
    s_settings.shake_action = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_BtVibe))) {
    s_settings.bt_vibe = prv_tuple_int(t) != 0;
  }
  if (s_settings.display_mode == 1 &&
      (s_settings.shake_action == 2 || s_settings.shake_action == 3)) {
    s_settings.shake_action = 1;  // peek/toggle don't exist in seconds mode
  }
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  prv_apply_settings();
  if (!s_splash_timer) {
    prv_show_time();  // re-render immediately in the (possibly new) mode
  }
}

static void prv_load_settings(void) {
  s_settings = (Settings) {
    .theme = 0,  // Tokyo Night — closest heir to the original FdF look
    .wave_mode = 0,
    .gradient = 1,
    .display_mode = 0,
    .splash42 = true,
    .shake_action = 1,
    .bt_vibe = false,
  };
  if (persist_exists(SETTINGS_KEY)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  }
  // Resume a persisted sticky toggle (the splash then morphs into the
  // seconds view instead of the time).
  s_peeking = s_settings.shake_action == 3 && s_settings.display_mode == 0 &&
              persist_read_bool(PEEK_KEY);
}

// --- window lifecycle ---

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, prv_update_proc);
  layer_add_child(root, s_layer);

  fdf_model_init(&s_model, bounds);

#if defined(PBL_COLOR)
  // Seed the swell phase from the wall clock so the first tick continues
  // the motion instead of teleporting the waves.
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  if (s_settings.wave_mode == 0) {
    s_model.wave_phase = lt->tm_sec * (TRIG_MAX_ANGLE / 30);
  } else if (s_settings.wave_mode == 1) {
    s_model.wave_phase = lt->tm_min * (TRIG_MAX_ANGLE / 30);
  }
#endif

  // Homage splash: the original 42.fdf, then morph into the time. Ticks
  // are live during the splash so the swell rolls; the tick handler holds
  // back the time morph until the splash is done.
  prv_apply_settings();
  if (s_settings.splash42) {
    fdf_model_set_demo42(&s_model);
    prv_start_morph_ms(MORPH_DURATION_MS);
    s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);
  } else {
    prv_show_time();
  }
}

static void prv_window_unload(Window *window) {
  if (s_splash_timer) {
    app_timer_cancel(s_splash_timer);
    s_splash_timer = NULL;
  }
  animation_unschedule_all();
#if defined(PBL_COLOR)
  if (s_wave_timer) {
    app_timer_cancel(s_wave_timer);
    s_wave_timer = NULL;
  }
#endif
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  connection_service_unsubscribe();
  layer_destroy(s_layer);
}

static void prv_init(void) {
  prv_load_settings();
  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(256, 64);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
