## Description

[![Component Registry](https://components.espressif.com/components/espressif/extended_vfs/badge.svg)](https://components.espressif.com/components/espressif/extended_vfs)

Extended VFS is designed to make applications read/write/configure peripherals by POSIX APIs, like open, close, read, write, ioctl and so on. This components supports following peripheral:

* GPIO
* I2C
* LEDC
* SPI

You can refer to [examples](https://github.com/espressif/esp-iot-solution/tree/master/examples/extended_vfs) to learn usage and more details.

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

A1. This is because an older version packege manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.
