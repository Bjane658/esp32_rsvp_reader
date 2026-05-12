#pragma once
#include <WString.h>

void ap_start();
void ap_stop();
bool ap_is_active();
void ap_loop();
const String& ap_get_last_path();
