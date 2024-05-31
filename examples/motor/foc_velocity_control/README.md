# Application example of ESP SIMPLEFOC Speed Control

This example demonstrates how to develop a motor speed control program using the `esp_ speed control component on an ESP32 series chip.
The specific functions of the demonstration are as follows:

- Motor speed closed loop.
- Serial port instruction to control motor speed

## How to use the example

### Hardware requirement

1. Hardware connection:
    * for ESP32-S3 chips, the serial port is connected to the default RXD0 and TXD0. If you need to use other serial ports, you need to modify the corresponding uart_num, RXD and TXD in the initialization part of the serial port.
    * the sample motor has a pole pair of 14 and a rated voltage of 12V.
    * the example MOS predrive uses EG2133, and connects LIN with HIN, using 3PWM driver.
    * the example angle sensor uses AS5600. If you use other angle sensors, you need to manually enter the initialization and angle reading functions into GenericSensor.
    * the example hardware schematic: [single bldc](https://dl.espressif.com/AE/esp-iot-solution/docs/foc_hardware_single_bldc.pdf)
    > debugging parameters need to be modified for different types of motors and hardware. **make sure to debug under the correct parameters and beware of anomalies.**

### Compile and write

1. Enter the `foc_velocity_ control` directory:

    ```linux
    cd ./esp-iot-solution/examples/motor/foc_velocity_control
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

The following is the speed control log:

```log
 Usage          Type ST Offset   Length
I (66) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (74) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (81) boot:  2 factory          factory app      00 00 00010000 00100000
I (88) boot: End of partition table
I (93) esp_image: segment 0: paddr=00010020 vaddr=3c030020 size=0fc00h ( 64512) map
I (113) esp_image: segment 1: paddr=0001fc28 vaddr=3fc92f00 size=003f0h (  1008) load
I (113) esp_image: segment 2: paddr=00020020 vaddr=42000020 size=24d48h (150856) map
I (145) esp_image: segment 3: paddr=00044d70 vaddr=3fc932f0 size=025dch (  9692) load
I (148) esp_image: segment 4: paddr=00047354 vaddr=40374000 size=0ee18h ( 60952) load
I (171) boot: Loaded app from partition at offset 0x10000
I (171) boot: Disabling RNG early entropy source...
I (182) cpu_start: Multicore app
I (183) cpu_start: Pro cpu up.
I (183) cpu_start: Starting app cpu, entry point is 0x403752d4
I (0) cpu_start: App cpu up.
I (201) cpu_start: Pro cpu start user code
I (201) cpu_start: cpu freq: 240000000 Hz
I (201) cpu_start: Application information:
I (204) cpu_start: Project name:     foc_example
I (209) cpu_start: App version:      d580537b
I (214) cpu_start: Compile time:     Jul 19 2023 10:20:23
I (221) cpu_start: ELF file SHA256:  686b47738...
I (226) cpu_start: ESP-IDF:          v5.2-dev-1113-g28c643a56d
I (232) cpu_start: Min chip rev:     v0.0
I (237) cpu_start: Max chip rev:     v0.99
I (242) cpu_start: Chip rev:         v0.1
I (247) heap_init: Initializing. RAM available for dynamic allocation:
I (254) heap_init: At 3FC96AF8 len 00052C18 (331 KiB): DRAM
I (260) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (267) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (273) heap_init: At 600FE010 len 00001FD8 (7 KiB): RTCRAM
I (280) spi_flash: detected chip: gd
I (283) spi_flash: flash io: dio
W (287) spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (301) sleep: Configure to isolate all GPIO pins in sleep state
I (307) sleep: Enable automatic switching of GPIO sleep configuration
I (315) app_start: Starting scheduler on CPU0
I (319) app_start: StartY�I (345) gpio: GPIO[17]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (345) gpio: GPIO[16]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (352) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
MCPWM Group: 0 is idle
Auto. Current Driver uses Mcpwm GroupId:0
I (368) gpio: GPIO[17]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (377) gpio: GPIO[16]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (386) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
MOT: Monitor enabled!
MOT: Init
MOT: Enable driver.
MOT: Align sensor.
MOT: sensor_direction==CW
MOT: PP check: OK!
MOT: Zero elec. angle: 4.49
MOT: No current sense.
MOT: Ready.

T2.0
2.000
```

![FOC example](https://dl.espressif.com/ae/esp-iot-solution/foc_close_loop.gif)
