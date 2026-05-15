#include <Arduino.h>
#include "wifimenu.h"
#include "display.h"
#include "ap.h"
#include "textengine.h"

static bool open = false;

static void render() {
  display_clear();
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
}

void wifimenu_open() {
  open = true;
  render();
}

bool wifimenu_is_open() {
  return open;
}

void wifimenu_short_press() {
  open = false;
}

void wifimenu_cancel() {
  open = false;
}

void wifimenu_long_press() {
  if (ap_is_active()) {
    ap_stop();
    te_reload_from_ap();
  } else {
    ap_start();
  }
  render();
}
