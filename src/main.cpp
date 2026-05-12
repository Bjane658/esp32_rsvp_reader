#include <Arduino.h>
#include "rsvp.h"

#define BOOT_BUTTON 0

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON), rsvp_onButtonChange, CHANGE);
  rsvp_setup();
}

void loop() {
  rsvp_loop();
}