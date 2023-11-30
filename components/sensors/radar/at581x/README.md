[![Component Registry](https://components.espressif.com/components/espressif/at581x/badge.svg)](https://components.espressif.com/components/espressif/at581x)

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# Component: AT581X
I2C driver and definition of AT581X radar sensor.

See [AT581X datasheet](https://dl.espressif.com/AE/esp_iot_solution/MS58-3909S68U4-3V3-G-NLS-IIC%20Data%20Sheet%20V1.0.pdf).

## Operation modes
After calling `at581x_new_sensor()` the user is responsible for reading detecting status from IO.

> Note: The user is responsible for initialization and configuration of I2C bus.
