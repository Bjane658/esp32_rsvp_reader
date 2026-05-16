#pragma once

typedef enum {
  FONT_SMALL,   // FreeSans9pt  — 5 rows, ~28 chars/row
  FONT_LARGE,   // FreeSans12pt — 3 rows, ~20 chars/row
} DisplayFont;

// Grid dims vary by font — query at runtime
int display_cols();
int display_rows();

void display_setup();
void display_set_font(DisplayFont f);
void display_clear();           // full hardware refresh + wipe buffer (use on screen transitions)
void display_reset();           // wipe buffer only, no hardware clear (use before re-rendering same screen)
void display_print(int row, const char* text);  // buffer only
void display_cursor(int row);   // buffer only
void display_set_progress(float fraction); // 0.0–1.0, drawn at bottom on next flush
void display_flush();           // push buffer to screen
void display_word(const char* prev, const char* word, const char* next); // buffer + flush
