#pragma once

void IRAM_ATTR reader_onButtonChange();
#ifdef HAS_BUTTON2
void IRAM_ATTR reader_onButton2Change();
#endif
void reader_setup();
void reader_loop();
void reader_cycle_app();
void reader_sleep();
const char* reader_get_mode_name();
