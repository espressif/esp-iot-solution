# Component: LIS2DH12

The LIS2DH12 is an ultra-low-power highperformance three-axis linear accelerometer belonging to the "femto" family with digital I2C/SPI serial interface standard output. The device may be configured to generate interrupt signals by detecting two independent inertial wake-up/free-fall events as well as by the position of the device itself.


## Add component to your project

Please use the component manager command `add-dependency` to add the `lis2dh12` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/lis2dh12=*"
```

## Example of LIS2DH12 usage

* This component will show you how to use I2C module read external i2c sensor data, here we use LIS2DH12, which is a 3-axis accelerometer.
* Pin assignment:
    * master:
        * GPIO2 is assigned as the data signal of i2c master port
        * GPIO1 is assigned as the clock signal of i2c master port
* Connection:
    * connect sda of sensor with GPIO1
    * connect scl of sensor with GPIO2
    * no need to add external pull-up resistors, driver will enable internal pull-up resistors.

