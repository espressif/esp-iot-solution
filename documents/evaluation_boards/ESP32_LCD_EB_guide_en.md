[[中文]](./ESP32_LCD_EB_guide_cn.md)

# ESP32-LCD-EB 

---

# 1. Overview

ESP32-LCD-EB is an HMI (Human Machine Interface) development board with the ESP32 module at its core. ESP32-LCD-EB is integrated with such peripherals as SD-Card, DAC-Audio, and can be connected to an external display. The board is mainly used for HMI-related development and evaluation.

You may find HMI-related examples running with ESP32-LCD-EB in [HMI Example](../../examples/hmi/).

<img src="../../documents/_static/lcd_eb/esp32_lcd_eb.jpg" width = "800">

For more information on ESP32, please refer to [ESP32 Series Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf).

# 2. Block Diagram and PCB Layout

## 2.1 Block Diagram

The figure below shows the block diagram for ESP32-LCD-EB.

<img src="../../documents/_static/lcd_eb/esp32_lcd_eb_block.jpg" width = "600">

## 2.2 PCB Layout

The figure below shows the layout of ESP32-LCD-EB PCB.

<img src="../../documents/_static/lcd_eb/esp32_lcd_eb_pcb.jpg" width = "800">

Table 1: Description of PCB Components

Components   | Description
------    | ------
Display connection module | Allows to connect serial or parallel LCD displays (8/16 bit)
ESP32 DevKitC connection module | Offers connection to an ESP32 DevKitC development board
SD-Card module | Provides an SD-Card slot for memory expansion
DAC-Audio module | Features an audio power amplifier and two output ports for external speakers


# 3. Functional Modules

This section introduces the functional modules (interfaces) of ESP32-LCD-EB and their hardware schematics.

## 3.1 ESP32 DevKitC Connection Module

For the HMI-related development with ESP32-LCD-EB, you also need the [ESP32 DevKitC](https://docs.espressif.com/projects/esp-idf/en/latest/hw-reference/modules-and-boards.html#esp32-devkitc-v4) development board.

The figure below shows the schematics for the ESP32 DevKitC connection module.

<img src="../../documents/_static/lcd_eb/coreboard_module.jpg" width = "800">

## 3.2 Power Supply Management Module

The figure below shows the schematics for the USB power supply management module.

<img src="../../documents/_static/lcd_eb/power_module.jpg" width = "800">

## 3.3 Display Connection Module

The display connection module supports the following interfaces:

- SPI serial interface
- 8-bit parallel interface
- 16-bit parallel interface

With this module, you can connect ESP32-LCD-EB to an external display and interact with the pre-programmed GUI if the display has a touchscreen.

The figure below shows the schematics for this module.

<img src="../../documents/_static/lcd_eb/serial_screen_module.jpg" width = "800">

<img src="../../documents/_static/lcd_eb/parallel_screen_module.jpg" width = "800">

## 3.4 SD-Card and DAC-Audio Modules

The SD-Card module provides an SD Card slot for memory expansion. The DAC-Audio module features the MIX3006 power amplifier and two output ports for connection of external speakers.

The figure below shows the schematics for the SD-Card and DAC-Audio modules.

<img src="../../documents/_static/lcd_eb/sd_card_dac_module.jpg" width = "800">
