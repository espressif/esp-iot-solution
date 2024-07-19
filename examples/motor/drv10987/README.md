# Application example of DRV10987

This example demonstrates how to use the `drv10987` component to develop motor control programs on ESP32 series chips. The specific functions demonstrated are as follows:

- Motor parameter writing and speed control.
- Detection of motor operating status.

## How to use the example

### Hardware requirement

You need to prepare a three-phase brushless motor and a DRV10987 motor driver board. For this example, the parameters of the brushless motor used are as follows:
* Phase to center Tap Resistance (Ω): 1.0088
* BEMF Constant (Kt) (mV/Hz): 66.24

For motor parameter measurements, refer to the `Device Functional Modes` section of the DRV10987 documentation. Also, for the calculated results, you can verify them against the Look-UP Table.

As an example, suppose the current phase resistance of the motor is 2.328, and the corresponding HEX is 0X4F according to the Motor Phase Resistance Look-Up Table query, then config1 needs to be modified:

```c
drv10987_config1_t config1 = DEFAULT_DRV10987_CONFIG1;
config1.rm_value = 0x0F;
config1.rm_shift = 0x04;
```

In addition, it is important to note that the measured results must be converted to calculated values and displacement numbers as required by the TI documentation.

### Compile and write

1. Enter the `drv10987` directory:

    ```linux
    cd ./esp-iot-solution/examples/motor/drv10987
    ```

2. Use the `idf.py` tool to set the compiler chip, and then compile and download it. The instructions are:

    ```linux
    # Set compiler chip
    idf.py idf.py set-target esp32s3

    # Compile and download
    idf.py -p PORT build flash
    ```

    Please replace `PORT` with the port number currently in use

### Example output

The log of the motor speed control is as follows, we blocked the motor after it had been running for some time to test if the error flag could be detected.

When the driver detects an error, we clear the error flag bit and wait drv10987 reboot, at which point the fault flag bit is cleared.

```log
I (31) boot: ESP-IDF v5.4-dev-1592-g18f9050cb1-dirty 2nd stage bootloader
I (31) boot: compile time Jul 30 2024 11:42:45
I (33) boot: Multicore bootloader
I (38) boot: chip revision: v3.0
I (41) boot.esp32: SPI Speed      : 40MHz
I (46) boot.esp32: SPI Mode       : DIO
I (51) boot.esp32: SPI Flash Size : 2MB
I (55) boot: Enabling RNG early entropy source...
I (61) boot: Partition Table:
I (64) boot: ## Label            Usage          Type ST Offset   Length
I (71) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (79) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (86) boot:  2 factory          factory app      00 00 00010000 00100000
I (94) boot: End of partition table
I (98) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=0b35ch ( 45916) map
I (122) esp_image: segment 1: paddr=0001b384 vaddr=3ffb0000 size=02394h (  9108) load
I (126) esp_image: segment 2: paddr=0001d720 vaddr=40080000 size=028f8h ( 10488) load
I (132) esp_image: segment 3: paddr=00020020 vaddr=400d0020 size=176dch ( 95964) map
I (169) esp_image: segment 4: paddr=00037704 vaddr=400828f8 size=0b744h ( 46916) load
I (195) boot: Loaded app from partition at offset 0x10000
I (195) boot: Disabling RNG early entropy source...
I (207) cpu_start: Multicore app
I (215) cpu_start: Pro cpu start user code
I (215) cpu_start: cpu freq: 240000000 Hz
I (216) app_init: Application information:
I (218) app_init: Project name:     drv10987
I (223) app_init: App version:      7a207e10-dirty
I (229) app_init: Compile time:     Aug  1 2024 10:56:51
I (235) app_init: ELF file SHA256:  6df6df1d4...
I (240) app_init: ESP-IDF:          v5.4-dev-1592-g18f9050cb1-dirty
I (247) efuse_init: Min chip rev:     v0.0
I (252) efuse_init: Max chip rev:     v3.99 
I (257) efuse_init: Chip rev:         v3.0
I (262) heap_init: Initializing. RAM available for dynamic allocation:
I (269) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (275) heap_init: At 3FFB2CD0 len 0002D330 (180 KiB): DRAM
I (281) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (287) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (294) heap_init: At 4008E03C len 00011FC4 (71 KiB): IRAM
I (301) spi_flash: detected chip: generic
I (305) spi_flash: flash io: dio
W (309) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
W (322) i2c: This driver is an old driver, please migrate your application code to adapt `driver/i2c_master.h`
I (333) main_task: Started on CPU0
I (337) main_task: Calling app_main()
I (341) i2c_bus: i2c0 bus inited
I (344) i2c_bus: I2C Bus Config Succeed, Version: 0.1.0
I (350) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
E (12447) DRV10987: Fault detected:1
E (13448) DRV10987: Fault detected:1
E (14449) DRV10987: Fault detected:1
E (15450) DRV10987: Fault detected:1
E (16451) DRV10987: Fault detected:1
E (17452) DRV10987: Fault detected:1
E (34453) DRV10987: Fault detected:1
E (35454) DRV10987: Fault detected:1
E (36455) DRV10987: Fault detected:1
E (37456) DRV10987: Fault detected:1
E (38457) DRV10987: Fault detected:1
E (39458) DRV10987: Fault detected:1

```

![](https://dl.espressif.com/AE/esp-iot-solution/drv10987_motor.gif)