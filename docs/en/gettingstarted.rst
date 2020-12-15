Getting Started
=================



Quick Start
-------------

Hardware
~~~~~~~~~~~~

* [ESP32_ULP_EB_V1 Evaluation Board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
* [ESP32-Sense kit Board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
    * [ESP32 Touch Sensor development kit](./documents/evaluation_boards/ESP32-MeshKit-Sense_guide_en.md)[[中文]](./documents/evaluation_boards/ESP32-MeshKit-Sense_guide_cn.md)
* [ESP-Prog board](./documents/evaluation_boards/readme_en.md) [[中文]](./documents/evaluation_boards/readme_cn.md)
    * [Firmware download and JTAG debug tool](./documents/evaluation_boards/ESP-Prog_guide_en.md)[[中文]](./documents/evaluation_boards/ESP-Prog_guide_cn.md)

Setting up Development Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. [Install ESP-IDF v4.0.1 Step By Step](https://docs.espressif.com/projects/esp-idf/en/v4.0.1/get-started/index.html#installation-step-by-step)

>Current branch based on ESP-IDF `v4.0.1`

2. Get ESP-IoT-Solution

.. code:: shell

    git clone --recursive https://github.com/espressif/esp-iot-solution

3. Initialize the Submodules

.. code:: shell

    git submodule update --init --recursive

4. Set up the Environment Variables

.. code:: shell

    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution

Creating Your Project (CMake)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* We can regard IoT solution project as a platform that contains different device drivers and features
* `Add-on project`: If you want to use those drivers and build your project base on the framework, you need to include the IoT components into your project's `CMakeLists.txt`.

.. code:: 

    cmake_minimum_required(VERSION 3.5)

    include($ENV{IOT_SOLUTION_PATH}/component.cmake)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

    project(empty-project)


* `Stand-alone component`: if you just want to pick one of the component and put it into your existing project, you just need to copy the single component to your project components directory. But you can also append the component list in your project's `CMakeLists.txt` like this:

.. code:: 

    set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} ${IOT_SOLUTION_PATH}/components/{component_you_choose}")


Creating Your Project (Legacy GNU Make)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* We can regard IoT solution project as a platform that contains different device drivers and features
* `Add-on project`: If you want to use those drivers and build your project base on the framework, you need to include the IoT components into your project's `Makefile`.

.. code:: 

    include $(IOT_SOLUTION_PATH)/component.mk
    include $(IDF_PATH)/make/project.mk


* `Stand-alone component`: if you just want to pick one of the component and put it into your existing project, you just need to copy the single component to your project components directory. But you can also append the component list in your project Makefile like this:

.. code:: 

    EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/{component_you_choose}

