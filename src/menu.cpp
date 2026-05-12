#include <Arduino.h>
#include "menu.h"
#include "display.h"
#include "ap.h"
#include "rsvp.h"
#include "filepicker.h"

static const int WPM_OPTIONS[] = {200, 300, 400};
static const int WPM_COUNT = 3;

static bool open = false;
static int cursorPos = 0;
static int wpmIndex = 1;

#define ITEM_WPM    0
#define ITEM_FILE   1
#define ITEM_WIFI   2
#define ITEM_EXIT   3
#define ITEM_COUNT  4

int menu_get_wpm() {
  return WPM_OPTIONS[wpmIndex];
}

static void render() {
  char wpmLabel[16];
  snprintf(wpmLabel, sizeof(wpmLabel), "WPM: %d", WPM_OPTIONS[wpmIndex]);

  display_clear();
  display_cursor(cursorPos);
  display_print(ITEM_WPM,  wpmLabel);
  display_print(ITEM_FILE, "Choose file");
  display_print(ITEM_WIFI, ap_is_active() ? "WiFi AP: ON" : "WiFi AP: OFF");
  display_print(ITEM_EXIT, "Exit");
}

bool menu_is_open() {
  return open;
}

void menu_open() {
  open = true;
  cursorPos = 0;
  render();
}

void menu_short_press() {
  if (filepicker_is_open()) {
    filepicker_short_press();
    return;
  }
  cursorPos = (cursorPos + 1) % ITEM_COUNT;
  render();
}

void menu_long_press() {
  if (filepicker_is_open()) {
    filepicker_long_press();
    render();
    return;
  }
  switch (cursorPos) {
    case ITEM_WPM:
      wpmIndex = (wpmIndex + 1) % WPM_COUNT;
      break;
    case ITEM_FILE:
      filepicker_open();
      return;
    case ITEM_WIFI:
      if (ap_is_active()) {
        ap_stop();
        rsvp_reload_text();
      } else {
        ap_start();
      }
      break;
    case ITEM_EXIT:
      open = false;
      if (ap_is_active()) {
        ap_stop();
        rsvp_reload_text();
      }
      rsvp_show_current_word();
      return;
  }
  render();
}

void menu_loop() {
  if (open && ap_is_active()) {
    ap_loop();
  }
}
