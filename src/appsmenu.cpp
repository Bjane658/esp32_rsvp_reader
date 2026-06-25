#include <Arduino.h>
#include "appsmenu.h"
#include "app_registry.h"
#include "menu.h"
#include "display.h"

#define APPSMENU_MAX 8

static bool isOpen = false;
static int cursorPos   = 0;          // index into s_entries
static int entryCount  = 0;
static App* s_entries[APPSMENU_MAX];  // non-cycleable apps, in registry order

static void scan() {
  entryCount = 0;
  int n = app_registry_count();
  for (int i = 0; i < n && entryCount < APPSMENU_MAX; i++) {
    App* a = app_registry_get(i);
    if (a && !a->cycleable) s_entries[entryCount++] = a;
  }
}

static void render() {
  int total = entryCount + 1;  // +1 for Back
  if (cursorPos < 0) cursorPos = 0;
  if (cursorPos >= total) cursorPos = total - 1;
  int scrollOffset = 0;
  int listRows = display_rows();
  if (cursorPos >= scrollOffset + listRows) scrollOffset = cursorPos - listRows + 1;

  display_reset();
  display_cursor(cursorPos - scrollOffset);
  for (int i = scrollOffset; i < scrollOffset + listRows && i < total; i++) {
    const char* label = (i < entryCount) ? s_entries[i]->display_name : "< Back";
    display_print(i - scrollOffset, label);
  }
  display_flush();
}

bool appsmenu_is_open() { return isOpen; }

void appsmenu_open() {
  isOpen = true;
  cursorPos = 0;
  scan();
  render();
}

void appsmenu_short_press() {
  int total = entryCount + 1;
  cursorPos = (cursorPos + 1) % total;
  render();
}

void appsmenu_double_press() {
  int total = entryCount + 1;
  cursorPos = (cursorPos - 1 + total) % total;
  render();
}

void appsmenu_long_press() {
  isOpen = false;
  if (cursorPos < entryCount) {
    App* target = s_entries[cursorPos];
    // Avoid pushing a duplicate if the tool is already on top of the stack.
    if (target != app_get_active()) {
      menu_close();      // close parent menu first so it doesn't re-render
      app_push(target);  // pushes and calls start() → renders the new app
    } else {
      menu_close();
      App* a = app_get_active();
      if (a && a->show) a->show();
    }
  }
  // Back selected: leave menu open so the main menu re-renders
}
