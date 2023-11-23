| Supported Targets  ESP32-S3 | ESP32 | ESP32-C6 | ESP32-H2

# Example: Zero_Cross_Detection

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This routine demonstrates the detection and analysis of zero-crossing signals, as well as the control of relays.

## GPIO functions:

| GPIO                         | Direction | Configuration                                          |
| ---------------------------- | --------- | ------------------------------------------------------ |
| CONFIG_GPIO_OUTPUT_5         | output    | control the relay                                      |
| CONFIG_GPIO_INPUT_2          | input     | pulled up, interrupt from rising edge and falling edge |

## How to use example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board with any Espressif SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for Power supply and programming
* Some jumper wires to connect GPIOs.
* Zero cross singal Source

### Configure the project

Before compiling the project, configure it using `idf.py menuconfig`

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```log
 (309) main_task: Started on CPU0
I (319) main_task: Calling app_main()
I (319) zero_detect: Install gptimer
I (329) zero_detect: Install gptimer alarm
I (329) zero_detect: Register powerdown callback
I (339) zero_detect: Install capture timer
I (339) zero_detect: Install capture channel
I (349) gpio: GPIO[2]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (359) zero_detect: Register capture callback
I (359) zero_detect: Enable capture channel
I (369) zero_detect: Enable and start capture timer
I (369) gpio: GPIO[5]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (879) zero_detect: Measured Time: 6.00ms Hz:166.67
EVENT: OUT OF RANGE COUNT:170 OFF COUNT:0 OPEN COUNT:0
W (879) example: Process suspened, please wait till relay open
I (879) example: Set control relay on
I (1389) zero_detect: Measured Time: 8.00ms Hz:125.00
EVENT: OUT OF RANGE COUNT:298 OFF COUNT:0 OPEN COUNT:0
W (1389) example: Process suspened, please wait till relay open
I (1389) example: Set control relay on
I (1899) zero_detect: Measured Time: 10.00ms Hz:100.00
EVENT: OUT OF RANGE COUNT:425 OFF COUNT:0 OPEN COUNT:0
W (1899) example: Process suspened, please wait till relay open
I (1899) example: Set control relay on
I (2409) zero_detect: Measured Time: 10.00ms Hz:100.00
EVENT: OUT OF RANGE COUNT:527 OFF COUNT:0 OPEN COUNT:0
W (2409) example: Process suspened, please wait till relay open
I (2409) example: Set control relay on
I (2919) zero_detect: Measured Time: 12.00ms Hz:83.33
EVENT: OUT OF RANGE COUNT:628 OFF COUNT:0 OPEN COUNT:0
W (2919) example: Process suspened, please wait till relay open
I (2919) example: Set control relay on
I (3429) zero_detect: Measured Time: 12.00ms Hz:83.33
EVENT: OUT OF RANGE COUNT:713 OFF COUNT:0 OPEN COUNT:0
W (3429) example: Process suspened, please wait till relay open
I (3429) example: Set control relay on
I (3939) zero_detect: Measured Time: 14.00ms Hz:71.43
EVENT: OUT OF RANGE COUNT:797 OFF COUNT:0 OPEN COUNT:0
W (3939) example: Process suspened, please wait till relay open
I (3939) example: Set control relay on
I (4449) zero_detect: Measured Time: 14.00ms Hz:71.43
EVENT: OUT OF RANGE COUNT:870 OFF COUNT:0 OPEN COUNT:0
W (4449) example: Process suspened, please wait till relay open
I (4449) example: Set control relay on
I (4959) zero_detect: Measured Time: 16.00ms Hz:62.50
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (4959) example: Set control relay on
I (5459) zero_detect: Measured Time: 16.00ms Hz:62.50
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (5459) example: Set control relay on
I (5959) zero_detect: Measured Time: 18.00ms Hz:55.56
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (5959) example: Set control relay on
I (6459) zero_detect: Measured Time: 18.00ms Hz:55.56
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (6459) example: Set control relay on
I (6959) zero_detect: Measured Time: 20.00ms Hz:50.00
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (6959) example: Set control relay on
I (7459) zero_detect: Measured Time: 20.00ms Hz:50.00
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (7459) example: Set control relay on
I (7959) zero_detect: Measured Time: 22.00ms Hz:45.45
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (7959) example: Set control relay on
I (8459) zero_detect: Measured Time: 22.00ms Hz:45.45
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:1
I (8459) example: Set control relay on
I (8959) zero_detect: Measured Time: 24.00ms Hz:41.67
EVENT: OUT OF RANGE COUNT:938 OFF COUNT:0 OPEN COUNT:1
I (8959) example: Set control relay on
I (9459) zero_detect: Measured Time: 24.00ms Hz:41.67
EVENT: OUT OF RANGE COUNT:980 OFF COUNT:0 OPEN COUNT:1
I (9459) example: Set control relay on
I (9959) zero_detect: Measured Time: 26.00ms Hz:38.46
EVENT: OUT OF RANGE COUNT:1021 OFF COUNT:0 OPEN COUNT:1
I (9959) example: Set control relay on
I (10459) zero_detect: Measured Time: 26.00ms Hz:38.46
EVENT: OUT OF RANGE COUNT:1060 OFF COUNT:0 OPEN COUNT:1
I (10459) example: Set control relay on
I (10959) zero_detect: Measured Time: 28.00ms Hz:35.71
EVENT: OUT OF RANGE COUNT:1098 OFF COUNT:0 OPEN COUNT:1
I (10959) example: Set control relay on
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
