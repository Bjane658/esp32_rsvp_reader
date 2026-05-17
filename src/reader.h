#pragma once

void IRAM_ATTR reader_onButtonChange();
void reader_setup();
void reader_loop();
void reader_toggle_mode();
bool reader_is_ereader_mode();
const char* reader_get_mode_name();
