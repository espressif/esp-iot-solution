# ChangeLog

## v0.1.1 - 2023-12-4
### Enhancements:
* FOC:
    * Change angle sensor driver with c++.

## v0.1.0 - 2023-11-4
### Enhancements:
* FOC:
    * Support chip whitch without mcpwm driver.

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


