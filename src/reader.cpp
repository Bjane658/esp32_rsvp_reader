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
#define DOUBLE_CLICK_MS       400
#define SLEEP_TIMEOUT_MS  300000UL  // 5 minutes

static unsigned long lastActivityTime = 0;

static void resetActivity() {
  lastActivityTime = millis();
}

void reader_sleep() {
  display_clear();
  display_set_font(FONT_SMALL);

  const String& filePath = te_get_current_file();
  if (!filePath.isEmpty()) {
    const char* name = filePath.c_str();
    const char* slash = strrchr(name, '/');
    display_print(0, slash ? slash + 1 : name);
  } else {
    display_print(0, "(no file)");
  }

  int chCount = te_get_chapter_count();
  if (chCount > 0) {
    const Chapter* chapters = te_get_chapters();
    size_t pos = te_current_pos();
    int idx = 0;
    for (int i = 0; i < chCount; i++) {
      if (chapters[i].offset <= pos) idx = i;
      else break;
    }
    char chBuf[32];
    snprintf(chBuf, sizeof(chBuf), "Ch: %s", chapters[idx].title);
    display_print(1, chBuf);
  }

  size_t total = te_file_size();
  if (total > 0) {
    int pct = (int)(te_current_pos() * 100UL / total);
    char pctBuf[20];
    snprintf(pctBuf, sizeof(pctBuf), "Progress: %d%%", pct);
    display_print(2, pctBuf);
  }

  display_flush();
  delay(100);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  esp_deep_sleep_start();
}

static volatile unsigned long pressStart  = 0;
static volatile bool          buttonDown  = false;
static volatile bool          longFired   = false;
static volatile bool          shortPressFlag = false;
static volatile bool          longPressFlag  = false;

void IRAM_ATTR reader_onButtonChange() {
  if (digitalRead(0) == LOW) {
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

static void saveActiveApp() {
  App* a = app_get_active();
  if (!a) return;
  Preferences p;
  p.begin("reader", false);
  p.putString("app_id", a->id);
  p.end();
}

static void loadActiveApp() {
  Preferences p;
  p.begin("reader", false);
  String saved = p.getString("app_id", "rsvp");
  p.end();
  App* a = app_registry_find_by_id(saved.c_str());
  if (!a) a = app_registry_get(0);  // stale/unknown ID: fall back to first
  if (a) app_push(a);
}

void reader_setup() {
  te_setup();
  loadActiveApp();  // app_push calls start() on the active app
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
    if (menu_is_open()) {
      menu_long_press();
    } else {
      App* a = app_get_active();
      if (a && a->set_running) a->set_running(false);
      menu_open();
    }
  }

  // short / double-click handling
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

  // sleep when idle
  App* a = app_get_active();
  bool appRunning = a && a->is_running && a->is_running();
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
}

void reader_cycle_app() {
  app_cycle();
  saveActiveApp();
}

const char* reader_get_mode_name() {
  App* a = app_get_active();
  return a ? a->display_name : "?";
}
