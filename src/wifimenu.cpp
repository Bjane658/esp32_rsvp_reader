#include <Arduino.h>
#include "wifimenu.h"
#include "display.h"
#include "ap.h"

static bool isOpen = false;

static void render() {
  display_reset();
  if (ap_is_active()) {
    char buf[40];
    display_print(0, "WiFi AP: ON");
    snprintf(buf, sizeof(buf), "SSID: %s", ap_get_ssid());
    display_print(1, buf);
    snprintf(buf, sizeof(buf), "PW: %s", AP_PASSWORD);
    display_print(2, buf);
    snprintf(buf, sizeof(buf), "IP: %s", ap_get_ip().c_str());
    display_print(3, buf);
  } else {
    display_print(0, "WiFi AP: OFF");
    display_print(1, "Long press: turn on");
    display_print(2, "Short press: back");
  }
  display_flush();
}

void wifimenu_open() {
  isOpen = true;
  render();
}

bool wifimenu_is_open() {
  return isOpen;
}

void wifimenu_short_press() {
  isOpen = false;
}

void wifimenu_cancel() {
  isOpen = false;
}

void wifimenu_long_press() {
  if (ap_is_active()) {
    ap_stop();
  } else {
    ap_start();
  }
  render();
}
