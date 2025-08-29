## USB HID Device example

This example demonstrates how to use ESP32-S2/S3/P4 USB function as the HID device. Buttons are used to trigger such signals as a keyboard or mouse.

* Supports traditional six-key keyboard mode
* Supports full key no-conflict keyboard mode

## How to use the example

### Hardware Required

- Any ESP32-S2/S3/P4 development board with **buttons**

- Hardware Connection：

|              | USB_DP | USB_DM |
| ------------ | ------ | ------ |
| ESP32-S2/S3  | GPIO20 | GPIO19 |
| ESP32-P4 2.0 | pin 50 | pin 49 |
| ESP32-P4 1.1 | GPIO27 | GPIO26 |

### Configure the project

By default the buttons act as a mouse, you can use `idf.py menuconfig` change `USB HID Device Demo->hid class subclass` to `keyboard`.

![](https://dl.espressif.com/AE/esp-iot-solution/usb_hid_device_choose_subclass.png)

### Build and Flash

1. Make sure `ESP-IDF` is installed successfully

2. Set up the `ESP-IDF` environment variables， you can refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables), Linux can use:

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3. Set ESP-IDF build target to `esp32s2` or `esp32s3` or `esp32p4`

    ```bash
    idf.py set-target esp32s2
    ```

4. Build, Flash, output log

    ```bash
    idf.py build flash monitor
    ```

## Example Output

```
I (393) gpio: GPIO[14]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (403) HID Example: Button io = 14 created
I (403) HID Example: HID Mouse demo: press button to simulate mouse
I (413) HID Example: Wait Mount through USB interface
I (643) HID Example: USB Mount
I (4323) HID Example: Mouse x=0 y=-8
I (4813) HID Example: Mouse x=0 y=-8
I (5313) HID Example: Mouse x=0 y=-8
I (6093) HID Example: Mouse x=0 y=8
I (6663) HID Example: Mouse x=0 y=8
I (7103) HID Example: Mouse x=0 y=8
I (7563) HID Example: Mouse x=0 y=8
I (10403) HID Example: Mouse x=0 y=8
I (10823) HID Example: Mouse x=0 y=8
```
