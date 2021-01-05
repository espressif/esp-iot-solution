入门指南
=================

:link_to_translation:`en:[English]`

配置开发环境
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. `安装IDF v4.0.1 <https://docs.espressif.com/projects/esp-idf/en/v4.0.1/get-started/index.html#installation-step-by-step>`_

.. note:: 目前使用的IDF版本为 `v4.0.1`

2. 获取 ESP-IoT-Solution

.. code:: shell

    git clone --recursive https://github.com/espressif/esp-iot-solution

3. 初始化子模块

.. code:: shell

    git submodule update --init --recursive

4. 设置环境变量

.. code:: shell

    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution

创建工程 (CMake)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* IoT solution 中的工程可以被看做是包含各种设备和驱动的综合平台。
* `Add-on project`: 如果你想利用这些驱动的固件创建自己的工程文件，你需要将IoT组件加入到你的工程文件目录下的 `CMakeLists.txt` 中。

.. code:: 

    cmake_minimum_required(VERSION 3.5)

    include($ENV{IOT_SOLUTION_PATH}/component.cmake)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

    project(empty-project)


* `Stand-alone component`: 如果你想要将某个组件放入已有的工程文件中，你只需要单独将某个组件复制到工程目录下存放组件的文件夹中。你也可以通过将组件列表链接到你的工程中的 `CMakeLists.txt` 来实现:

.. code:: 

    set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} ${IOT_SOLUTION_PATH}/components/{component_you_choose}")


创建工程 (Legacy GNU Make)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* IoT solution 中的工程可以被看做是包含各种设备和驱动的综合平台。
* `Add-on project`: 如果你想利用这些驱动的固件创建自己的工程文件，你需要将IoT组件加入到你的工程文件目录下的 `Makefile` 中。

.. code:: 

    include $(IOT_SOLUTION_PATH)/component.mk
    include $(IDF_PATH)/make/project.mk


* `Stand-alone component`: 如果你想要将某个组件放入已有的工程文件中，你只需要单独将某个组件复制到工程目录下存放组件的文件夹中。你也可以通过将组件列表链接到你的工程中的 `Makefile` 来实现:

.. code:: 

    EXTRA_COMPONENT_DIRS += $(IOT_SOLUTION_PATH)/components/{component_you_choose}

