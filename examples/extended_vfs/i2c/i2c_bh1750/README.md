## I2C BH1750 Example

This example shows how to configure I2C bus and read data from BH1750(light intensity sensor) by POSIX APIs, this decreases dependence on hardware and platform.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board based on espressif SoC   
* A USB cable for power supply and programming
* A BH1750 connecting to your configured GPIO, Power and GND, if your development board doesn't integrate BH1750 senor

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Set the I2C GPIO number used for the signal in the following option:
    - I2C SDA Pin Number
    - I2C SCL Pin Number
* Set the reading sensor data count in the `I2C Testing Count of Reading Sensor's Data` option.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

As you run the example, you will see the light intensity value from following log:

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (328) ext_vfs: Extended VFS version: 0.1.0
Opening device /dev/i2c/0 for writing OK, fd=3.
Sensor val: 190.00 [Lux].
Sensor val: 173.33 [Lux].
Sensor val: 173.33 [Lux].
Sensor val: 173.33 [Lux].
Sensor val: 173.33 [Lux].
Sensor val: 176.67 [Lux].
Sensor val: 180.00 [Lux].
Sensor val: 186.67 [Lux].
Sensor val: 190.00 [Lux].
Sensor val: 193.33 [Lux].
Close device OK.
```