# locket-frame

An e-ink photo frame that always shows the latest picture sent to a [Locket](https://locket.camera/) account.

An ESP32-S3 wakes up every few hours, signs in to Locket, checks whether a new moment arrived, and if so draws it on a 7.3" 6-color e-paper panel along with the time it was sent — then goes back to deep sleep. No server, no companion app: the frame talks to Locket's API directly.

![stack](https://img.shields.io/badge/ESP32--S3-PlatformIO-orange) ![panel](https://img.shields.io/badge/Waveshare_7.3%22-Spectra_E6-blue)

## Repository layout

| Path         | What it is                                                                                          |
| ------------ | --------------------------------------------------------------------------------------------------- |
| `firmware/`  | **The actual project** — PlatformIO firmware for the frame. See [`firmware/README.md`](firmware/README.md) for how it works. |
| `scrapping/` | Python prototype used to reverse-engineer and test the Locket API before writing the C++. Still handy for archiving thumbnails locally and for debugging API changes from a desktop. |

## Quick start

Flash the frame:

```sh
cd firmware
cp include/secrets.h.example include/secrets.h   # then fill in your credentials
pio run -t upload
pio device monitor
```

You will need a Locket account and the Firebase API key shipped with the Locket iOS app (see `firmware/include/secrets.h.example`).

For testing the API and scraping moments locally, see the [`scrapping/` directory README](scrapping/README.md).

## Thanks

- [michioxd/luckit](https://github.com/michioxd/luckit): For documenting the Locket API and helping me understand how it works.
