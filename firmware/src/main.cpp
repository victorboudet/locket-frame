#include <Arduino.h>
#include <esp_sleep.h>

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=========================================");
    Serial.println(" locket-frame firmware - boot");
    Serial.println("=========================================");
    Serial.printf(" PSRAM size: %u bytes\n", ESP.getPsramSize());
    Serial.println(" empty build - sleeping forever");
    Serial.flush();
    esp_deep_sleep_start();
}

void loop() {}
