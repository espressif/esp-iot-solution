| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

# LVGL Mono OLED Demo

This example demonstrates how to use the `esp_lvgl_adapter` component with a monochrome SSD1306 OLED panel over I2C. It provides a minimal end-to-end setup from panel initialization to LVGL rendering.

## Overview

The example showcases:
- **Monochrome LVGL setup**: 1bpp display configuration with vertical tiled layout
- **SSD1306 over I2C**: Basic panel bring-up for a common 128x64 OLED
- **Adapter integration**: Unified LVGL adapter init and display registration
- **Simple UI**: Two labels rendered using a bundled font

This example is ideal for:
- Verifying I2C OLED wiring and panel initialization
- Learning the minimal LVGL adapter flow for mono displays
- Reusing the SSD1306 init sequence in other projects

## How to Use the Example

### Hardware Required

* An ESP32-P4, ESP32-S3, or ESP32-C3 development board
* A SSD1306-compatible OLED panel (128x64, I2C)
* A USB cable for power supply and programming

### Hardware Connection

Connect the OLED panel to the board I2C pins and power:
- **SDA**: I2C data
- **SCL**: I2C clock
- **VCC**: 3.3V (or 5V if supported by the panel)
- **GND**: Ground

Default pin and bus settings are defined in `examples/display/gui/lvgl_mono_demo/main/hw_oled.c`. Update them if your board uses different pins or an alternate I2C port.

### Configure the Project

This example does not use menuconfig options. If your panel uses a different I2C address or size, update:
- `HW_OLED_I2C_ADDR` in `examples/display/gui/lvgl_mono_demo/main/hw_oled.c`
- `HW_OLED_H_RES` / `HW_OLED_V_RES` in `examples/display/gui/lvgl_mono_demo/main/hw_oled.h`

### Build and Flash

1. Set the target chip:
```bash
idf.py set-target esp32p4
# or
idf.py set-target esp32s3
# or
idf.py set-target esp32c3
```

2. Build, flash and monitor:
```bash
idf.py -p PORT build flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

The first time you run `idf.py` for the example will take extra time as the build system needs to download components from the registry into the `managed_components` folder.

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Expected Output

After flashing, the OLED should display two text lines (a short greeting and "ESP LVGL Adapter"). The serial console will show initialization logs:

```
I (xxx) mono_demo: Initialize OLED panel
I (xxx) hw_oled: Initialize I2C bus
I (xxx) hw_oled: Install SSD1306 panel IO
I (xxx) hw_oled: Install SSD1306 panel driver
I (xxx) mono_demo: Initialize LVGL adapter
I (xxx) mono_demo: Register OLED display
I (xxx) mono_demo: Start LVGL adapter task
I (xxx) mono_demo: OLED initialization complete
```

## Code Structure

- **`main.c`**: Application entry point
  - Calls the OLED init helper
  - Configures and starts the LVGL adapter
  - Creates two labels on the default screen
- **`hw_oled.c` / `hw_oled.h`**: OLED hardware helper
  - I2C bus configuration
  - SSD1306 panel IO and driver setup
  - Panel reset/init sequence

## Troubleshooting

**Display shows nothing:**
- Check I2C wiring and power (VCC/GND)
- Confirm the I2C address (0x3C vs 0x3D) in `examples/display/gui/lvgl_mono_demo/main/hw_oled.c`
- Verify the board uses the GPIO pins configured in `examples/display/gui/lvgl_mono_demo/main/hw_oled.c`

**Garbage or mirrored output:**
- Verify `HW_OLED_H_RES` / `HW_OLED_V_RES` match the panel size
- Try toggling `HW_OLED_INVERT_COLOR` for panel-specific polarity

**Build errors:**
- Ensure ESP-IDF version is 5.5.0 or later
- Run `idf.py fullclean` and rebuild
- Check that all managed components downloaded correctly

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
