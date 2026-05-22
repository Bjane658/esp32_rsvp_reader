#include <Arduino.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include <string.h>
#include "reader.h"
#include "textengine.h"
#include "rsvp_mode.h"
#include "ereader_mode.h"
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
  te_save_position();
  display_clear();
  display_set_font(FONT_SMALL);

  // Line 0: filename (strip path)
  const String& filePath = te_get_current_file();
  if (!filePath.isEmpty()) {
    const char* name = filePath.c_str();
    const char* slash = strrchr(name, '/');
    display_print(0, slash ? slash + 1 : name);
  } else {
    display_print(0, "(no file)");
  }

  // Line 1: current chapter title
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

  // Line 2: progress percentage
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

typedef enum { MODE_RSVP, MODE_EREADER } ReaderMode;
static ReaderMode currentMode = MODE_RSVP;

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

static void startCurrentMode() {
  if (currentMode == MODE_RSVP)    rsvp_mode_start();
  else                              ereader_mode_start();
}

static void stopCurrentMode() {
  if (currentMode == MODE_RSVP)    rsvp_mode_stop();
  else                              ereader_mode_stop();
}

static void saveMode() {
  Preferences p;
  p.begin("reader", false);
  p.putUChar("mode", (uint8_t)currentMode);
  p.end();
}

static void loadMode() {
  Preferences p;
  p.begin("reader", false);  // read-write, creates namespace if missing
  currentMode = (ReaderMode)p.getUChar("mode", (uint8_t)MODE_RSVP);
  p.end();
}

void reader_setup() {
  te_setup();
  loadMode();
  resetActivity();
  startCurrentMode();
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
      if (currentMode == MODE_RSVP) rsvp_mode_set_running(false);
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
      // outside menu
      if (currentMode == MODE_RSVP && rsvp_mode_is_running()) {
        // running: stop immediately, no double-press possible
        rsvp_mode_set_running(false);
        te_save_position();
        waitingForDouble = false;
      } else if (waitingForDouble && (millis() - firstPressTime <= DOUBLE_CLICK_MS)) {
        // second press while stopped → double-press action
        waitingForDouble = false;
        if (currentMode == MODE_RSVP)   rsvp_mode_double_press();
        else                             ereader_mode_double_press();
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
      if (currentMode == MODE_RSVP) {
        resetActivity();
        rsvp_mode_set_running(true);
      } else {
        ereader_mode_short_press();
      }
    }
  }

  // sleep when idle (reader stopped or in menu, no button activity)
  bool readerRunning = (currentMode == MODE_RSVP) && rsvp_mode_is_running();
  if (!readerRunning && (millis() - lastActivityTime > SLEEP_TIMEOUT_MS)) {
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

  if (currentMode == MODE_RSVP)   rsvp_mode_loop();
  else                             ereader_mode_loop();
}

void reader_toggle_mode() {
  stopCurrentMode();
  currentMode = (currentMode == MODE_RSVP) ? MODE_EREADER : MODE_RSVP;
  saveMode();
  startCurrentMode();
}

bool reader_is_ereader_mode() {
  return currentMode == MODE_EREADER;
}

const char* reader_get_mode_name() {
  return (currentMode == MODE_RSVP) ? "RSVP" : "EReader";
}
