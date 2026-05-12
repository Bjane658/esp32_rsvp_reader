#include <Arduino.h>
#include "rsvp.h"
#include "display.h"

#define BOOT_BUTTON 0

void setup() {
  Serial.begin(115200);
  display_setup();
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON), rsvp_onButtonChange, CHANGE);
  rsvp_setup();
}

void loop() {
  rsvp_loop();
}