## USB Extension Screen Example

Try with [LaunchPad](https://espressif.github.io/esp-launchpad/?flashConfigURL=https://dl.espressif.com/AE/esp-iot-solution/config.toml)

The USB Extension Screen Example allows the P4 development board to be used as an additional screen for Windows. It supports the following features:

* Supports a screen refresh rate of 1024*600@60FPS
* Supports up to five touch points
* Supports audio input and output

## Required Hardware

* Development Board
  1. [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html#getting-started) development board
  2. A 1024*600 MIPI screen from the development kit
  3. A speaker

* Connections
  1. Connect the high-speed USB port on the development board to the PC

## Compilation and Flashing

### P4 Device Side

Build the project and flash it to the board, then run the monitor tool to view serial output:

* Run `. ./export.sh` to set up the IDF environment
* Run `idf.py set-target esp32p4` to set the target chip
* If there are any errors in the previous step, run `pip install "idf-component-manager~=1.1.4"` to upgrade your component manager
* Run `idf.py -p PORT flash monitor` to build, flash, and monitor the project

(To exit the serial monitor, press `Ctrl-]`.)

Refer to the Getting Started Guide for all steps to configure and use ESP-IDF to build projects.

> Note: This example will fetch an AVI file online. Ensure that you are connected to the internet during the initial compilation.

### PC Side

For preparation, refer to the [windows_driver](./windows_driver/README.md)

![Demo](https://dl.espressif.com/AE/esp-iot-solution/p4_usb_extern_screen.gif)

## Other Issues

### Touch Screen Does Not Control the P4 Screen

* In the Control Panel, select `Tablet PC Settings`
* In the configuration section, select `Setup`
* Follow the prompts to choose the P4 extension screen

### Adjusting JPEG Image Quality

* Modify the `string_desc_arr` vendor interface string in the `usb_descriptor.c` file. Change `Ejpg4` to the desired image quality level; the higher the number, the better the quality, but it will use more memory for the same frame.

### Modify Screen Resolution

- Update the `usb_descriptor.c` file by changing the `string_desc_arr` vendor interface string. Modify `R1024x600` to the desired screen resolution.

**Note**: Currently, the driver does not support portrait mode screens. Please use hardware that is designed for landscape orientation.
