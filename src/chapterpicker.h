#pragma once

void chapterpicker_open();
bool chapterpicker_is_open();
void chapterpicker_short_press();
bool chapterpicker_long_press(); // returns true if a chapter was selected, false if Back
void chapterpicker_cancel();
