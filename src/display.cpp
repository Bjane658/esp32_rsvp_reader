#include <Arduino.h>
#include <string.h>
#include "display.h"

// ORP focal column: ~40% across the display
#define ORP_FOCAL (DISPLAY_COLS * 2 / 5)

static char buffer[DISPLAY_ROWS][DISPLAY_COLS + 1];
static int cursorRow = -1;

void display_setup() {
  display_clear();
}

void display_clear() {
  for (int i = 0; i < DISPLAY_ROWS; i++) {
    buffer[i][0] = '\0';
  }
  cursorRow = -1;
}

void display_print(int row, const char* text) {
  if (row < 0 || row >= DISPLAY_ROWS) return;
  strncpy(buffer[row], text, sizeof(buffer[row]) - 1);
  buffer[row][sizeof(buffer[row]) - 1] = '\0';

  for (int i = 0; i < DISPLAY_ROWS; i++) {
    if (buffer[i][0] == '\0') continue;
    if (i == cursorRow) Serial.print("> ");
    else                 Serial.print("  ");
    Serial.println(buffer[i]);
  }
}

void display_cursor(int row) {
  cursorRow = row;
}

void display_word(const char* prev, const char* word, const char* next) {
  int wlen = (int)strlen(word);

  // ORP index: ~30% into the word, minimum 0
  int orp = wlen > 1 ? (wlen * 3) / 10 : 0;

  // context chars to show from prev/next (more context for short words)
  int ctx = 8 - wlen;
  if (ctx < 2) ctx = 2;

  // left padding so ORP character of the main word lands on ORP_FOCAL column
  // account for prev context + space before the word
  int prevLen = prev ? (int)strlen(prev) : 0;
  int prevCtx = prevLen > ctx ? ctx : prevLen;
  int pad = ORP_FOCAL - orp - prevCtx - (prevCtx > 0 ? 1 : 0);
  if (pad < 0) pad = 0;

  char line[DISPLAY_COLS + 1];
  int pos = 0;

  // leading spaces
  while (pos < pad && pos < DISPLAY_COLS)
    line[pos++] = ' ';

  // prev word tail
  if (prevCtx > 0 && prev) {
    const char* start = prev + prevLen - prevCtx;
    while (*start && pos < DISPLAY_COLS)
      line[pos++] = *start++;
    if (pos < DISPLAY_COLS)
      line[pos++] = ' ';
  }

  // current word
  const char* w = word;
  while (*w && pos < DISPLAY_COLS)
    line[pos++] = *w++;

  // next word head
  if (next && *next && pos < DISPLAY_COLS) {
    line[pos++] = ' ';
    int ni = 0;
    while (next[ni] && ni < ctx && pos < DISPLAY_COLS)
      line[pos++] = next[ni++];
  }

  line[pos] = '\0';

  display_clear();
  display_print(DISPLAY_ROWS / 2, line);
}
