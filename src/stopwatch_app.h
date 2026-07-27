#pragma once

// Stopwatch app (tool, non-cycleable). Launched from the main menu's "Apps"
// entry. Single-button interaction; long-press is owned by the harness
// (opens the menu). 1x: start/stop, 2x: reset.
void stopwatch_app_start();
void stopwatch_app_stop();
void stopwatch_app_loop();
void stopwatch_app_short_press();   // unused for streaming dispatch; harness calls set_running
void stopwatch_app_double_press();  // reset to 00:00
void stopwatch_app_show();          // re-render current state
bool stopwatch_app_is_running();
void stopwatch_app_set_running(bool r);
