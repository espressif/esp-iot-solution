# SHT3x

- This component will show you how to use I2C module read external i2c sensor data, here we use SHT3x-series temperature and humidity sensor(SHT30 is used this component).
- Pin assignment:
  - GPIO21 is assigned as the data signal of i2c master port
     - GPIO22 is assigned as the clock signal of i2c master port
   - Connection:
     * connect sda of sensor with GPIO21 
     * connect scl of sensor with GPIO22
- SHT3x measurement mode:
  * single shot data acquisition mode: in this mode one issued measurement command triggers the acquisition of one data pair.
  * periodic data acquisition mode: in this mode one issued measurement command yields a stream of data pairs. when use periodic data acquisition mode, you should send hex code 0xE000 firstly, and send 0x3093  to stop periodic mode.

# Notice

- SHT3x uses 8-bit CRC checksum,  you can see `CheckCrc8() `for detail.
- The raw measurement data needs to be convert to physical scale.  formulas are shown in `sht3x_get_humiture()`