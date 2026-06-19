#include <Arduino.h>
#include <esp_sleep.h>

#include "image.h"        // wifi_connect / wifi_disconnect
#include "locket_auth.h"
#include "secrets.h"

// HTTPS handshake needs more than the 8 KB default loopTask stack.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=========================================");
    Serial.println(" locket-frame firmware - phase 4");
    Serial.println("=========================================");

    if (!wifi_connect()) {
        Serial.println("[main] no wifi - sleeping");
        Serial.flush();
        esp_deep_sleep_start();
    }

    LocketAuthState auth;
    if (locket_sign_in(LOCKET_EMAIL, LOCKET_PASSWORD, &auth)) {
        Serial.printf("[locket] sign-in OK: idToken length=%u, expires in %u s\n",
                      auth.id_token.length(), auth.expires_in_s);
    } else {
        Serial.println("[locket] sign-in failed (see above)");
    }

    wifi_disconnect();

    Serial.println(" deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
}

void loop() {}
