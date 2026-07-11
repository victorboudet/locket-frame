#include "display.h"
#include "config.h"

#include "DEV_Config.h"
#include "EPD_7in3e.h"
#include "GUI_Paint.h"
#include "fonts.h"

#include <esp_heap_caps.h>

// 800 * 480 / 2 — two 4-bit pixels per byte. The authoritative encoding
// comes from EPD_7in3e.h (sec.3 of CLAUDE.md: never invent this).
static constexpr size_t FB_BYTES = (EPD_7IN3E_WIDTH / 2) * EPD_7IN3E_HEIGHT;
static_assert(FB_BYTES == 192000, "framebuffer must be 192000 bytes");

static UBYTE* framebuffer = nullptr;

bool display_init() {
    // Raise the panel power rail FIRST, before any SPI traffic. The driver's
    // GPIO_Config() does this too, but we want a settle delay before the
    // panel reset pulse hits.
    pinMode(EPD_PWR, OUTPUT);
    digitalWrite(EPD_PWR, HIGH);
    delay(10);

    framebuffer = (UBYTE*)heap_caps_malloc(FB_BYTES, MALLOC_CAP_SPIRAM);
    if (!framebuffer) {
        Serial.println("[display] PSRAM alloc FAILED");
        return false;
    }
    Serial.printf("[display] framebuffer @ %p (%u bytes, PSRAM)\n",
                  framebuffer, (unsigned)FB_BYTES);

    if (DEV_Module_Init() != 0) {
        Serial.println("[display] DEV_Module_Init FAILED");
        return false;
    }
    EPD_7IN3E_Init();

    // Scale 6 selects the 6-colour 4-bits-per-pixel packing the panel needs.
    Paint_NewImage(framebuffer, EPD_7IN3E_WIDTH, EPD_7IN3E_HEIGHT, 0, EPD_7IN3E_WHITE);
    Paint_SetScale(6);
    Paint_SelectImage(framebuffer);
    Paint_Clear(EPD_7IN3E_WHITE);

    return true;
}

void display_test_pattern() {
    if (!framebuffer) return;

    // Order follows the EPD_7IN3E_* macro values 0,1,2,3,5,6. Code 0x4 is
    // commented out in the vendored driver — that's why this is a 6-colour
    // panel, not 7.
    const UBYTE colours[6] = {
        EPD_7IN3E_BLACK,
        EPD_7IN3E_WHITE,
        EPD_7IN3E_YELLOW,
        EPD_7IN3E_RED,
        EPD_7IN3E_BLUE,
        EPD_7IN3E_GREEN,
    };
    const char* labels[6] = {"BLACK", "WHITE", "YELLOW", "RED", "BLUE", "GREEN"};

    Paint_Clear(EPD_7IN3E_WHITE);

    for (int i = 0; i < 6; ++i) {
        UWORD x0 = (UWORD)((long)i       * EPD_7IN3E_WIDTH / 6);
        UWORD x1 = (UWORD)((long)(i + 1) * EPD_7IN3E_WIDTH / 6 - 1);
        Paint_DrawRectangle(x0, 0, x1, EPD_7IN3E_HEIGHT - 1,
                            colours[i], DOT_PIXEL_1X1, DRAW_FILL_FULL);

        // White card behind the label so text reads on every bar colour.
        UWORD lx = x0 + 8;
        UWORD ly = EPD_7IN3E_HEIGHT / 2 - Font16.Height / 2;
        UWORD card_w = Font16.Width * 7;   // "YELLOW" is the widest label
        Paint_DrawRectangle(lx - 4, ly - 4,
                            lx + card_w + 4, ly + Font16.Height + 4,
                            EPD_7IN3E_WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawString_EN(lx, ly, labels[i], &Font16,
                            EPD_7IN3E_BLACK, EPD_7IN3E_WHITE);
    }

    // Banner across the top so we can confirm Font24 and text positioning.
    Paint_DrawRectangle(0, 0, EPD_7IN3E_WIDTH - 1, 36,
                        EPD_7IN3E_WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(8, 6, "locket-frame phase 2: panel ok",
                        &Font24, EPD_7IN3E_BLACK, EPD_7IN3E_WHITE);

    display_refresh();
}

UBYTE* display_framebuffer() {
    return framebuffer;
}

void display_refresh() {
    if (!framebuffer) return;
    Serial.println("[display] sending framebuffer (full refresh takes ~30s)");
    uint32_t t0 = millis();
    EPD_7IN3E_Display(framebuffer);
    Serial.printf("[display] refresh complete in %u ms\n", millis() - t0);
}

void display_clean() {
    Serial.println("[display] anti-ghosting clean (full clear cycle, ~30s)");
    uint32_t t0 = millis();
    EPD_7IN3E_Clear(EPD_7IN3E_WHITE);
    Serial.printf("[display] clean complete in %u ms\n", millis() - t0);
}

void display_free() {
    if (framebuffer) {
        free(framebuffer);
        framebuffer = nullptr;
    }
}

void display_hibernate() {
    Serial.println("[display] hibernate panel + drop EPD_PWR");
    EPD_7IN3E_Sleep();
    delay(2000);          // Waveshare reference: wait >= 2s before cutting power.
    DEV_Module_Exit();    // drops EPD_PWR LOW
}
