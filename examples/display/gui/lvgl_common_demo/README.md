| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

# LVGL Common Demo (Benchmark)

This example demonstrates how to use the `esp_lvgl_adapter` component with LVGL to run the official LVGL benchmark demo on various LCD interfaces. It serves as a reference implementation for integrating LVGL with different display types in a unified way.

## Overview

The example showcases:
- **Unified LCD interface support**: Works with MIPI DSI, RGB, QSPI, and SPI LCD panels through a single codebase
- **LVGL benchmark demo**: Runs the standard LVGL benchmark to measure rendering performance
- **Multiple input methods**: Supports both touch panels and rotary encoders
- **FPS monitoring**: Optional frame rate statistics for performance analysis
- **Display rotation**: Configurable screen orientation (0°/90°/180°/270°)
- **Tear avoidance**: Built-in support for tearing prevention mechanisms

This example is ideal for:
- Evaluating display performance with different LCD interfaces
- Verifying hardware setup and driver configuration
- Benchmarking LVGL rendering capabilities
- Learning the basic structure of LVGL applications with `esp_lvgl_adapter`

## How to Use the Example

### Hardware Required

* An ESP32-P4, ESP32-S3, or ESP32-C3 development board
* A LCD panel with one of the supported interfaces:
  - **MIPI DSI**: For high-resolution displays (e.g., 1024x600)
  - **RGB**: For parallel RGB interface displays (e.g., 800x480)
  - **QSPI**: For quad-SPI displays (e.g., 360x360, 400x400)
  - **SPI**: For standard SPI displays (e.g., 240x240, 320x240)
* (Optional) Touch panel or rotary encoder for input
* A USB cable for power supply and programming

**Recommended Hardware Combinations:**

| Chip | LCD Interface | Development Board |
|------|---------------|-------------------|
| ESP32-P4 | MIPI DSI | [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) |
| ESP32-S3 | RGB | [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) |
| ESP32-S3 | QSPI | [EchoEar](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/echoear/index.html) |
| ESP32-S3 | SPI | [ESP32-S3-BOX-3](https://github.com/espressif/esp-box/blob/master/docs/hardware_overview/esp32_s3_box_3/hardware_overview_for_box_3.md) |
| ESP32-C3 | SPI | [ESP32-C3-LCDkit](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-lcdkit/index.html) |

### Hardware Connection

The LCD and touch panel connections depend on your hardware configuration. This example uses the `hw_init` component which provides hardware abstraction for different board configurations.

**Common interface options:**
- **MIPI DSI**: Typically uses dedicated MIPI lanes (D0+/-, D1+/-, CLK+/-)
- **RGB**: Uses parallel data lines (RGB565: 16 data pins + HSYNC/VSYNC/DE/PCLK)
- **QSPI**: Uses 4 data lines (IO0-IO3 + CLK + CS)
- **SPI**: Uses standard SPI pins (MOSI/MISO/CLK + CS + DC)

**Input devices:**
- Touch panels typically use I2C or SPI interface
- Rotary encoders use 3 GPIO pins (A, B, and button)

Refer to your board's schematic or the `hw_init` component configuration for specific GPIO mappings.

### Configure the Project

Run `idf.py menuconfig` and navigate to `Example Configuration`:

**LCD Interface Selection:**
- `LCD Interface Type`: Choose between MIPI DSI, RGB, QSPI, or SPI

**Display Settings:**
- `Display Rotation`: Select screen orientation (0°/90°/180°/270°)
- LCD resolution and timing parameters (configured in `hw_init`)

**Input Device:**
- Choose between touch panel or encoder input
- Configure I2C/SPI parameters for touch controller

**Performance Options:**
- `Enable FPS Statistics`: Enable to monitor frame rate in logs

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

After flashing, the LCD should display the LVGL benchmark demo, which includes various animations and UI elements to stress-test rendering performance. The serial console will show initialization logs:

```
I (xxx) main: Selected LCD interface: MIPI DSI
I (xxx) main: Initializing LCD: 1024x600
I (xxx) main: Starting LVGL benchmark demo
```

If FPS statistics are enabled, you will see periodic frame rate reports:
```
I (xxx) main: Current FPS: 45
```

The benchmark will cycle through different test scenes automatically, displaying shapes, images, text, and animations.

## Code Structure

- **`main.c`**: Main application logic
  - Hardware initialization using `hw_init` component
  - `esp_lvgl_adapter` configuration and display registration
  - Input device setup (touch or encoder)
  - LVGL benchmark demo launch

- **`hw_init` component**: Hardware abstraction layer
  - LCD interface initialization
  - GPIO and peripheral configuration
  - Touch/encoder driver setup

## Key Features Demonstrated

1. **Unified LCD API**: Single codebase works with multiple LCD interface types
2. **LVGL Integration**: Proper initialization and task management for LVGL
3. **Input Handling**: Touch and encoder input device registration
4. **Performance Monitoring**: Optional FPS statistics using adapter features
5. **Thread Safety**: Mutex-based protection for LVGL API calls

## Troubleshooting

**Display shows nothing:**
- Verify LCD power and backlight connections
- Check GPIO pin mappings in menuconfig
- Ensure the correct LCD interface type is selected
- Verify LCD initialization sequence for your panel

**Touch/encoder not responding:**
- Check I2C/GPIO connections
- Enable input device in menuconfig
- Verify touch controller I2C address

**Build errors:**
- Ensure ESP-IDF version is 5.5.0 or later
- Run `idf.py fullclean` and rebuild
- Check that all managed components downloaded correctly

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.

