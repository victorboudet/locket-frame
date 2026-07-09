# locket-frame

An ESP32-S3 e-ink photo frame that shows the latest picture from a [Locket](https://locket.camera/) account.

- `firmware/` — PlatformIO project for an ESP32-S3 driving a Waveshare 7.3" 6-color e-paper panel. It signs in to Locket, fetches the newest moment, dithers it to the panel's palette, displays it with the sent time, and deep-sleeps between refreshes.
- `scrapping/` — Python scraper that signs in to the same API and archives moment thumbnails locally.
