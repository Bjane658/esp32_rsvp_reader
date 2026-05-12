#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "ap.h"

#define AP_SSID "RSVP-Reader"

static WebServer server(80);
static bool active = false;
static String lastPath = "";

static void handleRoot() {
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  size_t free  = total - used;
  int pct = total ? (int)(used * 100 / total) : 0;

  String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>RSVP Upload</title></head><body>"
    "<h2>Upload Text File</h2>"
    "<form method='POST' action='/upload' enctype='multipart/form-data'>"
    "<input type='file' name='file' accept='.txt' required><br><br>"
    "<input type='submit' value='Upload'>"
    "</form><hr><h3>Storage</h3><p>");
  html += String(used / 1024) + " KB used / " + String(total / 1024) + " KB total ("
       + String(free / 1024) + " KB free, " + String(pct) + "%)</p>";

  html += F("<hr><h3>Files</h3><ul>");
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      html += "<li>" + String(f.name()) + " (" + String(f.size() / 1024) + " KB)</li>";
    }
    f = root.openNextFile();
  }
  html += F("</ul></body></html>");

  server.send(200, "text/html", html);
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
      Serial.println(lastPath);
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
