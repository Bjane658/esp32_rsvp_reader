#include <Arduino.h>
#include "settingsmenu.h"
#include "display.h"
#include "ap.h"
#include "reader.h"
#include "wifimenu.h"
#include "ota.h"
#include "version.h"

static const int WPM_OPTIONS[] = {150, 200, 300};
static const int WPM_COUNT = 3;

#define SITEM_WPM     0
#define SITEM_MODE    1
#define SITEM_WIFI    2
#define SITEM_OTA     3
#define SITEM_VERSION 4
#define SITEM_COUNT   5

static bool isOpen = false;
static int cursorPos = 0;

// wpmIndex is owned by menu.cpp; access it via menu_get_wpm() and cycle here separately.
// To avoid coupling, settings menu manages its own wpm index and syncs via menu_set_wpm_index().
extern int menu_wpm_index;  // defined in menu.cpp

static void item_label(int item, char* buf, size_t len) {
  switch (item) {
    case SITEM_WPM:  snprintf(buf, len, "WPM: %d", WPM_OPTIONS[menu_wpm_index]); break;
    case SITEM_MODE: snprintf(buf, len, "Mode: %s", reader_get_mode_name()); break;
    case SITEM_WIFI: snprintf(buf, len, ap_is_active() ? "WiFi AP: ON >" : "WiFi AP: OFF >"); break;
    case SITEM_OTA:     snprintf(buf, len, "Update firmware"); break;
    case SITEM_VERSION: snprintf(buf, len, "v: %s", FW_VERSION); break;
  }
}

static void render() {
  if (cursorPos < 0) cursorPos = 0;
  int scrollOffset = 0;
  if (cursorPos >= display_rows()) scrollOffset = cursorPos - display_rows() + 1;

  display_reset();
  display_cursor(cursorPos - scrollOffset);

  char label[40];
  for (int i = scrollOffset; i < scrollOffset + display_rows() && i < SITEM_COUNT; i++) {
    item_label(i, label, sizeof(label));
    display_print(i - scrollOffset, label);
  }
  display_flush();
}

bool settingsmenu_is_open() {
  return isOpen;
}

void settingsmenu_open() {
  isOpen = true;
  cursorPos = 0;
  render();
}

void settingsmenu_short_press() {
  if (wifimenu_is_open()) {
    wifimenu_short_press();
    render();
    return;
  }
  cursorPos = (cursorPos + 1) % SITEM_COUNT;
  render();
}

void settingsmenu_double_press() {
  if (wifimenu_is_open()) {
    wifimenu_short_press();
    render();
    return;
  }
  // double-press from top item closes settings and goes back to main menu
  if (cursorPos == 0) {
    isOpen = false;
    return;
  }
  cursorPos = (cursorPos - 1 + SITEM_COUNT) % SITEM_COUNT;
  render();
}

void settingsmenu_long_press() {
  if (wifimenu_is_open()) {
    wifimenu_long_press();
    render();
    return;
  }
  switch (cursorPos) {
    case SITEM_WPM:
      menu_wpm_index = (menu_wpm_index + 1) % WPM_COUNT;
      break;
    case SITEM_MODE:
      reader_toggle_mode();
      break;
    case SITEM_WIFI:
      wifimenu_open();
      return;
    case SITEM_OTA:
      if (ap_is_active()) ap_stop();
      ota_run();
      return;
    case SITEM_VERSION:
      break;  // read-only
  }
  render();
}
