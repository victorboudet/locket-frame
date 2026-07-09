#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "locket_auth.h"

struct LocketMoment {
    String   thumbnail_url;
    String   canonical_uid;    // stable moment id, used to skip re-renders
    uint64_t date_seconds;     // Unix epoch seconds (moment.date._seconds)
};

struct LocketLatestResult {
    int           moments_count;          // length of result.data
    int           missed_moments_count;   // result.missed_moments_count
    LocketMoment  first;                  // valid only when moments_count > 0
};

// POSTs https://api.locketcamera.com/getLatestMomentV2 with
//   {"data": {"last_fetch": <seconds>, "should_count_missed_moments": true}}
// and Authorization: Bearer <auth.id_token>, unwraps the {"result": ...}
// envelope, fills `out`, returns true on HTTP 200 + valid JSON.
// On failure logs status + body and returns false.
//
// last_fetch_seconds = 1 returns the absolute latest moment (same default as
// scrapping/locket/client.py:get_latest_moment).
//
// moments_count == 0 is a valid success (no new moments — caller decides).
bool locket_get_latest_moment(const LocketAuthState& auth,
                              uint64_t last_fetch_seconds,
                              LocketLatestResult* out);
