#include "locket_auth.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

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
    client.setInsecure();   // TODO: pin GTS Root R1 once we leave dev.

    HTTPClient http;
    http.setTimeout(15000);
    if (!http.begin(client, url)) {
        Serial.println("[locket] http.begin failed");
        return false;
    }

    for (const auto& h : FIREBASE_HEADERS) {
        http.addHeader(h.name, h.value);
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
