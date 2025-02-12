# Component Power_Measure

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
