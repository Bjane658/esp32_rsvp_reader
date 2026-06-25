#pragma once

// Countdown timer app (tool, non-cycleable). Launched from the main menu's
// "Apps" entry. Single-button interaction; long-press is owned by the harness
// (opens the menu). See docs/pi/specs/2026-06-25-timer-app-design.md.
void timer_app_start();
void timer_app_stop();
void timer_app_loop();
void timer_app_short_press();   // unused for streaming dispatch; harness calls set_running
void timer_app_double_press();  // IDLE: +1 min  |  PAUSED: reset to IDLE
void timer_app_show();           // re-render current state
bool timer_app_is_running();
void timer_app_set_running(bool r);
