#include "locket_client.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {

constexpr const char* BASE_URL = "https://api.locketcamera.com";

}  // namespace

bool locket_get_latest_moment(const LocketAuthState& auth,
                              uint64_t last_fetch_seconds,
                              LocketLatestResult* out) {
    if (!out) return false;

    String url = String(BASE_URL) + "/getLatestMomentV2";

    WiFiClientSecure client;
    client.setInsecure();   // TODO: pin api.locketcamera.com root CA

    HTTPClient http;
    http.setTimeout(30000);
    if (!http.begin(client, url)) {
        Serial.println("[locket] http.begin failed");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + auth.id_token);

    // Body: {"data": {"last_fetch": <n>, "should_count_missed_moments": true}}
    JsonDocument req;
    req["data"]["last_fetch"] = last_fetch_seconds;
    req["data"]["should_count_missed_moments"] = true;
    String req_str;
    serializeJson(req, req_str);

    Serial.printf("[locket] POST getLatestMomentV2 (last_fetch=%llu)\n",
                  (unsigned long long)last_fetch_seconds);

    int code = http.POST(req_str);
    String resp = http.getString();
    http.end();

    if (code != HTTP_CODE_OK) {
        Serial.printf("[locket] getLatestMomentV2 status=%d\n", code);
        Serial.printf("[locket] response:\n%s\n", resp.c_str());
        return false;
    }

    // Filter the response: moment objects contain a lot of fields we don't
    // care about (user, music, captions, etc.). The [0] in the filter is the
    // ArduinoJson wildcard for "any array index — keep this shape for all".
    JsonDocument filter;
    filter["result"]["data"][0]["thumbnail_url"]       = true;
    filter["result"]["data"][0]["date"]["_seconds"]    = true;
    filter["result"]["missed_moments_count"]           = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, resp, DeserializationOption::Filter(filter));
    if (err) {
        Serial.printf("[locket] JSON parse failed: %s\n", err.c_str());
        Serial.printf("[locket] response:\n%s\n", resp.c_str());
        return false;
    }

    JsonObjectConst result = doc["result"].as<JsonObjectConst>();
    if (result.isNull()) {
        Serial.println("[locket] response missing 'result' envelope");
        Serial.printf("[locket] body:\n%s\n", resp.c_str());
        return false;
    }

    JsonArrayConst data = result["data"].as<JsonArrayConst>();
    out->moments_count        = data.isNull() ? 0 : (int)data.size();
    out->missed_moments_count = result["missed_moments_count"] | 0;

    if (out->moments_count > 0) {
        JsonObjectConst m0 = data[0].as<JsonObjectConst>();
        const char* thumb = m0["thumbnail_url"];
        out->first.thumbnail_url = thumb ? thumb : "";
        // date._seconds is a Firebase Timestamp; usually an int, occasionally
        // serialized as a float. .as<uint64_t>() handles both.
        out->first.date_seconds = m0["date"]["_seconds"].as<uint64_t>();
    } else {
        out->first.thumbnail_url = "";
        out->first.date_seconds = 0;
    }

    return true;
}
