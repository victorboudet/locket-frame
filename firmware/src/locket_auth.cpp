#include "locket_auth.h"
#include "certs.h"
#include "config.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cstring>

// Mirrors scrapping/locket/auth.py:FIREBASE_HEADERS verbatim. Locket's
// Firebase API key is bundle-restricted to com.locket.Locket; these headers
// spoof the iOS Firebase SDK so the key is accepted.
namespace {

struct Header { const char* name; const char* value; };

const Header FIREBASE_HEADERS[] = {
    {"Content-Type",            "application/json"},
    {"Accept-Language",         "en-US"},
    {"User-Agent",              "FirebaseAuth.iOS/10.23.1 com.locket.Locket/1.82.0 iPhone/18.0 hw/iPhone12_1"},
    {"X-Ios-Bundle-Identifier", "com.locket.Locket"},
    {"X-Client-Version",        "iOS/FirebaseSDK/10.23.1/FirebaseCore-iOS"},
    {"X-Firebase-GMPID",        "1:641029076083:ios:cc8eb46290d69b234fa606"},
    {"X-Firebase-Client",       "H4sIAAAAAAAAAKtWykhNLCpJSk0sKVayio7VUSpLLSrOzM9TslIyUqoFAFyivEQfAAAA"},
};

}  // namespace

bool locket_sign_in(const char* email, const char* password,
                    LocketAuthState* state) {
    if (!state) return false;

    String url = String("https://www.googleapis.com/identitytoolkit/v3/relyingparty/verifyPassword?key=") + FIREBASE_API_KEY;

    Serial.println("[locket] POST verifyPassword");

    WiFiClientSecure client;
#if ALLOW_INSECURE_TLS
    client.setInsecure();
    Serial.println("[locket] WARNING: TLS verification disabled");
#else
    client.setCACert(GTS_ROOT_CA_BUNDLE);
#endif

    HTTPClient http;
    http.setTimeout(30000);   // same response timeout as the scraper
    if (!http.begin(client, url)) {
        Serial.println("[locket] http.begin failed");
        return false;
    }

    for (const auto& h : FIREBASE_HEADERS) {
        // ESP32 HTTPClient silently ignores User-Agent in addHeader().
        // Use its dedicated setter so the Firebase iOS identity is sent.
        if (strcmp(h.name, "User-Agent") == 0) {
            http.setUserAgent(h.value);
        } else {
            http.addHeader(h.name, h.value);
        }
    }

    JsonDocument body;
    body["email"] = email;
    body["password"] = password;
    body["returnSecureToken"] = true;
    body["clientType"] = "CLIENT_TYPE_IOS";
    String body_str;
    serializeJson(body, body_str);

    int code = http.POST(body_str);
    String resp = http.getString();
    if (code < 0) {
        Serial.printf("[locket] sign-in transport error: %s\n",
                      HTTPClient::errorToString(code).c_str());
        char tls_error[160] = {};
        int tls_code = client.lastError(tls_error, sizeof(tls_error));
        if (tls_code != 0) {
            Serial.printf("[locket] TLS error %d: %s\n", tls_code, tls_error);
        }
    }
    http.end();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[locket] sign-in FAILED status=%d\n", code);
        Serial.printf("[locket] response body:\n%s\n", resp.c_str());
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        Serial.printf("[locket] JSON parse failed: %s\n", err.c_str());
        Serial.printf("[locket] response body:\n%s\n", resp.c_str());
        return false;
    }

    const char* id_token      = doc["idToken"];
    const char* refresh_token = doc["refreshToken"];
    // Firebase ships expiresIn as a JSON string ("3600"); ArduinoJson v7
    // coerces strings to int via .as<int>() so this handles both cases.
    int expires_in = doc["expiresIn"].as<int>();

    if (!id_token || !*id_token) {
        Serial.println("[locket] response missing idToken");
        Serial.printf("[locket] response body:\n%s\n", resp.c_str());
        return false;
    }

    state->id_token      = id_token;
    state->refresh_token = refresh_token ? refresh_token : "";
    state->expires_in_s  = (expires_in > 0) ? (uint32_t)expires_in : 3600;
    state->expires_at_ms = millis() + state->expires_in_s * 1000UL;

    return true;
}
