## I2C TT21100 Example

This example shows how to configure I2C bus and read data from TT21100(touch pad) by POSIX APIs, this decreases dependence on hardware and platform.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board based on espressif SoC   
* A USB cable for power supply and programming
* A TT21100 connecting to your configured GPIO, Power and GND, if your development board doesn't integrate TT21100

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Set the I2C GPIO number used for the signal in the following option:
    - I2C SDA Pin Number
    - I2C SCL Pin Number
    - TT21100 Ready Pin Number
* Set the reading sensor data count in the `I2C Testing Count of Reading Sensor's Data` option.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

As you run the example, you will see following log:

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (315) ext_vfs: Extended VFS version: 0.1.0
I (315) gpio: GPIO[3]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
TT21100 touch screen is initialized, and start reading touch point information:
```

Then you can touch the TT21100 touch pad and see touch point location information as following:

```
  X0=122 Y0=146
  X0=122 Y0=146
  X0=122 Y0=146
  X0=122 Y0=146
  X0=122 Y0=146
  X0=122 Y0=146
  X0=152 Y0=169
  X0=152 Y0=169
  X0=152 Y0=169
  X0=152 Y0=169
  X0=152 Y0=169
  X0=152 Y0=169
  X0=153 Y0=169
  X0=153 Y0=169
  X0=217 Y0=170
  X0=217 Y0=170
  X0=219 Y0=170
  X0=220 Y0=170
  X0=221 Y0=169
  X0=221 Y0=169
  X0=217 Y0=168
  X0=217 Y0=168
  X0=106 Y0=186
  X0=106 Y0=186
  X0=106 Y0=186
  X0=106 Y0=186
  X0=106 Y0=186
  X0=106 Y0=186
  X0=106 Y0=186
  X0=106 Y0=186
  X0=68 Y0=173
  X0=68 Y0=173
  X0=68 Y0=173
  X0=67 Y0=173
  X0=67 Y0=173
  X0=66 Y0=171
  X0=65 Y0=166
  X0=65 Y0=166
  X0=95 Y0=106
  X0=95 Y0=106
  X0=95 Y0=106
  X0=95 Y0=106
  X0=94 Y0=106
  X0=93 Y0=106
  X0=93 Y0=106
  X0=93 Y0=106
  X0=91 Y0=79
  X0=91 Y0=79
  X0=91 Y0=79
  X0=90 Y0=79
  X0=87 Y0=80
  X0=84 Y0=82
  X0=82 Y0=84
  X0=80 Y0=85
  X0=80 Y0=85
  X0=54 Y0=106
  X0=54 Y0=106
  X0=54 Y0=106
  X0=53 Y0=108
  X0=52 Y0=110
  X0=50 Y0=111
  X0=50 Y0=112
  X0=49 Y0=112
  X0=49 Y0=112
  X0=48 Y0=141
  X0=48 Y0=141
  X0=48 Y0=141
  X0=47 Y0=141
  X0=47 Y0=141
  X0=46 Y0=141
  X0=45 Y0=141
  X0=45 Y0=141
  X0=67 Y0=152
  X0=67 Y0=152
  X0=67 Y0=152
  X0=67 Y0=152
  X0=67 Y0=152
  X0=67 Y0=152
  X0=66 Y0=152
  X0=66 Y0=152
  X0=78 Y0=165
  X0=78 Y0=165
  X0=78 Y0=164
  X0=78 Y0=164
  X0=78 Y0=163
  X0=79 Y0=163
  X0=79 Y0=162
  X0=79 Y0=162
  X0=140 Y0=139
  X0=140 Y0=139
  X0=140 Y0=139
  X0=140 Y0=139
  X0=140 Y0=139
  X0=141 Y0=138
  X0=142 Y0=137
  X0=144 Y0=134
  X0=144 Y0=134
  X0=178 Y0=128
  X0=178 Y0=128
  X0=178 Y0=128
I (4145) gpio: GPIO[3]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
Test done and de-initialize TT21100 touch screen.
```
