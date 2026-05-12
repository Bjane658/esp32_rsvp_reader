#include <Arduino.h>
#include <LittleFS.h>
#include "filepicker.h"
#include "display.h"
#include "rsvp.h"

#define MAX_FILES 16
#define MAX_NAME  32

static bool open = false;
static char files[MAX_FILES][MAX_NAME];
static int fileCount = 0;
static int cursorPos = 0;

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
  display_clear();
  display_cursor(cursorPos);
  for (int i = 0; i < fileCount; i++) {
    display_print(i, files[i]);
  }
}

void filepicker_open() {
  open = true;
  cursorPos = 0;
  scan();
  render();
}

bool filepicker_is_open() {
  return open;
}

void filepicker_short_press() {
  if (fileCount == 0) return;
  cursorPos = (cursorPos + 1) % fileCount;
  render();
}

void filepicker_long_press() {
  open = false;
  if (fileCount > 0) {
    String path = "/";
    path += files[cursorPos];
    rsvp_load_file(path);
  }
}
