#pragma once

void ereader_mode_start();
void ereader_mode_stop();
void ereader_mode_loop();
void ereader_mode_show_page();     // re-render current page (e.g. after menu seek)
void ereader_mode_reset_history(); // discard page history (e.g. after chapter jump)

void ereader_mode_short_press();   // next page
void ereader_mode_double_press();  // previous page
