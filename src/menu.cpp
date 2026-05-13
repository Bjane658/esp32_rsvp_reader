#include <Arduino.h>
#include "menu.h"
#include "display.h"
#include "ap.h"
#include "rsvp.h"
#include "filepicker.h"
#include "wifimenu.h"
#include "chapterpicker.h"

static const int WPM_OPTIONS[] = {200, 300, 400};
static const int WPM_COUNT = 3;

static bool open = false;
static int cursorPos = 0;
static int scrollOffset = 0;
static int wpmIndex = 1;
static bool fileChanged = false;

#define ITEM_CHAPTER    0
#define ITEM_WPM        1
#define ITEM_FILE       2
#define ITEM_WIFI       3
#define ITEM_EXIT       4
#define ITEM_COUNT      5

#define DISPLAY_ROWS    4

int menu_get_wpm() {
  return WPM_OPTIONS[wpmIndex];
}

static void item_label(int item, char* buf, size_t len) {
  switch (item) {
    case ITEM_CHAPTER: snprintf(buf, len, "Chapter >"); break;
    case ITEM_WPM:     snprintf(buf, len, "WPM: %d", WPM_OPTIONS[wpmIndex]); break;
    case ITEM_FILE: {
      const String& f = rsvp_get_current_file();
      if (f.isEmpty()) {
        snprintf(buf, len, "File: -");
      } else {
        const char* name = f.c_str();
        const char* slash = strrchr(name, '/');
        snprintf(buf, len, "File: %s", slash ? slash + 1 : name);
      }
      break;
    }
    case ITEM_WIFI: snprintf(buf, len, ap_is_active() ? "WiFi AP: ON >" : "WiFi AP: OFF >"); break;
    case ITEM_EXIT: snprintf(buf, len, "Exit"); break;
  }
}

static void render() {
  // keep cursor visible
  if (cursorPos < scrollOffset) scrollOffset = cursorPos;
  if (cursorPos >= scrollOffset + DISPLAY_ROWS) scrollOffset = cursorPos - DISPLAY_ROWS + 1;

  display_clear();
  display_cursor(cursorPos - scrollOffset);

  char label[40];
  for (int i = scrollOffset; i < scrollOffset + DISPLAY_ROWS && i < ITEM_COUNT; i++) {
    item_label(i, label, sizeof(label));
    display_print(i - scrollOffset, label);
  }
}

bool menu_is_open() {
  return open;
}

void menu_open() {
  open = true;
  cursorPos = 0;
  scrollOffset = 0;
  fileChanged = false;
  render();
}

void menu_short_press() {
  if (filepicker_is_open()) {
    filepicker_short_press();
    return;
  }
  if (wifimenu_is_open()) {
    wifimenu_short_press();
    render();
    return;
  }
  if (chapterpicker_is_open()) {
    chapterpicker_short_press();
    return;
  }
  cursorPos = (cursorPos + 1) % ITEM_COUNT;
  render();
}

void menu_long_press() {
  if (filepicker_is_open()) {
    if (filepicker_long_press()) fileChanged = true;
    render();
    return;
  }
  if (wifimenu_is_open()) {
    wifimenu_long_press();
    if (!ap_is_active()) fileChanged = true;
    return;
  }
  if (chapterpicker_is_open()) {
    if (chapterpicker_long_press()) open = false; // close menu only if chapter selected
    else render();                                // Back: stay in menu
    return;
  }
  switch (cursorPos) {
    case ITEM_CHAPTER:
      chapterpicker_open();
      return;
    case ITEM_WPM:
      wpmIndex = (wpmIndex + 1) % WPM_COUNT;
      break;
    case ITEM_FILE:
      filepicker_open();
      return;
    case ITEM_WIFI:
      wifimenu_open();
      return;
    case ITEM_EXIT:
      open = false;
      if (ap_is_active()) {
        ap_stop();
        rsvp_reload_text();
        fileChanged = true;
      }
      if (fileChanged) {
        rsvp_show_preview();
      } else {
        rsvp_show_current_word();
      }
      return;
  }
  render();
}

void menu_loop() {
  if (open && ap_is_active()) {
    ap_loop();
  }
}
