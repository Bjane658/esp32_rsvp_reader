#include <Arduino.h>
#include "HT_lCMEN2R13EFC1.h"
#include "HT_DisplayFonts.h"

// Pins + power sequence taken verbatim from Heltec's Vision Master E213
// factory test: rst, dc, cs, busy, sck, mosi, miso, frequency.
HT_ICMEN2R13EFC1 display(3, 2, 5, 1, 4, 6, -1, 6000000);

int demoMode = 0;

// E213 enables panel power by driving GPIO18 and GPIO46 HIGH (both required).
void VextON() {
  pinMode(18, OUTPUT);
  digitalWrite(18, HIGH);
  pinMode(46, OUTPUT);
  digitalWrite(46, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  VextON();
  delay(100);

  display.init();
  display.screenRotate(ANGLE_0_DEGREE);
  display.setFont(ArialMT_Plain_10);
  Serial.println("[HWTEST] setup done");
}

void loop() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  char buf[24];
  snprintf(buf, sizeof(buf), "WRAP FRAME %d", demoMode);
  display.drawString(0, 0, buf);
  display.update(BLACK_BUFFER);
  display.display();
  Serial.printf("[HWTEST] frame %d drawn\n", demoMode);
  demoMode++;
  delay(5000);
}
