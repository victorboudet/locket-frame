#pragma once

// ============================================================================
// Pin map - CLAUDE.md sec.2. EDIT to match the actual wiring.
// These are example GPIOs from the spec; treat config.h as the single source
// of truth and mirror any change into the vendored driver's DEV_Config.
// ============================================================================

// E-paper panel (SPI, via Waveshare 7.3" HAT (E))
#define EPD_BUSY   13   // input
#define EPD_RST    12   //
#define EPD_DC     11   //
#define EPD_CS     10   // SPI CS 
#define EPD_CLK     9   // SPI SCK
#define EPD_DIN     8   // SPI MOSI
#define EPD_PWR     7   // panel/HAT power rail; HIGH only during refresh

// Analog battery sense. MUST be on ADC1 (GPIO 1-10);
// ADC2 is unusable while WiFi is active.
#define BATT_ADC    4   // TODO: confirm

// Wake button: to GND, internal pull-up, RTC-capable GPIO (0-21), EXT1 source.
#define BUTTON      5   // TODO: confirm

// ============================================================================
// Operational defaults - CLAUDE.md sec.9. EDIT before deployment.
// ============================================================================

// Timer-wake period in seconds. Keep modest -- full refresh is slow and the
// E6 panel has a finite refresh-cycle budget.
#define REFRESH_INTERVAL_S       (3 * 3600)

// Open-Meteo coordinates + local timezone offset (seconds from UTC), applied
// to the moment's date._seconds to render "sent HH:MM".
#define LATITUDE                 0.0f   // TODO: set
#define LONGITUDE                0.0f   // TODO: set
#define TZ_OFFSET_S              0      // TODO: set

// Battery: Vbatt = Vadc * BATT_DIVIDER_RATIO. 2.0 matches a 2x equal-resistor
// divider. LiPo voltage->% endpoints follow.
#define BATT_DIVIDER_RATIO       2.0f
#define BATT_FULL_MV             4200
#define BATT_EMPTY_MV            3300

// Button hold >= this enters config mode (reserved for a later phase).
#define LONG_PRESS_MS            2000

// Give up on WiFi association after this and go back to sleep rather than hang.
#define WIFI_TIMEOUT_MS          20000
