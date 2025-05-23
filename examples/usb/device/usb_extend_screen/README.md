## USB Extended Display Example

Use [LaunchPad](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml) to flash this example.

The USB extended display example allows the [ESP32_S3_LCD_EV_BOARD](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/index.html) / [ESP32_P4_FUNCTION_EV_BOARD](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) development boards to function as a secondary display for Windows. It supports the following features:

* **P4**: Supports a screen refresh rate of **1024×576@60FPS**.
* **S3**: Supports a screen refresh rate of **800×480@13FPS**.
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

## Hardware Connection

1. Connect the high-speed USB port on the development board to the PC.

## Compilation and Flashing

### Device Side

Build the project, flash it to the board, and run the monitor tool to check the serial output:

1. Run `. ./export.sh` to set up the IDF environment.
2. Run `idf.py set-target esp32p4` to set the target chip.
3. If you encounter any errors in the previous step, run `pip install "idf-component-manager~=1.1.4"` to upgrade the component manager.
4. Run `idf.py -p PORT flash monitor` to build, flash, and monitor the project.

(To exit the serial monitor, press `Ctrl-]`.)

Refer to the **Getting Started Guide** for full instructions on configuring and using ESP-IDF to build projects.

>> **Note:** This example fetches AVI files online. Ensure you have an internet connection during the first compilation.

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

* Modify `CONFIG_USB_EXTEND_SCREEN_HEIGHT` and `CONFIG_USB_EXTEND_SCREEN_WIDTH`.

**Note:** The driver currently does not support portrait-oriented screens. Please use a screen designed for landscape mode.

### Adjusting Image Output Frame Rate

* Modify `CONFIG_USB_EXTEND_SCREEN_MAX_FPS`. Lowering this value effectively reduces USB bandwidth usage. If USB audio stuttering occurs, consider decreasing this value.

### Modifying the Maximum Frame Size

* Modify `CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B` to limit the maximum image size received from the PC driver.

### Disabling Touchscreen Functionality

* Set `CONFIG_HID_TOUCH_ENABLE` to `n`.

### Disabling Audio Functionality

* Set `CONFIG_UAC_AUDIO_ENABLE` to `n`.

**Note:** If only the secondary screen function is enabled, change the PID to `0x2987`.
