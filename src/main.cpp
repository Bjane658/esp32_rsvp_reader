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
  delay(300);  // let USB-CDC enumerate so early display logs are visible
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
    // released before threshold — continue normal boot; the app start below
    // does its own full refresh, so no clear here (avoids a double flicker)
    display_reset();
  }

  attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO), reader_onButtonChange, CHANGE);
#ifdef HAS_BUTTON2
#ifndef BUTTON2_GPIO
#define BUTTON2_GPIO 0
#endif
  pinMode(BUTTON2_GPIO, INPUT_PULLUP);  // second button, active-LOW
  attachInterrupt(digitalPinToInterrupt(BUTTON2_GPIO), reader_onButton2Change, CHANGE);
#endif
  reader_setup();
}

void loop() {
  reader_loop();
}
