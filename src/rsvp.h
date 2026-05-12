#pragma once

void IRAM_ATTR rsvp_onButtonChange();
void rsvp_setup();
void rsvp_loop();
void rsvp_reload_text();
void rsvp_load_file(const String& path);
void rsvp_show_current_word();
