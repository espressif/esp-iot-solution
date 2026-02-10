Get Started
==================

:link_to_translation:`zh_CN:[中文]`

This document briefly introduces how to obtain and use components in ESP-IoT-Solution, how to compile and run examples, helping beginners get started quickly.

ESP-IoT-Solution Version Description
------------------------------------

ESP-IoT-Solution has adopted component-based management since ``release/v2.0``, with each component and example independently iterating. Please check the component's ``idf_component.yml`` file for the dependent ESP-IDF version. Release branches only maintain historical major versions, while the master branch continuously integrates new features. It is recommended that new projects obtain required components through the component registry.

Different version descriptions are as follows. For details and component lists, please see `README_CN.md <https://github.com/espressif/esp-iot-solution/blob/master/README_CN.md>`_:

.. list-table:: ESP-IoT-Solution Version Support
   :header-rows: 1
   :widths: 20 20 30 20

   * - ESP-IoT-Solution
     - Dependent ESP-IDF
     - Main Changes
     - Support Status
   * - master
     - >= v5.3
     - New chip support
     - New feature development branch
   * - release/v2.0
     - <= v5.3, >= v4.4
     - Component manager support
     - Historical version maintenance
   * - release/v1.1
     - v4.0.1
     - IDF version update, code migration
     - Backup, maintenance stopped
   * - release/v1.0
     - v3.2.2
     - Historical version
     - Backup, maintenance stopped

.. note::

    The recommended ESP-IDF version varies for different chips. For details, please refer to `Compatibility Between ESP-IDF Releases and Revisions of Espressif SoCs <https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY.md>`__.

Development Environment Setup
-----------------------------

ESP-IDF is the IoT development framework provided by Espressif for ESP series chips:

- ESP-IDF includes a series of libraries and header files, providing core components required for building software projects based on ESP SoC
- ESP-IDF also provides the most commonly used tools and functions during development and mass production, such as: build, flash, debug and measurement.

.. note::

    Please refer to: `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html>`__ to complete the ESP-IDF development environment setup.

Hardware Preparation
--------------------

You can choose any ESP series development board, or refer to the supported development boards in `esp-bsp <https://github.com/espressif/esp-bsp>`__ for a quick start. For specifications of each series chip, please see `ESP Product Selector <https://products.espressif.com/>`__.

ESP series SoCs support the following features:

- Wi-Fi (2.4 GHz/5 GHz dual-band)
- Bluetooth 5.x (BLE/Mesh)
- High-performance multi-core processor, maximum frequency up to 400 MHz
- Ultra-low power coprocessor and deep sleep mode
- Rich peripheral interfaces:
    - General interfaces: GPIO, UART, I2C, I2S, SPI, SDIO, USB OTG, etc.
    - Dedicated interfaces: LCD, camera, Ethernet, CAN, Touch, LED PWM, temperature sensor, etc.
- Large capacity memory:
    - Internal RAM up to 768 KB
    - Support for external PSRAM expansion
    - Support for external Flash storage
- Enhanced security features:
    - Hardware encryption engine
    - Secure boot
    - Flash encryption
    - Digital signature

ESP series SoCs use advanced process technology, providing industry-leading RF performance, low power consumption characteristics and stable reliability, suitable for IoT, industrial control, smart home, wearable devices and other application scenarios.

.. note::

   For specific specifications and functions of each series chip, please refer to `ESP Product Selector <https://products.espressif.com/>`__.


How to Obtain and Use Components
--------------------------------

It is recommended to obtain ESP-IoT-Solution components through `ESP Component Registry <https://components.espressif.com/>`__.

Taking the button component as an example, the steps to add dependencies are as follows:

1. Execute in the project root directory:

    .. code-block:: bash

        idf.py add-dependency "espressif/button"

2. Reference header files in code and call APIs, for example:

    .. code-block:: c

        #include "iot_button.h"
        // For specific API usage, please refer to component documentation

For more information on component manager usage, please refer to `ESP Registry Docs <https://docs.espressif.com/projects/idf-component-manager/en/latest/>`__.

How to Use Example Programs
---------------------------

ESP-IoT-Solution provides rich example programs to help users get started quickly. Taking the ``button_power_save`` example:

1. Ensure that the ESP-IDF development environment setup has been completed and environment variables have been successfully configured

2. Download the ESP-IoT-Solution code repository:

    .. code-block:: bash

        git clone https://github.com/espressif/esp-iot-solution.git

3. Enter the example directory or copy it to your working directory:

    .. code-block:: bash

        cd examples/get-started/button_power_save

    .. note::

        If you copy the example to another directory, due to file path changes, please delete all ``override_path`` configurations in ``main/idf_component.yml``.

4. Select target chip (such as ESP32, required when using for the first time or switching chips):

    .. code-block:: bash

        idf.py set-target esp32

5. Configure project (optional):

    .. code-block:: bash

        idf.py menuconfig

6. Compile and flash to development board:

    .. code-block:: bash

        idf.py build
        idf.py -p <PORT> flash

7. Monitor output through serial port:

    .. code-block:: bash

        idf.py -p <PORT> monitor

For more examples, please see the ``examples/`` directory. For specific usage methods, please refer to the README files under each example.
