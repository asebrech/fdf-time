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
// User-drawn splash grid (splash_style 5): FDF_CUSTOM_ROWS row bitmasks,
// drawn cell-by-cell in the phone-side pixel-grid editor (which can also
// stamp any emoji/text the phone can render). Stored outside the settings
// struct — it is 100 bytes and has its own lifecycle.
#define CUSTOM_KEY 3
// Lowest low-battery threshold already announced (0 = none pending). Persisted
// so leaving and re-entering the watchface doesn't re-alert for the same step;
// cleared when the charge recovers or the watch is plugged in.
#define BATT_KEY 4
typedef struct {
  uint8_t theme;        // palette index, see fdf_set_style (0 = tokyo night)
  uint8_t wave_mode;    // 0 fluid (second ticks), 1 eco (minute drift),
                        // 2 frozen, 3 silk (continuous ~15 fps timer),
                        // 4 pulse (silk timer, speed follows the heart
                        // rate — see s_pulse_bpm)
  uint8_t gradient;     // per-line wall gradients on/off
  uint8_t display_mode; // 0 classic HH/MM terrain, 1 seconds SS terrain
  uint8_t splash_style; // launch splash: 0 off, 1 "42", 2 NixOS, 4 Pebble
                        // slashed-e, 5 user-drawn grid, 6 today's date,
                        // 7 orbit (the time rises during a full camera
                        // turn), 8 battery gauge, 9 heart rate (falls back
                        // to "42" when no HR sensor). 3 was Arch, removed.
                        // Was a bool ("42" on/off): 0/1 keep meaning when
                        // persisted.
  uint8_t shake_action; // 0 off, 1 orbit spin, 2 peek at seconds (reverts
                        // at the minute), 3 toggle seconds (sticky until
                        // the next shake), 4-10 view peeks with 6 s
                        // auto-revert: 4 date, 5 the user's drawing (falls
                        // back to orbit if none saved), 6 "42", 7 NixOS,
                        // 8 Pebble, 9 battery, 10 heart rate (falls back
                        // to orbit without a sensor). 2/3 exist in classic
                        // mode only; values match the pre-select boolean so
                        // persisted settings keep meaning orbit.
  bool bt_vibe;         // double pulse when the phone connection drops
  uint8_t wave_rest;    // pause the swell while the backlight is off; only
                        // effective on hardware with the BacklightService
                        // (Core Devices boards). Appended so older
                        // persisted blobs keep their meaning.
  uint8_t time_format;  // 0 auto (follow the watch's clock_is_24h_style),
                        // 1 force 12h, 2 force 24h
  uint8_t show_ampm;    // 12h only: draw the AM/PM tag (classic corner +
                        // seconds overlay suffix)
  uint8_t wake_first;   // gestures only act on a lit screen (or shortly
                        // after a first, "waking" shake): a jolt on a
                        // sleeping watch wakes it instead of triggering
                        // the action. Default ON since 2026-08-17.
  uint8_t reserved1;    // was tilt_sway, feature removed 2026-08-15 (see
                        // the firmware-bug note near CAN_REST_WAVES);
                        // byte kept so persisted blobs keep their layout
  uint8_t low_batt;     // show the battery scene by itself when the charge
                        // first drops under 20% and again under 10%
                        // (independent of shake_action 9). Appended: older
                        // persisted blobs keep their meaning.
} Settings;

static Settings s_settings;
// The user-drawn splash, same row-bitmask convention as the built-ins.
static uint32_t s_custom[FDF_CUSTOM_ROWS];
static bool s_has_custom;

static Window *s_window;
static Layer *s_layer;
static FdfModel s_model;
static Animation *s_morph_anim;
static Animation *s_spin_anim;
static AppTimer *s_splash_timer;
static bool s_bt_connected;
// Text drawn over the ocean's top band: HH:MM in the seconds views,
// weekday+month in the date peek (the terrain then shows the day number).
static char s_overlay[16];
// Seconds peek (classic mode + shake_action 2): the terrain shows SS until
// the minute rolls over — the revert then IS the classic minute morph — or
// until a second flick. Entry shakes fire multiple taps, hence the debounce.
static bool s_peeking;
// One physical shake fires several tap events: events within TAP_BURST_MS
// of the first are the same gesture. With wake_first, a burst only acts if
// the screen is lit (backlight hardware) or a previous burst already
// "armed" the session — the firmware applies the same idea to touch on the
// idle watchface (touch_session.c).
static uint64_t s_burst_ms;
#define TAP_BURST_MS 1200
#define GESTURE_ARM_MS 6000
#define LIT_GRACE_MS 1000
// Date peek (shake_action 4): the terrain shows the day number,
// weekday+month float above, and a short timer restores the time view. It
// overlays whatever view is underneath — s_peeking is left untouched so
// reverting lands back on it.
// NOTE: touch swipes were implemented twice (raw touch_service, then the
// Recognizer + touch-bridge route) and CANNOT work: PebbleOS reserves
// touch for watchapps — every applib touch entry point silently no-ops
// when sys_app_is_watchface() (see touch_service.c in the firmware).
// Don't try again unless the firmware grows a watchface opt-in.
// View peek (shake_action 4-9): one scene from the shared catalog — date,
// the user's drawing, a logo splash or the battery gauge — rises from the
// ocean for a moment, then reverts. Full-region scenes (drawing, NixOS,
// Pebble, battery) temporarily force the classic framing: the pair framing of
// the seconds display mode would overflow the screen; the revert hands the
// mode its own framing back.
static bool s_view_peek;
static AppTimer *s_view_timer;
#define VIEW_PEEK_MS 6000
// Low-battery alert: the same battery scene the shake can show, raised by
// itself when the charge crosses a threshold. Only the CROSSING alerts, once
// per step — s_batt_warned holds the lowest threshold already announced and is
// persisted, so relaunching the watchface (every time the user leaves an app)
// doesn't replay it. It rearms when the charge recovers or the watch is
// plugged in. An alert fired at a dark screen would be wasted, so it waits in
// s_batt_pending until the backlight comes on (or the splash finishes).
static uint8_t s_batt_warned;
static bool s_batt_pending;
#define BATT_WARN_HIGH 20
#define BATT_WARN_LOW 10
// Date splash (splash_style 6): the weekday+month overlay must draw while
// the splash holds, outside any peek state.
static bool s_splash_overlay;
// Silk wave mode: a repeating timer interpolates the swell phase between
// seconds. ~15 fps is ample — the swell moves a fraction of a cell per
// second, so the per-frame delta is sub-pixel smooth.
#if defined(PBL_COLOR)
static AppTimer *s_wave_timer;
#endif
#define WAVE_FRAME_MS 66
#define WAVE_PERIOD_MS 30000  // one wavelength / 30 s, same pace as Fluid
// Wave rest: the swell pauses while the backlight is off. The
// BacklightService only exists on Core Devices hardware (emery, flint,
// gabbro); original Pebbles ship stubs — light_is_on() there is a literal 0,
// so everything must stay behind this guard or waves would rest forever.
// (Testing the _PBL_API_EXISTS_ marker directly keeps -Wexpansion-to-defined
// quiet; PBL_API_EXISTS expands to defined() inside a macro.)
#if defined(PBL_COLOR) && defined(_PBL_API_EXISTS_backlight_service_subscribe)
#define CAN_REST_WAVES 1
static bool s_lit = true;
static uint64_t s_lit_on_ms;  // when the backlight last turned on
// NOTE: the tilt-sway feature (accelerometer-driven extrusion shear, see
// FdfModel.sway_x8/y8 which remain plumbed in fdf.c) was fully implemented
// on 2026-08-15 and REMOVED the same day: on the Time 2, a continuous
// accel data subscription is the only way to get fresh samples (peek is
// stale in bursts), but it cannot be turned off safely (unsubscribe with
// an in-flight event kernel_free()s the app's STATIC session — kernel
// heap corruption, crashloop; live reconfiguration wedges delivery
// permanently) AND while streaming, the driver's ODR change degrades the
// hardware wake-up so badly that MOTION WAKE STOPS WORKING system-wide
// (lsm6dso.c's own comments warn about it). Do not re-attempt until the
// firmware fixes land; the render plumbing is kept so it can return.
#endif
#if defined(PBL_COLOR)
// Rebase applied to the wall-clock phase so the swell resumes exactly where
// it froze after a rest, instead of teleporting to the current wall phase.
static int32_t s_wave_offset;
// Pulse wave mode (4): the swell's speed follows the wearer's heart rate —
// one wavelength per 30 beats, so at 60 BPM it breathes exactly like Silk
// and speeds up as the heart does. Deliberately NO continuous sensor
// stream (that is a battery leak, see the heart-scene note): the BPM is
// the firmware's own duty-cycled reading, peeked once a minute — plus any
// fresh reading the heart scene happens to receive. The phase INTEGRATES
// (unlike the wall-clock modes) so a wave rest pauses it for free — no
// rebase needed, just a dt clamp on resume. 0 BPM (sensor-less color
// watches, health off, no reading yet) paces like a calm 60.
static int s_pulse_bpm;
static uint64_t s_pulse_last_ms;
#define PULSE_BEATS_PER_WAVE 30
#endif

static bool prv_use_24h(void);

static void prv_update_proc(Layer *layer, GContext *ctx) {
  // AA is fine at the current cell size (~6 px); it smeared into noise at
  // the pre-trimetric density. No-op on 1-bit displays.
  graphics_context_set_antialiased(ctx, true);
  fdf_draw(&s_model, ctx);
  if (s_settings.display_mode == 1 || s_peeking || s_view_peek ||
      s_splash_overlay) {
    // Seconds/date views: the terrain shows SS (or the day number); the
    // overlay floats small over the ocean's top band in the theme's
    // foreground color.
    GRect b = layer_get_bounds(layer);
    graphics_context_set_text_color(ctx, fdf_top_color());
    graphics_draw_text(ctx, s_overlay,
                       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, PBL_IF_ROUND_ELSE(10, 2), b.size.w, 28),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else if (!prv_use_24h() && s_settings.show_ampm) {
    // Classic terrain in 12h: a discreet AM/PM tag — the digits alone are
    // ambiguous. Top-right on rect screens (the HH pair leans left, the
    // ocean is free there). On ROUND, top-center sat right on top of the HH
    // pair (the round fit centers the model, so the digits reach the top of
    // the disc); mid-right is the free spot — the trimetric lean puts HH's
    // right end above it and MM's below, and the disc is at its widest there.
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    GRect b = layer_get_bounds(layer);
    graphics_context_set_text_color(ctx, fdf_top_color());
    graphics_draw_text(ctx, t->tm_hour < 12 ? "AM" : "PM",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       PBL_IF_ROUND_ELSE(GRect(0, b.size.h / 2 - 11,
                                               b.size.w - 12, 22),
                                         GRect(0, 0, b.size.w - 4, 22)),
                       GTextOverflowModeFill, GTextAlignmentRight, NULL);
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
static void prv_enter_view_peek(int scene);
static void prv_exit_view_peek(void);
static void prv_start_spin(void);
static void prv_heart_start(void);
static void prv_heart_stop(void);
static bool prv_heart_ok(void);

static uint64_t prv_now_ms(void) {
  time_t s;
  uint16_t ms = time_ms(&s, NULL);
  return (uint64_t)s * 1000 + ms;
}

#if defined(PBL_COLOR)
static bool prv_waves_resting(void) {
#if defined(CAN_REST_WAVES)
  return s_settings.wave_rest && !s_lit;
#else
  return false;
#endif
}

// The wall-clock swell phase for the current wave mode (silk from
// milliseconds, fluid from seconds, eco from minutes, frozen at 0).
static int32_t prv_raw_wave_phase(void) {
  if (s_settings.wave_mode == 3) {
    return (int32_t)((prv_now_ms() % WAVE_PERIOD_MS) * TRIG_MAX_ANGLE /
                     WAVE_PERIOD_MS);
  }
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  if (s_settings.wave_mode == 0) {
    return t->tm_sec * (TRIG_MAX_ANGLE / 30);
  }
  if (s_settings.wave_mode == 1) {
    return t->tm_min * (TRIG_MAX_ANGLE / 30);
  }
  return 0;
}

static void prv_set_wave_phase_now(void) {
  s_model.wave_phase = (prv_raw_wave_phase() + s_wave_offset) &
                       (TRIG_MAX_ANGLE - 1);
}

static void prv_pulse_refresh(void);
#endif

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

// --- heart-rate scene (splash 9 / shake 10) ---

// The ECG strip scrolls one period per heartbeat, driven by its own ~15 fps
// timer while the scene is up. The optical sensor is duty-cycled by the
// firmware (a peek can be minutes old), so for the scene's few seconds we
// request 1 s sampling and listen for HealthEventHeartRateUpdate — the
// digits morph if a fresh reading lands mid-peek. The request MUST be
// cancelled on every exit path: the header warns it outlives the app
// otherwise, and a standing 1 s optical sample period is a battery leak.
// No sensor (or Pebble Health off): prv_heart_ok() gates scene entry — the
// shake falls back to the orbit like the empty drawing does, the splash to
// the "42" — so neither gesture is ever dead. Until the first reading the
// scene shows the monitor idiom for "no signal": flatline and "--".
#define HEART_FRAME_MS 66
#if defined(PBL_HEALTH)
static AppTimer *s_heart_timer;
static bool s_heart_active;
static int s_heart_bpm;
static uint32_t s_heart_phase16;  // beat phase, 0..65535 = one trace period
static uint64_t s_heart_last_ms;
// Beat lock (2026-08-21): HealthEventHRVUpdate carries the peak-to-peak
// interval — the REAL time between two beats — so the trace can run at the
// instantaneous rhythm instead of a filtered average, and the spike can be
// pulled onto the beat itself. Both degrade gracefully: with no PPI the
// period falls back to the averaged BPM and the trace free-runs exactly as
// before. The two sample-period requests share one sensor subscription
// (driven at the shorter period), so asking for HRV on top of the 1 s heart
// rate costs essentially nothing extra — but it must be cancelled on the
// same paths, for the same reason.
static uint16_t s_heart_ppi_ms;   // last peak-to-peak interval, 0 = none
static uint64_t s_heart_ppi_at;   // when it arrived (staleness guard)
#define HEART_PPI_MIN_MS 300      // 200 BPM — anything shorter is noise
#define HEART_PPI_MAX_MS 2000     // 30 BPM
#define HEART_PPI_STALE_MS 10000

static void prv_heart_frame_cb(void *data) {
  s_heart_timer = app_timer_register(HEART_FRAME_MS, prv_heart_frame_cb, NULL);
  uint64_t now = prv_now_ms();
  uint32_t dt = (uint32_t)(now - s_heart_last_ms);
  s_heart_last_ms = now;
  if (dt > 500) {
    dt = 500;  // a hiccup must not teleport the trace (nor overflow below)
  }
  // A fresh peak-to-peak interval is the truest period available: it tracks
  // beat-to-beat variation, where the averaged BPM only tracks the trend.
  uint32_t period_ms = 0;
  if (s_heart_ppi_ms && now - s_heart_ppi_at < HEART_PPI_STALE_MS) {
    period_ms = s_heart_ppi_ms;
  } else if (s_heart_bpm > 0) {
    period_ms = 60000u / (uint32_t)s_heart_bpm;
  }
  if (period_ms) {
    s_heart_phase16 = (s_heart_phase16 + dt * 65536u / period_ms) & 0xFFFF;
  }
  fdf_model_heart_frame(&s_model, s_heart_bpm, s_heart_phase16);
  layer_mark_dirty(s_layer);
}

static void prv_health_handler(HealthEventType event, void *context) {
  if (!s_heart_active) {
    return;
  }
  if (event == HealthEventHRVUpdate) {
    uint16_t ppi = health_service_peek_hrv_ppi_ms();
    APP_LOG(APP_LOG_LEVEL_INFO, "HRV ppi=%u ms", (unsigned)ppi);
    if (ppi < HEART_PPI_MIN_MS || ppi > HEART_PPI_MAX_MS) {
      return;  // dropped beat or noise: keep coasting on the last good one
    }
    s_heart_ppi_ms = ppi;
    s_heart_ppi_at = prv_now_ms();
    // Pull the sweep a QUARTER of the way onto the beat instead of snapping
    // it. A snap makes every late or jittery event yank the trace sideways;
    // a quarter step converges in three or four beats and still looks like a
    // continuous paper feed. This is the whole difference between "runs at
    // your rate" and "beats with you".
    int32_t target = (int32_t)fdf_heart_beat_phase(s_heart_bpm);
    int32_t err = (int32_t)s_heart_phase16 - target;
    if (err > 32768) {
      err -= 65536;
    } else if (err < -32768) {
      err += 65536;
    }
    s_heart_phase16 = (uint32_t)((int32_t)s_heart_phase16 - err / 4) & 0xFFFF;
    return;
  }
  if (event != HealthEventHeartRateUpdate) {
    return;
  }
  int bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
#if defined(PBL_COLOR)
  if (bpm > 0) {
    s_pulse_bpm = bpm;  // free freshness for the pulse wave mode
  }
#endif
  if (bpm != s_heart_bpm) {
    s_heart_bpm = bpm;
    // The number changed under the user's eyes: morph like the seconds do
    // (the frame timer keeps rewriting the destination, so the scroll
    // continues through the morph).
    fdf_model_set_heart(&s_model, bpm, s_heart_phase16);
    prv_start_morph_ms(SECONDS_MORPH_MS);
  }
}
#endif

static bool prv_heart_ok(void) {
#if defined(PBL_HEALTH)
  time_t now = time(NULL);
  return (health_service_metric_accessible(HealthMetricHeartRateBPM, now,
                                           now) &
          HealthServiceAccessibilityMaskAvailable) != 0;
#else
  return false;
#endif
}

// Stamps the scene (so the caller's morph plays over it) and, with health
// available, spins up the sensor + scroll timer. Idempotent via _stop.
static void prv_heart_start(void) {
  int bpm = 0;
#if defined(PBL_HEALTH)
  prv_heart_stop();
  s_heart_active = true;
  health_service_set_heart_rate_sample_period(1);
  health_service_set_hrv_sample_period(1);  // shares the same subscription
  health_service_events_subscribe(prv_health_handler, NULL);
  s_heart_bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
  s_heart_ppi_ms = 0;  // a PPI from a previous peek is meaningless now
  s_heart_phase16 = 0;
  s_heart_last_ms = prv_now_ms();
  s_heart_timer = app_timer_register(HEART_FRAME_MS, prv_heart_frame_cb, NULL);
  bpm = s_heart_bpm;
#endif
  fdf_model_set_heart(&s_model, bpm, 0);
}

static void prv_heart_stop(void) {
#if defined(PBL_HEALTH)
  if (!s_heart_active) {
    return;
  }
  s_heart_active = false;
  if (s_heart_timer) {
    app_timer_cancel(s_heart_timer);
    s_heart_timer = NULL;
  }
  health_service_events_unsubscribe();
  health_service_set_heart_rate_sample_period(0);
  health_service_set_hrv_sample_period(0);  // clears only its own request
#endif
}

// NOTE on gesture ideas: distinguishing wrist flicks from glass taps was
// measured on a real Time 2 (2026-08-15) and is NOT possible — the LSM6DSO
// driver hardcodes axis=Z/dir=0 (TODO in the firmware), and timing patterns
// overlap completely (flicks fire ~207 ms pairs MORE often than real double
// taps do, which usually register as single events). Needs the IMU's real
// double-tap engine exported by the firmware first.
static void prv_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_splash_timer) {
    return;
  }
  uint64_t now = prv_now_ms();
  if (now - s_burst_ms < TAP_BURST_MS) {
    return;  // same physical shake — one gesture, several tap events
  }
  uint64_t prev_burst = s_burst_ms;
  s_burst_ms = now;
  if (s_settings.wake_first) {
    // A jolt on a sleeping watch wakes it; a gesture only acts if the
    // screen is already lit (with a small grace so the waking shake itself
    // doesn't count) or a recent first shake armed the session — the arm
    // window covers daylight, where the ALS keeps the backlight dark.
    bool armed = now - prev_burst < GESTURE_ARM_MS;
#if defined(CAN_REST_WAVES)
    if (s_lit && now - s_lit_on_ms > LIT_GRACE_MS) {
      armed = true;
    }
#endif
    if (!armed) {
      return;
    }
  }
  // View peeks (date / drawing / logos) work in every display mode. With
  // no drawing saved yet, that choice falls back to the orbit so the
  // gesture is never dead.
  if (s_settings.shake_action >= 4 && s_settings.shake_action <= 10) {
    if (s_view_peek) {
      prv_exit_view_peek();
    } else if (s_settings.shake_action == 5 && !s_has_custom) {
      prv_start_spin();
    } else if (s_settings.shake_action == 10 && !prv_heart_ok()) {
      prv_start_spin();  // no HR sensor / Pebble Health off
    } else {
      prv_enter_view_peek(s_settings.shake_action);
    }
    return;
  }
  // The peek/toggle gestures only exist in classic display mode; the
  // dedicated seconds mode keeps the orbit on shake.
  if ((s_settings.shake_action == 2 || s_settings.shake_action == 3) &&
      s_settings.display_mode == 0) {
    if (!s_peeking) {
      s_peeking = true;
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

static bool prv_use_24h(void) {
  if (s_settings.time_format == 1) {
    return false;
  }
  if (s_settings.time_format == 2) {
    return true;
  }
  return clock_is_24h_style();
}

static int prv_display_hours(const struct tm *t) {
  int hours = t->tm_hour;
  if (!prv_use_24h()) {
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
  if (prv_use_24h() || !s_settings.show_ampm) {
    snprintf(s_overlay, sizeof(s_overlay), "%d:%02d", prv_display_hours(t),
             t->tm_min);
  } else {
    snprintf(s_overlay, sizeof(s_overlay), "%d:%02d %s",
             prv_display_hours(t), t->tm_min, t->tm_hour < 12 ? "AM" : "PM");
  }
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

// --- view peek (shared scene catalog) ---

// Stamp one scene onto the terrain: 4 date (day-number pair, weekday+month
// overlay), 5 the user's drawing, 6/7/8 the "42"/NixOS/Pebble splashes,
// 9 the battery staircase. Full-region scenes force the classic framing (see
// the s_view_peek note); the "42" and the date are pairs and look right in
// either framing.
static void prv_show_view_now(int action) {
  s_overlay[0] = '\0';
  // Whatever scene comes up, the heart sensor/timer must not outlive its
  // own (e.g. the low-battery alert barging in over a heart peek).
  prv_heart_stop();
  switch (action) {
    case 4: {
      time_t now = time(NULL);
      struct tm *t = localtime(&now);
      strftime(s_overlay, sizeof(s_overlay), "%a %b", t);
      fdf_model_set_seconds(&s_model, t->tm_mday);
      break;
    }
    case 5:
      fdf_model_set_mode(&s_model, false);
      fdf_model_set_custom(&s_model, s_custom);
      break;
    case 9: {
      // No overlay text: the percentage is terrain here, like the time.
      BatteryChargeState st = battery_state_service_peek();
      fdf_model_set_mode(&s_model, false);
      fdf_model_set_battery(&s_model, st.charge_percent, st.is_charging);
      break;
    }
    case 10:
      // Heart rate: the BPM is terrain too; prv_heart_start stamps the
      // scene itself (the morph below plays over it).
      fdf_model_set_mode(&s_model, false);
      prv_heart_start();
      break;
    default:
      if (action != 6) {
        fdf_model_set_mode(&s_model, false);
      }
      fdf_model_set_splash(&s_model, action == 6 ? 1 : action == 7 ? 2 : 4);
      break;
  }
  prv_start_morph_ms(SECONDS_MORPH_MS);
}

static void prv_view_revert_cb(void *data) {
  s_view_timer = NULL;
  if (s_view_peek) {
    s_view_peek = false;
    prv_heart_stop();  // before the time stamp — the frame timer rewrites z_to
    fdf_model_set_mode(&s_model, s_settings.display_mode == 1);
    prv_show_time();  // lands back on time, or seconds if s_peeking held
  }
}

// The scene is a parameter, not s_settings.shake_action: the low-battery
// alert raises scene 9 whatever the shake is set to.
static void prv_enter_view_peek(int scene) {
  if (s_view_timer) {
    app_timer_cancel(s_view_timer);
  }
  s_view_peek = true;
  prv_show_view_now(scene);
  s_view_timer = app_timer_register(VIEW_PEEK_MS, prv_view_revert_cb, NULL);
}

static void prv_exit_view_peek(void) {
  if (s_view_timer) {
    app_timer_cancel(s_view_timer);
    s_view_timer = NULL;
  }
  s_view_peek = false;
  prv_heart_stop();
  fdf_model_set_mode(&s_model, s_settings.display_mode == 1);
  prv_show_time();
}

// --- low-battery alert ---

// Raise the battery scene by itself, whatever the shake gesture is set to.
// Firing at a dark screen would waste the alert, so it waits for the backlight
// (or for a running splash to hand the stage back).
static void prv_batt_alert(void) {
  if (s_splash_timer) {
    s_batt_pending = true;
    return;
  }
#if defined(CAN_REST_WAVES)
  if (!s_lit) {
    s_batt_pending = true;
    return;
  }
#endif
  s_batt_pending = false;
  prv_enter_view_peek(9);
}

// Claim the threshold the current charge falls under, if it hasn't been
// announced yet — returns true (and marks it announced, persistently) exactly
// once per step down. Plugging in or recovering above 20% rearms the ladder.
static bool prv_batt_claim_alert(void) {
  if (!s_settings.low_batt) {
    return false;
  }
  BatteryChargeState st = battery_state_service_peek();
  uint8_t t = 0;
  if (!st.is_charging && !st.is_plugged) {
    if (st.charge_percent <= BATT_WARN_LOW) {
      t = BATT_WARN_LOW;
    } else if (st.charge_percent <= BATT_WARN_HIGH) {
      t = BATT_WARN_HIGH;
    }
  }
  if (t == 0) {
    s_batt_pending = false;
    if (s_batt_warned) {
      s_batt_warned = 0;
      persist_write_int(BATT_KEY, 0);
    }
    return false;
  }
  if (s_batt_warned && t >= s_batt_warned) {
    return false;  // this step (or a lower one) already had its alert
  }
  s_batt_warned = t;
  persist_write_int(BATT_KEY, t);
  return true;
}

static void prv_battery_handler(BatteryChargeState st) {
  (void)st;  // prv_batt_claim_alert peeks the authoritative state itself
  if (prv_batt_claim_alert()) {
    prv_batt_alert();
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // During the splash the 42 owns the stage: hold the time morph until
  // prv_splash_done plays it (the swell below still rolls). A date peek
  // likewise holds the stage: its revert timer restores the fresh view.
  if (!s_splash_timer && s_view_peek) {
    // hold
  } else if (!s_splash_timer && s_peeking) {
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
  // eco mode drifts one step per minute; frozen keeps phase 0; silk/pulse
  // are driven by their own timer, not ticks. 1-bit has no terrain and
  // always stays on minute ticks. While the waves rest (backlight off) the
  // phase freezes in place; the backlight handler rebases it on wake.
  if (units_changed & MINUTE_UNIT) {
    prv_pulse_refresh();  // pulse pace follows the duty-cycled HR reading
  }
  if ((s_settings.wave_mode == 0 || s_settings.wave_mode == 1) &&
      !prv_waves_resting()) {
    prv_set_wave_phase_now();
  }
  layer_mark_dirty(s_layer);
#endif
}

#if defined(PBL_COLOR)
static void prv_wave_timer_cb(void *data) {
  s_wave_timer = app_timer_register(WAVE_FRAME_MS, prv_wave_timer_cb, NULL);
  if (s_settings.wave_mode == 4) {
    // Pulse: integrate the phase at the heart's pace instead of mapping the
    // wall clock. The dt clamp keeps the first frame after a wave rest (or
    // any scheduling hiccup) from teleporting the swell.
    uint64_t now = prv_now_ms();
    uint32_t dt = (uint32_t)(now - s_pulse_last_ms);
    s_pulse_last_ms = now;
    if (dt > 500) {
      dt = 500;
    }
    int bpm = s_pulse_bpm > 0 ? s_pulse_bpm : 60;
    s_model.wave_phase =
        (s_model.wave_phase +
         (int32_t)(((int64_t)dt * bpm * TRIG_MAX_ANGLE) /
                   (60000 * PULSE_BEATS_PER_WAVE))) &
        (TRIG_MAX_ANGLE - 1);
  } else {
    prv_set_wave_phase_now();
  }
  layer_mark_dirty(s_layer);
}

// Refresh the pulse pace from the firmware's duty-cycled HR reading. A peek
// is free (no sensor request); it returns the filtered value, at most
// 15 minutes old, or 0 when there is none.
static void prv_pulse_refresh(void) {
#if defined(PBL_HEALTH)
  if (s_settings.wave_mode == 4) {
    s_pulse_bpm =
        (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
  }
#endif
}

// (Re)start or stop the silk/pulse frame timer to match the mode and rest
// state.
static void prv_apply_wave_engine(void) {
  if (s_wave_timer) {
    app_timer_cancel(s_wave_timer);
    s_wave_timer = NULL;
  }
  if ((s_settings.wave_mode == 3 || s_settings.wave_mode == 4) &&
      !prv_waves_resting()) {
    s_pulse_last_ms = prv_now_ms();
    s_wave_timer = app_timer_register(WAVE_FRAME_MS, prv_wave_timer_cb, NULL);
  }
}
#endif

#if defined(CAN_REST_WAVES)
// Backlight edge: tracked for BOTH the wave rest and the wake-first gesture
// gate. On off, the swell freezes where it stands; on wake, rebase the
// phase so it resumes from that frozen spot — resuming at the wall-clock
// phase would teleport the whole terrain under the user's eyes.
static void prv_backlight_handler(bool on) {
  if (on && !s_lit) {
    s_lit_on_ms = prv_now_ms();
  }
  s_lit = on;
  if (on) {
    s_wave_offset = (s_model.wave_phase - prv_raw_wave_phase()) &
                    (TRIG_MAX_ANGLE - 1);
  }
  prv_apply_wave_engine();
  prv_subscribe_ticks();  // fluid drops to minute ticks while resting
  if (on && s_batt_pending) {
    prv_batt_alert();  // the alert that crossed while the screen was dark
  }
}
#endif

static void prv_splash_done(void *data) {
  s_splash_timer = NULL;
  s_splash_overlay = false;
  prv_heart_stop();  // the heart splash owns a sensor request + frame timer
  // Full-region splashes force the classic framing; hand the mode its own
  // framing back before the time morph.
  fdf_model_set_mode(&s_model, s_settings.display_mode == 1);
  prv_show_time();
  if (s_batt_pending) {
    prv_batt_alert();  // held back while the splash owned the stage
  }
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
  if (s_settings.wave_mode == 0 && !prv_waves_resting()) {
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
  // Settings changes drop transient peeks so every option starts clean
  // (the peek/toggle only lives in classic mode).
  if (s_settings.display_mode == 1 ||
      (s_settings.shake_action != 2 && s_settings.shake_action != 3)) {
    s_peeking = false;
    persist_write_bool(PEEK_KEY, false);
  }
  if (s_view_timer) {
    app_timer_cancel(s_view_timer);
    s_view_timer = NULL;
  }
  s_view_peek = false;
  prv_heart_stop();
#if defined(PBL_COLOR)
  if (s_settings.wave_mode == 2) {
    s_model.wave_phase = 0;
  }
#if defined(CAN_REST_WAVES)
  // Always subscribed: s_lit feeds the wave rest AND the wake-first gesture
  // gate (prv_waves_resting already checks the wave_rest setting itself).
  // Refresh the lit state BEFORE deciding on the silk timer and tick rate.
  s_lit = light_is_on();
  if (s_lit && s_lit_on_ms == 0) {
    s_lit_on_ms = prv_now_ms();
  }
  backlight_service_subscribe(prv_backlight_handler);
#endif
  prv_pulse_refresh();
  prv_apply_wave_engine();
#endif
  prv_subscribe_ticks();

  if (s_settings.shake_action != 0) {
    accel_tap_service_subscribe(prv_tap_handler);
  } else {
    accel_tap_service_unsubscribe();
  }

  // Only the automatic alert needs the service; the shake scene peeks.
  if (s_settings.low_batt) {
    battery_state_service_subscribe(prv_battery_handler);
  } else {
    battery_state_service_unsubscribe();
    s_batt_pending = false;
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

// The editor serializes the grid as 6 lowercase hex chars per row (22 bits
// used), rows concatenated — 150 chars total. Anything else (including the
// empty string Clay sends before the user ever draws) is ignored.
static bool prv_parse_custom(const char *s) {
  if (strlen(s) != (size_t)(FDF_CUSTOM_ROWS * 6)) {
    return false;
  }
  uint32_t rows[FDF_CUSTOM_ROWS];
  for (int r = 0; r < FDF_CUSTOM_ROWS; r++) {
    uint32_t v = 0;
    for (int i = 0; i < 6; i++) {
      char c = s[r * 6 + i];
      int d;
      if (c >= '0' && c <= '9') {
        d = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        d = c - 'a' + 10;
      } else if (c >= 'A' && c <= 'F') {
        d = c - 'A' + 10;
      } else {
        return false;
      }
      v = (v << 4) | d;
    }
    rows[r] = v & ((1u << FDF_CUSTOM_COLS) - 1);
  }
  memcpy(s_custom, rows, sizeof(s_custom));
  return true;
}

// Clay sends select values as strings and toggles as ints; accept both.
static int prv_tuple_int(const Tuple *t) {
  return t->type == TUPLE_CSTRING ? atoi(t->value->cstring)
                                  : (int)t->value->int32;
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  bool custom_rx = false;
  if ((t = dict_find(iter, MESSAGE_KEY_CustomMap)) &&
      t->type == TUPLE_CSTRING && prv_parse_custom(t->value->cstring)) {
    persist_write_data(CUSTOM_KEY, s_custom, sizeof(s_custom));
    s_has_custom = true;
    custom_rx = true;
  }
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
    s_settings.splash_style = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ShakeOrbit))) {
    s_settings.shake_action = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_BtVibe))) {
    s_settings.bt_vibe = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WaveRest))) {
    s_settings.wave_rest = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_TimeFormat))) {
    s_settings.time_format = prv_tuple_int(t);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ShowAmPm))) {
    s_settings.show_ampm = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_WakeFirst))) {
    s_settings.wake_first = prv_tuple_int(t) != 0;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LowBatt))) {
    s_settings.low_batt = prv_tuple_int(t) != 0;
  }
  if (s_settings.display_mode == 1 &&
      (s_settings.shake_action == 2 || s_settings.shake_action == 3)) {
    s_settings.shake_action = 1;  // peek/toggle don't exist in seconds mode
  }
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  prv_apply_settings();
  if (custom_rx &&
      (s_settings.splash_style == 5 || s_settings.shake_action == 5)) {
    // Fresh drawing with a consumer selected: preview it right away, like a
    // launch splash — the terrain morphs into the drawing, holds a beat,
    // then melts back into the time.
    if (s_splash_timer) {
      app_timer_cancel(s_splash_timer);
    }
    fdf_model_set_mode(&s_model, false);  // full-region: classic framing
    fdf_model_set_custom(&s_model, s_custom);
    prv_start_morph_ms(MORPH_DURATION_MS);
    s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);
  } else if (!s_splash_timer) {
    prv_show_time();  // re-render immediately in the (possibly new) mode
    if (prv_batt_claim_alert()) {
      // Turning the alert on while already low shows it right away instead of
      // waiting for the next crossing.
      prv_batt_alert();
    }
  }
}

static void prv_load_settings(void) {
  s_settings = (Settings) {
    // Defaults revised 2026-08-17 (user's call). They only apply to a FRESH
    // install: any watch with a persisted blob keeps what it had.
    .theme = 1,         // Catppuccin
    .wave_mode = 3,     // silk
    .gradient = 1,
    .display_mode = 0,  // classic HH/MM terrain
    .splash_style = 6,  // today's date
    .shake_action = 9,  // the battery scene
    .bt_vibe = false,
    .wave_rest = 1,
    .time_format = 0,   // follow the watch's 12/24h setting
    .show_ampm = 1,
    .wake_first = 1,    // a jolt on a sleeping watch wakes it, it does not
                        // fire the gesture
    .low_batt = 1,      // announce 20% and 10% by itself
  };
  if (persist_exists(SETTINGS_KEY)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  }
  if (persist_exists(CUSTOM_KEY)) {
    s_has_custom = persist_read_data(CUSTOM_KEY, s_custom, sizeof(s_custom)) ==
                   (int)sizeof(s_custom);
  }
  if (persist_exists(BATT_KEY)) {
    s_batt_warned = persist_read_int(BATT_KEY);
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
  prv_set_wave_phase_now();
#endif

  // Homage splash: the original 42.fdf, then morph into the time. Ticks
  // are live during the splash so the swell rolls; the tick handler holds
  // back the time morph until the splash is done.
  prv_apply_settings();
  // Battery staircase at launch: either because the user picked it as their
  // splash, or because the charge is already under an unannounced threshold —
  // the alert takes the splash slot rather than interrupting a moment later.
  // (Short-circuit order matters: with the battery splash selected the
  // threshold stays unclaimed, so a later crossing still alerts.)
  bool batt_splash = s_settings.splash_style == 8 || prv_batt_claim_alert();
  if (batt_splash) {
    prv_show_view_now(9);  // starts its own morph — a second start here would
                           // unschedule it mid-flight
    s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);
  } else if (s_settings.splash_style == 5 && s_has_custom) {
    fdf_model_set_mode(&s_model, false);  // full-region: classic framing
    fdf_model_set_custom(&s_model, s_custom);
    prv_start_morph_ms(MORPH_DURATION_MS);
    s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);
  } else if (s_settings.splash_style == 6) {
    // Date splash: today's date rises first, then melts into the time.
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(s_overlay, sizeof(s_overlay), "%a %b", t);
    s_splash_overlay = true;
    fdf_model_set_seconds(&s_model, t->tm_mday);
    prv_start_morph_ms(MORPH_DURATION_MS);
    s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);
  } else if (s_settings.splash_style == 9 && prv_heart_ok()) {
    // Heart-rate splash: prv_heart_start stamps the scene and starts the
    // ECG scroll; without a sensor the branch is skipped and style 9 falls
    // through set_splash's default to the "42" below.
    fdf_model_set_mode(&s_model, false);
    prv_heart_start();
    prv_start_morph_ms(MORPH_DURATION_MS);
    s_splash_timer = app_timer_register(SPLASH_MS, prv_splash_done, NULL);
  } else if (s_settings.splash_style == 7) {
    // Orbit splash: no intermediate scene — the time rises while the
    // camera makes one full turn.
    prv_show_time();
    prv_start_spin();
  } else if (s_settings.splash_style != 0) {
    // Style 5 with no grid yet falls through here and set_splash's default
    // case renders the "42". NixOS and Pebble span the full inner region:
    // like the custom grid they need the classic framing (the "42" is a
    // pair and looks right in either framing).
    if (s_settings.splash_style == 2 || s_settings.splash_style == 4) {
      fdf_model_set_mode(&s_model, false);
    }
    fdf_model_set_splash(&s_model, s_settings.splash_style);
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
  if (s_view_timer) {
    app_timer_cancel(s_view_timer);
    s_view_timer = NULL;
  }
  animation_unschedule_all();
  // Cancels the standing 1 s HR sample-period request — it would outlive
  // the app otherwise (see the heart-scene note).
  prv_heart_stop();
#if defined(PBL_COLOR)
  if (s_wave_timer) {
    app_timer_cancel(s_wave_timer);
    s_wave_timer = NULL;
  }
#endif
  // Deliberately NOT calling accel_data_service_unsubscribe here: the
  // firmware's deferred_free bug (see the sway notes) makes any in-app
  // unsubscribe with a pending event corrupt the kernel heap. The kernel
  // cleans the accel session safely when the app process exits.
#if defined(CAN_REST_WAVES)
  backlight_service_unsubscribe();
#endif
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  layer_destroy(s_layer);
}

static void prv_init(void) {
  prv_load_settings();
  app_message_register_inbox_received(prv_inbox_received);
  // Inbox holds one Clay save: every settings key plus the 150-char custom
  // grid string — 256 was no longer enough with the grid.
  app_message_open(512, 64);

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
