#include <Arduino.h>
#include "chapterpicker.h"
#include "display.h"
#include "rsvp.h"

#define DISPLAY_ROWS 4

static bool open        = false;
static int  cursorPos   = 0;
static int  scrollOffset = 0;
static int  currentChapter = 0;

static int findCurrentChapter() {
  const RsvpChapter* chapters = rsvp_get_chapters();
  int count = rsvp_get_chapter_count();
  size_t pos = rsvp_get_file_position();
  int idx = 0;
  for (int i = 0; i < count; i++) {
    if (chapters[i].offset <= pos) idx = i;
    else break;
  }
  return idx;
}

static void render() {
  const RsvpChapter* chapters = rsvp_get_chapters();
  int count = rsvp_get_chapter_count();
  int total = count + 1; // +1 for Back

  if (cursorPos < scrollOffset) scrollOffset = cursorPos;
  if (cursorPos >= scrollOffset + DISPLAY_ROWS) scrollOffset = cursorPos - DISPLAY_ROWS + 1;

  display_clear();
  display_cursor(cursorPos - scrollOffset);

  for (int i = scrollOffset; i < scrollOffset + DISPLAY_ROWS && i < total; i++) {
    if (i == count) {
      display_print(i - scrollOffset, "< Back");
    } else {
      char label[RSVP_MAX_TITLE + 2];
      if (i == currentChapter) {
        snprintf(label, sizeof(label), "*%s", chapters[i].title);
      } else {
        snprintf(label, sizeof(label), "%s", chapters[i].title);
      }
      display_print(i - scrollOffset, label);
    }
  }
}

void chapterpicker_open() {
  if (rsvp_get_chapter_count() == 0) return;
  currentChapter = findCurrentChapter();
  cursorPos      = currentChapter;
  scrollOffset   = 0;
  open           = true;
  render();
}

bool chapterpicker_is_open() {
  return open;
}

void chapterpicker_short_press() {
  int count = rsvp_get_chapter_count();
  if (count == 0) return;
  cursorPos = (cursorPos + 1) % (count + 1);
  render();
}

bool chapterpicker_long_press() {
  open = false;
  const RsvpChapter* chapters = rsvp_get_chapters();
  int count = rsvp_get_chapter_count();
  if (cursorPos < count) {
    rsvp_seek(chapters[cursorPos].offset);
    return true;
  }
  return false; // Back selected
}
