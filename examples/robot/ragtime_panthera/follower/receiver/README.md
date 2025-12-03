# Ragtime Panthera Follower Receiver

This is a project for receiving [ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html) packets on the ESP32-C6 side.Since the current [ESP-HOSTED-MCU](https://github.com/espressif/esp-hosted-mcu) does not support ESP-NOW, the final ESP32-C6 firmware is downloaded to the ESP32-C6 chip on the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) development board using the [esp-serial-flasher](https://github.com/espressif/esp-serial-flasher).

In this project, only the checksum calculation for ESP-NOW data is performed to filter out valid data, with no protocol parsing involved. The parsing work is handled by the ESP32-P4.
