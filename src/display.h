#pragma once

#define DISPLAY_COLS 35  // characters per row (update when real screen driver is known)
#define DISPLAY_ROWS  7  // visible rows

void display_setup();
void display_clear();
void display_print(int row, const char* text);
void display_cursor(int row);
void display_word(const char* prev, const char* word, const char* next);
