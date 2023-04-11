## LEDC Simple

This example shows how to configure LEDC by POSIX APIs, this decreases dependence on hardware and platform.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board based on espressif SoC
* A USB cable for power supply and programming
* A LED connecting to your configured LEDC output GPIO and GND

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Set the LEDC GPIO Frequency value in the `LEDC Frequency(Hz)` option.
* Set the LEDC output GPIO number used for the signal in the `LEDC Output Pin` option.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

As you run the example, you will see the LED's brightness changes and following log:

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (333) ext_vfs: Extended VFS version: 0.1.0
Opening device /dev/ledc/0 for writing OK, fd=3.
Set LEDC duty to be 0
Set LEDC duty to be 20
Set LEDC duty to be 40
Set LEDC duty to be 60
Set LEDC duty to be 80
Set LEDC duty to be 0
Set LEDC duty to be 20
Set LEDC duty to be 40
Set LEDC duty to be 60
Set LEDC duty to be 80
Close device OK.
```