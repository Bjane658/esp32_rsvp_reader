#pragma once

void filepicker_open();
bool filepicker_is_open();
void filepicker_short_press();
bool filepicker_long_press(); // returns true if a file was selected, false if Back
