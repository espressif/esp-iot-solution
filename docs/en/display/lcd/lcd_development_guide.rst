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

Terminology
-----------

Please refer to the :ref:`LCD Terms Table <LCD_Terms_Table>` 。

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
      - |supported|
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

**LCD Peripheral** Driver is located in the `components/esp_lcd <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html>_` directory under **ESP-IDF**. Currently, it supports the ``I2C``, ``SPI (QSPI)``, ``I80``, and ``RGB`` interfaces. The following table lists the LCD driver components officially ported by Espressif based on esp_lcd, and these components will continue to be updated:

.. list-table::
    :widths: 10 15
    :header-rows: 1

    * - 接口
      - LCD 控制器
    * - I2C
      - `ssd1306 <https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/src/esp_lcd_panel_ssd1306.c>`_, `sh1107 <https://components.espressif.com/components/espressif/esp_lcd_sh1107>`_
    * - SPI
      - `axs15231b <https://components.espressif.com/components/espressif/esp_lcd_axs15231b>`_, `st7789 <https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/src/esp_lcd_panel_st7789.c>`_, `nt35510 <https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/src/esp_lcd_panel_nt35510.c>`_, `gc9b71 <https://components.espressif.com/components/espressif/esp_lcd_gc9b71>`_, `nv3022b <https://components.espressif.com/components/espressif/esp_lcd_nv3022b>`_, `sh8601 <https://components.espressif.com/components/espressif/esp_lcd_sh8601>`_, `spd2010 <https://components.espressif.com/components/espressif/esp_lcd_spd2010>`_, `st77916 <https://components.espressif.com/components/espressif/esp_lcd_st77916>`_, `st77922 <https://components.espressif.com/components/espressif/esp_lcd_st77922>`_, `gc9a01 <https://components.espressif.com/components/espressif/esp_lcd_gc9a01>`_, `gc9d01 <https://components.espressif.com/components/espressif/esp_lcd_gc9d01>`_, `ili9341 <https://components.espressif.com/components/espressif/esp_lcd_ili9341>`_, `ssd1681 <https://components.espressif.com/components/espressif/esp_lcd_ssd1681>`_, `st7796 <https://components.espressif.com/components/espressif/esp_lcd_st7796>`_,  `gc9107 <https://components.espressif.com/components/espressif/esp_lcd_gc9107>`_
    * - QSPI
      - `axs15231b <https://components.espressif.com/components/espressif/esp_lcd_axs15231b>`_, `gc9b71 <https://components.espressif.com/components/espressif/esp_lcd_gc9b71>`_, `sh8601 <https://components.espressif.com/components/espressif/esp_lcd_sh8601>`_, `spd2010 <https://components.espressif.com/components/espressif/esp_lcd_spd2010>`_, `st77903 <https://components.espressif.com/components/espressif/esp_lcd_st77903_qspi>`_, `st77916 <https://components.espressif.com/components/espressif/esp_lcd_st77916>`_, `st77922 <https://components.espressif.com/components/espressif/esp_lcd_st77922>`_, `co5300 <https://components.espressif.com/components/espressif/esp_lcd_co5300>`_
    * - I80
      - `axs15231b <https://components.espressif.com/components/espressif/esp_lcd_axs15231b>`_, `st7789 <https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/src/esp_lcd_panel_st7789.c>`_, `nt35510 <https://github.com/espressif/esp-idf/blob/master/components/esp_lcd/src/esp_lcd_panel_nt35510.c>`_, `ra8875 <https://components.espressif.com/components/espressif/esp_lcd_ra8875>`_, `st7796 <https://components.espressif.com/components/espressif/esp_lcd_st7796>`_
    * - MIPI-DSI
      - `co5300 <https://components.espressif.com/components/espressif/esp_lcd_co5300>`_, `ek79007 <https://components.espressif.com/components/espressif/esp_lcd_ek79007>`_, `er88577 <https://components.espressif.com/components/espressif/esp_lcd_er88577>`_, `hx8399 <https://components.espressif.com/components/espressif/esp_lcd_hx8399>`_, `ili9881c <https://components.espressif.com/components/espressif/esp_lcd_ili9881c>`_, `jd9165 <https://components.espressif.com/components/espressif/esp_lcd_jd9165>`_, `jd9365 <https://components.espressif.com/components/espressif/esp_lcd_jd9365>`_, `st7123 <https://components.espressif.com/components/espressif/esp_lcd_st7123>`_, `st7701 <https://components.espressif.com/components/espressif/esp_lcd_st7701>`_, `st7703 <https://components.espressif.com/components/espressif/esp_lcd_st7703>`_, `st77916 <https://components.espressif.com/components/espressif/esp_lcd_st77916>`_, `st77922 <https://components.espressif.com/components/espressif/esp_lcd_st77922>`_
    * - 3-wire SPI + RGB
      - `st7701 <https://components.espressif.com/components/espressif/esp_lcd_st7701>`_, `st77903_rgb <https://components.espressif.com/components/espressif/esp_lcd_st77903_rgb>`_, `st77922 <https://components.espressif.com/components/espressif/esp_lcd_st77922>`_, `gc9503 <https://components.espressif.com/components/espressif/esp_lcd_gc9503>`_, `nv3052 <https://components.espressif.com/components/espressif/esp_lcd_nv3052>`_

**Please note:**

.. _LCD_Driver_Component:

  - The **st7789**, **nt35510**, and **ssd1306** components are maintained in the `ESP-IDF <https://github.com/espressif/esp-idf/tree/master/components/esp_lcd/src>`_. Other components can be found in the `Espressif IDF Component Registry <https://components.espressif.com/components?q=espressif%2Fesp_lcd>`_.
  - Although the models of the **LCD driver IC** might be identical, **different screens** require specific configurations through initialization commands provided by their respective manufacturers. Most driver components support the customization of initialization commands during the initialization of the LCD device. If it is not supported, please refer to the `method <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html#steps-to-add-manufacture-specific-initialization>`_.

**LCD Example** is located in the `examples/peripherals/lcd <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/lcd>`_ directory under **ESP-IDF** and the :project:`examples/display/lcd <examples/display/lcd>` directory under **esp-iot-solution**. These serve as a reference for the usage of the LCD driver component.

.. note::

    - It is recommended to develop based on ESP-IDF `release/v5.1 <https://github.com/espressif/esp-idf/tree/release/v5.1>`_ and above version branches, because lower versions may lack to support some parts of the important new features, especially for the ``RGB`` interface.
    - For LCDs using the ``3-wire SPI + RGB`` interface, please refer to the example `esp_lcd_st7701 - Example use <https://components.espressif.com/components/espressif/esp_lcd_st7701>`_.

Development Framework
-------------------------

.. _LCD Development Guide_Development_Framework:

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
