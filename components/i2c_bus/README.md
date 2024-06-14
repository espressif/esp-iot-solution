# Component: I2C BUS
[Online documentation](https://docs.espressif.com/projects/esp-iot-solution/en/latest/basic_component/i2c_bus.html)

The I2C bus component (Bus) is a set of application-layer code built on top of the ESP-IDF peripheral driver code, It is mainly used for bus communication between ESP chips and external devices. From the point of application development, this component has the following features:

1. Simplified peripheral initialization processes
2. Thread-safe device operations
3. Simple and flexible RW operations

This component abstracts the following concepts:

1. Bus: the resource and configuration option shared between devices during communication
2. Device: device specific resource and configuration option during communication

Each physical peripheral bus can mount one or more devices if the electrical condition allows, the I2C bus addressing devices based on their addresses, thus achieving software independence between different devices on the same bus.

## Add component to your project

Please use the component manager command `add-dependency` to add the `i2c_bus` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/i2c_bus=*"
```