## SPI Slave Simple

This example shows how to configure SPI slave by POSIX APIs, this decreases dependence on hardware and platform.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.
And this example should be used together with [SPI Master Simple](../spi_master_simple/).

### Hardware Required

* 2 development boards based on espressif SoC
* 2 USB cables for power supply and programming
* 5 DuPont lines connecting SPI pins and GND between 2 development boards

### Configure the Project

Open the project configuration menu (`idf.py menuconfig`).

In the `Example Configuration` menu:

* Select SPI port in the `Select SPI Device` option, please note that not all SoC supports `SPI3`
* Set the SPI pins number in the following options according to your development board:
    - `SPI CS Pin Number`
    - `SPI SCLK Pin Number`
    - `SPI MOSI Pin Number`
    - `SPI MISO Pin Number`

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

As you run the example ,you will see the following log:

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (344) ext_vfs: Extended VFS version: 0.3.0
Opening device /dev/spi/2 for writing OK, fd=3.
Receive total 32 bytes from device: SPI Tx 0
Receive total 32 bytes from device: SPI Tx 1
Receive total 32 bytes from device: SPI Tx 2
Receive total 32 bytes from device: SPI Tx 3
Receive total 32 bytes from device: SPI Tx 4
Receive total 32 bytes from device: SPI Tx 5
Receive total 32 bytes from device: SPI Tx 6
Receive total 32 bytes from device: SPI Tx 7
Receive total 32 bytes from device: SPI Tx 8
Receive total 32 bytes from device: SPI Tx 9
Receive total 32 bytes from device: SPI Tx 10
Receive total 32 bytes from device: SPI Tx 11
```