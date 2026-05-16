#include <Arduino.h>
#include "ereader_mode.h"
#include "textengine.h"
#include "display.h"

// Ring buffer of page-start positions for going back
#define PAGE_HISTORY 8
static size_t pageHistory[PAGE_HISTORY];
static int    historyHead = 0;  // index of the oldest entry
static int    historyCount = 0;

static void historyPush(size_t pos) {
  int idx = (historyHead + historyCount) % PAGE_HISTORY;
  pageHistory[idx] = pos;
  if (historyCount < PAGE_HISTORY) {
    historyCount++;
  } else {
    historyHead = (historyHead + 1) % PAGE_HISTORY;
  }
}

static bool historyPop(size_t* out) {
  if (historyCount == 0) return false;
  historyCount--;
  *out = pageHistory[(historyHead + historyCount) % PAGE_HISTORY];
  return true;
}

static void renderPage() {
  int rows = display_rows();
  int cols = display_cols();
  display_reset();

  // Progress within current chapter (falls back to whole-file if no chapters)
  size_t pageStart = te_current_pos();
  const Chapter* chapters = te_get_chapters();
  int chapterCount = te_get_chapter_count();
  size_t chStart = 0, chEnd = te_file_size();
  for (int i = 0; i < chapterCount; i++) {
    if (chapters[i].offset <= pageStart) chStart = chapters[i].offset;
    if (chapters[i].offset > pageStart && chEnd == te_file_size()) chEnd = chapters[i].offset;
  }
  float fraction = (chEnd > chStart) ? (float)(pageStart - chStart) / (float)(chEnd - chStart) : 0.0f;
  display_set_progress(fraction);
  for (int row = 0; row < rows; row++) {
    char line[29]; // MAX_COLS + 1
    int len = 0;
    int c;

    while ((c = te_read_char()) != -1 && c == '\n');
    if (c == -1) break;

    line[len++] = (char)c;
    while (len < cols) {
      c = te_read_char();
      if (c == -1 || c == '\n') break;
      line[len++] = (char)c;
    }
    line[len] = '\0';

    if (c != -1 && c != '\n' && len == cols) {
      if (line[len - 1] != ' ') {
        int cut = len - 1;
        while (cut > 0 && line[cut] != ' ') cut--;
        if (cut > 0) {
          size_t rewind = len - cut;
          te_seek_to(te_current_pos() - rewind);
          line[cut] = '\0';
        }
      }
    }

    display_print(row, line);
  }
  display_flush();
}

void ereader_mode_start() {
  historyHead  = 0;
  historyCount = 0;
  display_set_font(FONT_SMALL);
  historyPush(te_current_pos());
  renderPage();
}

void ereader_mode_stop() {
  // nothing to tear down
}

void ereader_mode_loop() {
  // page mode is event-driven, nothing to do per tick
}

void ereader_mode_short_press() {
  size_t pos = te_current_pos();
  historyPush(pos);
  renderPage();
  te_save_position();
}

void ereader_mode_double_press() {
  size_t prev;
  historyPop(&prev); // discard current
  if (!historyPop(&prev)) prev = 0;
  te_seek_to(prev);
  historyPush(prev);
  renderPage();
  te_save_position();
}
