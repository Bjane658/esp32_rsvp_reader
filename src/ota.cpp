#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include "ota.h"
#include "display.h"
#include "version.h"

#define OTA_FIRMWARE_URL "https://github.com/Bjane658/esp32_rsvp_reader/releases/latest/download/firmware.bin"
#define OTA_VERSION_URL  "https://github.com/Bjane658/esp32_rsvp_reader/releases/latest/download/version.txt"
#define BOOT_BUTTON      0
#define CONFIRM_LONG_MS  1000

static void show(const char* line0, const char* line1 = nullptr, const char* line2 = nullptr, const char* line3 = nullptr) {
  display_reset();
  if (line0) display_print(0, line0);
  if (line1) display_print(1, line1);
  if (line2) display_print(2, line2);
  if (line3) display_print(3, line3);
  display_flush();
}

static bool wait_for_button() {
  while (digitalRead(BOOT_BUTTON) == HIGH) delay(10);
  unsigned long pressedAt = millis();
  while (digitalRead(BOOT_BUTTON) == LOW) delay(10);
  return (millis() - pressedAt) >= CONFIRM_LONG_MS;
}

static String fetch_string(const char* url) {
  Serial.printf("[OTA] GET %s\n", url);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  int code = http.GET();
  Serial.printf("[OTA] Response: %d\n", code);
  String result = "";
  if (code == HTTP_CODE_OK) {
    result = http.getString();
    result.trim();
    Serial.printf("[OTA] Body: %s\n", result.c_str());
  } else {
    Serial.printf("[OTA] Error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
  return result;
}

static bool do_flash(const char* url) {
  Serial.printf("[OTA] GET %s\n", url);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  int code = http.GET();
  Serial.printf("[OTA] Response: %d\n", code);
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Error: %s\n", http.errorToString(code).c_str());
    char err[28];
    snprintf(err, sizeof(err), "HTTP error: %d", code);
    show("Download failed.", err, "Rebooting...");
    http.end();
    delay(3000);
    ESP.restart();
  }
  Serial.printf("[OTA] Content-Length: %d\n", http.getSize());

  int totalSize = http.getSize();
  if (!Update.begin(totalSize > 0 ? totalSize : UPDATE_SIZE_UNKNOWN)) {
    show("Flash failed!", "Not enough space?", "Rebooting...");
    http.end();
    delay(3000);
    ESP.restart();
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[512];
  int written = 0;
  int lastPct = -1;

  while (http.connected() && (totalSize <= 0 || written < totalSize)) {
    int available = stream->available();
    if (available > 0) {
      int n = stream->readBytes(buf, min(available, (int)sizeof(buf)));
      Update.write(buf, n);
      written += n;
      if (totalSize > 0) {
        int pct = written * 100 / totalSize;
        if (pct != lastPct) {
          lastPct = pct;
          char line[24];
          snprintf(line, sizeof(line), "Flashing... %d%%", pct);
          display_set_progress(pct / 100.0f);
          show("Installing...", line);
        }
      }
    } else {
      delay(1);
    }
  }

  http.end();
  return Update.end(true);
}

void ota_run() {
  display_set_font(FONT_SMALL);
  display_clear();
  show("OTA Update", "Connecting to:", OTA_SSID);

  Serial.printf("[OTA] Connecting to SSID: %s\n", OTA_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(OTA_SSID, OTA_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 20000) {
      Serial.println("[OTA] Connection timed out");
      show("OTA Update", "Connect failed.", "Rebooting...");
      delay(3000);
      ESP.restart();
    }
    delay(200);
  }
  Serial.printf("[OTA] Connected, IP: %s\n", WiFi.localIP().toString().c_str());

  show("OTA Update", "Searching for", "updates...");

  String remoteVersion = fetch_string(OTA_VERSION_URL);
  if (remoteVersion.isEmpty()) remoteVersion = "unknown";

  char line1[32], line2[32];
  snprintf(line1, sizeof(line1), "New: %s", remoteVersion.c_str());
  snprintf(line2, sizeof(line2), "Now: %s", FW_VERSION);
  show("Update found!", line1, line2, "Long:install Short:no");

  if (!wait_for_button()) {
    WiFi.disconnect(true);
    show("Aborted.", "Rebooting...");
    delay(1500);
    ESP.restart();
  }

  show("Installing...", "Please wait...");

  if (!do_flash(OTA_FIRMWARE_URL)) {
    Serial.printf("[OTA] Flash failed: %s\n", Update.errorString());
    show("Flash failed!", "Rebooting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("[OTA] Flash complete, rebooting");
  show("Done!", "Rebooting...");
  delay(2000);
  ESP.restart();
}
