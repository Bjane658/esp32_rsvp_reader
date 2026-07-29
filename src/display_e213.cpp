// E-Ink display implementation for Heltec Vision Master E213
// (2.13" 250x122 s/w, driver HT_ICMEN2R13EFC1 or HT_E0213A367 depending on
// panel revision — chosen at runtime via a chip-ID probe).
// Selected automatically when building env:vision_master_e213.

#include <Arduino.h>
#include <string.h>
#include "HT_lCMEN2R13EFC1.h"
#include "HT_DisplayFonts.h"
#include "display.h"

// --- Pin definitions (Vision Master E213, from schematic) ----------------
#define PIN_EINK_BUSY  1
#define PIN_EINK_DC    2
#define PIN_EINK_RST   3
#define PIN_EINK_SCLK  4
#define PIN_EINK_CS    5
#define PIN_EINK_MOSI  6
#define PIN_VEXT       18   // panel power enable A (drive HIGH)
#define PIN_VEXT2      46   // panel power enable B (drive HIGH)

#define MARGIN_X   4
#define MARGIN_Y   2

// --- Font layout table ---------------------------------------------------
// Landscape 250 x 122 px. Heights: ArialMT_Plain_10 = 10px, _16 = 16px.
struct FontConfig {
  const uint8_t* font;  // Heltec bitmap font
  int line_height;      // px per row
  int rows;             // how many rows fit on screen
  int cols;             // approximate max chars per row (for buffer sizing)
};

static const FontConfig FONTS[] = {
  { ArialMT_Plain_10, 22, 5, 40 },  // FONT_SMALL
  { ArialMT_Plain_16, 36, 3, 26 },  // FONT_LARGE
};

#define MAX_ROWS 5
#define MAX_COLS 40

static DisplayFont currentFont     = FONT_SMALL;
static float       progressFraction = -1.0f;
static char        buffer[MAX_ROWS][MAX_COLS + 1];
static int         cursorRow = -1;

static HT_ICMEN2R13EFC1* display = nullptr;

// --- Public query functions ----------------------------------------------

int display_cols() { return FONTS[currentFont].cols; }
int display_rows() { return FONTS[currentFont].rows; }
int display_line_width_px() { return display->width() - MARGIN_X * 2; }

int display_char_width_px(unsigned char c) {
  display->setFont(FONTS[currentFont].font);
  char s[2] = { (char)c, '\0' };
  return display->getStringWidth(s);
}

// --- Internal render -----------------------------------------------------

static void serial_dump() {
  const FontConfig& fc = FONTS[currentFont];
  Serial.println("----------------");
  for (int i = 0; i < fc.rows; i++) {
    if (buffer[i][0] == '\0') continue;
    Serial.print(i == cursorRow ? "> " : "  ");
    Serial.println(buffer[i]);
  }
}

// The panel has two RAM banks and refreshes by comparing them. A correct
// update writes a cleared "old" bank, then the new content into the second
// bank. Writing the same content to both makes the controller see no change
// and it keeps the previous image (ghosting). draw_fn renders the new frame
// between the two bank writes.
static void hw_present(void (*draw_fn)()) {
  display->clear();
  display->update(BLACK_BUFFER);   // old bank = blank
  display->clear();
  if (draw_fn) draw_fn();
  display->update(COLOR_BUFFER);   // new bank = content
  display->display();
}

static void draw_rows() {
  const FontConfig& fc = FONTS[currentFont];
  display->setFont(fc.font);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  for (int i = 0; i < fc.rows; i++) {
    if (buffer[i][0] == '\0') continue;
    int y = MARGIN_Y + i * fc.line_height;
    String line = (i == cursorRow) ? String("> ") + buffer[i] : String(buffer[i]);
    display->drawString(MARGIN_X, y, line);
  }
  if (progressFraction >= 0.0f) {
    int barW = (int)(display->width() * progressFraction);
    if (barW > 0)
      display->fillRect(0, display->height() - 3, barW, 3);
  }
}

static void hw_render() {
  hw_present(draw_rows);
}

// E213 enables panel power by driving GPIO18 and GPIO46 HIGH (both required).
static void VextON() {
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, HIGH);
  pinMode(PIN_VEXT2, OUTPUT);
  digitalWrite(PIN_VEXT2, HIGH);
}

// --- Public API ----------------------------------------------------------

void display_setup() {
  VextON();
  delay(100);
  display = new HT_ICMEN2R13EFC1(PIN_EINK_RST, PIN_EINK_DC, PIN_EINK_CS,
                                PIN_EINK_BUSY, PIN_EINK_SCLK, PIN_EINK_MOSI, -1, 6000000);
  display->init();
  display->screenRotate(ANGLE_0_DEGREE);  // 250 x 122 landscape
  // In this lib WHITE = foreground ink (drawn pixels), BLACK = background.
  // Text must be drawn with WHITE.
  display->setColor(WHITE);
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
  hw_present(nullptr);  // blank both banks -> fully white panel
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
  int limit = (row == cursorRow) ? cols - 2 : cols;
  if (limit > MAX_COLS) limit = MAX_COLS;
  strncpy(buffer[row], text, limit);
  buffer[row][limit] = '\0';
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
  buffer[row][MAX_COLS] = '\0';

  Serial.print("  ");
  Serial.println(line);
  hw_render();
}

static char       bigText[MAX_COLS + 1];
static bool       bigInvert = false;

static void draw_big() {
  if (bigInvert) {
    display->setColor(WHITE);
    display->fillRect(0, 0, display->width(), display->height());
    display->setColor(BLACK);
  } else {
    display->setColor(WHITE);
  }

  // Pick the largest bundled font whose rendered width fits the usable area.
  int usableW = display->width() - MARGIN_X * 2;
  const uint8_t* pick = ArialMT_Plain_16;
  int fontH = 16;
  const uint8_t* candidates[] = { ArialMT_Plain_24, ArialMT_Plain_16, ArialMT_Plain_10 };
  int heights[] = { 24, 16, 10 };
  for (int i = 0; i < 3; i++) {
    display->setFont(candidates[i]);
    if ((int)display->getStringWidth(bigText) <= usableW) {
      pick = candidates[i];
      fontH = heights[i];
      break;
    }
  }

  display->setFont(pick);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(display->width() / 2, (display->height() - fontH) / 2, bigText);

  display->setColor(WHITE);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

// Full-screen, centered text. Used by the timer to show remaining time as
// large as the panel allows. invert = true -> black background + white text.
void display_print_big(const char* text, bool invert) {
  for (int i = 0; i < MAX_ROWS; i++)
    buffer[i][0] = '\0';
  cursorRow = -1;
  progressFraction = -1.0f;

  strncpy(bigText, text, MAX_COLS);
  bigText[MAX_COLS] = '\0';
  bigInvert = invert;
  hw_present(draw_big);
}
