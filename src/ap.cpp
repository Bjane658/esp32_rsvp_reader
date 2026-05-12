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
<h2>Upload Text File</h2>
<form method="POST" action="/upload" enctype="multipart/form-data">
  <input type="file" name="file" accept=".txt" required><br><br>
  <input type="submit" value="Upload">
</form>
</body>
</html>
)";

static void handleRoot() {
  server.send(200, "text/html", UPLOAD_HTML);
}

static File uploadFile;

static void handleUploadData() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("[AP] Receiving: ");
    Serial.println(filename);
    uploadFile = LittleFS.open(filename, "w");
    lastPath = filename;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.print("[AP] Saved to ");
      Serial.print(lastPath);
      File check = LittleFS.open(lastPath, "r");
      Serial.printf(" size=%d\n", check ? (int)check.size() : -1);
      if (check) check.close();
    }
  }
}

static void handleUpload() {
  server.send(200, "text/plain", "Uploaded! You can close this page.");
}

void ap_start() {
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[AP] Started. Connect to: ");
  Serial.println(AP_SSID);
  Serial.print("[AP] Open: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, handleUpload, handleUploadData);
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
