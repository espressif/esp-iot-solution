Get Started
=================

:link_to_translation:`zh_CN:[中文]`

This document is intended to help you set up the development environment for ESP-IoT-Solution (Espressif IoT Solution). After that, a simple example will show you how to use ESP-IoT-Solution to set up environment, create a project, build and flash firmware onto an ESP32/ESP32-S series board, etc.

ESP-IoT-Solution Introduction
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ESP-IoT-Solution contains peripheral drivers and code frameworks commonly used in IoT system development, which can be used as a complementary component to ESP-IDF to facilitate simpler development, mainly including the following contents:

- Device drivers such as sensors, screens, audio devices, input devices, actuators, and etc.
- Code framework and related documents of low power management, security encryption, storage and etc.
- Entrance guideline for Espressif's open-source solutions from the perspective of practical application.

ESP-IoT-Solution Versions
******************************

Specifications of different ESP-IoT-Solution versions are listed in the following table:

+--------------------------+-------------------------------+----------------------------------------------------+---------------------------------+
| ESP-IoT-Solution version | Corresponding ESP-IDF version | Main changes                                       | Status                          |
+==========================+===============================+====================================================+=================================+
| master                   | release/v4.3                  | Code structure changed, added ESP32-S2 support     | New features development branch |
+--------------------------+-------------------------------+----------------------------------------------------+---------------------------------+
| release/v1.1             | v4.0.1                        | IDF version updated, deleted codes that have been  | Limited maintenance             |
|                          |                               | moved to other repos                               |                                 |
+--------------------------+-------------------------------+----------------------------------------------------+---------------------------------+
| release/v1.0             | v3.2.2                        | Legacy version                                     | Stop maintenance                |
+--------------------------+-------------------------------+----------------------------------------------------+---------------------------------+

ESP-IDF Introduction
~~~~~~~~~~~~~~~~~~~~~~~

ESP-IDF is the IoT development framework for ESP32/ESP32-S2 series SoCs provided by Espressif, including:

- A series of libraries and header files, providing core components required for building software projects based on ESP32/ESP32-S2;
- Common tools and functions used during the development and manufacturing processes, e.g., build, flashing, debugging, measurement and etc.

.. Note::

    For detailed information, please go to `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html>`_.


ESP32/ESP32-S Introduction
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can select any development board from ESP32/ESP32-S series to get started with ESP-IoT-Solution, or select a supported board from the `Boards Component <./basic/boards.html>`_ directly for a quick start.

ESP32/ESP32-S series SoCs support the following features:

- 2.4 GHz Wi-Fi
- Bluetooth
- High-performance single core, dual-core processor, capable of running at 240 MHz
- Ultra-low-power co-processor
- Various peripherals including GPIO, I2C, I2S, SPI ,UART, SDIO, RMT, LEDC PWM, Ethernet, TWAI®, Touch, USB OTG and etc.
- Rich memory resources, including up to 520 KB internal RAM and can support external PSRAM
- Support security functions, e.g., hardware encryption

ESP32/ESP32-S series of SoCs are designed with the 40 nm technology, showing the best power and RF performance, versatility and reliability in a wide variety of application and power scenarios.

.. Note::

    The configuration varies by SOC series, please refer to `ESP Product Selector <http://products.espressif.com:8000/#/product-selector>`_ for details.


Setting up Development Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Get ESP-IDF
*******************

As ESP-IoT-Solution relies on ESP-IDF basic functions and build tools, please set up ESP-IDF development environment first following `ESP-IDF Installation Step by Step <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-get-prerequisites>`_. Please note that different versions of ESP-IoT-Solution may rely on different ESP-IDF versions, please refer to `ESP-IoT-Solution Versions`_ for specifications.

2. Get ESP-IoT-Solution
*****************************

For ``master`` version, please use the following command:

.. code:: shell

    git clone --recursive https://github.com/espressif/esp-iot-solution

For ``release/v1.1`` version, please use the following command:

.. code:: shell

    git clone -b release/v1.1 --recursive https://github.com/espressif/esp-iot-solution

For other versions, please also use this command with ``release/v1.1`` replaced by your target branch name.

Use ESP-IoT-Solution Components
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following ways can be used to add ESP-IoT-Solution components:

1. Add all components from ESP-IoT-Solution to the project directory by inserting the following code to ``CMakeLists.txt`` under the project:

    .. code:: 

        cmake_minimum_required(VERSION 3.5)

        include($ENV{IOT_SOLUTION_PATH}/component.cmake)
        include($ENV{IDF_PATH}/tools/cmake/project.cmake)

        project(empty-project)

2. Add specific components from ESP-IoT-Solution to the project directory by inserting the following code to ``CMakeLists.txt`` under the project:

    .. code:: 

        set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} $ENV{IOT_SOLUTION_PATH}/components/{component_you_choose}")
        #Please replace {component_you_choose} with the component name you choose. This command can be repeated if you need to add multiple components.

3. Copy specific components from ESP-IoT-Solution to the project directory by directly copying and pasting the components and their dependencies into the ``components`` folder under the project.

.. Note::

    It is recommended to use the build system based on CMake (the default build system for ESP-IDF v4.0 and later versions) for ESP-IoT-Solution. If you need to use the GNU Make system, please refer to `Build System (Legacy GNU Make) <https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/api-guides/build-system-legacy.html>`_.

Build and Download
~~~~~~~~~~~~~~~~~~~~~~

1. Set up the environment variables
********************************************

The tools installed in above steps are not yet added to the PATH environment variables. To make the tools usable from the command line, please follow the following steps to add environment variables:

* Add ESP-IDF environment variables:

    For Windows system, please open the Command Prompt and run:

    .. code:: shell

        %userprofile%\esp\esp-idf\export.bat

    For Linux and macOS, please run:

    .. code:: shell

        . $HOME/esp/esp-idf/export.sh
    
    Please remember to replace the paths in above commands as your actual paths.

* Add IOT_SOLUTION_PATH environment variables:

    For Windows system, please open the Command Prompt and run:

    .. code:: shell

        set IOT_SOLUTION_PATH=C:\esp\esp-iot-solution

    For Linux and macOS, please run:

    .. code:: shell

        export IOT_SOLUTION_PATH=~/esp/esp-iot-solution

.. Note::

    The environment variables set by the above method are only valid in the current terminal. Please repeat above steps if you open a new terminal.

2. Set build target
***********************

ESP-IDF supports multiple chips as ``esp32``, ``esp32s2`` and others, please set your target chip before building (the default target is ``esp32``). For example, you can set the build target as ``esp32s2``.

.. code:: shell

    idf.py set-target esp32s2

For examples in ESP-IoT-Solution developed based on `Boards Component <./basic/boards.html>`_, you can go to ``Board Options -> Choose Target Board`` in ``menuconfig`` to choose a target board: 

.. code:: shell

    idf.py menuconfig

3. Build and download the program
***************************************

Use the ``idf.py`` tool to build and download the program with:

.. code:: shell

    idf.py -p PORT build flash

Please replace PORT with your board's port name. Serial ports have the following patterns in their names: Windows is like ``COMx``; Linux starting with ``/dev/ttyUSBx``; macOS usually is ``/dev/cu.``.

4. Serial print log
***********************

Use the ``idf.py`` tool to see logs:

.. code:: shell

    idf.py -p PORT monitor

Do not forget to replace PORT with your serial port name (``COMx`` for Windows; ``/dev/ttyUSBx`` for Linux; ``/dev/cu.`` for macOS).

Related Documents
~~~~~~~~~~~~~~~~~~~~~~~~~

- `ESP-IDF Installation Step by Step <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-get-prerequisites>`_
- `ESP-IDF Get Started <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html>`_
- `ESP Product Selector <http://products.espressif.com:8000/#/product-selector>`_
