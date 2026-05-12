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

static volatile bool running = true;
static volatile unsigned long pressStart = 0;
static volatile bool buttonDown = false;
static volatile bool longFired = false;
static volatile bool shortPressFlag = false;
static volatile bool longPressFlag = false;

static String textBuffer;
static const char* cursor = nullptr;
static char lastWord[64] = "";

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

static void loadText() {
  if (LittleFS.exists("/text.txt")) {
    File f = LittleFS.open("/text.txt", "r");
    if (f) {
      textBuffer = f.readString();
      f.close();
      cursor = textBuffer.c_str();
      Serial.println("[RSVP] Loaded text from LittleFS.");
      return;
    }
  }
  textBuffer = "";
  cursor = FALLBACK_TEXT;
  Serial.println("[RSVP] Using fallback text.");
}

void rsvp_reload_text() {
  loadText();
}

void rsvp_show_current_word() {
  display_clear();
  display_print(1, lastWord);
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

  if (shortPressFlag) {
    shortPressFlag = false;
    if (menu_is_open()) {
      menu_short_press();
    } else {
      running = !running;
    }
  }

  if (menu_is_open()) {
    menu_loop();
    return;
  }

  if (!running) return;

  static unsigned long lastWordTime = 0;
  if (millis() - lastWordTime < msPerWord) return;
  lastWordTime = millis();

  while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r') cursor++;

  if (!*cursor) {
    Serial.println("--- END ---");
    running = false;
    loadText();
    return;
  }

  int i = 0;
  while (*cursor && *cursor != ' ' && *cursor != '\n' && *cursor != '\r' && i < (int)sizeof(lastWord) - 1) {
    lastWord[i++] = *cursor++;
  }
  lastWord[i] = '\0';

  Serial.println(lastWord);
}
