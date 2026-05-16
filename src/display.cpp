#include <Arduino.h>
#include <string.h>
#include <heltec-eink-modules.h>
#include "DejaVuSans9pt7b.h"
#include "DejaVuSans12pt7b.h"
#include "display.h"

#define MARGIN_X  4
#define MARGIN_Y  4

// Per-font layout params
struct FontConfig {
  const GFXfont* gfx;
  int line_height;
  int baseline_offset; // pixels from top of line to text baseline
  int rows;
  int cols;
};

static const FontConfig FONTS[] = {
  { &DejaVuSans9pt8b,  22, 17, 5, 28 },  // FONT_SMALL
  { &DejaVuSans12pt8b, 30, 23, 3, 20 },  // FONT_LARGE
};

static DisplayFont currentFont = FONT_SMALL;
static float progressFraction = -1.0f; // negative means don't draw

static EInkDisplay_WirelessPaperV1_2 display;
static bool fastmodeActive = false;

#define MAX_ROWS 5
#define MAX_COLS 28
static char buffer[MAX_ROWS][MAX_COLS + 1];
static int cursorRow = -1;

int display_cols() { return FONTS[currentFont].cols; }
int display_rows() { return FONTS[currentFont].rows; }

static void serial_dump() {
  const FontConfig& fc = FONTS[currentFont];
  Serial.println("----------------");
  for (int i = 0; i < fc.rows; i++) {
    if (buffer[i][0] == '\0') continue;
    Serial.print(i == cursorRow ? "> " : "  ");
    Serial.println(buffer[i]);
  }
}

static void ensure_fastmode() {
  if (!fastmodeActive) {
    display.fastmodeOn();
    fastmodeActive = true;
  }
}

static void hw_render() {
  const FontConfig& fc = FONTS[currentFont];
  ensure_fastmode();
  display.clearMemory();
  display.setRotation(1);
  display.setFont(fc.gfx);
  for (int i = 0; i < fc.rows; i++) {
    if (buffer[i][0] == '\0') continue;
    display.setTextColor(BLACK);
    display.setCursor(MARGIN_X, MARGIN_Y + i * fc.line_height + fc.baseline_offset);
    if (i == cursorRow) display.print("> ");
    display.print(buffer[i]);
  }
  if (progressFraction >= 0.0f) {
    int barW = (int)(display.width() * progressFraction);
    if (barW > 0)
      display.fillRect(0, display.height() - 3, barW, 3, BLACK);
  }
  display.update();
}

static void hw_render_row(int row) {
  const FontConfig& fc = FONTS[currentFont];
  ensure_fastmode();
  int y = MARGIN_Y + row * fc.line_height;
  display.setWindow(0, y, display.width(), fc.line_height);
  display.setFont(fc.gfx);
  display.setTextColor(BLACK);
  display.setCursor(MARGIN_X, y + fc.baseline_offset);
  display.print(buffer[row]);
  display.update();
  display.setWindow(0, 0, display.width(), display.height());
}

void display_setup() {
  display.setRotation(1);
  display_clear();
}

void display_set_font(DisplayFont f) {
  currentFont = f;
}

void display_set_progress(float fraction) {
  progressFraction = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
}

void display_clear() {
  for (int i = 0; i < MAX_ROWS; i++)
    buffer[i][0] = '\0';
  cursorRow = -1;
  if (fastmodeActive) {
    display.fastmodeOff();
    fastmodeActive = false;
  }
  display.clear();
}

void display_reset() {
  for (int i = 0; i < MAX_ROWS; i++)
    buffer[i][0] = '\0';
  cursorRow = -1;
  progressFraction = -1.0f;
}

void display_print(int row, const char* text) {
  int rows = FONTS[currentFont].rows;
  int cols = FONTS[currentFont].cols;
  if (row < 0 || row >= rows) return;
  strncpy(buffer[row], text, cols);
  buffer[row][cols] = '\0';
}

void display_cursor(int row) {
  cursorRow = row;
}

void display_flush() {
  serial_dump();
  hw_render();
}

void display_word(const char* prev, const char* word, const char* next) {
  int cols = FONTS[currentFont].cols;
  int rows = FONTS[currentFont].rows;
  int orp_focal = cols * 2 / 5;

  int wlen = (int)strlen(word);
  int orp = wlen > 1 ? (wlen * 3) / 10 : 0;
  int ctx = 8 - wlen;
  if (ctx < 2) ctx = 2;

  int prevLen = prev ? (int)strlen(prev) : 0;
  int prevCtx = prevLen > ctx ? ctx : prevLen;
  int pad = orp_focal - orp - prevCtx - (prevCtx > 0 ? 1 : 0);
  if (pad < 0) pad = 0;

  char line[MAX_COLS + 1];
  int pos = 0;

  while (pos < pad && pos < cols)
    line[pos++] = ' ';

  if (prevCtx > 0 && prev) {
    const char* start = prev + prevLen - prevCtx;
    while (*start && pos < cols)
      line[pos++] = *start++;
    if (pos < cols)
      line[pos++] = ' ';
  }

  const char* w = word;
  while (*w && pos < cols)
    line[pos++] = *w++;

  if (next && *next && pos < cols) {
    line[pos++] = ' ';
    int ni = 0;
    while (next[ni] && ni < ctx && pos < cols)
      line[pos++] = next[ni++];
  }

  line[pos] = '\0';

  int row = rows / 2;
  for (int i = 0; i < MAX_ROWS; i++)
    buffer[i][0] = '\0';
  cursorRow = -1;
  strncpy(buffer[row], line, MAX_COLS);

  Serial.print("  ");
  Serial.println(line);
  hw_render_row(row);
}
