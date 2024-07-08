## Knob Power Save Example

This example demonstrates how to utilize the `knob` component in conjunction with the light sleep low-power mode.

* `knob` [Component Introduction](https://docs.espressif.com/projects/esp-iot-solution/en/latest/input_device/knob.html)

## Hardware

Any GPIO on any development board can be used in this example, for the default GPIO, please refer to the table below.

|      Hardware       | GPIO  |
| :-----------------: | :---: |
| Encoder A (Phase A) | GPIO1 |
| Encoder B (Phase B) | GPIO2 |

## Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

* Run `. ./export.sh` to set IDF environment
* Run `idf.py set-target esp32xx` to set target chip
* Run `idf.py -p PORT flash monitor` to build, flash and monitor the project

(To exit the serial monitor, type `Ctrl-]`.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

## Example Output

```
I (316) pm: Frequency switching config: CPU_MAX: 80, APB_MAX: 80, APB_MIN: 10, Light sleep: ENABLED
I (322) sleep: Code start at 0x42000020, total 106515, data start at 0x3c020020, total 46384 Bytes
I (331) gpio: GPIO[1]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (341) gpio: GPIO[2]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (350) Knob: Iot Knob Config Succeed, encoder A:1, encoder B:2, direction:0, Version: 0.1.4
I (359) main_task: Returned from app_main()
I (1503) knob_power_save: knob event KNOB_LEFT, -1
I (1503) knob_power_save: Wake up from light sleep, reason 7
I (1691) knob_power_save: knob event KNOB_LEFT, -2
I (1691) knob_power_save: Wake up from light sleep, reason 7
I (1860) knob_power_save: knob event KNOB_LEFT, -3
I (1860) knob_power_save: Wake up from light sleep, reason 7
I (1940) knob_power_save: knob event KNOB_LEFT, -4
I (1940) knob_power_save: Wake up from light sleep, reason 7
I (2017) knob_power_save: knob event KNOB_LEFT, -5
I (2017) knob_power_save: Wake up from light sleep, reason 7
```