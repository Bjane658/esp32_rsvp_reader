#pragma once

// Character grid dimensions — approximate for the default font at landscape orientation
#define DISPLAY_COLS 35
#define DISPLAY_ROWS  7

void display_setup();
void display_clear();           // full hardware refresh + wipe buffer (use on screen transitions)
void display_reset();           // wipe buffer only, no hardware clear (use before re-rendering same screen)
void display_print(int row, const char* text);  // buffer only
void display_cursor(int row);   // buffer only
void display_flush();           // push buffer to screen
void display_word(const char* prev, const char* word, const char* next); // buffer + flush
