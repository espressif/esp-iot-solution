[![Component Registry](https://components.espressif.com/components/espressif/as5600/badge.svg)](https://components.espressif.com/components/espressif/as5600)

# Component: AS5600

The AS5600 is a 12-bit magnetic angle sensor from ams OSRAM. It outputs a 0-360 degree position value over I2C and is suitable for knobs, rotary encoders, and motor position sensing.

## Features

- 12-bit angle resolution
- I2C interface, default 7-bit address `0x36`
- Raw angle, degree, and radian output helpers
- Magnet presence and strength status detection

## Add component to your project

```bash
idf.py add-dependency "espressif/as5600=*"
```

## Example of AS5600 usage

This driver uses the ESP-IDF v5 I2C master bus API in `driver/i2c_master.h`.

```c
#include "as5600.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_FREQ_HZ  100000

i2c_master_bus_handle_t bus_handle = NULL;
as5600_handle_t as5600 = NULL;

i2c_master_bus_config_t bus_config = {
    .i2c_port = I2C_MASTER_NUM,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

as5600_i2c_config_t as5600_config = {
    .scl_speed_hz = I2C_MASTER_FREQ_HZ
};
ESP_ERROR_CHECK(as5600_new_sensor(bus_handle, &as5600_config, &as5600));

uint16_t raw_angle = 0;
float degrees = 0.0f;
as5600_magnet_status_t status;

ESP_ERROR_CHECK(as5600_get_angle_raw(as5600, &raw_angle));
ESP_ERROR_CHECK(as5600_get_angle_degrees(as5600, &degrees));
ESP_ERROR_CHECK(as5600_get_magnet_status(as5600, &status));

ESP_ERROR_CHECK(as5600_del_sensor(as5600));
ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
```

## API overview

| Function | Description |
|----------|-------------|
| `as5600_new_sensor()` | Create a sensor handle on an I2C bus from device configuration |
| `as5600_del_sensor()` | Delete the sensor handle and release the I2C device |
| `as5600_get_angle_raw()` | Read the raw 12-bit angle value |
| `as5600_get_angle_degrees()` | Read the angle in degrees |
| `as5600_get_angle_radians()` | Read the angle in radians |
| `as5600_get_magnet_status()` | Read the magnet status |

In `as5600_i2c_config_t`, both `scl_speed_hz` and `timeout_ms` are configurable. Setting either field to `0` uses the default value.

## Reference

- [AS5600 datasheet](https://ams-osram.com/products/sensor-solutions/position-sensors/ams-as5600-position-sensor)
