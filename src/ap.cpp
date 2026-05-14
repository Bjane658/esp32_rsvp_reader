#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "ap.h"
#include "frontend.h"

#define AP_SSID "RSVP-Reader"

static WebServer server(80);
static bool active = false;
static String lastPath = "";

static void sendChunk(const char* s) {
  server.sendContent(s);
}

static void handleRoot() {
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  size_t free_ = total - used;
  int pct = total ? (int)(used * 100 / total) : 0;

  char storage_info[64];
  snprintf(storage_info, sizeof(storage_info),
    "%u KB used / %u KB total (%u KB free, %d%%)",
    (unsigned)(used / 1024), (unsigned)(total / 1024),
    (unsigned)(free_ / 1024), pct);

  String file_list = "";
  File root = LittleFS.open("/");
  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String name = f.name();
      file_list += "<li>" + name + " (" + String(f.size() / 1024) + " KB) "
                + "<a href='/download?f=" + name + "'>Download</a> "
                + "<a href='/delete?f=" + name + "' onclick=\"return confirm('Delete " + name + "?')\">Delete</a>"
                + "</li>";
    }
    f = root.openNextFile();
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  frontend_send_html(sendChunk, storage_info, file_list.c_str());
  server.sendContent("");  // flush / end chunked transfer
}

static File uploadFile;

static void handleUploadData() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    // LittleFS limits filenames to 31 chars (excluding the leading slash)
    if (filename.length() > 31) {
      // keep extension, truncate stem
      int dot = filename.lastIndexOf('.');
      String ext = (dot >= 0) ? filename.substring(dot) : "";
      filename = filename.substring(0, 31 - ext.length()) + ext;
    }
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

static void handleDownload() {
  if (!server.hasArg("f")) { server.send(400, "text/plain", "Missing filename."); return; }
  String path = "/" + server.arg("f");
  File f = LittleFS.open(path, "r");
  if (!f) { server.send(404, "text/plain", "File not found."); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + server.arg("f") + "\"");
  server.streamFile(f, "text/plain");
  f.close();
}

static void handleDelete() {
  if (!server.hasArg("f")) { server.send(400, "text/plain", "Missing filename."); return; }
  String path = "/" + server.arg("f");
  if (LittleFS.remove(path)) {
    Serial.println("[AP] Deleted: " + path);
    server.sendHeader("Location", "/");
    server.send(303);
  } else {
    server.send(500, "text/plain", "Delete failed.");
  }
}

void ap_start() {
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[AP] Started. Connect to: ");
  Serial.println(AP_SSID);
  Serial.print("[AP] Open: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, handleUpload, handleUploadData);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/delete", HTTP_GET, handleDelete);
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

String ap_get_ip() {
  return active ? WiFi.softAPIP().toString() : String("");
}

const char* ap_get_ssid() {
  return AP_SSID;
}

const String& ap_get_last_path() {
  return lastPath;
}

void ap_loop() {
  if (active) {
    server.handleClient();
  }
}
