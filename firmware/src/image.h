#pragma once

#include <Arduino.h>
#include "DEV_Config.h"   // UBYTE

// Connect to WiFi using WIFI_SSID/WIFI_PASSWORD from secrets.h. Returns true
// when associated within WIFI_TIMEOUT_MS, false otherwise.
bool wifi_connect();

// Drop WiFi and shut the radio down.
void wifi_disconnect();

// Wrap a source URL in a weserv request that returns a wxh JPEG. Caller
// owns the returned String. Not used in phase 3 (test URL is already weserv);
// phase 4+ will use this for Locket thumbnail URLs.
String weserv_wrap(const char* source_url, int w = 480, int h = 480);

// Fetch a JPEG over HTTPS, decode it to a 480x480 RGB565 PSRAM buffer,
// Floyd-Steinberg dither it to the 6-colour panel palette, and write the
// resulting colour indices into the left 480x480 region of fb (the full
// 800x480 panel framebuffer). All temporary buffers are freed before return.
// fb's right 320 px is left untouched. Returns false on any failure.
bool image_render_from_url(const char* url, UBYTE* fb);
