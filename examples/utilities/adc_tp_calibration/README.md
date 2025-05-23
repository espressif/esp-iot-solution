| Supported Targets | ESP32 | ESP32-S2 |
| ----------------- | ----- | -------- |

# ADC Two-Point Calibration Example

This example demonstrates how to perform ADC two-point calibration and obtain calibration parameters for the `adc_tp_calibration` component. It also shows how to use these parameters to get calibrated ADC readings. You can use this example as a reference to implement your own application and store the calibration parameters in persistent storage like NVS, SD card, or other storage media.

## How to use the example

### Prerequisites

Please prepare an ESP32 or ESP32-S2 chip and an adjustable power supply. We will implement two-point calibration based on the stable output voltage data from the adjustable power supply.

In addition, please note that this example implements calibration for **both ADC1 and ADC2**. If your project only needs to use ADC1 or ADC2, you can calibrate just one of them.

Finally, please apply the corresponding voltage to the corresponding ADC channel according to the log prompts for calibration. After the current voltage calibration is completed, the system will switch to the next calibration point and prompt the required voltage until all voltages are calibrated.

### Configure the Project

Run `idf.py menuconfig` and navigate to the `Example Configuration` menu, where you can configure the relevant pins and example-related options.

First, you need to configure the ADC1 and ADC2 channels used for calibration. There are no restrictions on which channels you choose.

```
ESP32 ADC Configuration --->
    (6) ADC1 Channel Number
    (9) ADC2 Channel Number
```

Additionally, you need to configure the ``Number of times to read ADC`` for each calibration point. To avoid voltage fluctuations during calibration, it is recommended to configure a higher number of ADC readings. In this example, all ADC data is sorted and the average of the middle portion is taken. 

Finally, to prevent users from inputting incorrect calibration voltages during calibration, this example additionally uses IDF's default calibration scheme to verify the validity of input voltages. However, the default calibration scheme itself has some deviation, so you need to configure the ``ADC Error Tolerance Range`` to obtain the raw ADC data under the current calibration voltage.

```
Example Configuration --->
    (50) Number of times to read ADC
    (30) ADC Error Tolerance Range (mV)
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

To exit the serial monitor, type ``Ctrl-]``.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Example output

From the example output, we can see that ADC1 and ADC2 show good consistency after two-point calibration.

```
I (29) boot: ESP-IDF v5.5-dev-3234-gdc678decf7a-dirt 2nd stage bootloader
I (29) boot: compile time Apr 25 2025 18:42:44
I (29) boot: Multicore bootloader
I (33) boot: chip revision: v1.0
I (36) boot.esp32: SPI Speed      : 40MHz
I (39) boot.esp32: SPI Mode       : DIO
I (43) boot.esp32: SPI Flash Size : 2MB
I (46) boot: Enabling RNG early entropy source...
I (51) boot: Partition Table:
I (54) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (73) boot:  2 factory          factory app      00 00 00010000 00100000
I (79) boot: End of partition table
I (83) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=0ae48h ( 44616) map
I (105) esp_image: segment 1: paddr=0001ae70 vaddr=3ff80000 size=0001ch (    28) load
I (106) esp_image: segment 2: paddr=0001ae94 vaddr=3ffb0000 size=02324h (  8996) load
I (113) esp_image: segment 3: paddr=0001d1c0 vaddr=40080000 size=02e58h ( 11864) load
I (122) esp_image: segment 4: paddr=00020020 vaddr=400d0020 size=1586ch ( 88172) map
I (155) esp_image: segment 5: paddr=00035894 vaddr=40082e58 size=0a01ch ( 40988) load
I (177) boot: Loaded app from partition at offset 0x10000
I (177) boot: Disabling RNG early entropy source...
I (188) cpu_start: Multicore app
I (196) cpu_start: Pro cpu start user code
I (196) cpu_start: cpu freq: 160000000 Hz
I (196) app_init: Application information:
I (196) app_init: Project name:     adc_tp_calibration
I (201) app_init: App version:      580f03fa-dirty
I (206) app_init: Compile time:     Apr 25 2025 19:37:43
I (211) app_init: ELF file SHA256:  ddb51ead4...
I (215) app_init: ESP-IDF:          v5.5-dev-3234-gdc678decf7a-dirt
I (221) efuse_init: Min chip rev:     v0.0
I (225) efuse_init: Max chip rev:     v3.99 
I (229) efuse_init: Chip rev:         v1.0
I (233) heap_init: Initializing. RAM available for dynamic allocation:
I (239) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (244) heap_init: At 3FFB2C10 len 0002D3F0 (180 KiB): DRAM
I (249) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (255) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (260) heap_init: At 4008CE74 len 0001318C (76 KiB): IRAM
I (267) spi_flash: detected chip: generic
I (269) spi_flash: flash io: dio
W (272) spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (285) main_task: Started on CPU0
I (295) main_task: Calling app_main()
I (295) ADC_DRIVER: calibration scheme version is Line Fitting
I (295) ADC_TP_CALIBRATION: Memory allocated successfully. Please input 150mV to ADC1 channel 6
I (305) ADC_TP_CALIBRATION: ADC1 channel 6 Measuring 150mV - Progress: 1/50, raw: 340, voltage: 155 mV
I (405) ADC_TP_CALIBRATION: ADC1 channel 6 Measuring 150mV - Progress: 2/50, raw: 332, voltage: 153 mV
I (505) ADC_TP_CALIBRATION: ADC1 channel 6 Measuring 150mV - Progress: 3/50, raw: 336, voltage: 154 mV
I (605) ADC_TP_CALIBRATION: ADC1 channel 6 Measuring 150mV - Progress: 4/50, raw: 335, voltage: 154 mV
I (705) ADC_TP_CALIBRATION: ADC1 channel 6 Measuring 150mV - Progress: 5/50, raw: 335, voltage: 154 mV
I (805) ADC_TP_CALIBRATION: ADC1 channel 6 Measuring 150mV - Progress: 6/50, raw: 335, voltage: 154 mV
...
I (33065) ADC_TP_CALIBRATION: ADC2 channel 9 with 0dB attenuation 850mV average value: 3451
I (33065) ADC_TP_CALIBRATION: ESP32 ADC TP Calibration Success, adc1_raw_value_150mv_atten0: 335, adc1_raw_value_850mv_atten0: 3312
I (33075) ADC_TP_CALIBRATION: ESP32 ADC TP Calibration Success, adc2_raw_value_150mv_atten0: 480, adc2_raw_value_850mv_atten0: 3451
I (33085) ADC_DRIVER: calibration scheme version is Line Fitting
I (33095) ADC_DRIVER: calibration scheme version is Line Fitting
I (33095) ADC_TP_CALIBRATION: ADC1 channel 6 with 0dB attenuation raw: 3311, voltage: 849 mV
I (33205) ADC_TP_CALIBRATION: ADC2 channel 9 with 0dB attenuation raw: 3454, voltage: 850 mV
I (33305) ADC_TP_CALIBRATION: ADC1 channel 6 with 0dB attenuation raw: 3312, voltage: 849 mV
I (33405) ADC_TP_CALIBRATION: ADC2 channel 9 with 0dB attenuation raw: 3453, voltage: 850 mV
I (33505) ADC_TP_CALIBRATION: ADC1 channel 6 with 0dB attenuation raw: 3312, voltage: 849 mV
I (33605) ADC_TP_CALIBRATION: ADC2 channel 9 with 0dB attenuation raw: 3453, voltage: 850 mV
I (33705) ADC_TP_CALIBRATION: ADC1 channel 6 with 0dB attenuation raw: 3312, voltage: 849 mV
I (33805) ADC_TP_CALIBRATION: ADC2 channel 9 with 0dB attenuation raw: 3454, voltage: 850 mV
I (33905) ADC_TP_CALIBRATION: ADC1 channel 6 with 0dB attenuation raw: 3311, voltage: 849 mV
```
