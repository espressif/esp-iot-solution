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
* Zero cross signal Source

### Configure the project

Before compiling the project, configure it using `idf.py menuconfig`

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```log
I (318) zero_detect: Install gptimer
I (328) zero_detect: Install gptimer alarm
I (328) zero_detect: Register powerdown callback
I (338) zero_detect: Install capture timer
I (338) zero_detect: Install capture channel
I (348) gpio: GPIO[2]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (358) zero_detect: Register capture callback
I (358) zero_detect: Enable capture channel
I (368) zero_detect: Enable and start capture timer
I (368) gpio: GPIO[5]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (878) zero_detect: Measured Time: 6.00ms Hz:166.67
EVENT: OUT OF RANGE COUNT:246 OFF COUNT:0 OPEN COUNT:0
W (878) example: Process suspened, please wait till relay open
I (878) example: Set control relay on
I (1388) zero_detect: Measured Time: 6.00ms Hz:166.67
EVENT: OUT OF RANGE COUNT:416 OFF COUNT:0 OPEN COUNT:0
W (1388) example: Process suspened, please wait till relay open
I (1388) example: Set control relay on
I (1898) zero_detect: Measured Time: 8.00ms Hz:125.00
EVENT: OUT OF RANGE COUNT:580 OFF COUNT:0 OPEN COUNT:0
W (1898) example: Process suspened, please wait till relay open
I (1898) example: Set control relay on
I (2408) zero_detect: Measured Time: 8.00ms Hz:125.00
EVENT: OUT OF RANGE COUNT:708 OFF COUNT:0 OPEN COUNT:0
W (2408) example: Process suspened, please wait till relay open
I (2408) example: Set control relay on
I (2918) zero_detect: Measured Time: 10.00ms Hz:100.00
EVENT: OUT OF RANGE COUNT:831 OFF COUNT:0 OPEN COUNT:0
W (2918) example: Process suspened, please wait till relay open
I (2918) example: Set control relay on
I (3428) zero_detect: Measured Time: 10.00ms Hz:100.00
EVENT: OUT OF RANGE COUNT:933 OFF COUNT:0 OPEN COUNT:0
W (3428) example: Process suspened, please wait till relay open
I (3428) example: Set control relay on
I (3938) zero_detect: Measured Time: 12.00ms Hz:83.33
EVENT: OUT OF RANGE COUNT:1031 OFF COUNT:0 OPEN COUNT:0
W (3938) example: Process suspened, please wait till relay open
I (3938) example: Set control relay on
I (4448) zero_detect: Measured Time: 12.00ms Hz:83.33
EVENT: OUT OF RANGE COUNT:1116 OFF COUNT:0 OPEN COUNT:0
W (4448) example: Process suspened, please wait till relay open
I (4448) example: Set control relay on
I (4958) zero_detect: Measured Time: 14.00ms Hz:71.43
EVENT: OUT OF RANGE COUNT:1198 OFF COUNT:0 OPEN COUNT:0
W (4958) example: Process suspened, please wait till relay open
I (4958) example: Set control relay on
I (5468) zero_detect: Measured Time: 14.00ms Hz:71.43
EVENT: OUT OF RANGE COUNT:1271 OFF COUNT:0 OPEN COUNT:0
W (5468) example: Process suspened, please wait till relay open
I (5468) example: Set control relay on
I (5978) zero_detect: Measured Time: 16.00ms Hz:62.50
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (6478) zero_detect: Measured Time: 16.00ms Hz:62.50
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (6978) zero_detect: Measured Time: 18.00ms Hz:55.56
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (7478) zero_detect: Measured Time: 18.00ms Hz:55.56
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (7978) zero_detect: Measured Time: 20.00ms Hz:50.00
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (8478) zero_detect: Measured Time: 20.00ms Hz:50.00
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (8978) zero_detect: Measured Time: 22.00ms Hz:45.45
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (9478) zero_detect: Measured Time: 22.00ms Hz:45.45
EVENT: OUT OF RANGE COUNT:1323 OFF COUNT:0 OPEN COUNT:1
I (9978) zero_detect: Measured Time: 24.00ms Hz:41.67
EVENT: OUT OF RANGE COUNT:1335 OFF COUNT:0 OPEN COUNT:1
I (10478) zero_detect: Measured Time: 24.00ms Hz:41.67
EVENT: OUT OF RANGE COUNT:1376 OFF COUNT:0 OPEN COUNT:1
I (10978) zero_detect: Measured Time: 26.00ms Hz:38.46
EVENT: OUT OF RANGE COUNT:1417 OFF COUNT:0 OPEN COUNT:1
I (11478) zero_detect: Measured Time: 26.00ms Hz:38.46
EVENT: OUT OF RANGE COUNT:1456 OFF COUNT:0 OPEN COUNT:1
I (11978) zero_detect: Measured Time: 28.00ms Hz:35.71
EVENT: OUT OF RANGE COUNT:1493 OFF COUNT:0 OPEN COUNT:1
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
