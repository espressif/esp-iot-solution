# ChangeLog

## V0.0.1 - 2023-6-27

### Enhancements:
* FOC:
    * Add ESP-IDF driver for Arduino-FOC.
    * Fix mcpwm driver for esp-simplefoc.
    * Compatible with SimpleFOC Studio host computer.

## V0.0.2 - 2023-7-13
### Enhancements:
* FOC:
    * Remove the arduino-foc submodule and replace it with a component dependency.
    * Modify the Driver class of Arduino-FOC to support manual selection of pwm driver mode. If no PWM driver mode is specified, the system automatically applies for PWM driver.
    * Synchronize changes to test_apps.