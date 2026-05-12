#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "ap.h"

#define AP_SSID "RSVP-Reader"

static WebServer server(80);
static bool active = false;
static String lastPath = "";

static const char UPLOAD_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>RSVP Upload</title></head>
<body>
<h2>Upload Text</h2>
<form method="POST" action="/upload">
  <input type="text" name="title" placeholder="Title" required><br><br>
  <textarea name="text" rows="12" cols="50" placeholder="Paste your text here..."></textarea><br><br>
  <input type="submit" value="Upload">
</form>
</body>
</html>
)";

static void handleRoot() {
  server.send(200, "text/html", UPLOAD_HTML);
}

static void handleUpload() {
  if (!server.hasArg("title") || server.arg("title").isEmpty()) {
    server.send(400, "text/plain", "No title provided.");
    return;
  }
  if (!server.hasArg("text") || server.arg("text").isEmpty()) {
    server.send(400, "text/plain", "No text provided.");
    return;
  }

  String title = server.arg("title");
  String path = "/" + title + ".txt";

  File f = LittleFS.open(path, "w");
  if (!f) {
    server.send(500, "text/plain", "Failed to open file.");
    return;
  }
  f.print(server.arg("text"));
  f.close();
  lastPath = path;

  server.send(200, "text/plain", "Uploaded! You can close this page.");
  Serial.print("[AP] Text saved to ");
  Serial.println(path);
}

void ap_start() {
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[AP] Started. Connect to: ");
  Serial.println(AP_SSID);
  Serial.print("[AP] Open: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, handleUpload);
  server.begin();
  active = true;
}

void ap_stop() {
  server.stop();
  WiFi.softAPdisconnect(true);
  active = false;
  Serial.println("[AP] Stopped.");
}

bool ap_is_active() {
  return active;
}

const String& ap_get_last_path() {
  return lastPath;
}

void ap_loop() {
  if (active) {
    server.handleClient();
  }
}
