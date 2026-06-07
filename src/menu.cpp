#include <Arduino.h>
#include "menu.h"
#include "display.h"
#include "ap.h"
#include "textengine.h"
#include "app_registry.h"
#include "filepicker.h"
#include "wifimenu.h"
#include "chapterpicker.h"
#include "settingsmenu.h"

// Wireless Paper v1.x: GPIO20 = battery ADC (ADC2), GPIO19 = enable (active LOW)
// ADC2 conflicts with WiFi on ESP32-S3, so we cache the last reading taken without WiFi.
#define BAT_ADC_PIN   20
#define BAT_CTRL_PIN  19

static int cachedBatPct = -1;

static int battery_pct() {
  if (ap_is_active()) {
    return cachedBatPct >= 0 ? cachedBatPct : -1;
  }
  pinMode(BAT_CTRL_PIN, OUTPUT);
  digitalWrite(BAT_CTRL_PIN, LOW);
  delay(5);
  int raw = analogRead(BAT_ADC_PIN);
  digitalWrite(BAT_CTRL_PIN, HIGH);
  float volts = (raw / 4095.0f) * 3.3f / (100.0f / 490.0f);
  int pct = (int)((volts - 3.0f) / 1.2f * 100.0f);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  cachedBatPct = pct;
  return pct;
}

static const int WPM_OPTIONS[] = {150, 200, 300};
static const int WPM_COUNT = 3;

// Exposed to settingsmenu.cpp so WPM can be changed from within Settings
int menu_wpm_index = 1;

static bool isOpen = false;
static int cursorPos = 0;
static int scrollOffset = 0;

#define ITEM_CHAPTER   0
#define ITEM_FILE      1
#define ITEM_SETTINGS  2
#define ITEM_EXIT      3
#define ITEM_BATTERY   4
#define ITEM_SLEEP     5
#define ITEM_COUNT     6

int menu_get_wpm() {
  return WPM_OPTIONS[menu_wpm_index];
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
    case ITEM_SETTINGS: snprintf(buf, len, "Settings"); break;
    case ITEM_EXIT:     snprintf(buf, len, "Exit"); break;
    case ITEM_BATTERY: {
      int pct = battery_pct();
      if (pct < 0) snprintf(buf, len, "Battery: --");
      else         snprintf(buf, len, "Battery: %d%%", pct);
      break;
    }
    case ITEM_SLEEP: snprintf(buf, len, "Sleep"); break;
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
  display_set_font(FONT_SMALL);
  render();
}

void menu_short_press() {
  if (filepicker_is_open()) {
    filepicker_short_press();
    return;
  }
  if (chapterpicker_is_open()) {
    chapterpicker_short_press();
    return;
  }
  if (settingsmenu_is_open()) {
    settingsmenu_short_press();
    if (!settingsmenu_is_open()) render();  // settings closed, back to main menu
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
  if (chapterpicker_is_open()) {
    chapterpicker_double_press();
    return;
  }
  if (settingsmenu_is_open()) {
    settingsmenu_double_press();
    if (!settingsmenu_is_open()) render();  // settings closed, back to main menu
    return;
  }
  cursorPos = (cursorPos - 1 + ITEM_COUNT) % ITEM_COUNT;
  render();
}

void menu_long_press() {
  if (filepicker_is_open()) {
    filepicker_long_press();
    render();
    return;
  }
  if (chapterpicker_is_open()) {
    if (chapterpicker_long_press()) {
      App* a = app_get_active();
      if (a && a->on_chapter_changed) a->on_chapter_changed();
    }
    render();
    return;
  }
  if (settingsmenu_is_open()) {
    settingsmenu_long_press();
    if (!settingsmenu_is_open()) render();
    return;
  }
  switch (cursorPos) {
    case ITEM_CHAPTER:
      chapterpicker_open();
      return;
    case ITEM_FILE:
      filepicker_open();
      return;
    case ITEM_SETTINGS:
      settingsmenu_open();
      return;
    case ITEM_BATTERY:
      break;  // read-only, re-render with fresh reading
    case ITEM_SLEEP:
      if (ap_is_active()) ap_stop();
      reader_sleep();
      return;
    case ITEM_EXIT:
      isOpen = false;
      if (ap_is_active()) ap_stop();
      {
        App* a = app_get_active();
        if (a && a->show) a->show();
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
