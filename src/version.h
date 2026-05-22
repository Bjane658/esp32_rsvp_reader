#pragma once

// Build timestamp injected by the compiler — e.g. "2026-05-22 14:03:11"
// __DATE__ = "May 22 2026", __TIME__ = "14:03:11"
static const char* FW_VERSION = __DATE__ " " __TIME__;
