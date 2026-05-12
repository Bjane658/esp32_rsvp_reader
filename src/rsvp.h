#pragma once

void IRAM_ATTR rsvp_onButtonChange();
void rsvp_setup();
void rsvp_loop();
void rsvp_reload_text();
void rsvp_load_file(const String& path);
const String& rsvp_get_current_file();
void rsvp_show_current_word();
void rsvp_show_preview();
