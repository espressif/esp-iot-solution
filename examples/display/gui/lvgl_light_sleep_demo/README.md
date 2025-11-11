| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

# LVGL Light Sleep Demo

This example demonstrates how to use `esp_lvgl_adapter` with ESP-IDF Light Sleep to achieve low-power standby while preserving UI state. The demo shows the complete sleep cycle: preparing the adapter, releasing hardware resources, entering light sleep, and recovering after wake-up.

## Overview

The example showcases:
- **Light sleep integration**: Complete workflow for entering and recovering from light sleep
- **UI state preservation**: LVGL widgets remain intact after sleep/wake cycles
- **Hardware resource management**: Proper release and reinitialization of LCD hardware
- **Wake statistics**: Displays sleep duration and wake cause information
- **Simple interaction**: Button-triggered sleep cycle for easy testing

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

Run `idf.py menuconfig` and navigate to `LVGL Light Sleep Demo Configuration`:

**Sleep Settings:**
- `Light sleep wake timer (ms)`: Duration before automatic wake-up (default: 3000ms)

**Example Configuration:**
- `LCD Interface Type`: Choose between MIPI DSI, RGB, QSPI, or SPI
- `Display Rotation`: Select screen orientation (0°/90°/180°/270°)
- Input device selection (touch panel or encoder)

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

After flashing, the LCD displays a simple UI with:
- **Status label**: Shows wake timer configuration and wake statistics
- **Sleep button**: "Sleep Now" button to trigger sleep cycle

**Pressing the button triggers the following sequence:**

1. `esp_lv_adapter_sleep_prepare()` - Pauses adapter worker and waits for flush completion
2. `hw_lcd_deinit()` - Releases LCD hardware resources (panel, bus, backlight)
3. Configure wake-up timer
4. `esp_light_sleep_start()` - Enters light sleep mode
5. After wake-up, reinitialize LCD hardware
6. `esp_lv_adapter_sleep_recover()` - Rebinds panel and resumes adapter worker

**Serial Console:**
```
I (xxx) main: LVGL Light Sleep Demo
I (xxx) main: LCD resolution 800x480
I (xxx) main: Sleep button pressed, entering light sleep...
I (xxx) main: Step 1: Preparing adapter for sleep
I (xxx) main: Step 2: Deinitializing LCD hardware
I (xxx) main: Step 4: Entering light sleep for 3000 ms
I (xxx) main: Step 5: Reinitializing LCD hardware after wake-up
I (xxx) main: Step 6: Recovering adapter
I (xxx) main: Wake #1, cause 4, duration 3.00 s
```

The UI remains intact after each sleep/wake cycle, and the status label updates to show wake statistics.

## Code Structure

**Main Components:**

1. **Sleep Cycle** (`enter_light_sleep()`):
   - Prepares adapter for sleep
   - Deinitializes LCD hardware
   - Configures wake-up timer
   - Enters light sleep
   - Reinitializes hardware after wake-up
   - Recovers adapter

2. **UI Creation** (`create_demo_ui()`):
   - Creates status label and sleep button
   - Sets up button event handler

3. **Initialization** (`app_main()`):
   - Initializes LCD hardware
   - Configures and starts adapter
   - Registers display and input devices
   - Creates UI

**Key ESP-IDF APIs Used:**
- `esp_lv_adapter_sleep_prepare()` - Prepare adapter for sleep
- `esp_lv_adapter_sleep_recover()` - Recover adapter after wake-up
- `esp_light_sleep_start()` - Enter light sleep mode
- `hw_lcd_deinit()` / `hw_lcd_init()` - Hardware resource management

## Troubleshooting

**Display not working after wake-up:**
- Verify LCD hardware reinitialization succeeded (check serial logs)
- Ensure panel handles are properly passed to `esp_lv_adapter_sleep_recover()`
- Check that wake-up timer is configured correctly

**UI state lost after sleep:**
- Verify `esp_lv_adapter_sleep_prepare()` is called before hardware deinit
- Ensure `esp_lv_adapter_sleep_recover()` is called after hardware reinit
- Check that display handle is preserved across sleep cycles

**Sleep not entering:**
- Verify wake-up source is configured before `esp_light_sleep_start()`
- Check serial logs for error messages during sleep preparation
- Ensure no blocking operations are running during sleep entry

**Build errors:**
- Ensure ESP-IDF version is 5.5.0 or later
- Run `idf.py fullclean` and rebuild
- Check that all managed components downloaded correctly

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
