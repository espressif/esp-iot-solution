## ELF Loader Example

This example shows how to use ELF loader to run ELF file.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

* Note: Only ESP32, ESP32-S2 and ESP32-S3 are supported

### Hardware Required

* A development board based on espressif ESP32/ESP32-S2/ESP32-S3 SoC
* A USB cable for power supply and programming

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Enable ELF input arguments in the `Support arguments for ELF main` option if necessary.
* Set the arguments in the `Arguments of ELF main` option.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

As you run the example, you will see the following log:

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (382) elf_loader: Start to relocate ELF
I (392) elf_loader: Start to run ELF
hello world 0
hello world 1
hello world 2
hello world 3
hello world 4
hello world 5
hello world 6
hello world 7
hello world 8
hello world 9
I (10392) elf_loader: Success to exit from APP of ELF
```
