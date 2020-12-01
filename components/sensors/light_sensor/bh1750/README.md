# Component: BH1750

* This component will show you how to use I2C module read external i2c sensor data, here we use BH1750 light sensor(GY-30 module).
* Pin assignment:
 * master:
        * GPIO18 is assigned as the data signal of i2c master port
        * GPIO19 is assigned as the clock signal of i2c master port
* Connection:
    * connect sda of sensor with GPIO18  
    * connect scl of sensor with GPIO19
    * no need to add external pull-up resistors, driver will enable internal pull-up resistors.
* Bh1750 measurement mode:
    * one-time mode: bh1750 just measure only one time when receieved the one time measurement command, so you need to send this command when you want to get intensity value every time
    * continuouly mode: bh1750 will measure continuouly when receieved the continuously measurement command, so you just need to send this command once, and than call `iot_bh1750_get_data()` to get intensity value repeatedly.
* You can aslo call `iot_bh1750_get_light_intensity()` to get intensity value, the working process of this function is as followed:
    * send measurement command
    * delay longer than measurement time
    * get intensity value
# Notice:
* Bh1750 has different measurement time in different measurement mode, and also, measurement time can be changed by call `iot_bh1750_change_measure_time()`