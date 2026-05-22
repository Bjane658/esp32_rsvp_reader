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

// ── Background chapter index ──────────────────────────────────────────────────

static File   indexFile;
static bool   indexing    = false;
static int    indexPrev   = '\0';

#define INDEX_BYTES_PER_TICK 256

static void startIndexing() {
  chapterCount = 0;
  indexPrev    = '\0';
  if (indexFile) indexFile.close();
  if (currentFile.isEmpty()) { indexing = false; return; }
  indexFile = LittleFS.open(currentFile, "r");
  indexing  = indexFile ? true : false;
}

void te_index_tick() {
  if (!indexing) return;

  int processed = 0;
  int c;
  while (processed++ < INDEX_BYTES_PER_TICK && chapterCount < TE_MAX_CHAPTERS) {
    c = indexFile.read();
    if (c == -1) { indexFile.close(); indexing = false; return; }
    if (c == '#' && (indexPrev == '\n' || indexPrev == '\0')) {
      size_t headingStart = indexFile.position() - 1;
      int next = indexFile.read();
      processed++;
      if (next != ' ') { indexPrev = next; continue; }
      while ((c = indexFile.read()) != -1 && c == ' ') processed++;
      char title[TE_MAX_TITLE];
      int len = 0;
      while (c != -1 && c != '\n' && c != '\r' && len < TE_MAX_TITLE - 1) {
        title[len++] = (char)c;
        c = indexFile.read();
        processed++;
      }
      title[len] = '\0';
      chapterIndex[chapterCount].offset = headingStart;
      strncpy(chapterIndex[chapterCount].title, title, TE_MAX_TITLE);
      chapterCount++;
      indexPrev = c;
      continue;
    }
    indexPrev = c;
  }
}

bool te_is_indexing() { return indexing; }

const Chapter* te_get_chapters()     { return chapterIndex; }
int            te_get_chapter_count() { return chapterCount; }
size_t         te_file_size()         { return textFile ? textFile.size() : 0; }

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
  size_t pos = te_current_pos();
  prefs.begin("rpos", false);
  prefs.putUInt(key, (uint32_t)pos);
  prefs.end();
  Serial.printf("[TE] Saved pos: %u\n", (unsigned)pos);
}

static uint32_t loadSavedPosition(const String& path) {
  char key[15];
  filePosKey(path, key, sizeof(key));
  prefs.begin("rpos", false);  // read-write, creates namespace if missing
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
      prefs.begin("reader", false);
      prefs.putString("lastFile", path);
      prefs.end();
      uint32_t savedPos = loadSavedPosition(path);
      te_seek_to((size_t)savedPos);
      startIndexing();
      Serial.print("[TE] Opened: ");
      Serial.println(path);
      Serial.print("[TE] Saved pos: ");
      Serial.println(savedPos);
      return;
    }
  }
  if (indexFile) { indexFile.close(); }
  indexing         = false;
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
  prefs.begin("reader", false);  // false = read-write, creates namespace if missing
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

// Read one raw byte from the source.
static int read_raw() {
  if (usingFallback) {
    if (!*fallbackCursor) return -1;
    return (unsigned char)*fallbackCursor++;
  }
  if (!textFile) return -1;
  return textFile.read();
}

// Consume remaining bytes of a multi-byte UTF-8 sequence (continuation bytes 0x80–0xBF).
static void skip_utf8_tail(int leading) {
  int tail = 0;
  if      ((leading & 0xE0) == 0xC0) tail = 1;
  else if ((leading & 0xF0) == 0xE0) tail = 2;
  else if ((leading & 0xF8) == 0xF0) tail = 3;
  for (int i = 0; i < tail; i++) read_raw();
}

// Read one UTF-8 codepoint, return as Latin-1 byte (0x20–0xFF) or ASCII.
// The DejaVuSans font covers U+0020–U+00FF (Latin-1), so codepoints in that
// range are returned as their single-byte Latin-1 value directly.
// Codepoints outside Latin-1 are mapped to close ASCII equivalents or '?'.
int te_read_char() {
  int b = read_raw();
  if (b == -1) return -1;

  // Plain ASCII — pass through
  if (b < 0x80) return b;

  // Multi-byte sequence
  int tail = 0;
  if      ((b & 0xE0) == 0xC0) tail = 1;
  else if ((b & 0xF0) == 0xE0) tail = 2;
  else if ((b & 0xF8) == 0xF0) tail = 3;
  else return '?';

  uint32_t cp = b & (0x7F >> tail);
  for (int i = 0; i < tail; i++) {
    int cb = read_raw();
    if (cb == -1 || (cb & 0xC0) != 0x80) return '?';
    cp = (cp << 6) | (cb & 0x3F);
  }

  // Latin-1 supplement (U+00A0–U+00FF) — font has these glyphs, return directly
  if (cp >= 0x00A0 && cp <= 0x00FF) {
    if (cp == 0x00AD) return ' '; // soft hyphen
    return (int)cp;
  }

  // Common typographic characters outside Latin-1 — map to closest Latin-1
  switch (cp) {
    case 0x2018: case 0x2019: case 0x201A: case 0x201B: return '\'' ;
    case 0x201C: case 0x201D: case 0x201E: case 0x201F: return '"';
    case 0x2013: return 0x2D;  // en dash → hyphen
    case 0x2014: return 0x2D;  // em dash → hyphen
    case 0x2026: return 0x85;  // ellipsis → U+0085 (next line, but font has … at 0x85) -- fallback
    case 0x2002: case 0x2003: case 0x2009: case 0x00A0: return ' ';
    default:     return '?';
  }
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
  display_flush();
}
