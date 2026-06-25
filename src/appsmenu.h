#pragma once

// Modal "Apps" picker launched from the main menu. Lists every registered,
// non-cycleable (tool) app. Long-press on an entry pushes that app onto the
// app stack (app_push) and closes both this picker and the parent menu.
void appsmenu_open();
bool appsmenu_is_open();
void appsmenu_short_press();   // cursor down
void appsmenu_double_press();  // cursor up
void appsmenu_long_press();    // launch selected app, or Back
