#include "image.h"
#include "certs.h"
#include "config.h"
#include "secrets.h"

#include "EPD_7in3e.h"
#include "DEV_Config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>
#include <limits.h>

// ---------------------------------------------------------------------------
// 6-colour palette in sRGB. Index order matches palette_to_epd[].
// These approximate how the Spectra 6 ink mixes look — tweakable later.
// ---------------------------------------------------------------------------
static const uint8_t palette_rgb[6][3] = {
    {  0,   0,   0},   // BLACK
    {255, 255, 255},   // WHITE
    {255, 243,  56},   // YELLOW
    {191,   0,   0},   // RED
    {100,  64, 255},   // BLUE
    { 67, 138,  28},   // GREEN
};
static const UBYTE palette_to_epd[6] = {
    EPD_7IN3E_BLACK,
    EPD_7IN3E_WHITE,
    EPD_7IN3E_YELLOW,
    EPD_7IN3E_RED,
    EPD_7IN3E_BLUE,
    EPD_7IN3E_GREEN,
};

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
bool wifi_connect() {
    Serial.printf("[wifi] connecting to '%s'\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - t0 > WIFI_TIMEOUT_MS) {
            Serial.println("[wifi] timeout");
            return false;
        }
        delay(100);
    }
    Serial.printf("[wifi] up, IP %s, RSSI %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
}

void wifi_disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

// Any time after 2023 counts as "the clock has been set".
static constexpr time_t CLOCK_VALID_AFTER = 1672531200;

bool time_sync() {
    time_t now = time(nullptr);
    if (now > CLOCK_VALID_AFTER) return true;   // RTC survived deep sleep

    Serial.println("[time] syncing over NTP");
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    uint32_t t0 = millis();
    while ((now = time(nullptr)) <= CLOCK_VALID_AFTER) {
        if (millis() - t0 > 15000) {
            Serial.println("[time] NTP timeout");
            return false;
        }
        delay(100);
    }
    Serial.printf("[time] clock set, unix=%lu\n", (unsigned long)now);
    return true;
}

// ---------------------------------------------------------------------------
// URL encoding + weserv wrapper (phase 4 will use this for Locket URLs)
// ---------------------------------------------------------------------------
static String url_encode(const char* s) {
    String out;
    for (size_t i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

String weserv_wrap(const char* source_url, int w, int h) {
    String out = "https://wsrv.nl/?url=";
    out += url_encode(source_url);
    out += "&w=" + String(w) + "&h=" + String(h);
    out += "&fit=cover&output=jpg&q=90";
    return out;
}

// ---------------------------------------------------------------------------
// JPEGDEC decode callback. JPEGDEC's signature is a C-style fn pointer; we
// stash state in file-scope globals (single-threaded, single decode at a time).
// ---------------------------------------------------------------------------
static uint16_t* g_rgb = nullptr;
static int       g_rgb_w = 480;
static int       g_rgb_h = 480;

static int jpeg_draw_cb(JPEGDRAW* p) {
    int x = p->x, y = p->y, w = p->iWidth, h = p->iHeight;
    uint16_t* src = p->pPixels;
    for (int yy = 0; yy < h; ++yy) {
        int row = y + yy;
        if (row < 0 || row >= g_rgb_h) continue;
        int cw = w;
        if (x + cw > g_rgb_w) cw = g_rgb_w - x;
        if (x < 0 || cw <= 0) continue;
        memcpy(&g_rgb[row * g_rgb_w + x], &src[yy * w], cw * 2);
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Floyd-Steinberg dither rgb (480x480 RGB565) onto fb's left 480x480 region.
// Error carried in two int16 row buffers (R,G,B per pixel).
// ---------------------------------------------------------------------------
static bool dither_and_write(const uint16_t* rgb, UBYTE* fb) {
    const int W = 480, H = 480;
    const int FB_STRIDE = EPD_7IN3E_WIDTH / 2;   // 400 bytes/row

    const size_t row_bytes = 3 * W * sizeof(int16_t);
    int16_t* err_curr = (int16_t*)heap_caps_malloc(row_bytes, MALLOC_CAP_SPIRAM);
    int16_t* err_next = (int16_t*)heap_caps_malloc(row_bytes, MALLOC_CAP_SPIRAM);
    if (!err_curr || !err_next) {
        Serial.println("[dither] err buf alloc FAILED");
        if (err_curr) free(err_curr);
        if (err_next) free(err_next);
        return false;
    }
    memset(err_curr, 0, row_bytes);
    memset(err_next, 0, row_bytes);

    for (int y = 0; y < H; ++y) {
        if (y > 0) {
            int16_t* tmp = err_curr;
            err_curr = err_next;
            err_next = tmp;
            memset(err_next, 0, row_bytes);
        }

        for (int x = 0; x < W; ++x) {
            uint16_t pix = rgb[y * W + x];
            // RGB565 -> RGB888 (high bits replicated into low bits)
            int r = ((pix >> 11) & 0x1F) << 3; r |= r >> 5;
            int g = ((pix >> 5)  & 0x3F) << 2; g |= g >> 6;
            int b = ( pix        & 0x1F) << 3; b |= b >> 5;

            r += err_curr[x * 3 + 0];
            g += err_curr[x * 3 + 1];
            b += err_curr[x * 3 + 2];

            if (r < 0) r = 0; else if (r > 255) r = 255;
            if (g < 0) g = 0; else if (g > 255) g = 255;
            if (b < 0) b = 0; else if (b > 255) b = 255;

            // Nearest palette entry by squared euclidean distance in RGB.
            int best = 0, best_d = INT_MAX;
            for (int p = 0; p < 6; ++p) {
                int dr = r - palette_rgb[p][0];
                int dg = g - palette_rgb[p][1];
                int db = b - palette_rgb[p][2];
                int d = dr*dr + dg*dg + db*db;
                if (d < best_d) { best_d = d; best = p; }
            }

            int er = r - palette_rgb[best][0];
            int eg = g - palette_rgb[best][1];
            int eb = b - palette_rgb[best][2];

            // Floyd-Steinberg error distribution (7/16, 3/16, 5/16, 1/16).
            if (x + 1 < W) {
                err_curr[(x+1)*3 + 0] += (er * 7) / 16;
                err_curr[(x+1)*3 + 1] += (eg * 7) / 16;
                err_curr[(x+1)*3 + 2] += (eb * 7) / 16;
            }
            if (x > 0) {
                err_next[(x-1)*3 + 0] += (er * 3) / 16;
                err_next[(x-1)*3 + 1] += (eg * 3) / 16;
                err_next[(x-1)*3 + 2] += (eb * 3) / 16;
            }
            err_next[x*3 + 0] += (er * 5) / 16;
            err_next[x*3 + 1] += (eg * 5) / 16;
            err_next[x*3 + 2] += (eb * 5) / 16;
            if (x + 1 < W) {
                err_next[(x+1)*3 + 0] += (er * 1) / 16;
                err_next[(x+1)*3 + 1] += (eg * 1) / 16;
                err_next[(x+1)*3 + 2] += (eb * 1) / 16;
            }

            // Pack colour index: even-x -> high nibble, odd-x -> low nibble
            // (matches the convention in GUI_Paint.cpp's Paint_SetPixel).
            UBYTE code = palette_to_epd[best];
            int addr = y * FB_STRIDE + (x >> 1);
            UBYTE rd = fb[addr];
            rd &= ~(0xF0 >> ((x & 1) * 4));
            fb[addr] = rd | ((code << 4) >> ((x & 1) * 4));
        }
    }

    free(err_curr);
    free(err_next);
    return true;
}

// ---------------------------------------------------------------------------
// Main entry: HTTPS GET -> JPEG -> RGB565 -> dither -> framebuffer left region
// ---------------------------------------------------------------------------
bool image_render_from_url(const char* url, UBYTE* fb) {
    if (!fb) return false;

    Serial.printf("[image] GET %s\n", url);

    WiFiClientSecure client;
#if ALLOW_INSECURE_TLS
    client.setInsecure();
    Serial.println("[image] WARNING: TLS verification disabled");
#else
    client.setCACert(GTS_ROOT_R4);   // wsrv.nl chains to GTS Root R4
#endif
    HTTPClient http;
    http.setTimeout(15000);
    if (!http.begin(client, url)) {
        Serial.println("[image] http.begin failed");
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[image] GET status %d\n", code);
        http.end();
        return false;
    }
    int len = http.getSize();
    if (len <= 0 || len > 4 * 1024 * 1024) {
        Serial.printf("[image] unexpected content-length %d\n", len);
        http.end();
        return false;
    }

    uint8_t* jpg = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (!jpg) {
        Serial.println("[image] jpg PSRAM alloc FAILED");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    int got = 0;
    uint32_t t0 = millis();
    while (got < len) {
        int avail = stream->available();
        if (avail > 0) {
            int n = stream->readBytes(jpg + got, min(avail, len - got));
            got += n;
        } else if (!http.connected() && stream->available() == 0) {
            break;
        } else if (millis() - t0 > 30000) {
            Serial.println("[image] read timeout");
            break;
        } else {
            delay(1);
        }
    }
    http.end();
    if (got != len) {
        Serial.printf("[image] short read %d/%d\n", got, len);
        free(jpg);
        return false;
    }
    Serial.printf("[image] got %d bytes in %u ms\n", got,
                  (unsigned)(millis() - t0));

    // RGB565 destination buffer.
    g_rgb_w = 480;
    g_rgb_h = 480;
    g_rgb = (uint16_t*)heap_caps_malloc(g_rgb_w * g_rgb_h * 2, MALLOC_CAP_SPIRAM);
    if (!g_rgb) {
        Serial.println("[image] rgb PSRAM alloc FAILED");
        free(jpg);
        return false;
    }
    memset(g_rgb, 0xFF, g_rgb_w * g_rgb_h * 2);   // white if any holes

    JPEGDEC jpeg;
    if (!jpeg.openRAM(jpg, len, jpeg_draw_cb)) {
        Serial.printf("[image] JPEGDEC openRAM failed (last err %d)\n",
                      jpeg.getLastError());
        free(g_rgb); g_rgb = nullptr;
        free(jpg);
        return false;
    }
    Serial.printf("[image] JPEG %dx%d, decoding\n",
                  jpeg.getWidth(), jpeg.getHeight());
    t0 = millis();
    if (!jpeg.decode(0, 0, 0)) {
        Serial.printf("[image] JPEGDEC decode failed (last err %d)\n",
                      jpeg.getLastError());
        jpeg.close();
        free(g_rgb); g_rgb = nullptr;
        free(jpg);
        return false;
    }
    jpeg.close();
    Serial.printf("[image] decoded in %u ms\n", (unsigned)(millis() - t0));

    free(jpg); jpg = nullptr;

    t0 = millis();
    bool ok = dither_and_write(g_rgb, fb);
    Serial.printf("[image] dithered in %u ms\n", (unsigned)(millis() - t0));

    free(g_rgb); g_rgb = nullptr;
    return ok;
}
