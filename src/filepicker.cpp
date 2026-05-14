#include <Arduino.h>
#include <LittleFS.h>
#include "filepicker.h"
#include "display.h"
#include "rsvp.h"

#define MAX_FILES    16
#define MAX_NAME     64
#define DISPLAY_ROWS  4

static bool open = false;
static char files[MAX_FILES][MAX_NAME];
static int fileCount    = 0;
static int cursorPos    = 0;
static int scrollOffset = 0;

static void scan() {
  fileCount = 0;
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f && fileCount < MAX_FILES) {
    if (!f.isDirectory()) {
      String name = f.name();
      if (name.endsWith(".txt")) {
        name.toCharArray(files[fileCount], MAX_NAME);
        fileCount++;
      }
    }
    f = root.openNextFile();
  }
}

static void render() {
  int total = fileCount + 1; // +1 for Back
  if (cursorPos < scrollOffset) scrollOffset = cursorPos;
  if (cursorPos >= scrollOffset + DISPLAY_ROWS) scrollOffset = cursorPos - DISPLAY_ROWS + 1;

  display_clear();
  display_cursor(cursorPos - scrollOffset);
  for (int i = scrollOffset; i < scrollOffset + DISPLAY_ROWS && i < total; i++) {
    if (i < fileCount) display_print(i - scrollOffset, files[i]);
    else               display_print(i - scrollOffset, "< Back");
  }
}

void filepicker_open() {
  open         = true;
  cursorPos    = 0;
  scrollOffset = 0;
  scan();
  render();
}

bool filepicker_is_open() {
  return open;
}

void filepicker_short_press() {
  cursorPos = (cursorPos + 1) % (fileCount + 1);
  render();
}

bool filepicker_long_press() {
  open = false;
  if (cursorPos < fileCount) {
    String path = "/";
    path += files[cursorPos];
    rsvp_load_file(path);
    return true;
  }
  return false; // Back selected
}
