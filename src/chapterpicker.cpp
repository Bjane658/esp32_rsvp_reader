#include <Arduino.h>
#include "chapterpicker.h"
#include "display.h"
#include "textengine.h"

static bool isOpen       = false;
static int  cursorPos    = 0;
static int  scrollOffset = 0;
static int  currentChapter = 0;

static int findCurrentChapter() {
  const Chapter* chapters = te_get_chapters();
  int count = te_get_chapter_count();
  size_t pos = te_current_pos();
  int idx = 0;
  for (int i = 0; i < count; i++) {
    if (chapters[i].offset <= pos) idx = i;
    else break;
  }
  return idx;
}

static void render() {
  const Chapter* chapters = te_get_chapters();
  int count = te_get_chapter_count();
  bool scanning = te_is_indexing();
  int total = count + 1 + (scanning ? 1 : 0); // chapters + Back [+ Scanning...]

  if (cursorPos < scrollOffset) scrollOffset = cursorPos;
  if (cursorPos >= scrollOffset + display_rows()) scrollOffset = cursorPos - display_rows() + 1;

  display_reset();
  display_cursor(cursorPos - scrollOffset);

  for (int i = scrollOffset; i < scrollOffset + display_rows() && i < total; i++) {
    if (scanning && i == count) {
      display_print(i - scrollOffset, "Scanning...");
    } else if (i == count + (scanning ? 1 : 0)) {
      display_print(i - scrollOffset, "< Back");
    } else {
      char label[TE_MAX_TITLE + 2];
      if (i == currentChapter) {
        snprintf(label, sizeof(label), "*%s", chapters[i].title);
      } else {
        snprintf(label, sizeof(label), "%s", chapters[i].title);
      }
      display_print(i - scrollOffset, label);
    }
  }
  display_flush();
}

void chapterpicker_open() {
  if (te_get_chapter_count() == 0 && !te_is_indexing()) return;
  currentChapter = findCurrentChapter();
  cursorPos      = currentChapter;
  scrollOffset   = 0;
  isOpen         = true;
  render();
}

bool chapterpicker_is_open() {
  return isOpen;
}

void chapterpicker_short_press() {
  int count = te_get_chapter_count();
  int total = count + 1 + (te_is_indexing() ? 1 : 0);
  cursorPos = (cursorPos + 1) % total;
  render();
}

void chapterpicker_double_press() {
  int count = te_get_chapter_count();
  int total = count + 1 + (te_is_indexing() ? 1 : 0);
  cursorPos = (cursorPos - 1 + total) % total;
  render();
}

void chapterpicker_cancel() {
  isOpen = false;
}

bool chapterpicker_long_press() {
  isOpen = false;
  const Chapter* chapters = te_get_chapters();
  int count = te_get_chapter_count();
  // "Scanning..." sits at index `count` when active; Back is always last
  if (cursorPos < count) {
    te_seek(chapters[cursorPos].offset);
    return true;
  }
  return false; // Scanning... or Back selected
}
