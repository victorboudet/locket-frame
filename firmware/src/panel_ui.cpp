#include "panel_ui.h"
#include "config.h"

#include "EPD_7in3e.h"
#include "GUI_Paint.h"
#include "fonts.h"

#include <Arduino.h>
#include <time.h>

namespace {

// The image occupies the left 480x480; the info strip is what's right of it.
constexpr UWORD STRIP_X = 480;
constexpr UWORD STRIP_W = EPD_7IN3E_WIDTH - STRIP_X;   // 320

void draw_centered(UWORD y, const char* text, sFONT* font, UWORD color) {
    UWORD w = (UWORD)(strlen(text) * font->Width);
    UWORD x = STRIP_X + (STRIP_W > w ? (STRIP_W - w) / 2 : 0);
    Paint_DrawString_EN(x, y, text, font, color, EPD_7IN3E_WHITE);
}

}  // namespace

void panel_ui_draw_moment_info(uint64_t date_seconds) {
    time_t local = (time_t)date_seconds + TZ_OFFSET_S;
    struct tm t;
    gmtime_r(&local, &t);   // offset already applied; gmtime avoids TZ env

    char hhmm[8];
    char date_line[16];
    strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
    strftime(date_line, sizeof(date_line), "%d %b %Y", &t);

    Paint_ClearWindows(STRIP_X, 0, EPD_7IN3E_WIDTH, EPD_7IN3E_HEIGHT,
                       EPD_7IN3E_WHITE);

    UWORD y = EPD_7IN3E_HEIGHT / 2 - 44;
    draw_centered(y, "sent", &Font16, EPD_7IN3E_BLACK);
    y += Font16.Height + 10;
    draw_centered(y, hhmm, &Font24, EPD_7IN3E_BLACK);
    y += Font24.Height + 14;
    draw_centered(y, date_line, &Font16, EPD_7IN3E_BLACK);
}

void panel_ui_draw_error(const char* msg) {
    Paint_Clear(EPD_7IN3E_WHITE);
    Paint_DrawString_EN(24, 24, "locket-frame", &Font24,
                        EPD_7IN3E_BLACK, EPD_7IN3E_WHITE);
    Paint_DrawString_EN(24, 72, msg, &Font24,
                        EPD_7IN3E_RED, EPD_7IN3E_WHITE);
    Paint_DrawString_EN(24, 120, "will retry on next wake", &Font16,
                        EPD_7IN3E_BLACK, EPD_7IN3E_WHITE);
}
