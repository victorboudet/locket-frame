#include <Arduino.h>
#include <esp_sleep.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "image.h"
#include "locket_auth.h"
#include "locket_client.h"
#include "panel_ui.h"
#include "secrets.h"

// HTTPS handshake needs more than the 8 KB default loopTask stack.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

// Survives deep sleep: which moment the panel is currently showing, and
// whether the panel is showing an error screen instead of a photo.
RTC_DATA_ATTR static uint64_t last_shown_date_s = 0;
RTC_DATA_ATTR static char     last_shown_uid[48] = {0};
RTC_DATA_ATTR static bool     error_on_panel = false;
// Unix time of the last anti-ghosting clean cycle (0 = never cleaned; the
// first draw onto a blank panel resets the clock without a clear cycle).
RTC_DATA_ATTR static uint64_t last_clean_s = 0;

static void go_to_sleep() {
    esp_sleep_enable_timer_wakeup((uint64_t)REFRESH_INTERVAL_S * 1000000ULL);
    Serial.printf(" deep sleep for %d s\n", (int)REFRESH_INTERVAL_S);
    Serial.flush();
    esp_deep_sleep_start();
}

// Log the failure and put it on the panel — but only when the panel would
// otherwise be blank (nothing ever shown). If a photo is up, keep it: every
// refresh costs one of the E6 panel's finite cycles, and a stale photo beats
// an error screen. Never returns.
static void fail(const char* msg) {
    Serial.printf("[main] FAILED: %s\n", msg);
    wifi_disconnect();

    if (last_shown_date_s == 0 && !error_on_panel) {
        if (display_framebuffer() != nullptr || display_init()) {
            panel_ui_draw_error(msg);
            display_refresh();
            display_hibernate();
            display_free();
            error_on_panel = true;
        }
    }
    go_to_sleep();
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=========================================");
    Serial.println(" locket-frame firmware - phase 7");
    Serial.println("=========================================");
    Serial.printf(" PSRAM size: %u bytes\n", ESP.getPsramSize());
    Serial.printf(" wake cause: %d, last shown unix=%llu\n",
                  (int)esp_sleep_get_wakeup_cause(),
                  (unsigned long long)last_shown_date_s);

    if (!wifi_connect()) fail("wifi failed");
    if (!time_sync())    fail("time sync failed");   // TLS needs a set clock

    // Anti-ghosting: due once PANEL_CLEAN_INTERVAL_S has passed since the
    // last clear cycle — but only when a photo is up (a blank panel has
    // nothing to de-ghost, and the clean path needs an image to redraw).
    uint64_t now_s = (uint64_t)time(nullptr);
    bool clean_due = last_shown_date_s != 0 &&
                     now_s - last_clean_s >= PANEL_CLEAN_INTERVAL_S;
    if (clean_due) {
        Serial.printf("[main] panel clean due (last clean unix=%llu)\n",
                      (unsigned long long)last_clean_s);
    }

    LocketAuthState auth;
    if (!locket_sign_in(LOCKET_EMAIL, LOCKET_PASSWORD, &auth)) {
        fail("sign-in failed");
    }
    Serial.printf("[locket] sign-in OK: idToken length=%u, expires in %u s\n",
                  auth.id_token.length(), auth.expires_in_s);

    LocketLatestResult latest;
    if (!locket_get_latest_moment(auth, /*last_fetch=*/1, &latest)) {
        fail("fetch failed");
    }
    Serial.printf("[locket] got %d moment(s), %d missed\n",
                  latest.moments_count, latest.missed_moments_count);

    if (latest.moments_count == 0) {
        Serial.println("[locket] no moments - keeping current panel");
        wifi_disconnect();
        go_to_sleep();
    }

    Serial.printf("[locket] moment %s sent at unix=%llu\n",
                  latest.first.canonical_uid.c_str(),
                  (unsigned long long)latest.first.date_seconds);

    // Same moment as last wake and the panel shows it fine: skip the render
    // and the ~30 s refresh entirely (saves power and panel refresh cycles).
    // A due clean overrides the skip — the photo must be redrawn after the
    // clear cycle wipes the panel.
    if (!error_on_panel && !clean_due &&
        latest.first.date_seconds == last_shown_date_s &&
        latest.first.canonical_uid == last_shown_uid) {
        Serial.println("[main] moment unchanged - skipping refresh");
        wifi_disconnect();
        go_to_sleep();
    }

    if (!display_init()) fail("display init failed");

    String weserv_url = weserv_wrap(latest.first.thumbnail_url.c_str(), 480, 480);
    Serial.printf("[main] weserv: %s\n", weserv_url.c_str());

    if (!image_render_from_url(weserv_url.c_str(), display_framebuffer())) {
        fail("image render failed");
    }
    wifi_disconnect();

    panel_ui_draw_moment_info(latest.first.date_seconds);

    // Clean only after the image rendered successfully, so a failed render
    // never leaves the panel wiped with nothing to put back.
    if (clean_due) display_clean();

    display_refresh();
    display_hibernate();
    display_free();

    last_shown_date_s = latest.first.date_seconds;
    strlcpy(last_shown_uid, latest.first.canonical_uid.c_str(),
            sizeof(last_shown_uid));
    error_on_panel = false;
    // A fresh clean — or the very first draw onto a factory-blank panel —
    // restarts the 24 h anti-ghosting clock.
    if (clean_due || last_clean_s == 0) last_clean_s = now_s;

    go_to_sleep();
}

void loop() {}
