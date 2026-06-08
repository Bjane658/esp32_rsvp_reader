#include <Arduino.h>
#include "reader.h"
#include "display.h"
#include "ota.h"

#ifndef BUTTON_GPIO
#define BUTTON_GPIO 0
#endif
#define OTA_HOLD_MS 3000

void setup() {
  Serial.begin(115200);
  display_setup();
  pinMode(BUTTON_GPIO, INPUT_PULLUP);

  // Boot-hold: if button is held for 3 s at power-on, go straight to OTA
  if (digitalRead(BUTTON_GPIO) == LOW) {
    display_set_font(FONT_SMALL);
    display_print(0, "Hold for OTA...");
    display_flush();
    unsigned long held = millis();
    while (digitalRead(BUTTON_GPIO) == LOW) {
      if (millis() - held >= OTA_HOLD_MS) {
        ota_run();  // does not return — reboots on finish
      }
    }
    // released before threshold — continue normal boot
    display_clear();
  }

  attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO), reader_onButtonChange, CHANGE);
  reader_setup();
}

void loop() {
  reader_loop();
}
