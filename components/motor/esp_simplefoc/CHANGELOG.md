# ChangeLog

## v1.2.0 - 2024-12-31
### Enhancements:
* FOC:
  *  The angle sensor initializes I2C using the ``i2c_bus`` component to support both new and old I2C driver.

### Bug Fix:
* FOC:
  *  Fix the issue where the pins used by the SPI sensor in TEST_APPS exceed the pin range of the ESP32.

## v1.1.0 - 2024-11-26
### Enhancements:
* FOC:
    * Support 6PWM driver. Note that only chips that support ``MCPWM`` can use 6PWM driver.

## v1.0.0 - 2024-9-20
### Enhancements:
* FOC:
    * MT6701 encoder supports I2C mode.
    * Modify the ledc source clock of ESP32C6 and ESP32H2 to auto.
    * Version maintenance.

## v0.2.0 - 2024-7-5
### Enhancements:
* FOC:
    * Support for as5048a encoder added

## v0.1.2 - 2023-12-11
### Bug Fix:
* FOC:
    * Fix mcpwm's comparator configuration not initializing interrupt priority.

## v0.1.1 - 2023-12-4
### Enhancements:
* FOC:
    * Change angle sensor driver with c++.

## v0.1.0 - 2023-11-4
### Enhancements:
* FOC:
    * Support chip which without mcpwm driver.

## V0.0.3 - 2023-8-16
### Enhancements:
* FOC:
    * Fix int type to uart_port_t type conversion.

## V0.0.2 - 2023-7-13
### Enhancements:
* FOC:
    * Remove the arduino-foc submodule and replace it with a component dependency.
    * Modify the Driver class of Arduino-FOC to support manual selection of pwm driver mode. If no PWM driver mode is specified, the system automatically applies for PWM driver.
    * Synchronize changes to test_apps.

## V0.0.1 - 2023-6-27
### Enhancements:
* FOC:
    * Add ESP-IDF driver for Arduino-FOC.
    * Fix mcpwm driver for esp-simplefoc.
    * Compatible with SimpleFOC Studio host computer.


