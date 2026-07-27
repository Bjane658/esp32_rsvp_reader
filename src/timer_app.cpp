#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "timer_app.h"
#include "app_registry.h"
#include "display.h"

// --- State machine -------------------------------------------------------
enum TimerState { TS_IDLE, TS_RUNNING, TS_PAUSED, TS_ALARM };

static TimerState    state          = TS_IDLE;
static int           presetIdx      = 1;        // index into PRESETS; persisted
static unsigned long endTimeMs      = 0;        // RUNNING: when countdown hits 0
static unsigned long remainingMs    = 0;        // PAUSED: frozen remainder
static unsigned long lastBlinkMs    = 0;        // ALARM blink timing
static bool          alarmVisible   = true;
static long          lastShownSec   = -1;       // RUNNING redraw dedup

// Hardcoded preset durations (minutes). IDLE double-press cycles forward.
static const int PRESETS[] = {1, 2, 5, 10, 15, 30, 60};
#define PRESET_COUNT   (int)(sizeof(PRESETS) / sizeof(PRESETS[0]))

#define TIMER_NVS_NS  "timer"
#define TIMER_NVS_KEY "pidx"
#define ALARM_BLINK_MS 500

// --- Persistence ---------------------------------------------------------
static void loadPresetIdx() {
  Preferences p;
  p.begin(TIMER_NVS_NS, true);
  int i = p.getInt(TIMER_NVS_KEY, 1);  // default 5 min (PRESETS[1])
  p.end();
  if (i < 0) i = 0;
  if (i >= PRESET_COUNT) i = PRESET_COUNT - 1;
  presetIdx = i;
}

static void savePresetIdx() {
  Preferences p;
  p.begin(TIMER_NVS_NS, false);
  p.putInt(TIMER_NVS_KEY, presetIdx);
  p.end();
}

// --- Rendering -----------------------------------------------------------
static void render() {
  char val[8];

  switch (state) {
    case TS_IDLE:
      snprintf(val, sizeof(val), "%02d:00", PRESETS[presetIdx]);
      break;
    case TS_RUNNING:
    case TS_PAUSED: {
      unsigned long rem = (state == TS_RUNNING)
          ? (endTimeMs > millis() ? endTimeMs - millis() : 0)
          : remainingMs;
      unsigned long totalSec = rem / 1000UL;
      unsigned long mm = totalSec / 60UL;
      unsigned long ss = totalSec % 60UL;
      if (mm > 99) mm = 99;
      snprintf(val, sizeof(val), "%02lu:%02lu", mm, ss);
      break;
    }
    case TS_ALARM:
      snprintf(val, sizeof(val), "00:00");
      break;
  }

  display_set_font(FONT_LARGE);

  if (state == TS_ALARM) {
    // Full-screen flashing inversion for maximum noticeability:
    // alarmVisible true  → black background + white "00:00"
    // alarmVisible false → white background + black "00:00"
    display_print_big("00:00", alarmVisible);
  } else {
    // IDLE / RUNNING / PAUSED: show only the clock, no label, no hint.
    display_print_big(val, false);
  }

  // Track what we last drew so loop() can skip redundant redraws.
  if (state == TS_RUNNING) {
    unsigned long rem = endTimeMs > millis() ? endTimeMs - millis() : 0;
    lastShownSec = (long)(rem / 1000UL);
  } else {
    lastShownSec = -1;
  }
}

// --- App API --------------------------------------------------------------
void timer_app_start() {
  loadPresetIdx();
  state = TS_IDLE;
  alarmVisible = true;
  render();
}

void timer_app_stop() {
  // Do not persist running state. Leave presetIdx as-is (already saved on edit).
  state = TS_IDLE;
}

void timer_app_show() {
  render();
}

bool timer_app_is_running() {
  return state == TS_RUNNING || state == TS_ALARM;
}

void timer_app_set_running(bool r) {
  if (r) {
    // Harness: confirmed single press while not running (IDLE or PAUSED).
    if (state == TS_IDLE) {
      endTimeMs = millis() + (unsigned long)PRESETS[presetIdx] * 60000UL;
      state = TS_RUNNING;
      lastShownSec = -1;
      render();
    } else if (state == TS_PAUSED) {
      endTimeMs = millis() + remainingMs;
      state = TS_RUNNING;
      lastShownSec = -1;
      render();
    }
    // TS_ALARM: set_running(true) is not expected from the harness; ignore.
  } else {
    // Harness: single press while running/alarm, or menu opened.
    if (state == TS_RUNNING) {
      remainingMs = endTimeMs > millis() ? (endTimeMs - millis()) : 0;
      state = TS_PAUSED;
      render();
    } else if (state == TS_ALARM) {
      state = TS_IDLE;       // dismiss alarm
      alarmVisible = true;
      render();
    }
  }
}

void timer_app_short_press() {
  // For a streaming app the harness never calls short_press(): a single press
  // is consumed as set_running(true/false). Kept as a defensive no-op.
  (void)0;
}

void timer_app_double_press() {
  if (state == TS_IDLE) {
    presetIdx = (presetIdx + 1) % PRESET_COUNT;  // cycle forward through presets
    savePresetIdx();
    render();
  } else if (state == TS_PAUSED) {
    state = TS_IDLE;   // reset; keep current preset
    render();
  }
  // TS_RUNNING / TS_ALARM: double-press unreachable (is_running() == true).
}

void timer_app_loop() {
  if (state == TS_RUNNING) {
    unsigned long rem = endTimeMs > millis() ? (endTimeMs - millis()) : 0;
    if (rem == 0) {
      state = TS_ALARM;
      alarmVisible = true;
      lastBlinkMs = millis();
      render();
      return;
    }
    long sec = (long)(rem / 1000UL);
    if (sec != lastShownSec) {
      render();   // only redraw when the displayed second changes (e-ink wear)
    }
  } else if (state == TS_ALARM) {
    // is_running() == true keeps the harness's idle-sleep from firing.
    if (millis() - lastBlinkMs >= ALARM_BLINK_MS) {
      lastBlinkMs = millis();
      alarmVisible = !alarmVisible;
      render();
    }
  }
}

// --- Registration ---------------------------------------------------------
static App s_timer_app = {
  "timer", "Timer",
  timer_app_start, timer_app_stop, timer_app_loop,
  timer_app_short_press, timer_app_double_press,
  timer_app_show,
  timer_app_is_running, timer_app_set_running,
  nullptr,            // on_chapter_changed
  false               // cycleable: tool app, launched from Apps
};

static struct TimerRegistrar {
  TimerRegistrar() { app_registry_register(&s_timer_app); }
} s_timer_registrar;
