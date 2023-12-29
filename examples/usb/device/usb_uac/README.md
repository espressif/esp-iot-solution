## USB UAC Example


This example demonstrates how to utilize the USB function of ESP32-Sx to implement a UAC (USB Audio Class) device.

1. Supports 1/2 channels of analog microphone output.
2. Supports a maximum of 8 channels of speaker input.
3. Supports volume control and mute function.

## How to build the example

- Any ESP32-S2 or ESP32-S3 development board with USB interface.

- USB Connection：
  - GPIO19 to D
  - GPIO20 to D+

### UAC Device Configuration

1. Using `idf.py menuconfig`, through `USB Device UAC` users can configure the num of SPK and MIC channels.

Note: This example supports up to two channels of microphone input.

### Build and Flash

1. Make sure `ESP-IDF` is setup successfully

2. Set up the `ESP-IDF` environment variables, please refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can use:

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3. Set ESP-IDF build target to `esp32s2` or `esp32s3`

    ```bash
    idf.py set-target esp32s3
    ```

4. Build, Flash, output log

    ```bash
    idf.py build flash monitor
    ```
