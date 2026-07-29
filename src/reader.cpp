#include <Arduino.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include <string.h>
#include "reader.h"
#include "app_registry.h"
#include "textengine.h"
#include "menu.h"
#include "display.h"
#include "ap.h"

#define LONG_PRESS_MS        1000
#define DOUBLE_CLICK_MS       500
#define SLEEP_TIMEOUT_MS  300000UL  // 5 minutes

// Button GPIO — set via build flag per board; defaults to GPIO0 (BOOT)
#ifndef BUTTON_GPIO
#define BUTTON_GPIO 0
#endif

#ifdef HAS_BUTTON2
// Second physical button (E213 PRG/BOOT, active-LOW; verified on hardware).
// Acts as a dedicated reset/action button, replacing the single-button
// double-click gesture. Override via -DBUTTON2_GPIO=n if wired elsewhere.
#ifndef BUTTON2_GPIO
#define BUTTON2_GPIO 0
#endif
#endif

static unsigned long lastActivityTime = 0;

static void resetActivity() {
  lastActivityTime = millis();
}

void reader_sleep() {
  App* active = app_get_active();
  if (active) {
    Preferences p;
    p.begin("reader", false);
    p.putString("wake_app", active->id);
    p.end();
  }

  display_print_big("sleeping", false, true);
  delay(100);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_GPIO, 0);
  esp_deep_sleep_start();
}

static volatile unsigned long pressStart  = 0;
static volatile bool          buttonDown  = false;
static volatile bool          longFired   = false;
static volatile bool          shortPressFlag = false;
static volatile bool          longPressFlag  = false;

void IRAM_ATTR reader_onButtonChange() {
  if (digitalRead(BUTTON_GPIO) == LOW) {
    pressStart = millis();
    buttonDown = true;
    longFired  = false;
  } else if (buttonDown) {
    buttonDown = false;
    unsigned long dur = millis() - pressStart;
    if (!longFired && dur >= 20) {
      shortPressFlag = true;
    }
  }
}

#ifdef HAS_BUTTON2
static volatile unsigned long btn2PressStart = 0;
static volatile bool          btn2Down       = false;
static volatile bool          resetPressFlag = false;

void IRAM_ATTR reader_onButton2Change() {
  if (digitalRead(BUTTON2_GPIO) == LOW) {
    btn2PressStart = millis();
    btn2Down = true;
  } else if (btn2Down) {
    btn2Down = false;
    if (millis() - btn2PressStart >= 20) resetPressFlag = true;
  }
}
#endif

static void saveActiveApp() {
  App* a = app_get_active();
  if (!a || !a->cycleable) return;  // never persist a tool app as the boot app
  Preferences p;
  p.begin("reader", false);
  p.putString("app_id", a->id);
  p.end();
}

static void loadActiveApp() {
  Preferences p;
  p.begin("reader", false);
  String wake = p.getString("wake_app", "");
  String saved = p.getString("app_id", "rsvp");
  p.end();

  // Wake-restore: return to whatever app was active before sleep (tool or
  // reading mode), independent of the Settings > Mode cycle logic below.
  App* w = wake.isEmpty() ? nullptr : app_registry_find_by_id(wake.c_str());
  if (w) { app_push(w); return; }

  App* a = app_registry_find_by_id(saved.c_str());
  if (!a || !a->cycleable) {
    // stale/unknown/tool ID: fall back to first cycleable (reading) app
    a = nullptr;
    for (int i = 0; i < app_registry_count(); i++) {
      App* cand = app_registry_get(i);
      if (cand && cand->cycleable) { a = cand; break; }
    }
  }
  if (!a) a = app_registry_get(0);  // last resort
  if (a) app_push(a);
}

#ifdef TOOLS_ONLY
// Tools-only build (E213): the device has exactly two apps, Timer and
// Stopwatch, and no menu. Both run concurrently — the hidden one keeps
// ticking its state (so a backgrounded timer still counts down and fires its
// alarm) but its render output is dropped via display_set_suspended(). A long
// press swaps which one is visible.
static App* s_tools[2] = { nullptr, nullptr };
static int  s_activeIdx = 0;

static App* tools_hidden() { return s_tools[s_activeIdx ^ 1]; }

static void tools_setup() {
  s_tools[0] = app_registry_find_by_id("timer");
  s_tools[1] = app_registry_find_by_id("stopwatch");

  // Restore which tool was visible before sleep; default to the timer.
  Preferences p;
  p.begin("reader", false);
  String wake = p.getString("wake_app", "timer");
  p.end();
  s_activeIdx = (wake == "stopwatch") ? 1 : 0;

  // Start both so both hold live state, but only render the visible one.
  display_set_suspended(true);
  if (tools_hidden() && tools_hidden()->start) tools_hidden()->start();
  display_set_suspended(false);
  app_push(s_tools[s_activeIdx]);  // start() + becomes app_get_active()
}

static void tools_swap() {
  // Hide the current tool, reveal the other. Neither is stopped: state on both
  // sides is preserved and keeps advancing.
  s_activeIdx ^= 1;
  App* next = s_tools[s_activeIdx];

  // Point app_get_active() (used by short/double press and sleep) at the
  // now-visible tool without a stop()/start() cycle that would reset it.
  app_set_active_top(next);
  display_set_suspended(false);
  if (next && next->show) next->show();
}
#endif

void reader_setup() {
  te_setup();
#ifdef TOOLS_ONLY
  tools_setup();
#else
  loadActiveApp();  // app_push calls start() on the active app
#endif
  resetActivity();
}

void reader_loop() {
  // long-press detection
  if (!longFired && buttonDown && (millis() - pressStart >= LONG_PRESS_MS)) {
    longFired    = true;
    longPressFlag = true;
  }

  if (longPressFlag) {
    longPressFlag = false;
    resetActivity();
#ifdef TOOLS_ONLY
    tools_swap();  // no menu: long press just toggles Timer <-> Stopwatch
#else
    if (menu_is_open()) {
      menu_long_press();
    } else {
      App* a = app_get_active();
      if (a && a->set_running) a->set_running(false);
      menu_open();
    }
#endif
  }

#if defined(HAS_BUTTON2)
  // Two-button model: no double-click. Button 1 short press toggles the
  // active app's run state immediately; button 2 triggers its reset/action
  // (the old double-press semantics).
  if (shortPressFlag) {
    shortPressFlag = false;
    resetActivity();
    App* a = app_get_active();
    if (a && a->set_running) {
      bool isRunning = a->is_running && a->is_running();
      a->set_running(!isRunning);
    }
  }

  if (resetPressFlag) {
    resetPressFlag = false;
    resetActivity();
    App* a = app_get_active();
    if (a && a->double_press) a->double_press();
  }
#else
  // Single-button model: short/double-click handling
  static unsigned long firstPressTime   = 0;
  static bool          waitingForDouble = false;

  if (shortPressFlag) {
    shortPressFlag = false;
    resetActivity();

    if (menu_is_open()) {
      if (waitingForDouble && (millis() - firstPressTime <= DOUBLE_CLICK_MS)) {
        waitingForDouble = false;
        menu_double_press();
      } else {
        waitingForDouble = true;
        firstPressTime   = millis();
      }
    } else {
      App* a = app_get_active();
      bool isRunning = a && a->is_running && a->is_running();
      if (isRunning) {
        // streaming app running: stop immediately, no double-press
        if (a->set_running) a->set_running(false);
        te_save_position();
        waitingForDouble = false;
      } else if (waitingForDouble && (millis() - firstPressTime <= DOUBLE_CLICK_MS)) {
        waitingForDouble = false;
        if (a && a->double_press) a->double_press();
      } else {
        waitingForDouble = true;
        firstPressTime   = millis();
      }
    }
  }

  // double-click window expired → single press confirmed
  if (waitingForDouble && (millis() - firstPressTime > DOUBLE_CLICK_MS)) {
    waitingForDouble = false;
    if (menu_is_open()) {
      menu_short_press();
    } else {
      App* a = app_get_active();
      if (a && a->set_running) {
        // streaming app: confirmed single press starts playback
        resetActivity();
        a->set_running(true);
      } else {
        if (a && a->short_press) a->short_press();
      }
    }
  }
#endif

  // sleep when idle
  App* a = app_get_active();
  bool appRunning = a && a->is_running && a->is_running();
#ifdef TOOLS_ONLY
  // Stay awake while the backgrounded tool is running too (e.g. a hidden timer
  // still counting down).
  App* h = tools_hidden();
  if (h && h->is_running && h->is_running()) appRunning = true;
#endif
  if (!appRunning && (millis() - lastActivityTime > SLEEP_TIMEOUT_MS)) {
    reader_sleep();
  }

  te_index_tick();

  if (ap_is_active()) {
    ap_loop();
    return;
  }

  if (menu_is_open()) {
    menu_loop();
    return;
  }

  if (a && a->loop) a->loop();

#ifdef TOOLS_ONLY
  // Tick the backgrounded tool too, but drop its rendering.
  App* hidden = tools_hidden();
  if (hidden && hidden->loop) {
    display_set_suspended(true);
    hidden->loop();
    display_set_suspended(false);
  }
#endif
}

void reader_cycle_app() {
  app_cycle();
  saveActiveApp();
}

const char* reader_get_mode_name() {
  App* a = app_get_active();
  return a ? a->display_name : "?";
}
