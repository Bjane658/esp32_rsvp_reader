#include <Arduino.h>
#include <LittleFS.h>
#include "rsvp.h"
#include "ap.h"
#include "menu.h"
#include "display.h"

#define LONG_PRESS_MS 1000

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

static volatile bool running = false;
static volatile unsigned long pressStart = 0;
static volatile bool buttonDown = false;
static volatile bool longFired = false;
static volatile bool shortPressFlag = false;
static volatile bool longPressFlag = false;

static String textBuffer;
static String currentFile = "";
static const char* textStart = nullptr;
static const char* cursor = nullptr;
static char lastWords[3][64] = {"", "", ""};
static int lastWordIndex = 0;

#define DOUBLE_CLICK_MS 300

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

static void loadText(const String& path = "") {
  String p = path.isEmpty() ? "/text.txt" : path;

  if (LittleFS.exists(p)) {
    File f = LittleFS.open(p, "r");
    if (f) {
      textBuffer = f.readString();
      f.close();
      cursor = textBuffer.c_str();
      textStart = cursor;
      currentFile = p;
      lastWords[0][0] = '\0'; lastWords[1][0] = '\0'; lastWords[2][0] = '\0'; lastWordIndex = 0;
      Serial.print("[RSVP] Loaded text from ");
      Serial.println(p);
      return;
    }
  }

  textBuffer = "";
  cursor = FALLBACK_TEXT;
  textStart = cursor;
  currentFile = "";
  lastWords[0][0] = '\0'; lastWords[1][0] = '\0'; lastWords[2][0] = '\0'; lastWordIndex = 0;
  Serial.println("[RSVP] Using fallback text.");
}

const String& rsvp_get_current_file() {
  return currentFile;
}

void rsvp_reload_text() {
  loadText(ap_get_last_path());
  running = false;
  showPreview();
}

void rsvp_load_file(const String& path) {
  loadText(path);
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

static void showPreview() {
  display_clear();

  if (currentFile.isEmpty()) {
    display_print(0, "fallback text");
  } else {
    const char* name = currentFile.c_str();
    const char* slash = strrchr(name, '/');
    display_print(0, slash ? slash + 1 : name);
  }

  char preview[64] = "";
  int len = 0;
  const char* p = cursor;
  while (*p == ' ' || *p == '\n' || *p == '\r') p++;
  for (int w = 0; w < 3 && *p && len < (int)sizeof(preview) - 2; w++) {
    if (w > 0) preview[len++] = ' ';
    while (*p && *p != ' ' && *p != '\n' && *p != '\r' && len < (int)sizeof(preview) - 1) {
      preview[len++] = *p++;
    }
    while (*p == ' ') p++;
  }
  preview[len] = '\0';
  display_print(1, preview);
}

void rsvp_setup() {
  if (!LittleFS.begin(false)) {
    Serial.println("[RSVP] LittleFS mount failed, formatting...");
    LittleFS.format();
    if (!LittleFS.begin(false)) {
      Serial.println("[RSVP] LittleFS failed after format.");
    }
  }
  loadText();
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
        const char* p = cursor;
        if (p > textStart) p--;
        while (p > textStart && (*p == ' ' || *p == '\n' || *p == '\r')) p--;
        while (p > textStart) {
          if (*(p-1) == '.' || *(p-1) == '!' || *(p-1) == '?') break;
          p--;
        }
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        cursor = p;
        lastWords[0][0] = '\0'; lastWords[1][0] = '\0'; lastWords[2][0] = '\0'; lastWordIndex = 0;
        rsvp_show_current_word();
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

  bool pausePoint = false;
  while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r') {
    if (*cursor == '\n') {
      const char* next = cursor + 1;
      while (*next == '\r') next++;
      if (*next == '\n') {
        pausePoint = true;
      }
    }
    cursor++;
  }

  if (*cursor == '#') {
    pausePoint = true;
    while (*cursor == '#' || *cursor == ' ') cursor++;
  }

  if (pausePoint) {
    running = false;
    rsvp_show_current_word();
    return;
  }

  if (!*cursor) {
    Serial.println("--- END ---");
    running = false;
    loadText();
    return;
  }

  lastWordIndex = (lastWordIndex + 1) % 3;
  int i = 0;
  while (*cursor && *cursor != ' ' && *cursor != '\n' && *cursor != '\r' && i < (int)sizeof(lastWords[0]) - 1) {
    lastWords[lastWordIndex][i++] = *cursor++;
  }
  lastWords[lastWordIndex][i] = '\0';

  Serial.println(lastWords[lastWordIndex]);
}
