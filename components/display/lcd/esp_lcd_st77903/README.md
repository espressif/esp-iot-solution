# ESP LCD ST77903 QSPI

Implementation of the ST77903 LCD controller with [esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html) component.

| LCD controller | Communication interface | Component name |                                                                            Link to datasheet                                                                             |
| :------------: | :---------------------: | :------------: | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------: |
|     ST77903     |        QSPI/RGB         | esp_lcd_ST77903 | [PDF1](https://dl.espressif.com/AE/esp-iot-solution/ST77903_SPEC_P0.5.pdf), [PDF2](https://dl.espressif.com/AE/esp-iot-solution/ST77903_Customer_Application_Notes.pdf) |

## How to get the component

The QSPI driver for ST77903 relies on certain new features that have not been merged into the `ESP-IDF` on Github. Currently, this driver is available on the internal Glab repository. If you require access, please reach out to our business team or contact us at `sales@espressif.com` to obtain Glab permissions.
