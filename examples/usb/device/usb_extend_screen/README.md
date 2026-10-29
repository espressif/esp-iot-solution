## USB Extended Display Example

Use [LaunchPad](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml) to flash this example.

The USB extended display example turns a compatible ESP32-P4, ESP32-S31, or ESP32-S3 board into a secondary display for Windows. Board peripherals are described and initialized by [ESP Board Manager](https://github.com/espressif/esp-board-manager), so the application is not tied to a specific BSP.

This example uses Board Manager board definitions directly and accesses board peripherals through the agreed device names. A compatible board must provide a landscape `display_lcd` device with an RGB565 or RGB888 framebuffer. Touch requires `lcd_touch`; USB audio requires `audio_dac` and `audio_adc`; backlight control is enabled automatically when `lcd_brightness` exists. Custom Board Manager board definitions should use the same device names.

The example supports the following features:

* **P4**: Supports a screen refresh rate of **1024×600@60FPS**.
* **S3**: Supports a screen refresh rate of **800×480@13FPS** or **480×480**, depending on the LCD sub-board.
* **S31**: Supports a screen refresh rate of **800×480@60FPS**.
* Supports up to **five-point touch input**.
* Supports **audio input and output**.

## Required Hardware

### P4 Development Board

1. [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html#getting-started) development board.
2. A **1024×600** MIPI display from the development kit.
3. A speaker.

### S3 Development Board

1. [ESP32-S3-LCD-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html#getting-started) development board.
2. A **800×480** or **480×480** RGB display from the development kit.
3. A speaker.

### S31 Development Board

1. [ESP32-S31-Korvo](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html#getting-started) development board.
2. A **800×480** RGB display from the development kit.
3. A speaker.

## Hardware Connection

1. Connect the high-speed USB port on the development board to the PC.

## Compilation and Flashing

### Device Side

Build the project, flash it to the board, and run the monitor tool to check the serial output:

1. Run `. ./export.sh` to set up the IDF environment.
2. Install the Board Manager action helper once in the active ESP-IDF Python environment:

   ```bash
   pip install esp-bmgr-assist
   ```

3. Enter this example directory, list the available boards, and generate the selected board configuration:

   ```bash
   cd examples/usb/device/usb_extend_screen
   idf.py bmgr -l

   # Choose one:
   idf.py bmgr -b esp32_p4_function_ev_board
   idf.py bmgr -b esp32_s31_korvo_1
   idf.py bmgr -b esp32_s3_lcd_ev_board
   ```

   `bmgr` selects the target automatically and generates `components/gen_bmgr_codes`. Do not run `idf.py set-target` separately.

4. For the ESP32-S3-LCD-EV-Board 800×480 sub-board, apply its official amend instead:

   ```bash
   idf.py bmgr -b esp32_s3_lcd_ev_board -a sub_board_800_480_lcd
   ```

   The base board definition uses 480×480. The amend replaces `display_lcd` and `lcd_touch` with the 800×480 sub-board definitions.

5. Run `idf.py -p PORT flash monitor` to build, flash, and monitor the project.

(To exit the serial monitor, press `Ctrl-]`.)

Refer to the **Getting Started Guide** for full instructions on configuring and using ESP-IDF to build projects.

> **Note:** The first configuration/build downloads managed components. Ensure an internet connection is available.

When switching boards, use:

```bash
idf.py bmgr -x
idf.py fullclean
idf.py bmgr -b <new_board_name>
```

If the old `sdkconfig` remains, regenerate it so the new board defaults take effect.

### PC Side

For preparation, refer to [windows_driver](./windows_driver/README_cn.md).

![Demo](https://dl.espressif.com/AE/esp-iot-solution/p4_usb_extern_screen.gif)

## Troubleshooting

### The touchscreen controls the wrong display

1. Open **Control Panel** and select **Tablet PC Settings**.
2. Under the **Display** section, select **Setup**.
3. Follow the on-screen instructions to choose the correct extended display.

### Adjusting JPEG Image Quality

* Modify `CONFIG_USB_EXTEND_SCREEN_JPEG_QUALITY`. A higher value increases image quality but also consumes more memory per frame.

### Changing the Secondary Screen Resolution

* The resolution and RGB565/RGB888 framebuffer format come from the selected board's `display_lcd` configuration; they are also used to build the USB vendor string, HID coordinate range, frame validation, and JPEG output buffers.
* To use another panel, select a matching board definition or apply/create a Board Manager amend, then run `idf.py bmgr` again.

**Note:** The driver currently does not support portrait-oriented screens. Please use a screen designed for landscape mode.

### ESP32-P4 Chip Revisions and JPEG Alignment

* ESP-IDF firmware built for ESP32-P4 revisions `<3.0` and `>=3.0` is mutually incompatible. Board Manager selects the board and target, but it does not select the silicon revision. For a `<3.0` chip, enable `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` with `idf.py menuconfig` and perform a clean rebuild; do not put this setting in generic board defaults.
* Hardware JPEG output is rounded to the input JPEG's MCU size: 8×8 for YUV444, 16×8 for YUV422, and 16×16 for YUV420. The example parses each JPEG header, reserves worst-case 16×16-aligned storage, and removes row padding before drawing. A visible resolution such as 1024×600 therefore does not need to be changed to 1024×608.
* Revisions `<3.0` also restrict some YUV-to-YUV conversions. This example decodes directly to the display's RGB565 or RGB888 framebuffer format and does not use those restricted combinations.

### Adjusting Image Output Frame Rate

* Modify `CONFIG_USB_EXTEND_SCREEN_MAX_FPS`. Lowering this value effectively reduces USB bandwidth usage. If USB audio stuttering occurs, consider decreasing this value.

### Modifying the Maximum Frame Size

* Modify `CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B` to limit the maximum image size received from the PC driver.

### Disabling Touchscreen Functionality

* Set `CONFIG_HID_TOUCH_ENABLE` to `n`.
* This option is only available when the selected board provides `lcd_touch`.

### Disabling Audio Functionality

* Set `CONFIG_UAC_AUDIO_ENABLE` to `n`.
* This option is only available when the selected board provides Board Manager audio codec devices.

**Note:** If only the secondary screen function is enabled, change the PID to `0x2987`.
