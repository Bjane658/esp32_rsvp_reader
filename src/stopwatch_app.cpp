#include <Arduino.h>
#include <string.h>
#include "stopwatch_app.h"
#include "app_registry.h"
#include "display.h"

// --- State machine -------------------------------------------------------
enum SwState { SW_STOPPED, SW_RUNNING };

static SwState       state        = SW_STOPPED;
static unsigned long startMs      = 0;   // RUNNING: millis() at (re)start
static unsigned long accumMs      = 0;   // elapsed time frozen while stopped
static long          lastShownSec = -1;  // RUNNING redraw dedup

static unsigned long elapsedMs() {
  return state == SW_RUNNING ? accumMs + (millis() - startMs) : accumMs;
}

// --- Rendering -----------------------------------------------------------
// FONT_LARGE: 3 rows, 20 cols. Big value centered on the middle row.
static void print_centered(int row, const char* s) {
  int cols = display_cols();
  int len = (int)strlen(s);
  char line[40];
  int pad = (cols - len) / 2;
  if (pad < 0) pad = 0;
  int i = 0;
  while (i < pad && i < (int)sizeof(line) - 1) line[i++] = ' ';
  int j = 0;
  while (s[j] && i < (int)sizeof(line) - 1) line[i++] = s[j++];
  line[i] = '\0';
  display_print(row, line);
}

static void render() {
  unsigned long totalSec = elapsedMs() / 1000UL;
  unsigned long mm = totalSec / 60UL;
  unsigned long ss = totalSec % 60UL;
  if (mm > 99) mm = 99;
  char val[8];
  snprintf(val, sizeof(val), "%02lu:%02lu", mm, ss);

  display_set_font(FONT_LARGE);

  if (state == SW_RUNNING) {
    // Running: show only the time, as large as the panel allows. Invert the
    // display (black bg + white text) for the full-minute second (ss == 0),
    // e.g. at 01:00, returning to normal at 01:01.
    display_print_big(val, ss == 0 && totalSec > 0);
    lastShownSec = (long)totalSec;
  } else {
    display_reset();
    print_centered(0, "Stopwatch");
    print_centered(1, val);
    print_centered(2, "1x:start 2x:reset");
    display_flush();
    lastShownSec = -1;
  }
}

// --- App API --------------------------------------------------------------
void stopwatch_app_start() {
  state = SW_STOPPED;
  accumMs = 0;
  lastShownSec = -1;
  render();
}

void stopwatch_app_stop() {
  state = SW_STOPPED;
}

void stopwatch_app_show() {
  render();
}

bool stopwatch_app_is_running() {
  return state == SW_RUNNING;
}

void stopwatch_app_set_running(bool r) {
  if (r) {
    if (state == SW_STOPPED) {
      startMs = millis();
      state = SW_RUNNING;
      lastShownSec = -1;
      render();
    }
  } else {
    if (state == SW_RUNNING) {
      accumMs += millis() - startMs;
      state = SW_STOPPED;
      render();
    }
  }
}

void stopwatch_app_short_press() {
  // Streaming app: a single press is consumed as set_running(true/false).
  (void)0;
}

void stopwatch_app_double_press() {
  // Reset. Reachable only while stopped (is_running() gates running state).
  accumMs = 0;
  lastShownSec = -1;
  render();
}

void stopwatch_app_loop() {
  if (state == SW_RUNNING) {
    long sec = (long)(elapsedMs() / 1000UL);
    if (sec != lastShownSec) {
      render();  // only redraw when the displayed second changes (e-ink wear)
    }
  }
}

// --- Registration ---------------------------------------------------------
static App s_stopwatch_app = {
  "stopwatch", "Stopwatch",
  stopwatch_app_start, stopwatch_app_stop, stopwatch_app_loop,
  stopwatch_app_short_press, stopwatch_app_double_press,
  stopwatch_app_show,
  stopwatch_app_is_running, stopwatch_app_set_running,
  nullptr,            // on_chapter_changed
  false               // cycleable: tool app, launched from Apps
};

static struct StopwatchRegistrar {
  StopwatchRegistrar() { app_registry_register(&s_stopwatch_app); }
} s_stopwatch_registrar;
