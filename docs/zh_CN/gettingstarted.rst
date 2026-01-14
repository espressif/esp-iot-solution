快速入门
==================

:link_to_translation:`en:[English]`

本文档简要介绍如何获取和使用 ESP-IoT-Solution 内的组件，如何编译和运行示例，帮助初学者快速上手。

ESP-IoT-Solution 版本说明
--------------------------

ESP-IoT-Solution 自 ``release/v2.0`` 起采用组件化管理，各组件与示例独立迭代，依赖的 ESP-IDF 版本请查阅组件 ``idf_component.yml`` 文件。release 分支仅维护历史大版本，master 分支持续集成新特性。建议新项目通过组件注册表获取所需组件。

不同版本说明如下，详情及组件列表请见 `README_CN.md <https://github.com/espressif/esp-iot-solution/blob/master/README_CN.md>`_：

.. list-table:: ESP-IoT-Solution 版本支持情况
   :header-rows: 1
   :widths: 20 20 30 20

   * - ESP-IoT-Solution
     - 依赖的 ESP-IDF
     - 主要变更
     - 支持状态
   * - master
     - >= v5.3
     - 新芯片支持
     - 新功能开发分支
   * - release/v2.0
     - <= v5.3, >= v4.4
     - 支持组件管理器
     - 历史版本维护
   * - release/v1.1
     - v4.0.1
     - IDF 版本更新，代码迁移
     - 备份，停止维护
   * - release/v1.0
     - v3.2.2
     - 历史版本
     - 备份，停止维护

.. note::

    不同芯片推荐的 ESP-IDF 首选版本也不同，具体可参考 `ESP-IDF 版本与乐鑫芯片版本兼容性 <https://github.com/espressif/esp-idf/blob/master/COMPATIBILITY_CN.md>`__。

开发环境搭建
--------------------------

ESP-IDF 是乐鑫为 ESP 系列芯片提供的物联网开发框架：

- ESP-IDF 包含一系列库及头文件，提供了基于 ESP SoC 构建软件项目所需的核心组件；
- ESP-IDF 还提供了开发和量产过程中最常用的工具及功能，例如：构建、烧录、调试和测量等。

.. note::

    请参考：`ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html>`__ 完成 ESP-IDF 开发环境的搭建。

硬件准备
--------------------------

您可以选择任意 ESP 系列开发板，或参考 `esp-bsp <https://github.com/espressif/esp-bsp>`__ 中支持的开发板快速开始。各系列芯片规格请见 `ESP 产品选型工具 <https://products.espressif.com/>`__。

ESP 系列 SoC 支持以下功能：

- Wi-Fi（2.4 GHz/5 GHz 双频）
- 蓝牙 5.x（BLE/Mesh）
- 高性能多核处理器，主频最高可达 400 MHz
- 超低功耗协处理器和深度睡眠模式
- 丰富的外设接口：
    - 通用接口：GPIO、UART、I2C、I2S、SPI、SDIO、USB OTG 等
    - 专用接口：LCD、摄像头、以太网、CAN、Touch、LED PWM、温度传感器等
- 大容量内存：
    - 内部 RAM 最大可达 768 KB
    - 支持外部 PSRAM 扩展
    - 支持外部 Flash 存储
- 增强的安全特性：
    - 硬件加密引擎
    - 安全启动
    - Flash 加密
    - 数字签名

ESP 系列 SoC 采用先进工艺制程，提供业界领先的射频性能、低功耗特性和稳定可靠性，适用于物联网、工业控制、智能家居、可穿戴设备等多种应用场景。

.. note::

   各系列芯片的具体规格和功能请参考 `ESP 产品选型工具 <https://products.espressif.com/>`__。


如何获取和使用组件
--------------------------

推荐通过 `ESP Component Registry <https://components.espressif.com/>`__ 获取 ESP-IoT-Solution 组件。

以 button 组件为例，添加依赖的步骤如下：

1. 在项目根目录下执行：

    .. code-block:: bash

        idf.py add-dependency "espressif/button"

2. 在代码中引用头文件并调用 API，例如：

    .. code-block:: c

        #include "iot_button.h"
        // 具体 API 使用请参考组件文档

更多组件管理器的使用方法请参考 `ESP Registry Docs <https://docs.espressif.com/projects/idf-component-manager/en/latest/>`__。

如何使用示例程序
--------------------------

ESP-IoT-Solution 提供了丰富的示例程序，帮助用户快速上手。以 ``button_power_save`` 示例为例：

1. 确保已经完成 ESP-IDF 开发环境的搭建，并成功配置了环境变量

2. 下载 ESP-IoT-Solution 代码仓库：

    .. code-block:: bash

        git clone https://github.com/espressif/esp-iot-solution.git

3. 进入示例目录或将其复制到您的工作目录：

    .. code-block:: bash

        cd examples/get-started/button_power_save

    .. note::

        如果您将示例复制到其他目录，由于文件路径发生变更，请删除 ``main/idf_component.yml`` 中所有 ``override_path`` 的配置。

4. 选择目标芯片（如 ESP32，首次使用或切换芯片时需执行）：

    .. code-block:: bash

        idf.py set-target esp32

5. 配置项目（可选）：

    .. code-block:: bash

        idf.py menuconfig

6. 编译并烧录到开发板：

    .. code-block:: bash

        idf.py build
        idf.py -p <PORT> flash

7. 通过串口监视输出：

    .. code-block:: bash

        idf.py -p <PORT> monitor

更多示例请见 ``examples/`` 目录，具体使用方法请参考各示例下的 README 文件。
