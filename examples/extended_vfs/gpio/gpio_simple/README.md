## GPIO Simple

This example shows how to configure and flip GPIO by POSIX APIs, this decreases dependence on hardware and platform.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board based on espressif SoC
* A USB cable for power supply and programming
* A LED connecting to your configured GPIO and GND

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Set the LED GPIO number used for the signal in the `GPIO Pin Number` option.
* Set the LED GPIO blinking count in the `GPIO Pin Testing coun` option.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

As you run the example, you will see the LED blinking and following log:

```
(0) cpu_start: Starting scheduler on APP CPU.
I (315) ext_vfs: Extended VFS version: 0.1.0
I (315) gpio: GPIO[9]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
Opening device /dev/gpio/9 for writing OK, fd=3.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
Set GPIO-9 to be 1 OK.
Set GPIO-9 to be 0 OK.
I (20335) gpio: GPIO[9]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
Close device OK.
```