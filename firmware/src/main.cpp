#include <Arduino.h>
#include <esp_sleep.h>

#include "display.h"
#include "image.h"

// setup()/loop() run in the Arduino "loopTask". Default stack is 8 KB, which
// mbedTLS blows through during the TLS handshake inside http.GET(). Bump it
// to 32 KB (still tiny vs available SRAM).
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

// Phase 3 test image. Already a weserv URL (480x480), so we GET it directly;
// weserv_wrap() in image.cpp is reserved for phase 4 (Locket thumbnails).
static const char* TEST_IMAGE_URL =
    "https://wsrv.nl/?url=wsrv.nl/lichtenstein.jpg&w=480&h=480&fit=cover&a=top";

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=========================================");
    Serial.println(" locket-frame firmware - phase 3");
    Serial.println("=========================================");
    Serial.printf(" PSRAM size: %u bytes\n", ESP.getPsramSize());

    if (!display_init()) {
        Serial.println("display_init failed - sleeping");
        Serial.flush();
        esp_deep_sleep_start();
    }

    bool got_image = false;
    if (wifi_connect()) {
        got_image = image_render_from_url(TEST_IMAGE_URL, display_framebuffer());
        wifi_disconnect();
    }
    if (!got_image) {
        Serial.println("[main] no image — left region stays white");
    }

    display_refresh();
    display_hibernate();
    display_free();

    Serial.println(" deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
}

void loop() {}
