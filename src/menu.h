#pragma once

int menu_get_wpm();
bool menu_is_open();
void menu_open();
void menu_close();   // close menu without touching the app stack (used after app_push)
void menu_short_press();
void menu_double_press();
void menu_long_press();
void menu_loop();
