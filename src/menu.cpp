#include <Arduino.h>
#include "menu.h"
#include "display.h"
#include "ap.h"
#include "textengine.h"
#include "rsvp_mode.h"
#include "ereader_mode.h"
#include "reader.h"
#include "filepicker.h"
#include "wifimenu.h"
#include "chapterpicker.h"

static const int WPM_OPTIONS[] = {150, 200, 300};
static const int WPM_COUNT = 3;

static bool isOpen = false;
static int cursorPos = 0;
static int scrollOffset = 0;
static int wpmIndex = 1;
static bool fileChanged = false;

#define ITEM_CHAPTER    0
#define ITEM_WPM        1
#define ITEM_MODE       2
#define ITEM_FILE       3
#define ITEM_WIFI       4
#define ITEM_EXIT       5
#define ITEM_COUNT      6

int menu_get_wpm() {
  return WPM_OPTIONS[wpmIndex];
}

static void item_label(int item, char* buf, size_t len) {
  switch (item) {
    case ITEM_CHAPTER: {
      const Chapter* chapters = te_get_chapters();
      int count = te_get_chapter_count();
      if (te_is_indexing() && count == 0) {
        snprintf(buf, len, "Chapter: Loading...");
      } else if (count == 0) {
        snprintf(buf, len, "Chapter >");
      } else {
        size_t pos = te_current_pos();
        int idx = 0;
        for (int i = 0; i < count; i++) {
          if (chapters[i].offset <= pos) idx = i;
          else break;
        }
        snprintf(buf, len, "Ch: %s", chapters[idx].title);
      }
      break;
    }
    case ITEM_WPM:     snprintf(buf, len, "WPM: %d", WPM_OPTIONS[wpmIndex]); break;
    case ITEM_MODE:    snprintf(buf, len, "Mode: %s", reader_get_mode_name()); break;
    case ITEM_FILE: {
      const String& f = te_get_current_file();
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
  if (cursorPos < scrollOffset) scrollOffset = cursorPos;
  if (cursorPos >= scrollOffset + display_rows()) scrollOffset = cursorPos - display_rows() + 1;

  display_reset();
  display_cursor(cursorPos - scrollOffset);

  char label[40];
  for (int i = scrollOffset; i < scrollOffset + display_rows() && i < ITEM_COUNT; i++) {
    item_label(i, label, sizeof(label));
    display_print(i - scrollOffset, label);
  }
  display_flush();
}

bool menu_is_open() {
  return isOpen;
}

void menu_open() {
  isOpen = true;
  cursorPos = 0;
  scrollOffset = 0;
  fileChanged = false;
  display_set_font(FONT_SMALL);
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

void menu_double_press() {
  if (filepicker_is_open()) {
    filepicker_double_press();
    return;
  }
  if (wifimenu_is_open()) {
    wifimenu_short_press();  // only one item, up/down same effect
    render();
    return;
  }
  if (chapterpicker_is_open()) {
    chapterpicker_double_press();
    return;
  }
  // main menu: cursor up
  cursorPos = (cursorPos - 1 + ITEM_COUNT) % ITEM_COUNT;
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
    if (chapterpicker_long_press()) ereader_mode_reset_history();
    render();
    return;
  }
  switch (cursorPos) {
    case ITEM_CHAPTER:
      chapterpicker_open();
      return;
    case ITEM_WPM:
      wpmIndex = (wpmIndex + 1) % WPM_COUNT;
      break;
    case ITEM_MODE:
      reader_toggle_mode();
      break;
    case ITEM_FILE:
      filepicker_open();
      return;
    case ITEM_WIFI:
      wifimenu_open();
      return;
    case ITEM_EXIT:
      isOpen = false;
      if (ap_is_active()) {
        ap_stop();
      }
      if (reader_is_ereader_mode()) {
        ereader_mode_show_page();
      } else if (fileChanged) {
        rsvp_mode_show_preview();
      } else {
        rsvp_mode_show_current_word();
      }
      return;
  }
  render();
}

void menu_loop() {
  if (isOpen && ap_is_active()) {
    ap_loop();
  }
}
