#include <Arduino.h>
#include "display.h"

#define DISPLAY_ROWS 4

static char buffer[DISPLAY_ROWS][32];
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

  Serial.println("----");
  for (int i = 0; i < DISPLAY_ROWS; i++) {
    if (buffer[i][0] == '\0') continue;
    if (i == cursorRow) {
      Serial.print("> ");
    } else {
      Serial.print("  ");
    }
    Serial.println(buffer[i]);
  }
  Serial.println("----");
}

void display_cursor(int row) {
  cursorRow = row;
}
