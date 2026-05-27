// TFT display implementation for Heltec Vision Master T190
// (1.9" ST7789, 240x320px, landscape = 320x240)
// Selected automatically when building env:vision_master_t190.

#include <Arduino.h>
#include <SPI.h>
#include <string.h>
#include <Adafruit_GFX.h>
#include "HT_st7789spi.h"
#include "DejaVuSans9pt7b.h"
#include "DejaVuSans12pt7b.h"
#include "display.h"

// --- Pin definitions (Vision Master T190 v1.x) --------------------------
#define TFT_CS    39
#define TFT_DC    47
#define TFT_RST   40
#define TFT_SCLK  38
#define TFT_MOSI  48
#define TFT_LED   17   // backlight cathode: LOW = on
#define TFT_PWR    7   // display power enable: LOW = on

#define MARGIN_X   6
#define MARGIN_Y   6

// 16-bit RGB565 colours
#define TFT_WHITE  0xFFFF
#define TFT_BLACK  0x0000

// --- Font layout table ---------------------------------------------------
// Rotated display: 320 x 240 px (landscape)
// Usable width:  320 - 2*MARGIN_X = 308 px
// Usable height: 240 - 2*MARGIN_Y = 228 px
struct FontConfig {
  const GFXfont* gfx;
  int line_height;      // px per row
  int baseline_offset;  // px from top of row to text baseline
  int rows;             // how many rows fit on screen
  int cols;             // approximate max chars per row (for buffer sizing)
};

static const FontConfig FONTS[] = {
  { &DejaVuSans9pt8b,  22, 17, 9, 35 },  // FONT_SMALL
  { &DejaVuSans12pt8b, 30, 23, 6, 24 },  // FONT_LARGE
};

#define MAX_ROWS 10
#define MAX_COLS 36

static DisplayFont currentFont    = FONT_SMALL;
static float       progressFraction = -1.0f;
static char        buffer[MAX_ROWS][MAX_COLS + 1];
static int         cursorRow = -1;

// Software SPI (explicit MOSI/SCLK pins, bypasses default SPI bus)
static HT_ST7789 tft(240, 320, TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// --- Public query functions ----------------------------------------------

int display_cols() { return FONTS[currentFont].cols; }
int display_rows() { return FONTS[currentFont].rows; }
int display_line_width_px() { return tft.width() - MARGIN_X * 2; }

int display_char_width_px(unsigned char c) {
  const GFXfont* font = FONTS[currentFont].gfx;
  uint8_t first = pgm_read_byte(&font->first);
  uint8_t last  = pgm_read_byte(&font->last);
  if (c < first || c > last) return 0;
  GFXglyph* glyph = &(((GFXglyph*)pgm_read_ptr(&font->glyph))[c - first]);
  return pgm_read_byte(&glyph->xAdvance);
}

// --- Internal render -----------------------------------------------------

static void hw_render() {
  const FontConfig& fc = FONTS[currentFont];

  tft.fillScreen(TFT_WHITE);
  tft.setFont(fc.gfx);
  tft.setTextColor(TFT_BLACK);
  tft.setTextWrap(false);  // we handle truncation ourselves

  for (int i = 0; i < fc.rows; i++) {
    if (buffer[i][0] == '\0') continue;
    tft.setCursor(MARGIN_X, MARGIN_Y + i * fc.line_height + fc.baseline_offset);
    if (i == cursorRow) tft.print("> ");
    tft.print(buffer[i]);
  }

  if (progressFraction >= 0.0f) {
    int barW = (int)(tft.width() * progressFraction);
    if (barW > 0)
      tft.fillRect(0, tft.height() - 4, barW, 4, TFT_BLACK);
  }
}

// --- Public API ----------------------------------------------------------

void display_setup() {
  // Power on display and backlight
  pinMode(TFT_PWR, OUTPUT);
  digitalWrite(TFT_PWR, LOW);   // active LOW
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW);   // LED cathode: LOW = backlight on
  delay(10);

  tft.init(240, 320);
  tft.setRotation(1);           // landscape: 320 x 240
  tft.fillScreen(TFT_WHITE);
}

void display_set_font(DisplayFont f) {
  currentFont = f;
}

void display_set_progress(float fraction) {
  progressFraction = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
}

void display_clear() {
  for (int i = 0; i < MAX_ROWS; i++) buffer[i][0] = '\0';
  cursorRow = -1;
  progressFraction = -1.0f;
  tft.fillScreen(TFT_WHITE);
}

void display_reset() {
  for (int i = 0; i < MAX_ROWS; i++) buffer[i][0] = '\0';
  cursorRow = -1;
  progressFraction = -1.0f;
}

void display_print(int row, const char* text) {
  int rows = FONTS[currentFont].rows;
  int cols = FONTS[currentFont].cols;
  if (row < 0 || row >= rows) return;
  int limit = (row == cursorRow) ? cols - 2 : cols;
  strncpy(buffer[row], text, limit);
  buffer[row][limit] = '\0';
}

void display_cursor(int row) {
  cursorRow = row;
}

void display_flush() {
  // Serial debug (same format as e-ink version)
  const FontConfig& fc = FONTS[currentFont];
  Serial.println("----------------");
  for (int i = 0; i < fc.rows; i++) {
    if (buffer[i][0] == '\0') continue;
    Serial.print(i == cursorRow ? "> " : "  ");
    Serial.println(buffer[i]);
  }
  hw_render();
}

void display_word(const char* prev, const char* word, const char* next) {
  int cols = FONTS[currentFont].cols;
  int rows = FONTS[currentFont].rows;
  int orp_focal = cols * 2 / 5;

  int wlen = (int)strlen(word);
  int orp  = wlen > 1 ? (wlen * 3) / 10 : 0;
  int ctx  = 8 - wlen;
  if (ctx < 2) ctx = 2;

  int prevLen  = prev ? (int)strlen(prev) : 0;
  int prevCtx  = prevLen > ctx ? ctx : prevLen;
  int pad      = orp_focal - orp - prevCtx - (prevCtx > 0 ? 1 : 0);
  if (pad < 0) pad = 0;

  char line[MAX_COLS + 1];
  int  pos = 0;

  while (pos < pad && pos < cols)
    line[pos++] = ' ';

  if (prevCtx > 0 && prev) {
    const char* start = prev + prevLen - prevCtx;
    while (*start && pos < cols) line[pos++] = *start++;
    if (pos < cols) line[pos++] = ' ';
  }

  const char* w = word;
  while (*w && pos < cols) line[pos++] = *w++;

  if (next && *next && pos < cols) {
    line[pos++] = ' ';
    int ni = 0;
    while (next[ni] && ni < ctx && pos < cols) line[pos++] = next[ni++];
  }
  line[pos] = '\0';

  int row = rows / 2;
  for (int i = 0; i < MAX_ROWS; i++) buffer[i][0] = '\0';
  cursorRow = -1;
  strncpy(buffer[row], line, MAX_COLS);
  buffer[row][MAX_COLS] = '\0';

  Serial.print("  ");
  Serial.println(line);
  hw_render();
}
