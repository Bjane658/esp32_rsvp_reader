#include <Arduino.h>
#include "reader.h"
#include "display.h"

#define BOOT_BUTTON 0

void setup() {
  Serial.begin(115200);
  display_setup();
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON), reader_onButtonChange, CHANGE);
  reader_setup();
}

void loop() {
  reader_loop();
}
