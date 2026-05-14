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

void display_word(const char* word) {
  int wlen = (int)strlen(word);

  // ORP index: ~30% into the word, minimum 0
  int orp = wlen > 1 ? (wlen * 3) / 10 : 0;

  // left padding so ORP character lands on ORP_FOCAL column
  int pad = ORP_FOCAL - orp;
  if (pad < 0) pad = 0;

  // build padded string into the centre row buffer
  char line[DISPLAY_COLS + 1];
  int pos = 0;
  while (pos < pad && pos < DISPLAY_COLS)
    line[pos++] = ' ';
  int wi = 0;
  while (wi < wlen && pos < DISPLAY_COLS)
    line[pos++] = word[wi++];
  line[pos] = '\0';

  display_clear();
  display_print(DISPLAY_ROWS / 2, line);
}
