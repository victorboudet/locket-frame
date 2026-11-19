#include "locket_client.h"
#include "certs.h"
#include "config.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {

constexpr const char* BASE_URL = "https://api.locketcamera.com";
const char* DIAGNOSTIC_HEADERS[] = {
    "server", "content-type", "via", "x-request-id",
    "x-cloud-trace-context", "cf-ray", "cf-mitigated",
};

// Match the scraper's truthiness check, including empty error containers.
bool has_errors(JsonVariantConst errors) {
    if (errors.isNull()) return false;
    if (errors.is<JsonArrayConst>()) return errors.size() > 0;
    if (errors.is<JsonObjectConst>()) return errors.size() > 0;
    if (errors.is<const char*>()) return errors.as<const char*>()[0] != '\0';
    if (errors.is<bool>()) return errors.as<bool>();
    return errors.as<double>() != 0;
}

}  // namespace

bool locket_get_latest_moment(const LocketAuthState& auth,
                              uint64_t last_fetch_seconds,
                              LocketLatestResult* out) {
    if (!out) return false;

    String url = String(BASE_URL) + "/getLatestMomentV2";

    WiFiClientSecure client;
#if ALLOW_INSECURE_TLS
    client.setInsecure();
    Serial.println("[locket] WARNING: TLS verification disabled");
#else
    client.setCACert(GTS_ROOT_CA_BUNDLE);
#endif

    HTTPClient http;
    http.setTimeout(30000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) {
        Serial.println("[locket] http.begin failed");
        return false;
    }
    http.collectHeaders(DIAGNOSTIC_HEADERS,
                        sizeof(DIAGNOSTIC_HEADERS) / sizeof(DIAGNOSTIC_HEADERS[0]));
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

    if (code != HTTP_CODE_OK) {
        Serial.printf("[locket] getLatestMomentV2 status=%d\n", code);
        Serial.printf("[locket] response:\n%s\n", resp.c_str());
        for (const char* name : DIAGNOSTIC_HEADERS) {
            if (http.hasHeader(name)) {
                Serial.printf("[locket] %s: %s\n", name, http.header(name).c_str());
            }
        }
        http.end();
        return false;
    }
    http.end();

    // Filter the response: moment objects contain a lot of fields we don't
    // care about (user, music, captions, etc.). The [0] in the filter is the
    // ArduinoJson wildcard for "any array index — keep this shape for all".
    JsonDocument filter;
    filter["result"]["status"]                         = true;
    filter["result"]["errors"]                         = true;
    filter["result"]["data"][0]["thumbnail_url"]       = true;
    filter["result"]["data"][0]["canonical_uid"]       = true;
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
        Serial.println("[locket] unexpected response shape: 'result' must be an object");
        Serial.printf("[locket] body:\n%s\n", resp.c_str());
        return false;
    }

    // HTTP 200 can still carry an application failure. Check before treating
    // missing data as an empty feed, just like scrapping/locket/client.py.
    JsonVariantConst status = result["status"];
    if (has_errors(result["errors"]) ||
        (status.is<int64_t>() && status.as<int64_t>() >= 400)) {
        String details;
        serializeJson(result, details);
        Serial.printf("[locket] getLatestMomentV2 API error: %.300s\n", details.c_str());
        return false;
    }

    JsonArrayConst data = result["data"].as<JsonArrayConst>();
    out->moments_count        = data.isNull() ? 0 : (int)data.size();
    out->missed_moments_count = result["missed_moments_count"] | 0;

    if (out->moments_count > 0) {
        JsonObjectConst m0 = data[0].as<JsonObjectConst>();
        const char* thumb = m0["thumbnail_url"];
        const char* uid   = m0["canonical_uid"];
        out->first.thumbnail_url = thumb ? thumb : "";
        out->first.canonical_uid = uid ? uid : "";
        // date._seconds is a Firebase Timestamp; usually an int, occasionally
        // serialized as a float. .as<uint64_t>() handles both.
        out->first.date_seconds = m0["date"]["_seconds"].as<uint64_t>();
    } else {
        out->first.thumbnail_url = "";
        out->first.canonical_uid = "";
        out->first.date_seconds = 0;
    }

    return true;
}
