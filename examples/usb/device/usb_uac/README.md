| Supported Targets | ESP32-P4 | ESP32-S3 | ESP32-S31 |
| ----------------- | -------- | -------- | --------- |

## USB UAC Example

This example demonstrates how to utilize the USB function of ESP32-Sx to implement a UAC (USB Audio Class) device.

1. Supports 2 channels of analog microphone output.
2. Supports 2 channels of speaker input.
3. Supports volume control and mute function.

For information about the [usb_device_uac](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_device/usb_device_uac.html) component.

## How to build the example

Please read the user guide for the selected development board to learn about its software and hardware information, especially the USB and audio connector descriptions.

### Hardware Required

This example is based on the [esp_board_manager](https://components.espressif.com/components/espressif/esp_board_manager) component. Any board supported by Board Manager that provides an audio codec device can be used, for example:

* An ESP32-S3-LCD-EV-Board development board (`esp32_s3_lcd_ev_board`)
* An ESP32-P4-Function-EV-Board development board (`esp32_p4_function_ev_board`)
* An [ESP32-S31-Korvo-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html) development board (`esp32_s31_korvo_1`, requires ESP-IDF v6.1 or later)

Additionally:

* USB cables suitable for power supply, programming, and USB communication. On ESP32-S31-Korvo-1, connect the computer to the onboard **USB 2.0 Type-A** port; the onboard USB Type-C ports are not USB-OTG ports.
* A speaker

### UAC Device Configuration

1. Using `idf.py menuconfig`, through `USB Device UAC` users can configure the num of SPK and MIC channels.

Note: This example supports up to two channels of microphone input.

### Example Output

After the programming is completed, insert the USB interface on the development board into the computer. An audio device will be displayed. At this point, connect the speaker to the development board to play music or record audio.

### Build and Flash

1. Make sure `ESP-IDF` is setup successfully

2. Set up the `ESP-IDF` environment variables, please refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can use:

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3. Select the development board through Board Manager. For example:

    ```bash
    idf.py bmgr -b esp32_s3_lcd_ev_board
    ```

    For ESP32-S31-Korvo-1, use ESP-IDF v6.1 or later and select:

    ```bash
    idf.py bmgr -b esp32_s31_korvo_1
    ```

    The board configuration files will be generated to `components/gen_bmgr_codes`. Use `idf.py bmgr -l` to list all supported boards, and `idf.py bmgr -x` to clear the current board configuration.

    > Note: For ESP-IDF versions earlier than v6.0, install `esp-bmgr-assist` in the IDF Python environment first (or configure `IDF_EXTRA_ACTIONS_PATH` manually) to enable the `idf.py bmgr` command. See the [esp_board_manager README](https://components.espressif.com/components/espressif/esp_board_manager) for details.

4. Build, Flash, output log

    ```bash
    idf.py build flash monitor
    ```
