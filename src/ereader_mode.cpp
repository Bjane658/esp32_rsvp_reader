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

// Render DISPLAY_ROWS lines of wrapped text from current file position.
// Advances the file cursor to just past the last char rendered.
static void renderPage() {
  display_clear();
  for (int row = 0; row < DISPLAY_ROWS; row++) {
    char line[DISPLAY_COLS + 1];
    int len = 0;
    int c;

    // skip leading newlines between paragraphs (keep spaces for indentation)
    while ((c = te_read_char()) != -1 && c == '\n');
    if (c == -1) {
      // end of file — leave remaining rows blank
      break;
    }
    // first non-newline char
    line[len++] = (char)c;

    // fill up to DISPLAY_COLS
    while (len < DISPLAY_COLS) {
      c = te_read_char();
      if (c == -1 || c == '\n') break;
      line[len++] = (char)c;
    }
    line[len] = '\0';

    // if we stopped mid-word and there's more content, backtrack to word boundary
    if (c != -1 && c != '\n' && len == DISPLAY_COLS) {
      // check if last char and next char are both word chars (mid-word break)
      if (line[len - 1] != ' ') {
        // walk back to last space
        int cut = len - 1;
        while (cut > 0 && line[cut] != ' ') cut--;
        if (cut > 0) {
          // seek back the chars we're dropping
          size_t rewind = len - cut;
          te_seek_to(te_current_pos() - rewind);
          line[cut] = '\0';
        }
      }
    }

    display_print(row, line);
  }
}

void ereader_mode_start() {
  historyHead  = 0;
  historyCount = 0;
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
}

void ereader_mode_double_press() {
  size_t prev;
  // pop twice: once for current page, once for the one before it
  historyPop(&prev); // discard current
  if (!historyPop(&prev)) prev = 0;
  te_seek_to(prev);
  historyPush(prev);
  renderPage();
}
