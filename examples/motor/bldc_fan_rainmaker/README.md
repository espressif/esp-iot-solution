# bldc_fan_rainmaker

The `bldc_fan_rainmaker` example connects a brushless motor-driven fan to the ESP Rainmaker cloud, achieving the following functionalities:

* Stepless fan speed control
* Oscillation
* Natural wind mode
* Remote start and stop
* OTA updates
* BLE provisioning

![rainmaker_fan](https://dl.espressif.com/AE/esp-iot-solution/esp_bldc_rainmaker.gif)

## Component Overview

* [esp_sensorless_bldc_control](https://components.espressif.com/components/espressif/esp_sensorless_bldc_control) is a sensorless BLDC square wave control library based on the ESP32 series chips. It supports the following features:
    * Zero-crossing detection based on ADC sampling
    * Zero-crossing detection based on a comparator
    * Initial rotor position detection using pulse method
    * Stall protection
    * Overcurrent, over/under-voltage protection [feature]
    * Phase loss protection [feature]

* [esp_rainmaker](https://components.espressif.com/components/espressif/esp_rainmaker) is a complete and lightweight AIoT solution that enables private cloud deployment for your business in a simple, cost-effective, and efficient manner.

## Hardware Overview

You can click [here](https://dl.espressif.com/AE/esp-iot-solution/bldc_fan_rainmaker_sch.pdf) to download the hardware schematic. By using the corresponding buttons, you can control the fan's start and stop, rotate the stepper motor, enable natural wind mode, and reset Rainmaker.
