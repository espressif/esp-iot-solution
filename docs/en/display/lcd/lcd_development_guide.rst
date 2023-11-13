LCD Development Guide
=====================

:link_to_translation:`zh_CN:[中文]`

This guide mainly contains the following sections:

.. list::

  - `Supported Interface Types`_: Espressif's support for different LCD interfaces in each series of chips.
  - `Driver and Examples`_: LCD driver and examples provided by Espressif Systems.
  - `Development Framework`_: Develop software and hardware framework for LCD.
  - `Development Steps`_: Detailed steps for developing LCD applications.
  - `Common Problems`_: Lists common problems in developing LCD applications.
  - `Related Documents`_: Links to relevant documentation are listed.

Supported Interface Types
----------------------------

Espressif Systems provides comprehensive support for all interface types introduced in the :ref:`LCD Overview - Driver Interface <LCD_Overview_Driver_Interface>`. Specific support for each series of ESP chips within this section is as follows:

.. list-table::
    :widths: 20 20 20 20 20
    :header-rows: 1

    * - Soc
      - SPI(QSPI)
      - I80
      - RGB
      - MIPI-DSI
    * - ESP32
      - |supported|
      -
      -
      -
    * - ESP32-C3
      - |supported|
      -
      -
      -
    * - ESP32-C6
      - |supported|
      -
      -
      -
    * - ESP32-S2
      - |supported|
      - |supported|
      -
      -
    * - ESP32-S3
      - |supported|
      - |supported|
      - |supported|
      -
    * - ESP32-P4
      - |supported|
      - |supported|
      - |supported|
      - |supported|

.. |supported| image:: https://img.shields.io/badge/-Supported-green

.. _LCD_Development_Guide_Driver_and_Examples:

Driver and Examples
---------------------

**LCD Peripheral Driver** is located in `components/esp_lcd <https://github.com/espressif/esp-idf/tree/master/components/esp_lcd>`_ directory under **ESP-IDF**. Currently, it supports the following interfaces ``I2C``, ``SPI(QSPI)``, ``I80`` as well as ``RGB``. For detailed information, please refer to the `documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html>`_. The following is the official Espressif system **LCD Driver component** based on ``esp_lcd``.

.. _LCD_Driver_Component:

  - `ESP-IDF <https://github.com/espressif/esp-idf/tree/master/components/esp_lcd/src>`_ : st7789、nt35510、ssd1306。
  - `ESP component manager <https://components.espressif.com/components?q=espressif%2Fesp_lcd>`_ : gc9a01、ili9341、ra8875、sh1107、st7796、st7701、gc9503、gc9b71、spd2010、...(continuously updated)

**LCD Example** is located in the `examples/peripherals/lcd <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd>`_ directory under **ESP-IDF** and the :project:`examples/display/lcd <examples/display/lcd>` directory under **esp-iot-solution**.

.. note::

    - It is recommended to develop based on ESP-IDF `release/v5.1 <https://github.com/espressif/esp-idf/tree/release/v5.1>`_ and above version branches, because lower versions may lack to support some parts of the important new features, especially for the ``RGB`` interface.
    - For LCDs using the ``3-wire SPI + RGB`` interface, please refer to the example `esp_lcd_st7701 - Example use <https://components.espressif.com/components/espressif/esp_lcd_st7701>`_.
    - Even if the LCD driver IC is of the same model, different screens often need to be configured using the initialization commands provided by their respective manufacturers. Most driver components support after giving custom initialization commands when initializing the LCD device. if not, please refer to `method <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html#steps-to-add-manufacture-specific-initialization>`_.

Development Framework
-------------------------

Hardware Framework
^^^^^^^^^^^^^^^^^^^^^^^^^

For SPI/I80 LCD, ESP can send **commands** to configure the LCD and transmit **regional color data** to refresh the screen through a single peripheral interface. The LCD driver IC will store the received color data in **full-screen size GRAM** and display **full-screen color data** on the panel at a fixed refresh rate. Importantly, these two processes are performed in asynchronously. The schematic diagram  below illustrates the hardware driver framework of SPI/I80 LCD:

.. figure:: ../../../_static/display/screen/lcd_hw_framework_spi_i80.png
    :align: center
    :scale: 40%
    :alt: Hardware driver framework diagram - SPI/I80 LCD

    Schematic diagram of hardware driver framework - SPI/I80 LCD

For most RGB LCDs, the ESP needs to use two different interfaces. On one hand, it utilizes the `3-wire SPI` interface to send commands for configuring the LCD. On the other hand, it uses the `RGB` interface to transmit **full-screen color data** for screen refresh. Since the LCD's driver IC does not have a built-in Graphic RAM (GRAM), it directly displays the received color data on the panel, making these two processes synchronous. The following is a schematic diagram of the hardware driving framework for RGB LCDs:

.. figure:: ../../../_static/display/screen/lcd_hw_framework_rgb.png
    :align: center
    :scale: 40%
    :alt: Schematic diagram of hardware driver framework - RGB LCD

    Schematic diagram of hardware driver framework - RGB LCD

By comparing these two frameworks, it can be observed that, in contrast to SPI/I80 LCDs, RGB LCDs not only require the ESP to use two interfaces for transmitting commands and color data separately but also require that the ESP provides a full-screen-sized Graphic RAM (GRAM) for screen refresh. Due to the limited space in the on-chip SRAM, GRAM is typically placed in the PSRAM.

For QSPI LCDs, different models of driver ICs may require different driving methods. For example, the *SPD2010* IC has a built-in GRAM, and its driving method is similar to SPI/I80 LCDs. On the other hand, the *ST77903* IC does not have internal GRAM, and its driving method is similar to RGB LCDs. However, both of them use a single peripheral interface to transmit commands and color data. Below are schematic diagrams illustrating the hardware driving frameworks for these two types of QSPI LCDs:

.. figure:: ../../../_static/display/screen/lcd_hw_framework_qspi_with_gram.png
    :align: center
    :scale: 50%
    :alt: Hardware driver framework diagram - QSPI LCD (with GRAM)

    Schematic diagram of hardware driver framework - QSPI LCD (with GRAM)

.. figure:: ../../../_static/display/screen/lcd_hw_framework_qspi_without_gram.png
    :align: center
    :scale: 50%
    :alt: Schematic diagram of hardware driver framework - QSPI LCD (without GRAM)

    Schematic diagram of hardware driver framework - QSPI LCD (without GRAM)

Software Framework
^^^^^^^^^^^^^^^^^^^^^^^^^

The software development framework primarily consists of three layers: SDK (Software Development Kit), Driver, and APP (Application).

  #. **SDK layer**: ESP-IDF serves as the foundational element of the framework. It not only includes ``I2C``, ``SPI (QSPI)``, ``I80`` and ``RGB`` required to drive LCD and other peripherals, it also provides unified APIs through the ``esp_lcd`` component to operate the interface and LCD, such as command and parameter transmission, LCD image refresh, inversion, mirroring and other functions.
  #. **Driver layer**: Based on the APIs provided by the SDK, various device drivers can be implemented, and the porting of LVGL (GUI framework) can be implemented by initializing interface devices and LCD devices.
  #. **APP layer**: Use the APIs provided by LVGL to implement various GUI functions, such as displaying pictures, animations, text, etc.

.. figure:: ../../../_static/display/screen/lcd_sw_framework.png
    :align: center
    :scale: 50%
    :alt: Schematic diagram of software development framework

    Schematic diagram of software development framework

Development Steps
-------------------------

Initialize interface device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, initialize the peripherals corresponding to the LCD interface. Then, create the interface device and get its handle, the data type of the handle should be ``esp_lcd_panel_io_handle_t``. In this way, unified `interface common APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_io.h>`_ can be used for data transmission.

.. note::

  For LCDs that only use the ``RGB`` interface, there is no need to create its interface device, please refer directly to :ref:`LCD Initialization <LCD_Initialization>`.

Different types of LCD interfaces require the use of different peripherals. The following describes the device initialization process of several common interfaces:

  - :ref:`SPI LCD Introduction - Initialize interface device <spi_initialization_interface_device>`
  - :ref:`RGB LCD Introduction - Initialize interface device <rgb_init_interface_device>`
  - I80 LCD Introduction -  Initialization interface device (to be updated)
  - QSPI LCD Introduction - Initializing interface devices (to be updated)

For a more detailed description of this part, please refer to `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html>`_.

Initialize LCD device
^^^^^^^^^^^^^^^^^^^^^^^^

Since different models of LCD driver ICs may have different commands (registers) and parameters, and different interface types may also use different data formats and driving methods, here first need to use `interface common APIs  <https:// github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_io.h>`_ for specific interfaces to port the target LCD driver, then create the LCD device and obtain the data type ``esp_lcd_panel_handle_t`` handle, ultimately enabling applications to pass unified `LCD common APIs <https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_lcd/include/esp_lcd_panel_ops.h>`_ to operate the LCD device.

.. note::

  For LCDs that only use the ``RGB`` interface, there is no need to port its driver components. Please refer directly to :ref:`LCD Initialization <LCD_Initialization>`.

Before porting the driver component, please first try to obtain the components of the target LCD driver IC directly from :ref:`LCD driver component <LCD_Driver_Component>`. If the component does not exist, it can also be porting based on an existing component with the same interface type. LCD drivers with different interface types may have different porting principles. The following describes the porting methods of LCD driver components with several common interfaces:

  - :ref:`SPI LCD Introduction - Porting driver components <spi_porting_driver_components>`
  - :ref:`RGB LCD Introduction - Porting driver components <rgb_porting_driver_components>`
  - I80 LCD Introduction - Porting driver component (to be updated)
  - QSPI LCD Introduction - Porting driver component (to be updated)

.. _LCD_Initialization:

Then, the LCD initialization can be realized by using the driver component. The LCD initialization of several common interfaces is explained below:

  - :ref:`SPI LCD Introduction - Initialize LCD device  <spi_init_lcd>`
  - :ref:`RGB LCD Introduction - Initialize LCD device  <rgb_initialize_lcd>`
  - I80 LCD Introduction - Initialize LCD device (To be updated)
  - QSPI LCD Introduction - Initialize LCD device (To be updated)

For a more detailed description of this part, please refer to the `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html>`_.

Porting LVGL
^^^^^^^^^^^^^^^^^^^^^^^^^

(To be updated)

Design GUI
^^^^^^^^^^^^^^^^^^^^^^^^^

(To be updated)

Common Problems
-------------------------

The following lists some common issues encountered during the development of LCD applications. Please click on the issues to navigate and view the solutions.

* `How to use Arduino IDE to develop GUI for ESP series chips <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#do-esp-series-development-boards-with-screens-support-gui-development-using-the-arduino-ide>`_
* `Maximum Resolution and Frame Rate Supported by ESP Series Chips for LCD <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#what-is-the-maximum-resolution-supported-by-esp-lcd-what-is-the-corresponding-frame-rate>`_
* `How ESP series chips improve LCD rendering frame rate <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#how-can-i-improve-the-display-frame-rate-of-lcd-screens>`_
* `How to increase the PCLK (refresh frame rate) of RGB LCD with ESP32-S3 <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#how-can-i-increase-the-upper-limit-of-pclk-settings-on-esp32-s3-while-ensuring-normal-rgb-screen-display>`_
* `How to solve the problem of screen offset or flickering when driving RGB LCD with ESP32-S3 <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#why-do-i-get-drift-overall-drift-of-the-display-when-esp32-s3-is-driving-an-rgb-lcd-screen>`_
* `How to configure ESP32-S3R8 PSRAM 120M Octal(DDR) <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#how-can-i-enable-psram-120m-octal-ddr-on-esp32-s3r8>`_

Related Documents
-------------------------

* `ESP-IDF Programming Guide- LCD <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html>`_
* `ESP-FAQ - LCD <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html>`_
* `LVGL Documentation <https://docs.lvgl.io/8.3/>`_
