# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An ESP32-based e-ink RSVP (Rapid Serial Visual Presentation) reader running on the **Heltec Wireless Paper v1.2** — a device with an e-ink display and a single physical button (BOOT/GPIO0). Books are uploaded as plain text over WiFi and read word-by-word at configurable WPM or as paginated e-reader text.

## Build & Flash Commands

```bash
# Build
pio run -e heltec_wireless_paper

# Build + upload firmware
pio run -e heltec_wireless_paper -t upload

# Upload filesystem (jszip.js and any pre-loaded text files)
pio run -e heltec_wireless_paper -t uploadfs

# Serial monitor (115200 baud)
pio device monitor

# Build + upload + monitor in one step
pio run -e heltec_wireless_paper -t upload && pio device monitor
```

The `AP_PASSWORD` build flag in `platformio.ini` must be set before flashing. The `esp32dev` environment exists for generic ESP32 boards but lacks the e-ink library.

## Content Pipeline

Converting an EPUB to device format requires `pandoc` and Python 3:

```bash
tools/epub2rsvp.sh input.epub output.txt
```

This runs pandoc to markdown, then `tools/epub2rsvp.py` to strip markup, normalize typography, and emit clean text with `# Chapter Title` headings. Upload the resulting `.txt` via the WiFi AP web interface.

## Architecture

### Single-button interaction model (`src/reader.cpp`)

All input comes from one button (GPIO0/BOOT). The ISR (`reader_onButtonChange`, IRAM_ATTR) sets flags that `reader_loop()` processes each iteration:
- **Short press** (after double-click timeout expires): advance/start
- **Double press** (two clicks within 400 ms): go back / rewind
- **Long press** (≥1000 ms): open/navigate menu

The device deep-sleeps after 5 minutes of inactivity, waking on the same button via `ext0`.

### Mode dispatch

`reader.cpp` owns a `ReaderMode` enum (`MODE_RSVP` / `MODE_EREADER`) persisted to NVS. Each mode implements a start/stop/loop/press API:

- **`rsvp_mode`** (`src/rsvp_mode.cpp`): streams words from `textengine` at WPM-controlled intervals, pausing at paragraph breaks and headings. Displays prev/current/next word context using ORP (Optimal Recognition Point) alignment via `display_word()`.
- **`ereader_mode`** (`src/ereader_mode.cpp`): paginated reading. Uses pixel-width-aware line wrapping (not character count) and a ring buffer of 8 page-start positions for going back.

### Text engine (`src/textengine.cpp`)

Wraps LittleFS file access with:
- UTF-8 → Latin-1 decoding (the DejaVuSans font covers U+0020–U+00FF)
- Chapter index built by scanning for `# ` headings at line starts
- Position persistence per-file in NVS namespace `rpos`, keyed by FNV-1a hash of the filename (15-char NVS key limit)
- Falls back to a built-in demo string if no file is loaded

### Display (`src/display.cpp`)

Wraps the `heltec-eink-modules` library. Two fonts (FONT_SMALL: 5 rows × 28 cols, FONT_LARGE: 3 rows × 20 cols). Uses e-ink fastmode for partial updates; `display_clear()` exits fastmode for a full hardware refresh (use on screen transitions), `display_reset()` only wipes the software buffer.

`display_word()` does a partial-row update (`setWindow`) for RSVP — only the middle row is refreshed, avoiding a full-screen redraw per word.

### Menu system (`src/menu.cpp`)

Overlay over the current mode. Items: Chapter picker, WPM (150/200/300), Mode toggle, File picker, WiFi AP, Exit. Sub-pickers (`filepicker`, `wifimenu`, `chapterpicker`) are modal overlays within the menu — each implements the same short/double/long press API and signals back to `menu.cpp` whether to re-render.

### WiFi AP (`src/ap.cpp`)

Starts a `softAP` named `RSVP-Reader` with password from `AP_PASSWORD`. Serves an upload/download/delete web UI (`frontend.cpp` generates the HTML via chunked transfer). `jszip.js` is served from LittleFS (`data/jszip.js`) — it must be flashed via `uploadfs`. Routes are registered once (not on every `ap_start()`) because `server.stop()` does not clear handlers.

## Filesystem & Partitions

Text files live in LittleFS. Two partition tables exist:
- `partitions.csv` — standard 4 MB layout (for `esp32dev`)
- `partitions_8mb.csv` — 8 MB layout for Heltec Wireless Paper

LittleFS filename limit: 31 characters (enforced in `ap.cpp` upload handler).
