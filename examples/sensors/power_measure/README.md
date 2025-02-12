# Power Measurement Example

## Overview

This example demonstrates how to use the **BL0937** power measurement chip to detect electrical parameters such as voltage, current, active power, and energy consumption. It is implemented for **ESP32 series chips** using FreeRTOS, and shows how to configure and interface with the BL0937 power measurement chip. The example initializes the power measurement system, fetches various parameters, and logs them at regular intervals.

This example supports the **BL0937** power measurement chip, which is capable of measuring:

1. **Voltage**
2. **Current**
3. **Active Power**
4. **Energy**

The primary goal is to demonstrate how to configure the hardware pins, initialize the power measurement system, and retrieve the data from the chip.

## Features

* Measures  **voltage** ,  **current** ,  **active power** , and  **energy** .
* Configures **BL0937** power measurement chip.
* Supports overcurrent, overvoltage, and undervoltage protection.
* Energy detection is enabled for accurate readings.
* Regularly fetches power readings every second and logs them.

## Hardware Requirements

The example uses the **BL0937** power measurement chip and ESP32-C3(C2) for test. To connect it, the following pins must be configured on the ESP32-C3(C2):

| Variable            | GPIO Pin       | Chip Pin |
| ------------------- | -------------- | -------- |
| `BL0937_CF_GPIO`  | `GPIO_NUM_3` | CF Pin   |
| `BL0937_SEL_GPIO` | `GPIO_NUM_4` | SEL Pin  |
| `BL0937_CF1_GPIO` | `GPIO_NUM_7` | CF1 Pin  |

Make sure that these GPIO pins are correctly connected to the respective pins on the **BL0937** chip in your hardware setup.

## Software Requirements

* ESP-IDF (Espressif IoT Development Framework)
* FreeRTOS for task management
* Power measurement driver (`power_measure.h`)

## How It Works

The `app_main()` function in the power_measure_example.c file is the entry point for the application. Here's how it works:

1. **Initialization of Calibration Factors** :

* The application retrieves the default calibration factors for the power measurement chip using the power_measure_get_calibration_factor() function.
* Default calibration factors (`DEFAULT_KI`, `DEFAULT_KU`, `DEFAULT_KP`) are used for the measurement.

2. **Configuration of Power Measurement System** :

* The power_measure_init_config_t structure is configured with the chip’s GPIO pins, resistor values, and calibration factors.
* Specific thresholds for overcurrent, overvoltage, and undervoltage are set, and energy detection is enabled.

3. **Fetching Measurement Data** :

* The application enters a loop where it repeatedly fetches and logs the following parameters every second:
  * **Voltage** (power_measure_get_voltage)
  * **Current** (power_measure_get_current)
  * **Active Power** (power_measure_get_active_power)
  * **Energy** (power_measure_get_energy)

4. **Error Handling** :

* If fetching any of the measurements fails, an error message is logged using `ESP_LOGE`.

5. **De-initialization** :

* After exiting the loop, the power measurement system is de-initialized using power_measure_deinit().

## Configuration

You can modify the following parameters based on your specific setup:

* **GPIO Pin Definitions** : If you're using different GPIO pins for  **CF** ,  **SEL** , or  **CF1** , modify the respective `#define` values.
* **Calibration Factors** : Update the default calibration factors to match the specifications of your connected hardware for more accurate measurements.
* **Thresholds** : Adjust the `overcurrent`, `overvoltage`, and `undervoltage` settings to suit your application's safety limits.

## Logging

The application uses `ESP_LOGI` for informational logging and `ESP_LOGE` for error logging. The log messages will show up in the serial monitor or logging console.

Example log output:

**I (12345) PowerMeasureExample: Voltage: 230.15 V**

**I (12346) PowerMeasureExample: Current: 0.45 A**

**I (12347) PowerMeasureExample: Power: 103.35 W**

**I (12348) PowerMeasureExample: Energy: 0.12 Kw/h**

## Troubleshooting

1. **Failed Initialization** : If the initialization fails, ensure that all GPIO pins are correctly defined and connected to the **BL0937** chip.
2. **Measurement Failures** : If the measurements fail (e.g., voltage, current), verify that the **BL0937** chip is properly powered and communicating with your ESP32 series chips.
