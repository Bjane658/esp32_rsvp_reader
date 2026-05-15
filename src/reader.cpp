#include <Arduino.h>
#include "reader.h"
#include "textengine.h"
#include "rsvp_mode.h"
#include "ereader_mode.h"
#include "menu.h"
#include "display.h"

#define LONG_PRESS_MS   1000
#define DOUBLE_CLICK_MS  300

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
    if (!longFired && (millis() - pressStart >= 20)) {
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

void reader_setup() {
  te_setup();
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

    if (menu_is_open()) {
      if (waitingForDouble && (millis() - firstPressTime <= DOUBLE_CLICK_MS)) {
        waitingForDouble = false;
        menu_double_press();
      } else {
        waitingForDouble = true;
        firstPressTime   = millis();
        menu_short_press();
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

  // double-click window expired
  if (waitingForDouble && (millis() - firstPressTime > DOUBLE_CLICK_MS)) {
    waitingForDouble = false;
    if (!menu_is_open()) {
      if (currentMode == MODE_RSVP) {
        rsvp_mode_set_running(true);
      } else {
        ereader_mode_short_press();
      }
    }
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
  startCurrentMode();
}

const char* reader_get_mode_name() {
  return (currentMode == MODE_RSVP) ? "RSVP" : "EReader";
}
