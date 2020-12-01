# Component: LIS2DH12
* This component will show you how to use I2C module read external i2c sensor data, here we use LIS2DH12, which is a 3-axis accelerometer.
* Pin assignment:
    * master:
        * GPIO18 is assigned as the data signal of i2c master port
        * GPIO19 is assigned as the clock signal of i2c master port
* Connection:
    * connect sda of sensor with GPIO18  
    * connect scl of sensor with GPIO19
    * no need to add external pull-up resistors, driver will enable internal pull-up resistors.

