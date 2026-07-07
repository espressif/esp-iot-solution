## USB UAC I2S Example

This example demonstrates how to utilize the USB function of ESP32-Sx to implement a UAC (USB Audio Class) device with an I2S PDM microphone and I2S PCM amplifier

1. Supports 2 channels of analog microphone output.
2. Supports 2 channels of speaker input.
3. Supports volume control and mute function.

For information about the [usb_device_uac](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_device/usb_device_uac.html) component.

## How to build the example

### Hardware Required

For ESP32-S3

* An ESP32-S3 development board
* At least one USB Type-C cable for Power supply, programming and USB communication.
* An I2S PDM microphone
* An I2S PCM amplifier

### UAC Device Configuration

1. Using `idf.py menuconfig`, through `USB Device UAC` users can configure the num of SPK and MIC channels.

Note: This example supports one channel of microphone input.

### Example Output

After the programming is completed, insert the USB interface on the development board into the computer. An audio device will be displayed. Select this device for recording or for playback.

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
