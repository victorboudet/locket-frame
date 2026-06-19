#pragma once

#include <Arduino.h>
#include "DEV_Config.h"   // UBYTE

// Bring the panel up: raise EPD_PWR, init SPI + GPIO, run the panel's reset
// sequence, allocate a 192000-byte framebuffer in PSRAM, and prep GUI_Paint
// in 6-colour (scale 6) mode over it. Returns false if PSRAM alloc fails.
bool display_init();

// Pointer to the full 800x480 framebuffer (4-bit indexed). Caller may write
// directly using the same packing convention as GUI_Paint scale 6.
UBYTE* display_framebuffer();

// Draw six vertical colour bars (one per EPD_7IN3E_* macro) plus banner +
// per-bar labels into the framebuffer, then call display_refresh().
void display_test_pattern();

// Push the current framebuffer to the panel (one full refresh, ~25-30s).
void display_refresh();

// Tell the panel to hibernate and drop EPD_PWR. Safe to follow with
// esp_deep_sleep_start().
void display_hibernate();

// Free the framebuffer. Call after the panel is asleep, before deep sleep.
void display_free();
