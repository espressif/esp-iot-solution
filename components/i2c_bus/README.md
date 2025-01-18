# Component: I2C BUS
[Online documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/basic/bus/i2c_bus.html)

The I2C bus component (Bus) is a set of application-layer code built on top of the ESP-IDF peripheral driver code, It is mainly used for bus communication between ESP chips and external devices. From the point of application development, this component has the following features:

1. Simplified peripheral initialization processes
2. Thread-safe device operations
3. Simple and flexible RW operations
4. Compatible with `driver/i2c` and `esp_driver_i2c`
5. Supports additional software I2C

This component abstracts the following concepts:

1. Bus: the resource and configuration option shared between devices during communication
2. Device: device specific resource and configuration option during communication

Each physical peripheral bus can mount one or more devices if the electrical condition allows, the I2C bus addressing devices based on their addresses, thus achieving software independence between different devices on the same bus.

## Add component to your project

Please use the component manager command `add-dependency` to add the `i2c_bus` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/i2c_bus=*"
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#define I2C_MASTER_SCL_IO   (gpio_num_t)15       /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO   (gpio_num_t)16       /*!< gpio number for I2C master data  */
#define I2C_MASTER_FREQ_HZ  100000               /*!< I2C master clock frequency */
#define ESP_SLAVE_ADDR      0x28                 /*!< ESP32 slave address, you can set any 7bit value */
#define DATA_LENGTH         64                   /*!<Data buffer length for test buffer*/

void app_main(void)
{

    uint8_t *data_wr = (uint8_t *)malloc(DATA_LENGTH);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_bus_handle_t i2c0_bus = i2c_bus_create(I2C_NUM_0, &conf);
    i2c_bus_device_handle_t i2c0_device1 = i2c_bus_device_create(i2c0_bus, ESP_SLAVE_ADDR, 0);
    for (int i = 0; i < DATA_LENGTH; i++)
    {
        data_wr[i] = i;
    }

    i2c_bus_write_bytes(i2c0_device1, NULL_I2C_MEM_ADDR, DATA_LENGTH, data_wr);
    free(data_wr);
    i2c_bus_device_delete(&i2c0_device1);
    i2c_bus_delete(&i2c0_bus);
}

```
