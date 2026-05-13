#pragma once

#define RSVP_MAX_CHAPTERS 64
#define RSVP_MAX_TITLE    24

struct RsvpChapter {
  size_t offset;
  char   title[RSVP_MAX_TITLE];
};

void IRAM_ATTR rsvp_onButtonChange();
void rsvp_setup();
void rsvp_loop();
void rsvp_reload_text();
void rsvp_load_file(const String& path);
const String& rsvp_get_current_file();
void rsvp_show_current_word();
void rsvp_show_preview();
void rsvp_next_chapter();
void rsvp_prev_chapter();
size_t rsvp_get_file_position();
void rsvp_seek(size_t pos);
const RsvpChapter* rsvp_get_chapters();
int rsvp_get_chapter_count();
