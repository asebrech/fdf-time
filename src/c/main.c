#include <pebble.h>
#include "fdf.h"

#define MORPH_DURATION_MS 700
#define SPIN_DURATION_MS 1400
#define SPLASH_MS 1500

// User settings, edited from the phone via Clay and persisted on the watch.
#define SETTINGS_KEY 1
typedef struct {
  uint8_t theme;        // palette index, see fdf_set_style (0 = catppuccin)
  uint8_t wave_mode;    // 0 fluid (second ticks), 1 eco (minute drift), 2 frozen
  uint8_t relief;       // slope-gradient cap: 0 subtle, 1 balanced, 2 vivid
  bool splash42;        // play the "42" splash on launch
  bool shake_orbit;     // orbit spin on accelerometer tap
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

static void prv_update_proc(Layer *layer, GContext *ctx) {
  // AA is fine at the current cell size (~6 px); it smeared into noise at
  // the pre-trimetric density. No-op on 1-bit displays.
  graphics_context_set_antialiased(ctx, true);
  fdf_draw(&s_model, ctx);
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

static void prv_start_morph(void) {
  if (s_morph_anim) {
    animation_unschedule(s_morph_anim);
  }
  s_morph_anim = animation_create();
  animation_set_duration(s_morph_anim, MORPH_DURATION_MS);
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

static void prv_tap_handler(AccelAxisType axis, int32_t direction) {
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

// --- time handling ---

static void prv_show_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int hours = t->tm_hour;
  if (!clock_is_24h_style()) {
    hours = hours % 12;
    if (hours == 0) {
      hours = 12;
    }
  }
  fdf_model_set_time(&s_model, hours, t->tm_min);
  prv_start_morph();
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // During the splash the 42 owns the stage: hold the time morph until
  // prv_splash_done plays it (the swell below still rolls).
  if ((units_changed & MINUTE_UNIT) && !s_splash_timer) {
    prv_show_time();
  }
#if defined(PBL_COLOR)
  // The terrain swell rolls with the phase: one wavelength every 30 ticks,
  // continuous across the minute boundary. Fluid mode ticks every second;
  // eco mode drifts one step per minute; frozen keeps phase 0. 1-bit has
  // no terrain and always stays on minute ticks.
  if (s_settings.wave_mode == 0) {
    s_model.wave_phase = tick_time->tm_sec * (TRIG_MAX_ANGLE / 30);
  } else if (s_settings.wave_mode == 1) {
    s_model.wave_phase = tick_time->tm_min * (TRIG_MAX_ANGLE / 30);
  }
  layer_mark_dirty(s_layer);
#endif
}

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

// (Re)wire every service the settings influence. Safe to call repeatedly:
// re-subscribing replaces the previous subscription.
static void prv_apply_settings(void) {
  fdf_set_style(s_settings.theme, s_settings.relief);

  TimeUnits unit = MINUTE_UNIT;
#if defined(PBL_COLOR)
  if (s_settings.wave_mode == 0) {
    unit = SECOND_UNIT;
  } else if (s_settings.wave_mode == 2) {
    s_model.wave_phase = 0;
  }
#endif
  tick_timer_service_subscribe(unit, prv_tick_handler);

  if (s_settings.shake_orbit) {
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
  if ((t = dict_find(iter, MESSAGE_KEY_Relief))) {
    s_settings.relief = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Splash42))) {
    s_settings.splash42 = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ShakeOrbit))) {
    s_settings.shake_orbit = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_BtVibe))) {
    s_settings.bt_vibe = prv_tuple_int(t) != 0;
  }
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  prv_apply_settings();
}

static void prv_load_settings(void) {
  s_settings = (Settings) {
    .theme = 0,
    .wave_mode = 0,
    .relief = 1,
    .splash42 = true,
    .shake_orbit = true,
    .bt_vibe = false,
  };
  if (persist_exists(SETTINGS_KEY)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  }
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
  if (s_settings.splash42) {
    fdf_model_set_demo42(&s_model);
    prv_start_morph();
    s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);
  } else {
    prv_show_time();
  }
  prv_apply_settings();
}

static void prv_window_unload(Window *window) {
  if (s_splash_timer) {
    app_timer_cancel(s_splash_timer);
    s_splash_timer = NULL;
  }
  animation_unschedule_all();
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
