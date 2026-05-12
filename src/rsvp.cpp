#include <Arduino.h>
#include <LittleFS.h>
#include "rsvp.h"
#include "ap.h"
#include "menu.h"
#include "display.h"

#define LONG_PRESS_MS 1500
#define DOUBLE_CLICK_MS 300

static volatile bool running = false;
static volatile unsigned long pressStart = 0;
static volatile bool buttonDown = false;
static volatile bool longFired = false;
static volatile bool shortPressFlag = false;
static volatile bool longPressFlag = false;

static String currentFile = "";
static File textFile;
static bool usingFallback = false;
static char lastWords[3][64] = {"", "", ""};
static int lastWordIndex = 0;

static const char* FALLBACK_TEXT =
  "Rapid serial visual presentation is a method of displaying text "
  "so that it can be read quickly. Words are shown one at a time in "
  "a fixed position on the screen, eliminating the need for eye "
  "movement across a page. This allows the reader to focus entirely "
  "on a single focal point and absorb each word as it appears. "
  "Studies suggest that most people can comfortably read at speeds "
  "between two hundred and three hundred words per minute using this "
  "technique, while trained readers can reach five hundred or more. "
  "The key advantage is that it reduces subvocalization and "
  "unnecessary saccades, making the reading process more efficient.";
static const char* fallbackCursor = nullptr;

void IRAM_ATTR rsvp_onButtonChange() {
  if (digitalRead(0) == LOW) {
    pressStart = millis();
    buttonDown = true;
    longFired = false;
  } else if (buttonDown) {
    buttonDown = false;
    if (!longFired && (millis() - pressStart >= 20)) {
      shortPressFlag = true;
    }
  }
}

static void showPreview();

static void openFile(const String& path) {
  if (textFile) textFile.close();
  usingFallback = false;
  if (!path.isEmpty() && LittleFS.exists(path)) {
    textFile = LittleFS.open(path, "r");
    if (textFile) textFile.close();
    textFile = LittleFS.open(path, "r");
    if (textFile) {
      currentFile = path;
      lastWords[0][0] = '\0'; lastWords[1][0] = '\0'; lastWords[2][0] = '\0'; lastWordIndex = 0;
      Serial.print("[RSVP] Opened: ");
      Serial.println(path);
      return;
    }
  }
  usingFallback = true;
  fallbackCursor = FALLBACK_TEXT;
  currentFile = "";
  lastWords[0][0] = '\0'; lastWords[1][0] = '\0'; lastWords[2][0] = '\0'; lastWordIndex = 0;
  Serial.println("[RSVP] Using fallback text.");
}

const String& rsvp_get_current_file() {
  return currentFile;
}

void rsvp_reload_text() {
  openFile(ap_get_last_path());
  running = false;
  showPreview();
}

void rsvp_load_file(const String& path) {
  openFile(path);
  running = false;
  showPreview();
}

void rsvp_show_current_word() {
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

void rsvp_show_preview() {
  showPreview();
}

// Read one char from current source, returns -1 at end
static int readChar() {
  if (usingFallback) {
    if (!*fallbackCursor) return -1;
    return (unsigned char)*fallbackCursor++;
  }
  if (!textFile) return -1;
  int c = textFile.read();
  return c;  // read() returns -1 at EOF natively
}

static void seekTo(size_t pos) {
  if (!usingFallback && textFile) textFile.seek(pos);
}

static size_t currentPos() {
  if (usingFallback) return fallbackCursor - FALLBACK_TEXT;
  if (!textFile) return 0;
  return textFile.position();
}

static void showPreview() {
  display_clear();
  char preview[64] = "";
  int len = 0;

  size_t savedPos = currentPos();

  int wordsRead = 0;
  while (wordsRead < 3 && len < (int)sizeof(preview) - 2) {
    int c;
    // skip whitespace
    while ((c = readChar()) != -1 && (c == ' ' || c == '\n' || c == '\r'));
    if (c == -1) break;
    // read word
    if (len > 0) preview[len++] = ' ';
    do {
      if (len < (int)sizeof(preview) - 1) preview[len++] = (char)c;
      c = readChar();
    } while (c != -1 && c != ' ' && c != '\n' && c != '\r');
    wordsRead++;
  }
  preview[len] = '\0';
  seekTo(savedPos);

  display_print(0, preview);
}

void rsvp_setup() {
  if (!LittleFS.begin(false)) {
    Serial.println("[RSVP] LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin(false);
  }
  openFile("/text.txt");
  showPreview();
}

void rsvp_loop() {
  const unsigned long msPerWord = 60000UL / menu_get_wpm();

  if (!longFired && buttonDown && (millis() - pressStart >= LONG_PRESS_MS)) {
    longFired = true;
    longPressFlag = true;
  }

  if (longPressFlag) {
    longPressFlag = false;
    if (menu_is_open()) {
      menu_long_press();
    } else {
      running = false;
      menu_open();
    }
  }

  static unsigned long firstPressTime = 0;
  static bool waitingForDouble = false;

  if (shortPressFlag) {
    shortPressFlag = false;
    if (menu_is_open()) {
      menu_short_press();
    } else if (!running) {
      if (waitingForDouble && (millis() - firstPressTime <= DOUBLE_CLICK_MS)) {
        waitingForDouble = false;
        // Rewind to start of current sentence by scanning backwards
        if (!usingFallback && textFile) {
          size_t pos = textFile.position();
          // step back past current position
          if (pos > 0) pos--;
          // skip trailing whitespace
          while (pos > 0) {
            textFile.seek(pos - 1);
            char c = textFile.read();
            if (c != ' ' && c != '\n' && c != '\r') break;
            pos--;
          }
          // scan back for sentence-ending punctuation
          while (pos > 0) {
            textFile.seek(pos - 1);
            char c = textFile.read();
            if (c == '.' || c == '!' || c == '?') break;
            pos--;
          }
          textFile.seek(pos);
        } else if (usingFallback) {
          const char* p = fallbackCursor;
          if (p > FALLBACK_TEXT) p--;
          while (p > FALLBACK_TEXT && (*p == ' ' || *p == '\n' || *p == '\r')) p--;
          while (p > FALLBACK_TEXT && *(p-1) != '.' && *(p-1) != '!' && *(p-1) != '?') p--;
          fallbackCursor = p;
        }
        lastWords[0][0] = '\0'; lastWords[1][0] = '\0'; lastWords[2][0] = '\0'; lastWordIndex = 0;
        showPreview();
      } else {
        waitingForDouble = true;
        firstPressTime = millis();
      }
    } else {
      running = false;
      waitingForDouble = false;
    }
  }

  if (waitingForDouble && (millis() - firstPressTime > DOUBLE_CLICK_MS)) {
    waitingForDouble = false;
    running = true;
  }

  if (menu_is_open()) {
    menu_loop();
    return;
  }

  if (!running) return;

  static unsigned long lastWordTime = 0;
  if (millis() - lastWordTime < msPerWord) return;
  lastWordTime = millis();

  // Skip whitespace, detect pause points
  bool pausePoint = false;
  bool prevNewline = false;
  int c;
  while ((c = readChar()) != -1 && (c == ' ' || c == '\n' || c == '\r')) {
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
    openFile(currentFile);
    showPreview();
    return;
  }

  // Heading detection
  if (c == '#') {
    pausePoint = true;
    while ((c = readChar()) != -1 && (c == '#' || c == ' '));
    // c is now first char of heading word, put it back by seeking back 1
    if (c != -1) {
      if (!usingFallback && textFile) textFile.seek(textFile.position() - 1);
      else if (usingFallback && fallbackCursor > FALLBACK_TEXT) fallbackCursor--;
    }
  }

  if (pausePoint) {
    // seek back before the next word so it re-reads from here after resume
    if (c != -1) {
      if (!usingFallback && textFile) textFile.seek(textFile.position() - 1);
      else if (usingFallback && fallbackCursor > FALLBACK_TEXT) fallbackCursor--;
    }
    running = false;
    rsvp_show_current_word();
    return;
  }

  // Read word — c already has the first character
  lastWordIndex = (lastWordIndex + 1) % 3;
  int i = 0;
  do {
    if (i < (int)sizeof(lastWords[0]) - 1) lastWords[lastWordIndex][i++] = (char)c;
    c = readChar();
  } while (c != -1 && c != ' ' && c != '\n' && c != '\r');
  lastWords[lastWordIndex][i] = '\0';

  Serial.println(lastWords[lastWordIndex]);
}
