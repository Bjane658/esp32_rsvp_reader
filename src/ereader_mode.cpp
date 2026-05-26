#include <Arduino.h>
#include "ereader_mode.h"
#include "textengine.h"
#include "display.h"
#include "app_registry.h"

// Ring buffer of page-start positions for going back
#define PAGE_HISTORY 8
#define MAX_LINE     40  // generous upper bound; pixel-width check is the real limit
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
  int lineWidthPx = display_line_width_px();
  for (int row = 0; row < rows; row++) {
    char line[MAX_LINE + 1];
    size_t linePos[MAX_LINE + 1]; // file pos before each char was read
    int len = 0;
    int c;

    // skip leading newlines
    while ((c = te_read_char()) != -1 && c == '\n');
    if (c == -1) break;

    // accumulate characters until pixel width is exhausted
    linePos[0] = te_current_pos(); // pos after first char
    int px = display_char_width_px((unsigned char)c);
    line[len++] = (char)c;

    while (len < MAX_LINE) {
      size_t posBefore = te_current_pos();
      c = te_read_char();
      if (c == -1 || c == '\n') break;
      int w = display_char_width_px((unsigned char)c);
      if (px + w > lineWidthPx) {
        te_seek_to(posBefore);
        c = -2; // sentinel: stopped due to width
        break;
      }
      linePos[len] = posBefore;
      line[len++] = (char)c;
      px += w;
    }
    line[len] = '\0';

    // if we stopped mid-word, back up to last space boundary
    if (c == -2 && len > 0 && line[len - 1] != ' ') {
      int cut = len - 1;
      while (cut > 0 && line[cut] != ' ') cut--;
      if (cut > 0) {
        // seek to position after the space so the next page starts clean
        te_seek_to(linePos[cut + 1]);
        line[cut] = '\0';
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

void ereader_mode_reset_history() {
  historyHead  = 0;
  historyCount = 0;
}

void ereader_mode_show_page() {
  historyPush(te_current_pos());
  renderPage();
}

void ereader_mode_short_press() {
  historyPush(te_current_pos());
  renderPage();
  te_save_position();
}

// Simulate renderPage() without drawing; return position after the last char consumed.
static size_t probePageEnd(size_t from) {
  te_seek_to(from);
  int rows = display_rows();
  int lineWidthPx = display_line_width_px();
  for (int row = 0; row < rows; row++) {
    int c;
    while ((c = te_read_char()) != -1 && c == '\n');
    if (c == -1) break;
    int px = display_char_width_px((unsigned char)c);
    while (true) {
      size_t posBefore = te_current_pos();
      c = te_read_char();
      if (c == -1 || c == '\n') break;
      int w = display_char_width_px((unsigned char)c);
      if (px + w > lineWidthPx) { te_seek_to(posBefore); break; }
      px += w;
    }
  }
  return te_current_pos();
}

// Find the start of the page that ends at or just before `target`.
// Steps back in chunks, snaps to a line boundary, then probes forward.
static size_t findPrevPageStart(size_t currentPageStart) {
  if (currentPageStart == 0) return 0;

  // Estimate: 5 rows * ~28 chars * ~5 bytes/char average = ~700 bytes,
  // but real pages are much smaller; 300 is a safe starting step.
  const size_t STEP = 300;
  size_t searchFrom = currentPageStart > STEP ? currentPageStart - STEP : 0;

  // Snap back to start of a line (after a newline or file start)
  while (searchFrom > 0) {
    te_seek_to(searchFrom - 1);
    int c = te_read_char();
    if (c == '\n') break;
    searchFrom--;
  }

  // Probe forward from searchFrom: find the largest candidate whose page ends
  // at or before currentPageStart.
  size_t bestStart = searchFrom;
  size_t pos = searchFrom;
  while (pos < currentPageStart) {
    size_t end = probePageEnd(pos);
    if (end <= currentPageStart) bestStart = pos;
    if (end >= currentPageStart) break;
    // Next candidate: skip to after the newline that ends the first line of this page
    te_seek_to(pos);
    int c;
    // skip past first line
    while ((c = te_read_char()) != -1 && c != '\n');
    size_t next = te_current_pos();
    if (next <= pos) break; // safety: no forward progress
    pos = next;
  }

  return bestStart;
}

void ereader_mode_double_press() {
  size_t currentStart;
  historyPop(&currentStart);

  size_t prev;
  if (historyCount > 0) {
    // We have a real history entry — use it
    historyPop(&prev);
  } else {
    // No history: compute previous page geometrically
    prev = findPrevPageStart(currentStart);
  }

  te_seek_to(prev);
  historyPush(prev);
  renderPage();
  te_save_position();
}

static App s_ereader_app = {
  "ereader", "EReader",
  ereader_mode_start, ereader_mode_stop, ereader_mode_loop,
  ereader_mode_short_press, ereader_mode_double_press,
  ereader_mode_show_page,
  nullptr, nullptr,
  ereader_mode_reset_history
};

static struct EreaderRegistrar {
  EreaderRegistrar() { app_registry_register(&s_ereader_app); }
} s_ereader_registrar;
