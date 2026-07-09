#include <Arduino.h>
#include <esp_sleep.h>

#include "display.h"
#include "image.h"
#include "locket_auth.h"
#include "locket_client.h"
#include "secrets.h"

// HTTPS handshake needs more than the 8 KB default loopTask stack.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=========================================");
    Serial.println(" locket-frame firmware - phase 6");
    Serial.println("=========================================");
    Serial.printf(" PSRAM size: %u bytes\n", ESP.getPsramSize());

    if (!display_init()) {
        Serial.println("display_init failed - sleeping");
        Serial.flush();
        esp_deep_sleep_start();
    }

    if (!wifi_connect()) {
        Serial.println("[main] no wifi - skipping fetch, refreshing blank panel");
    } else {
        LocketAuthState auth;
        if (locket_sign_in(LOCKET_EMAIL, LOCKET_PASSWORD, &auth)) {
            Serial.printf("[locket] sign-in OK: idToken length=%u, expires in %u s\n",
                          auth.id_token.length(), auth.expires_in_s);

            LocketLatestResult latest;
            if (locket_get_latest_moment(auth, /*last_fetch=*/1, &latest)) {
                Serial.printf("[locket] got %d moment(s), %d missed\n",
                              latest.moments_count, latest.missed_moments_count);

                if (latest.moments_count > 0) {
                    Serial.printf("[locket] thumbnail: %s\n",
                                  latest.first.thumbnail_url.c_str());
                    Serial.printf("[locket] sent at unix=%llu\n",
                                  (unsigned long long)latest.first.date_seconds);

                    String weserv_url = weserv_wrap(
                        latest.first.thumbnail_url.c_str(), 480, 480);
                    Serial.printf("[main] weserv: %s\n", weserv_url.c_str());

                    if (!image_render_from_url(weserv_url.c_str(),
                                               display_framebuffer())) {
                        Serial.println("[main] image render failed — panel will be blank");
                    }
                } else {
                    Serial.println("[locket] no moments to display");
                }
            } else {
                Serial.println("[main] getLatestMomentV2 failed");
            }
        } else {
            Serial.println("[main] sign-in failed");
        }
        wifi_disconnect();
    }

    display_refresh();
    display_hibernate();
    display_free();

    Serial.println(" deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
}

void loop() {}
