#pragma once

void rsvp_mode_start();   // called when switching to this mode
void rsvp_mode_stop();    // called when leaving this mode
void rsvp_mode_loop();    // called every iteration while active

void rsvp_mode_short_press();
void rsvp_mode_double_press();
void rsvp_mode_set_running(bool r);
bool rsvp_mode_is_running();

// pause display helpers used by menu
void rsvp_mode_show_current_word();
void rsvp_mode_show_preview();
