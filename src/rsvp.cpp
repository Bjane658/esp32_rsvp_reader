#include <Arduino.h>
#include <LittleFS.h>
#include "rsvp.h"
#include "ap.h"

#define WPM 250
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
static volatile bool shortPressFlag = false;
static volatile bool longPressFlag = false;

static String textBuffer;
static const char* cursor = nullptr;

void IRAM_ATTR rsvp_onButtonChange() {
  if (digitalRead(0) == LOW) {
    pressStart = millis();
    buttonDown = true;
  } else if (buttonDown) {
    unsigned long held = millis() - pressStart;
    buttonDown = false;
    if (held >= LONG_PRESS_MS) {
      longPressFlag = true;
    } else if (held >= 20) {
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
  const unsigned long msPerWord = 60000UL / WPM;

  if (longPressFlag) {
    longPressFlag = false;
    if (ap_is_active()) {
      ap_stop();
      loadText();
    } else {
      running = false;
      ap_start();
    }
    return;
  }

  if (shortPressFlag) {
    shortPressFlag = false;
    if (!ap_is_active()) {
      running = !running;
    }
    return;
  }

  if (ap_is_active()) {
    ap_loop();
    delay(10);
    return;
  }

  if (!running) {
    delay(50);
    return;
  }

  while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r') cursor++;

  if (!*cursor) {
    Serial.println("--- END ---");
    running = false;
    loadText();
    return;
  }

  char word[64];
  int i = 0;
  while (*cursor && *cursor != ' ' && *cursor != '\n' && *cursor != '\r' && i < (int)sizeof(word) - 1) {
    word[i++] = *cursor++;
  }
  word[i] = '\0';

  Serial.println(word);
  delay(msPerWord);
}
