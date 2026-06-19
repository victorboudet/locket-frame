#pragma once

#include <Arduino.h>
#include <stdint.h>

// Result of a successful Firebase sign-in. Refresh token is kept for the
// later phase that handles token refresh; phase 4 ignores it.
struct LocketAuthState {
    String   id_token;
    String   refresh_token;
    uint32_t expires_in_s;     // seconds-from-now, as reported by Firebase
    uint32_t expires_at_ms;    // millis() at which the token expires
};

// POST to identitytoolkit verifyPassword with Locket's iOS spoof headers.
// Fills `state` and returns true on HTTP 200 + valid response.
// On any failure logs the HTTP status and full response body to Serial,
// then returns false. Caller must already be on WiFi.
bool locket_sign_in(const char* email, const char* password,
                    LocketAuthState* state);
