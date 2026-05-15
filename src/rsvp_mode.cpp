#include <Arduino.h>
#include "rsvp_mode.h"
#include "textengine.h"
#include "display.h"
#include "menu.h"

static bool    running      = false;
static char    lastWords[3][64] = {"", "", ""};
static int     lastWordIndex = 0;

static void clearLastWords() {
  lastWords[0][0] = '\0';
  lastWords[1][0] = '\0';
  lastWords[2][0] = '\0';
  lastWordIndex = 0;
}

void rsvp_mode_show_current_word() {
  char line[96] = "";
  int len = 0;
  for (int i = 0; i < 3; i++) {
    int idx = (lastWordIndex + 1 + i) % 3;
    if (lastWords[idx][0] == '\0') continue;
    if (len > 0) line[len++] = ' ';
    const char* w = lastWords[idx];
    while (*w && len < (int)sizeof(line) - 1) line[len++] = *w++;
  }
  line[len] = '\0';
  display_clear();
  display_print(1, line);
}

void rsvp_mode_show_preview() {
  te_show_preview();
}

void rsvp_mode_start() {
  running = false;
  clearLastWords();
  te_show_preview();
}

void rsvp_mode_stop() {
  running = false;
}

void rsvp_mode_short_press() {
  if (running) {
    running = false;
  }
  // single press while paused: start running (handled by double-click timeout in reader)
}

void rsvp_mode_double_press() {
  if (running) { running = false; return; }
  // rewind to start of current sentence
  size_t pos = te_current_pos();
  if (pos > 0) pos--;
  // skip trailing whitespace backwards
  while (pos > 0) {
    te_seek_to(pos - 1);
    int c = te_read_char();
    if (c != ' ' && c != '\n' && c != '\r') break;
    pos--;
  }
  // scan back for sentence-ending punctuation
  while (pos > 0) {
    te_seek_to(pos - 1);
    int c = te_read_char();
    if (c == '.' || c == '!' || c == '?') break;
    pos--;
  }
  te_seek_to(pos);
  clearLastWords();
  te_show_preview();
}

void rsvp_mode_set_running(bool r) {
  running = r;
}

bool rsvp_mode_is_running() {
  return running;
}

void rsvp_mode_loop() {
  if (!running) return;

  const unsigned long msPerWord = 60000UL / menu_get_wpm();
  static unsigned long lastWordTime = 0;
  if (millis() - lastWordTime < msPerWord) return;
  lastWordTime = millis();

  // skip whitespace, detect pause points
  bool pausePoint  = false;
  bool prevNewline = false;
  int c;
  while ((c = te_read_char()) != -1 && (c == ' ' || c == '\n' || c == '\r')) {
    if (c == '\n') {
      if (prevNewline) pausePoint = true;
      prevNewline = true;
    } else if (c != '\r') {
      prevNewline = false;
    }
  }

  if (c == -1) {
    Serial.println("--- END ---");
    running = false;
    te_seek_to(0);
    clearLastWords();
    te_show_preview();
    return;
  }

  // heading detection
  if (c == '#') {
    pausePoint = true;
    while ((c = te_read_char()) != -1 && (c == '#' || c == ' '));
    if (c != -1) te_seek_to(te_current_pos() - 1);
  }

  if (pausePoint) {
    if (c != -1) te_seek_to(te_current_pos() - 1);
    running = false;
    te_save_position();
    rsvp_mode_show_current_word();
    return;
  }

  // read word
  lastWordIndex = (lastWordIndex + 1) % 3;
  int i = 0;
  do {
    if (i < (int)sizeof(lastWords[0]) - 1) lastWords[lastWordIndex][i++] = (char)c;
    c = te_read_char();
  } while (c != -1 && c != ' ' && c != '\n' && c != '\r');
  lastWords[lastWordIndex][i] = '\0';

  // peek next word for context display
  char nextWord[64] = "";
  size_t peekPos = te_current_pos();
  int nc;
  while ((nc = te_read_char()) != -1 && (nc == ' ' || nc == '\n' || nc == '\r'));
  if (nc != -1 && nc != '#') {
    int ni = 0;
    do {
      if (ni < (int)sizeof(nextWord) - 1) nextWord[ni++] = (char)nc;
      nc = te_read_char();
    } while (nc != -1 && nc != ' ' && nc != '\n' && nc != '\r');
    nextWord[ni] = '\0';
  }
  te_seek_to(peekPos);

  const char* prevWord = lastWords[(lastWordIndex + 2) % 3];
  display_word(prevWord, lastWords[lastWordIndex], nextWord);
}
