# locket-frame firmware

Firmware for an ESP32-S3 that turns a Waveshare 7.3" color e-paper panel into a Locket photo frame: it periodically fetches the latest [Locket](https://locket.camera/) moment and displays it, together with the time it was sent. Between refreshes the whole board is in deep sleep, so it can run headless indefinitely.

## Hardware

| Part    | Reference                                                        |
| ------- | ---------------------------------------------------------------- |
| MCU     | ESP32-S3-DevKitC-1 (N16R8: 16 MB flash, 8 MB PSRAM)              |
| Display | Waveshare 7.3" e-Paper HAT (E) — Spectra E6, 800×480, 6 colors   |

The panel does black / white / yellow / red / blue / green, needs ~30 s per full refresh, and has a finite refresh-cycle budget — the firmware is built around refreshing as rarely as possible.

Wiring (defined in `include/config.h`, the single source of truth for pins):

| Panel signal | GPIO |
| ------------ | ---- |
| BUSY         | 13   |
| RST          | 12   |
| DC           | 11   |
| CS           | 10   |
| CLK          | 9    |
| DIN (MOSI)   | 8    |
| PWR rail     | 7    |

`BATT_ADC` (GPIO 4) and `BUTTON` (GPIO 5) are reserved in the pin map but not implemented yet.

## Setup & build

Requires [PlatformIO](https://platformio.org/). From `firmware/`:

```sh
cp include/secrets.h.example include/secrets.h   # fill in WiFi + Locket credentials
pio run                  # build
pio run -t upload        # flash (expects /dev/ttyACM0)
pio device monitor       # serial log, 115200 baud
```

`secrets.h` is gitignored; it needs your WiFi credentials, Locket account, and the public Firebase API key from the Locket iOS app (pre-filled in `scrapping/.env.example`).

## How it works

Everything happens once in `setup()` — there is no `loop()`. At the end of every path the board arms a timer wakeup (`REFRESH_INTERVAL_S`, default 3 h) and enters deep sleep, which reboots it from scratch on wake.

```
boot / timer wake
  │
  ├─ wifi_connect()                      image.cpp
  ├─ time_sync()          NTP, no-op when the RTC still has time
  ├─ locket_sign_in()                    locket_auth.cpp
  ├─ locket_get_latest_moment()          locket_client.cpp
  │
  ├─ same moment as last wake? ──yes──▶ back to sleep (no panel refresh)
  │                     no
  ├─ display_init()                      display.cpp
  ├─ image_render_from_url()             image.cpp
  ├─ panel_ui_draw_moment_info()         panel_ui.cpp
  ├─ display_refresh()   ~30 s
  └─ deep sleep (REFRESH_INTERVAL_S)
```

### Fetching the moment

`locket_auth.cpp` performs a Firebase password sign-in against `identitytoolkit/.../verifyPassword`, sending the same iOS spoof headers as the Python prototype in `scrapping/` — Locket's Firebase key is bundle-restricted to the iOS app, so without those headers sign-in fails. `locket_client.cpp` then POSTs `getLatestMomentV2` on `api.locketcamera.com` (a Firebase Callable Function: request wrapped in `{"data": …}`, response in `{"result": …}`) and extracts three fields from the newest moment: `thumbnail_url`, `canonical_uid`, and `date._seconds`.

### Skipping needless refreshes

The identity of the currently displayed moment (`canonical_uid` + timestamp) is stored in `RTC_DATA_ATTR` variables, which survive deep sleep. If the fetched moment is the one already on the panel, the firmware goes straight back to sleep — no image download, no 30 s refresh, no wasted panel cycle. E-paper is bistable, so the picture stays up with zero power.

### Image pipeline

The Locket thumbnail is a full-size photo; scaling it on-device would be slow, so the URL is rewritten through the public [wsrv.nl](https://wsrv.nl) image proxy (`weserv_wrap()` in `image.cpp`), which returns a 480×480 `fit=cover` JPEG. Then, entirely in PSRAM:

1. HTTPS GET the JPEG (~50–150 KB).
2. Decode with [JPEGDEC](https://github.com/bitbank2/JPEGDEC) to RGB565.
3. **Floyd–Steinberg dither** each pixel to the nearest of the panel's 6 ink colors, diffusing the quantization error to neighbors — this is what makes photos look surprisingly good on 6 colors.
4. Pack the 4-bit color indices into the left 480×480 of the 192 000-byte framebuffer.

The right 320×480 strip is drawn by `panel_ui.cpp` using the vendored Waveshare `GUI_Paint` fonts: "sent HH:MM" and the date, converted to local time with `TZ_OFFSET_S` (fixed offset — no DST handling yet).

### TLS

All three HTTPS hosts (`www.googleapis.com`, `api.locketcamera.com`, `wsrv.nl`) chain to Google Trust Services roots, so certificate verification is on everywhere, pinned to GTS Root R1 / R4 in `include/certs.h` (roots valid until 2036). Because mbedTLS checks certificate validity dates, the clock must be set before the first handshake — that's what `time_sync()` (NTP) is for; the RTC keeps time through deep sleep so it only actually syncs on the first boot. If a root rotation upstream ever breaks the handshake, `ALLOW_INSECURE_TLS` in `config.h` is a temporary compile-time escape hatch.

### Failure behavior

Any failure (WiFi, NTP, sign-in, fetch, render) logs to serial and sleeps until the next wake — errors are always retried. An error screen is drawn on the panel **only if it would otherwise be blank** (first boot): a photo that's already displayed is never replaced by an error message, both for looks and to save refresh cycles.

## Configuration (`include/config.h`)

| Setting              | Default    | Meaning                                            |
| -------------------- | ---------- | -------------------------------------------------- |
| `REFRESH_INTERVAL_S` | 3 h        | Deep-sleep duration between wakes                  |
| `TZ_OFFSET_S`        | 7200 (CEST)| Offset applied to the moment timestamp for display |
| `WIFI_TIMEOUT_MS`    | 20 s       | Give up on WiFi and sleep instead of hanging       |
| `ALLOW_INSECURE_TLS` | 0          | 1 = disable certificate verification (last resort) |

## Project layout

```
include/
  config.h            pins + operational settings
  certs.h             pinned Google Trust Services root CAs
  secrets.h(.example) WiFi + Locket credentials (gitignored)
src/
  main.cpp            boot cycle, sleep/skip logic, error policy
  locket_auth.cpp     Firebase sign-in with iOS spoof headers
  locket_client.cpp   getLatestMomentV2 call + JSON filtering
  image.cpp           WiFi, NTP, weserv URL, JPEG decode, dithering
  panel_ui.cpp        right-strip timestamp + error screens
  display.cpp         panel power, framebuffer, refresh, hibernate
lib/waveshare_epd/    vendored Waveshare EPD_7in3e driver + GUI_Paint
```

## Not implemented yet

- Button wake (EXT1) for refresh-on-demand — GPIO reserved.
- Battery voltage readout and on-panel battery indicator — ADC pin and divider constants reserved.
- Weather in the right strip (Open-Meteo; `LATITUDE`/`LONGITUDE` placeholders exist).
- DST-aware timezone handling.
- On-device image scaling, to drop the wsrv.nl dependency.
