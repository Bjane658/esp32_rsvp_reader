#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "textengine.h"
#include "display.h"
#include "ap.h"

static Preferences prefs;

static const char* FALLBACK_TEXT =
  "Rapid serial visual presentation is a method of displaying text "
  "so that it can be read quickly. Words are shown one at a time in "
  "a fixed position on the screen, eliminating the need for eye "
  "movement across a page. This allows the reader to focus entirely "
  "on a single focal point and absorb each word as it appears. "
  "Studies suggest that most people can comfortably read at speeds "
  "between two hundred and three hundred words per minute using this "
  "technique, while trained readers can reach five hundred or more. "
  "The key advantage is that it reduces subvocalization and "
  "unnecessary saccades, making the reading process more efficient.";

static String      currentFile   = "";
static File        textFile;
static bool        usingFallback = false;
static const char* fallbackCursor = nullptr;

static Chapter chapterIndex[TE_MAX_CHAPTERS];
static int     chapterCount = 0;

// ── Chapter index ─────────────────────────────────────────────────────────────

static void buildChapterIndex() {
  chapterCount = 0;
  if (currentFile.isEmpty()) return;
  File f = LittleFS.open(currentFile, "r");
  if (!f) return;
  int prev = '\0';
  int c;
  while ((c = f.read()) != -1 && chapterCount < TE_MAX_CHAPTERS) {
    if (c == '#' && (prev == '\n' || prev == '\0')) {
      size_t headingStart = f.position() - 1;
      int next = f.read();
      if (next != ' ') { prev = next; continue; }
      while ((c = f.read()) != -1 && c == ' ');
      char title[TE_MAX_TITLE];
      int len = 0;
      while (c != -1 && c != '\n' && c != '\r' && len < TE_MAX_TITLE - 1) {
        title[len++] = (char)c;
        c = f.read();
      }
      title[len] = '\0';
      chapterIndex[chapterCount].offset = headingStart;
      strncpy(chapterIndex[chapterCount].title, title, TE_MAX_TITLE);
      chapterCount++;
    }
    prev = c;
  }
  f.close();
}

const Chapter* te_get_chapters()     { return chapterIndex; }
int            te_get_chapter_count() { return chapterCount; }

// ── Position persistence ──────────────────────────────────────────────────────

// NVS keys are max 15 chars — derive a short key from the filename
static void filePosKey(const String& path, char* out, size_t outLen) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < (int)path.length(); i++)
    h = (h ^ (uint8_t)path[i]) * 16777619u;
  snprintf(out, outLen, "p%08x", (unsigned)h);
}

void te_save_position() {
  if (usingFallback || currentFile.isEmpty()) return;
  char key[15];
  filePosKey(currentFile, key, sizeof(key));
  prefs.begin("rpos", false);
  prefs.putUInt(key, (uint32_t)te_current_pos());
  prefs.end();
}

static uint32_t loadSavedPosition(const String& path) {
  char key[15];
  filePosKey(path, key, sizeof(key));
  prefs.begin("rpos", true);
  uint32_t pos = prefs.getUInt(key, 0);
  prefs.end();
  return pos;
}

// ── File management ──────────────────────────────────────────────────────────

static void openFile(const String& path) {
  if (textFile) textFile.close();
  usingFallback = false;
  if (!path.isEmpty() && LittleFS.exists(path)) {
    textFile = LittleFS.open(path, "r");
    if (textFile) textFile.close();
    textFile = LittleFS.open(path, "r");
    if (textFile) {
      currentFile = path;
      display_clear();
      display_print(0, "Loading...");
      buildChapterIndex();
      prefs.begin("reader", false);
      prefs.putString("lastFile", path);
      prefs.end();
      uint32_t savedPos = loadSavedPosition(path);
      te_seek_to((size_t)savedPos);
      Serial.print("[TE] Opened: ");
      Serial.println(path);
      Serial.print("[TE] Chapters: ");
      Serial.print(chapterCount);
      Serial.print(" @ ");
      Serial.println(savedPos);
      return;
    }
  }
  usingFallback    = true;
  fallbackCursor   = FALLBACK_TEXT;
  currentFile      = "";
  chapterCount     = 0;
  Serial.println("[TE] Using fallback text.");
}

void te_setup() {
  if (!LittleFS.begin(false)) {
    Serial.println("[TE] LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin(false);
  }
  prefs.begin("reader", true);
  String lastFile = prefs.getString("lastFile", "/text.txt");
  prefs.end();
  Serial.print("[TE] Restoring: ");
  Serial.println(lastFile);
  openFile(lastFile);
}

void te_open_file(const String& path) {
  openFile(path);
}

void te_reload_from_ap() {
  openFile(ap_get_last_path());
}

const String& te_get_current_file() {
  return currentFile;
}

// ── Cursor ────────────────────────────────────────────────────────────────────

size_t te_current_pos() {
  if (usingFallback) return fallbackCursor - FALLBACK_TEXT;
  if (!textFile) return 0;
  return textFile.position();
}

size_t te_get_position() {
  return te_current_pos();
}

void te_seek_to(size_t pos) {
  if (!usingFallback && textFile) textFile.seek(pos);
  else if (usingFallback) fallbackCursor = FALLBACK_TEXT + pos;
}

void te_seek(size_t pos) {
  te_seek_to(pos);
}

int te_read_char() {
  if (usingFallback) {
    if (!*fallbackCursor) return -1;
    return (unsigned char)*fallbackCursor++;
  }
  if (!textFile) return -1;
  return textFile.read();
}

// ── Preview ───────────────────────────────────────────────────────────────────

void te_show_preview() {
  display_clear();
  char preview[64] = "";
  int len = 0;
  size_t savedPos = te_current_pos();
  int wordsRead = 0;
  while (wordsRead < 3 && len < (int)sizeof(preview) - 2) {
    int c;
    while ((c = te_read_char()) != -1 && (c == ' ' || c == '\n' || c == '\r'));
    if (c == -1) break;
    if (len > 0) preview[len++] = ' ';
    do {
      if (len < (int)sizeof(preview) - 1) preview[len++] = (char)c;
      c = te_read_char();
    } while (c != -1 && c != ' ' && c != '\n' && c != '\r');
    wordsRead++;
  }
  preview[len] = '\0';
  te_seek_to(savedPos);
  display_print(0, preview);
}
