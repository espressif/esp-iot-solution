## Description

[![Component Registry](https://components.espressif.com/components/espressif/extended_vfs/badge.svg)](https://components.espressif.com/components/espressif/extended_vfs)

Extended VFS is designed to make applications read/write/configure peripherals by POSIX APIs, like open, close, read, write, ioctl and so on. This components supports following peripheral:

* GPIO
* I2C
* LEDC
* SPI

You can refer to [examples](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs) to learn usage and more details.

### I2C capability (ESP-IDF version)

| Mode | ESP-IDF &lt; 5.3 | ESP-IDF 5.3.x | ESP-IDF 5.4 / 5.5 | ESP-IDF &gt;= 6.0 |
| --- | --- | --- | --- | --- |
| Master | Legacy `driver/i2c` | New `esp_driver_i2c` | New `esp_driver_i2c` | New `esp_driver_i2c` |
| Slave | Legacy `driver/i2c` (RX/TX buffer depth required; fixed in 0.4.0) | Not available (`I2CIOCSCFG` / transfer → `ENOTSUP`; no slave driver v2) | Enable `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2` | Supported (slave driver v2) |

Notes:

* Custom transaction flags (`I2C_MSG_NO_START` / `I2C_MSG_NO_END`) and zero-length write probe need IDF >= 5.3.4 (5.3.x), >= 5.4.1 (5.4.x), or >= 5.5.0; earlier patches return `ENOTSUP`.
* IDF 5.3.x has no workable VFS I2C slave path (no slave driver v2; do not mix legacy `driver/i2c` with the 5.3+ master API). Upgrade to IDF >= 5.4 with `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2`, or IDF >= 6.0. Pinning `extended_vfs` &lt; 0.4.0 does **not** restore slave: older releases called `i2c_driver_install(..., 0, 0, 0)`, which IDF rejects for slave mode.

### Add component to your project

Please use the component manager command `add-dependency` to add the `extended_vfs` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/extended_vfs=*"
```

### Examples

Please use the component manager command `create-project-from-example` to create the project from example template

```
idf.py create-project-from-example "espressif/extended_vfs=*:gpio_simple"
```

Then the example `gpio_simple` will be downloaded in current folder, you can check into it for build and flash.

> Or you can download examples from esp-iot-solution repository:
1. [gpio_simple](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs/gpio/gpio_simple)
2. [i2c_bh1750](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs/i2c/i2c_bh1750)
3. [i2c_tt21100](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs/i2c/i2c_tt21100)
4. [ledc_simple](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs/ledc/ledc_simple)
5. [spi_master_simple](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs/spi/spi_master_simple)
6. [spi_slave_simple](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs/spi/spi_slave_simple)

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.
