# Keep an LCD on in low-power standby

An appliance may need to keep a clock or status indicator visible in standby. This example keeps a continuously scanned RGB or MIPI-DSI LCD and its backlight on.

Once a minute, the app redraws only the digits that changed. Between updates, the display keeps scanning the RGB565 framebuffer in PSRAM. Lowering the scan rate reduces framebuffer reads and pixel traffic. The CPU runs at a lower frequency while waiting and speeds up briefly to draw.

| Target and panel | Normal → standby scan | CPU while drawing → waiting in standby |
| --- | --- | --- |
| ESP32-P4 rev. 3.x, EK79007 1024 × 600, two-lane MIPI-DSI | ~60 → ~10 Hz | 100 → 20 MHz |
| ESP32-S3, ST7701 480 × 480, 16-bit RGB | ~60 → ~30 Hz (configured) | 160 → 80 MHz |

## Run the example

Use ESP-IDF v5.5.5 or later. Follow the matching pin map for [EK79007](../mipi_dsi_avoid_tearing) or [ST7701](../rgb_avoid_tearing). The P4 setup uses LDO3 at 2.5 V for the D-PHY and GPIO27 for panel reset. The S3 setup uses octal PSRAM and GPIO38 for the backlight.

```sh
idf.py set-target esp32p4      # use esp32s3 for the RGB setup
idf.py -p PORT flash monitor
```

The clock starts at 12:34. The log shows `Standby mode` for 130 seconds, then `Normal mode` for 10 seconds before the cycle repeats. Watch for two minute changes during standby.

## Follow the code

- [main.c](main/main.c) runs the two modes and waits for the next minute or mode deadline. It sets the CPU frequency with `esp_pm_configure()` and keeps system light sleep disabled during scanout.
- [clock_face.c](main/clock_face.c) draws only changed digits after the first frame and submits the rows that cover them.
- [panel_rgb.c](main/panel_rgb.c) changes the pixel clock at a frame boundary and reuses the framebuffer. [panel_dsi.c](main/panel_dsi.c) recreates the link to change the pixel clock and lane rate. This can briefly blank the screen. The app uses the new framebuffer and redraws.

Choose a standby scan rate supported by your panel. For RGB, set `STANDBY_PCLK_HZ` and verify the image after switching in both directions.

## Integrate with LVGL

In an app using `esp_lvgl_adapter`, let the display task own mode changes and coordinate CPU frequency across the app.

- **RGB.** Keep the panel registered. Show a static clock page, stop animations, and lower the pixel clock with `esp_lcd_rgb_panel_set_pclk()`. Restore the normal clock and UI when leaving standby.
- **MIPI-DSI.** Call `esp_lv_adapter_sleep_prepare()` to finish pending flushes and detach the panel. Rebuild the DSI link at standby timings and draw the clock into a single framebuffer. When leaving standby, recreate the normal panel with its original buffer configuration and rebind it with `esp_lv_adapter_sleep_recover()`.

## Board design and current

The `LCD_RST` circuit can affect standby current. On the EK79007 fixture, holding reset high with a push-pull GPIO increased P4 supply current.

On the P4 + EK79007 fixture with hardware reset, the P4 supply branch averaged about **58 mA in normal mode and 36 mA in standby**. Use your product's USB and PSRAM settings when comparing modes. Include panel and backlight power in the system budget.
