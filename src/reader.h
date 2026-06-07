#pragma once

void IRAM_ATTR reader_onButtonChange();
void reader_setup();
void reader_loop();
void reader_cycle_app();
void reader_sleep();
const char* reader_get_mode_name();
