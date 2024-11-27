## USB UAC Example

This example demonstrates how to utilize the USB function of ESP32-Sx to implement a UAC (USB Audio Class) device.

1. Supports 2 channels of analog microphone output.
2. Supports 2 channels of speaker input.
3. Supports volume control and mute function.

For information about the [usb_device_uac](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_device/usb_device_uac.html) component.

## How to build the example

Please first read the [User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-lcd-ev-board/user_guide.html#esp32-s3-lcd-ev-board) of the ESP32-S3-LCD-EV-Board to learn about its software and hardware information.

### Hardware Required

For ESP32-S3

* An ESP32-S3-LCD-EV-Board development board
* At least one USB Type-C cable for Power supply, programming and USB communication.
* A speaker

For ESP32-P4

* An ESP32_P4_FUNCTION_EV_BOARD development board
* At least one USB Type-C cable for Power supply, programming and USB communication.
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

3. Set ESP-IDF build target to `esp32s3` or `esp32-p4`

    ```bash
    idf.py set-target esp32s3
    ```

4. Build, Flash, output log

    ```bash
    idf.py build flash monitor
    ```
