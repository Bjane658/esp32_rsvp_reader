#pragma once

void filepicker_open();
bool filepicker_is_open();
void filepicker_short_press();   // cursor down
void filepicker_double_press();  // cursor up
bool filepicker_long_press();    // returns true if a file was selected, false if Back
void filepicker_cancel();
