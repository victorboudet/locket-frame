# locket-frame

An e-ink photo frame that always shows the latest picture sent to a [Locket](https://locket.camera/) account.

An ESP32-S3 wakes up every few hours, signs in to Locket, checks whether a new moment arrived, and if so draws it on a 7.3" 6-color e-paper panel along with the time it was sent — then goes back to deep sleep. No server, no companion app: the frame talks to Locket's API directly.

![stack](https://img.shields.io/badge/ESP32--S3-PlatformIO-orange) ![panel](https://img.shields.io/badge/Waveshare_7.3%22-Spectra_E6-blue)

## Repository layout

| Path         | What it is                                                                                          |
| ------------ | --------------------------------------------------------------------------------------------------- |
| `firmware/`  | **The actual project** — PlatformIO firmware for the frame. See [`firmware/README.md`](firmware/README.md) for how it works. |
| `scrapping/` | Python prototype used to reverse-engineer and test the Locket API before writing the C++. Still handy for archiving thumbnails locally and for debugging API changes from a desktop. |
| `luckit/`    | Third-party Chrome extension ([michioxd/luckit](https://github.com/michioxd/luckit)) cloned as API reference. Gitignored, not part of this repo. |

## Quick start

Flash the frame:

```sh
cd firmware
cp include/secrets.h.example include/secrets.h   # then fill in your credentials
pio run -t upload
pio device monitor
```

Run the desktop scraper (optional, for testing the API):

```sh
cd scrapping
cp .env.example .env    # then fill in your credentials
uv sync
uv run main.py          # downloads the latest moment(s) to downloads/
```

Both need a Locket account and the Firebase API key shipped with the Locket iOS app (see `scrapping/.env.example`).
