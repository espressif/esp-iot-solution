| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-C3 |
| ----------------- | -------- | -------- | -------- |

# LVGL Dummy Draw Example

This example demonstrates how to use the **dummy draw mode** of the `esp_lvgl_adapter` component. Dummy draw mode allows you to switch the screen refresh control from LVGL to user application, enabling you to take over the display refresh handling directly.

## Overview

The example showcases:
- **Rendering mode switching**: Switch between LVGL rendering and direct LCD control
- **Touch interaction**: Simple tap-to-toggle interface
- **Color cycling demo**: Automatic color transitions (Red → Green → Blue) in dummy mode
- **Minimal UI**: Clean design focusing on the core feature demonstration

## What is Dummy Draw Mode?

Dummy draw mode lets you bypass LVGL and write directly to the LCD hardware:

**How it works:**
1. Call `esp_lv_adapter_set_dummy_draw(true)` to disable LVGL rendering
2. Prepare your framebuffer content
3. Use `esp_lv_adapter_dummy_draw_blit()` to write directly to LCD
4. Call `esp_lv_adapter_set_dummy_draw(false)` to restore LVGL control

**Use cases:**
- Video playback - Write video frames without LVGL overhead
- Camera preview - Stream camera data directly to screen
- High-speed visualization - Custom rendering for oscilloscopes, analyzers
- Boot screens - Display content before LVGL initialization
- Mixed rendering - Combine LVGL UI with custom graphics

**Important:**
- LVGL rendering stops while dummy mode is active
- You must manually manage all display updates
- Color format must match LCD native format (typically RGB565)
- Thread-safe switching is required

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

This example uses the `hw_init` component for hardware abstraction. Refer to your board's schematic for specific GPIO connections.

**Typical LCD interfaces:**
- **MIPI DSI**: Dedicated MIPI lanes
- **RGB**: Parallel data bus + sync signals
- **QSPI**: 4 data lines + control signals
- **SPI**: Standard SPI interface

**Input devices:**
- Touch panel: Usually I2C or SPI
- Rotary encoder: 3 GPIO pins (A, B, button)

### Configure the Project

Run `idf.py menuconfig` and configure:

**Example Configuration:**
- LCD Interface - Select MIPI DSI / RGB / QSPI / SPI
- Display Rotation - Choose 0° / 90° / 180° / 270°
- Input Device - Enable touch panel or encoder if available

**Performance (Optional):**
- Enable FPS statistics to monitor rendering performance

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

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Expected Output

**Initial Screen (LVGL Mode):**
```
┌─────────────────────────┐
│                         │
│     Tap to start        │
│       800x480          │  ← Display resolution
│                         │
└─────────────────────────┘
```
Tap anywhere on the screen to enter Dummy Draw mode.

**Dummy Draw Mode:**
```
┌─────────────────────────┐
│                         │
│    Red (0xF800)        │  ← Current color info
│                         │
│   [Solid color fills    │
│    entire screen]       │
│                         │
└─────────────────────────┘
```
The screen cycles through solid colors automatically. Tap to exit back to LVGL mode.

**Serial Console:**
```
I (xxx) main: LCD: RGB
I (xxx) main: Init LCD: 800x480
I (xxx) main: UI created for 800x480
I (xxx) main: [Dummy] Enabled
I (xxx) main: [Dummy] Buffer allocated: 800x480
I (xxx) main: [Demo] Color cycle started
I (xxx) main: [Demo] Red (0xF800)
I (xxx) main: [Demo] Green (0x07E0)
I (xxx) main: [Demo] Blue (0x001F)
I (xxx) main: [Demo] Stopped
I (xxx) main: [Dummy] Disabled
```

## Code Structure

The code is organized into clear functional blocks:

**1. Dummy Draw Core Functions** - Direct LCD control wrapper
```c
dummy_draw_enable()          // Switch from LVGL to direct LCD control
dummy_draw_disable()         // Return control to LVGL
dummy_draw_fill_screen()     // Fill framebuffer with color
dummy_draw_update_display()  // Blit buffer to LCD hardware
```

**2. Demo Task** - Color cycling demonstration
```c
dummy_draw_cycle_colors_task()  // Cycles through Red/Green/Blue
```

**3. Mode Switching** - High-level mode management
```c
enter_dummy_mode()   // Enable dummy draw + start demo task
exit_dummy_mode()    // Disable dummy draw mode
```

**4. UI & Events** - User interaction
```c
create_control_ui()        // Create minimal UI (label + touch layer)
screen_touch_event_cb()    // Handle tap to toggle modes
```

**Key ESP-IDF APIs Used:**
- `esp_lv_adapter_set_dummy_draw()` - Enable/disable dummy mode
- `esp_lv_adapter_get_dummy_draw_enabled()` - Query current mode
- `esp_lv_adapter_dummy_draw_blit()` - Direct LCD write (bypasses LVGL)
- `heap_caps_malloc()` - Allocate framebuffer (PSRAM if available)

## Troubleshooting

**Screen stays black after entering dummy mode:**
- Check that framebuffer allocation succeeded (serial logs)
- Verify LCD interface is properly initialized
- Ensure RGB565 color format is correct

**Cannot allocate framebuffer:**
- Enable PSRAM in menuconfig (for large displays)
- Reduce display resolution if PSRAM unavailable
- Check available heap memory

**Colors appear incorrect:**
- Check byte order (big-endian vs little-endian)
- Consult LCD datasheet for native color format

**Touch not responding:**
- Verify touch panel is functioning (check serial logs during touch init)
- Try tapping center of screen
- For encoder: use the toggle button at bottom of screen

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.

