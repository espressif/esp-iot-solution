## Button Power Save Example

This example demonstrates how to utilize the `button` component in conjunction with the light sleep low-power mode.

* `button` [Component Introduction](https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/button.html)

## Hardware

* Any GPIO on any development board can be used in this example.

## Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

* Run `. ./export.sh` to set IDF environment
* Run `idf.py set-target esp32xx` to set target chip
* Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

## Example Output

```
I (1139) pm: Frequency switching config: CPU_MAX: 160, APB_MAX: 80, APB_MIN: 80, Light sleep: ENABLED
I (1149) sleep: Code start at 42000020, total 119.03 KiB, data start at 3c000000, total 49152.00 KiB
I (1159) button: IoT Button Version: 3.2.0
I (1163) gpio: GPIO[0]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (2922) button_power_save: Button event BUTTON_PRESS_DOWN
I (3017) button_power_save: Button event BUTTON_PRESS_UP
I (3017) button_power_save: Wake up from light sleep, reason 4
I (3200) button_power_save: Button event BUTTON_SINGLE_CLICK
I (3200) button_power_save: Wake up from light sleep, reason 4
I (3202) button_power_save: Button event BUTTON_PRESS_REPEAT_DONE
I (3208) button_power_save: Wake up from light sleep, reason 4
I (3627) button_power_save: Button event BUTTON_PRESS_DOWN
I (3702) button_power_save: Button event BUTTON_PRESS_UP
I (3702) button_power_save: Wake up from light sleep, reason 4
I (3887) button_power_save: Button event BUTTON_SINGLE_CLICK
I (3887) button_power_save: Wake up from light sleep, reason 4
I (3889) button_power_save: Button event BUTTON_PRESS_REPEAT_DONE
```