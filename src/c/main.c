#include <pebble.h>
#include "fdf.h"

#define MORPH_DURATION_MS 700
#define SPIN_DURATION_MS 1400
#define SPLASH_MS 1500

static Window *s_window;
static Layer *s_layer;
static FdfModel s_model;
static Animation *s_morph_anim;
static Animation *s_spin_anim;
static AppTimer *s_splash_timer;

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
  prv_show_time();
}

static void prv_splash_done(void *data) {
  s_splash_timer = NULL;
  prv_show_time();
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

// --- window lifecycle ---

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, prv_update_proc);
  layer_add_child(root, s_layer);

  fdf_model_init(&s_model, bounds);

  // Homage splash: the original 42.fdf, then morph into the time.
  fdf_model_set_demo42(&s_model);
  prv_start_morph();
  s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);

  accel_tap_service_subscribe(prv_tap_handler);
}

static void prv_window_unload(Window *window) {
  if (s_splash_timer) {
    app_timer_cancel(s_splash_timer);
    s_splash_timer = NULL;
  }
  animation_unschedule_all();
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  layer_destroy(s_layer);
}

static void prv_init(void) {
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
