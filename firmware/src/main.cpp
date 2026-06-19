#include <Arduino.h>
#include <esp_sleep.h>

#include "image.h"          // wifi_connect / wifi_disconnect
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
    Serial.println(" locket-frame firmware - phase 5");
    Serial.println("=========================================");

    if (!wifi_connect()) {
        Serial.println("[main] no wifi - sleeping");
        Serial.flush();
        esp_deep_sleep_start();
    }

    LocketAuthState auth;
    if (!locket_sign_in(LOCKET_EMAIL, LOCKET_PASSWORD, &auth)) {
        Serial.println("[main] sign-in failed - sleeping");
        wifi_disconnect();
        Serial.flush();
        esp_deep_sleep_start();
    }
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
        } else {
            Serial.println("[locket] no moments to display");
        }
    } else {
        Serial.println("[locket] getLatestMomentV2 failed (see above)");
    }

    wifi_disconnect();

    Serial.println(" deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
}

void loop() {}
