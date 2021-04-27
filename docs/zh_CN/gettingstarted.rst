快速入门
=================

:link_to_translation:`en:[English]`

本文档旨在指导用户搭建 ESP-IoT-Solution (Espressif IoT Solution) 开发环境，通过一个简单的示例展示如何使用 ESP-IoT-Solution 搭建环境、创建工程、编译和下载固件至 ESP32/ESP32-S 系列开发板等步骤。

ESP-IoT-Solution 简介
~~~~~~~~~~~~~~~~~~~~~~~~~~~

ESP-IoT-Solution 包含物联网系统开发中常用的外设驱动和代码框架，可作为 ESP-IDF 的补充组件，方便用户实现更简单的开发，其中包含的内容如下：

- 传感器、显示屏、音频设备、输入设备、执行机构等设备驱动；
- 低功耗、安全加密、存储方案等代码框架或说明文档；
- 从实际应用的角度出发，为乐鑫开源解决方案提供了入口指引。

ESP-IoT-Solution 版本
**************************

不同版本的 ESP-IoT-Solution 说明如下：

+-----------------------+---------------------+--------------------------------------------+----------------+
| ESP-IoT-Solution 版本 | 对应的 ESP-IDF 版本 |                  主要变更                  |    支持状态    |
+=======================+=====================+============================================+================+
| master                | release/v4.3        | 代码结构变更，增加支持 ESP32-S2            | 新功能开发分支 |
+-----------------------+---------------------+--------------------------------------------+----------------+
| release/v1.1          | v4.0.1              | IDF 版本更新，删除已经移动到其它仓库的代码 | 有限维护       |
+-----------------------+---------------------+--------------------------------------------+----------------+
| release/v1.0          | v3.2.2              | 历史版本                                   | 停止维护       |
+-----------------------+---------------------+--------------------------------------------+----------------+

ESP-IDF 简介
~~~~~~~~~~~~~~~~~~~~~~~

ESP-IDF 是乐鑫为 ESP32/ESP32-S2 提供的物联网开发框架：

- ESP-IDF 包含一系列库及头文件，提供了基于 ESP32/ESP32-S2 构建软件项目所需的核心组件;
- ESP-IDF 还提供了开发和量产过程中最常用的工具及功能，例如：构建、烧录、调试和测量等。

.. Note::

    详情请查阅：`ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html>`__。


ESP32/ESP32-S 简介
~~~~~~~~~~~~~~~~~~~~~~~~~

您可以选择任意 ESP32/ESP32-S 系列开发板使用 ESP-IoT-Solution，或者选择 `板级支持组件 <./basic/boards.html>`_ 中支持的开发板快速开始。

ESP32/ESP32-S 系列 SoC 支持以下功能：

- 2.4 GHz Wi-Fi
- 蓝牙
- 高性能单核、双核处理器，运行频率可达 240 MHz
- 超低功耗协处理器
- 多种外设，包括 GPIO、I2C、I2S、SPI、UART、SDIO、RMT、LEDC PWM、Ethernet、TWAI、Touch、USB OTG 等
- 丰富的内存资源，内部 RAM 可达 520 KB，同时支持扩展 PSRAM
- 支持硬件加密等安全功能

ESP32/ESP32-S 系列 SoC 采用 40 nm 工艺制成，具有最佳的功耗性能、射频性能、稳定性、通用性和可靠性，适用于各种应用场景和不同功耗需求。

.. Note::

    不同系列 SoC 配置不同，详情请查阅 `ESP 产品选型工具 <http://products.espressif.com:8000/#/product-selector>`_。


配置开发环境
~~~~~~~~~~~~~~~~

1. 安装 ESP-IDF
*******************

由于 ESP-IoT-Solution 依赖 ESP-IDF 的基础功能和编译工具，因此首先需要参考 `ESP-IDF 详细安装步骤 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-get-prerequisites>`_ 完成 ESP-IDF 开发环境的搭建。请注意，不同版本的 ESP-IoT-Solution 依赖的 ESP-IDF 版本可能不同，请参考 `ESP-IoT-Solution 版本`_ 进行选择。

2. 获取 ESP-IoT-Solution
*****************************

若选择 ``master`` 版本，可使用以下指令获取代码：

.. code:: shell

    git clone --recursive https://github.com/espressif/esp-iot-solution

对于 ``release/v1.1`` 版本，可使用以下指令获取代码：

.. code:: shell

    git clone -b release/v1.1 --recursive https://github.com/espressif/esp-iot-solution

对于其它版本，请将 ``release/v1.1`` 替换成目标分支名。

使用 ESP-IoT-Solution 组件
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

您可以参考以下几种方法使用 ESP-IoT-Solution 中的组件：

方法 1. 添加 ESP-IoT-Solution 所有组件到工程目录：可直接在工程的 ``CMakeLists.txt`` 中添加以下代码：

    .. code:: 

        cmake_minimum_required(VERSION 3.5)

        include($ENV{IOT_SOLUTION_PATH}/component.cmake)
        include($ENV{IDF_PATH}/tools/cmake/project.cmake)

        project(empty-project)

方法 2. 添加 ESP-IoT-Solution 指定组件到工程目录: 可直接在工程的 ``CMakeLists.txt`` 中添加以下代码：

    .. code:: 

        set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} $ENV{IOT_SOLUTION_PATH}/components/{component_you_choose}")
        #请将{component_you_choose} 替换为组件名称，如果有多个组件，可重复该命令

方法 3. 复制 ESP-IoT-Solution 指定组件到工程目录: 直接将该组件和其依赖的组件，复制粘贴至工程的 ``components`` 文件夹。

.. Note::

    ESP-IoT-Solution 推荐使用以 CMake 为基础的编译系统（IDF v4.0 及以后版本默认编译系统），如果需要使用 GNU Make 编译系统可参考 `老版本 GNU Make <https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/api-guides/build-system-legacy.html>`_。

编译和下载
~~~~~~~~~~~~~~~~

1. 设置环境变量
********************

以上步骤安装的代码和工具尚未添加至 PATH 环境变量，无法通过终端窗口使用这些工具。添加环境变量的步骤如下：

* 添加 ESP-IDF 环境变量：

    Windows 在 CMD 窗口运行：

    .. code:: shell

        %userprofile%\esp\esp-idf\export.bat

    Linux 和 macOS 在终端运行：

    .. code:: shell

        . $HOME/esp/esp-idf/export.sh
    
    请将以上指令中的路径，替换成实际安装路径。

* 添加 IOT_SOLUTION_PATH 环境变量：

    Windows 在 CMD 窗口运行：

    .. code:: shell

        set IOT_SOLUTION_PATH=C:\esp\esp-iot-solution

    Linux 和 macOS 在终端运行：

    .. code:: shell

        export IOT_SOLUTION_PATH=~/esp/esp-iot-solution

.. Note::

    以上方法设置的环境变量，仅对当前终端有效，如果打开新终端，请重新执行以上步骤。

2. 设置编译目标
********************

ESP-IDF 同时支持 ``esp32``、``esp32s2`` 等多款芯片，因此需要在编译代码之前设置的编译目标芯片（默认编译目标为 ``esp32``），例如设置编译目标为 ``esp32s2``：

.. code:: shell

    idf.py set-target esp32s2

对于 ESP-IoT-Solution 中基于 `板级支持组件 <./basic/boards.html>`_ 开发的 example，还可以使用 ``menuconfig`` 在 ``Board Options -> Choose Target Board`` 中选择一个目标开发板：

.. code:: shell

    idf.py menuconfig

3. 编译、下载程序
**********************

使用 ``idf.py`` 工具编译、下载程序，指令为：

.. code:: shell

    idf.py -p PORT build flash

请将 PORT 替换为当前使用的端口号，Windows 系统串口号一般为 ``COMx``，Linux 系统串口号一般为 ``/dev/ttyUSBx``，macOS 串口号一般为 ``/dev/cu.``。

4. 串口打印 log
*******************

使用 ``idf.py`` 工具查看 log，指令为：

.. code:: shell

    idf.py -p PORT monitor

请将 ``PORT`` 替换为当前使用的端口号，Windows 系统串口号一般为 ``COMx``，Linux 系统串口号一般为 ``/dev/ttyUSBx``，macOS 串口号一般为 ``/dev/cu.``。

相关文档
~~~~~~~~~~~~~~~~

- `ESP-IDF 详细安装步骤 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-get-prerequisites>`_
- `ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html>`__
- `ESP 产品选型工具 <http://products.espressif.com:8000/#/product-selector>`_