#pragma once

#define TE_MAX_CHAPTERS 64
#define TE_MAX_TITLE    24

struct Chapter {
  size_t offset;
  char   title[TE_MAX_TITLE];
};

// Filesystem / file management
void        te_setup();
void        te_open_file(const String& path);
void        te_reload_from_ap();
const String& te_get_current_file();

// Cursor
size_t      te_get_position();
void        te_seek(size_t pos);
int         te_read_char();   // returns -1 at EOF
void        te_seek_to(size_t pos);  // raw seek, no side effects
size_t      te_current_pos();

// Chapter index
const Chapter* te_get_chapters();
int            te_get_chapter_count();
bool           te_is_indexing();   // true while background scan is in progress
void           te_index_tick();    // call from main loop to advance the scan
size_t         te_file_size();

// Persist / restore position in NVS
void        te_save_position();

// Preview (first few words from current position)
void        te_show_preview();
