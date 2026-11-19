# Waveshare display driver

Vendored from [waveshareteam/e-Paper](https://github.com/waveshareteam/e-Paper/tree/d28b3c4cfc78eef1c6cd6e6e3013654e449d2864/Arduino_R4/src),
commit `d28b3c4cfc78eef1c6cd6e6e3013654e449d2864`.

Included upstream files:

- `Config/DEV_Config.cpp`, `DEV_Config.h`, `Debug.h`
- `e-Paper/EPD_7in3e.cpp`, `EPD_7in3e.h`
- `GUI/GUI_Paint.cpp`, `GUI_Paint.h`
- `Fonts/fonts.h`, `font16.cpp`, `font24.cpp`

Original license notices are preserved in the source files.

Local adaptations:

- Flattened the source layout and adjusted relative includes for PlatformIO.
- Mapped GPIO definitions to `firmware/include/config.h`.
- Passed the configured clock, MOSI and CS pins to ESP32 `SPI.begin`, with no MISO.
- Left serial initialization to the application; debug logging includes `Arduino.h`.
- Ended the SPI transaction before switching to GPIO mode or shutting down the panel.

Panel commands, timings, color codes, drawing routines and font data are unchanged.
