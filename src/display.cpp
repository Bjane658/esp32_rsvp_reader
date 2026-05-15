#include <Arduino.h>
#include <string.h>
#include <heltec-eink-modules.h>
#include "display.h"

#define ORP_FOCAL  (DISPLAY_COLS * 2 / 5)
#define LINE_HEIGHT 16
#define MARGIN_X     4
#define MARGIN_Y     4

static EInkDisplay_WirelessPaperV1_2 display;
static bool fastmodeActive = false;

static char buffer[DISPLAY_ROWS][DISPLAY_COLS + 1];
static int cursorRow = -1;

static void serial_dump() {
  Serial.println("----------------");
  for (int i = 0; i < DISPLAY_ROWS; i++) {
    if (buffer[i][0] == '\0') continue;
    Serial.print(i == cursorRow ? "> " : "  ");
    Serial.println(buffer[i]);
  }
}

static void hw_render() {
  if (!fastmodeActive) {
    display.fastmodeOn();
    fastmodeActive = true;
  }
  display.clearMemory();
  display.landscape();
  for (int i = 0; i < DISPLAY_ROWS; i++) {
    if (buffer[i][0] == '\0') continue;
    display.setTextColor(BLACK);
    display.setCursor(MARGIN_X, MARGIN_Y + i * LINE_HEIGHT + LINE_HEIGHT - 3);
    if (i == cursorRow) display.print("> ");
    display.print(buffer[i]);
  }
  display.update();
}

void display_setup() {
  display.landscape();
  display_clear();
}

void display_clear() {
  for (int i = 0; i < DISPLAY_ROWS; i++)
    buffer[i][0] = '\0';
  cursorRow = -1;
  if (fastmodeActive) {
    display.fastmodeOff();
    fastmodeActive = false;
  }
  display.clear();
}

void display_reset() {
  for (int i = 0; i < DISPLAY_ROWS; i++)
    buffer[i][0] = '\0';
  cursorRow = -1;
}

void display_print(int row, const char* text) {
  if (row < 0 || row >= DISPLAY_ROWS) return;
  strncpy(buffer[row], text, sizeof(buffer[row]) - 1);
  buffer[row][sizeof(buffer[row]) - 1] = '\0';
}

void display_cursor(int row) {
  cursorRow = row;
}

void display_flush() {
  serial_dump();
  hw_render();
}

void display_word(const char* prev, const char* word, const char* next) {
  int wlen = (int)strlen(word);

  int orp = wlen > 1 ? (wlen * 3) / 10 : 0;

  int ctx = 8 - wlen;
  if (ctx < 2) ctx = 2;

  int prevLen = prev ? (int)strlen(prev) : 0;
  int prevCtx = prevLen > ctx ? ctx : prevLen;
  int pad = ORP_FOCAL - orp - prevCtx - (prevCtx > 0 ? 1 : 0);
  if (pad < 0) pad = 0;

  char line[DISPLAY_COLS + 1];
  int pos = 0;

  while (pos < pad && pos < DISPLAY_COLS)
    line[pos++] = ' ';

  if (prevCtx > 0 && prev) {
    const char* start = prev + prevLen - prevCtx;
    while (*start && pos < DISPLAY_COLS)
      line[pos++] = *start++;
    if (pos < DISPLAY_COLS)
      line[pos++] = ' ';
  }

  const char* w = word;
  while (*w && pos < DISPLAY_COLS)
    line[pos++] = *w++;

  if (next && *next && pos < DISPLAY_COLS) {
    line[pos++] = ' ';
    int ni = 0;
    while (next[ni] && ni < ctx && pos < DISPLAY_COLS)
      line[pos++] = next[ni++];
  }

  line[pos] = '\0';

  for (int i = 0; i < DISPLAY_ROWS; i++)
    buffer[i][0] = '\0';
  cursorRow = -1;
  strncpy(buffer[DISPLAY_ROWS / 2], line, sizeof(buffer[DISPLAY_ROWS / 2]) - 1);

  display_flush();
}
