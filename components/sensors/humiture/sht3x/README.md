# SHT3x

SHT3x-DIS is the next generation of Sensirion’s temperature and humidity sensors. It builds on a new CMOSens® sensor chip that is at the heart of Sensirion’s new humidity and temperature platform. The SHT3x-DIS has increased intelligence, reliability and improved accuracy specifications compared to its predecessor. Its functionality includes enhanced signal processing, two distinctive and user selectable I2C addresses and communication speeds of up to 1 MHz. 

- This component will show you how to use I2C module read external i2c sensor data, here we use SHT3x-series temperature and humidity sensor(SHT30 is used this component).
  - Pin assignment:
     * GPIO1 is assigned as the data signal of i2c master port
     * GPIO2 is assigned as the clock signal of i2c master port
   - Connection:
     * connect sda of sensor with GPIO1 
     * connect scl of sensor with GPIO2
- SHT3x measurement mode:
  * single shot data acquisition mode: in this mode one issued measurement command triggers the acquisition of one data pair.
  * periodic data acquisition mode: in this mode one issued measurement command yields a stream of data pairs. when use periodic data acquisition mode, you should send hex code 0xE000 firstly, and send 0x3093  to stop periodic mode.

## Add component to your project

Please use the component manager command `add-dependency` to add the `sht3x` to your project's dependency, during the `CMake` step the component will be downloaded automatically

```
idf.py add-dependency "espressif/sht3x=*"
```

## Notice

- SHT3x uses 8-bit CRC checksum,  you can see `CheckCrc8() `for detail.
- The raw measurement data needs to be convert to physical scale.  formulas are shown in `sht3x_get_humiture()`