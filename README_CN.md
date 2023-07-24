[![Documentation Status](https://dl.espressif.com/AE/docs/docs_latest.svg)](https://docs.espressif.com/projects/esp-iot-solution/zh_CN)

## Espressif IoT Solution 概述

* [English Version](./README.md)

ESP-IoT-Solution 包含物联网系统开发中常用的外设驱动和代码框架，提供了 ESP-IDF 的扩展组件，方便用户实现更简单的开发。

ESP-IoT-Solution 包含的内容如下:

* 传感器、显示屏、音频设备、输入设备、执行机构等设备驱动；
* 低功耗、安全加密、存储方案等代码框架或说明文档；
* 从实际应用的角度出发，为乐鑫开源解决方案提供了入口指引。

## 文档中心

- 中文：https://docs.espressif.com/projects/esp-iot-solution/zh_CN
- English: https://docs.espressif.com/projects/esp-iot-solution/en

## 快速参考

### 硬件准备

您可以选择任意 ESP 系列开发板使用 ESP-IoT-Solution，或者选择[板级支持组件](./examples/common_components/boards)中支持的开发板快速开始。

ESP 系列 SoC 采用 40nm 工艺制成，具有最佳的功耗性能、射频性能、稳定性、通用性和可靠性，适用于各种应用场景和不同功耗需求。

### 环境搭建

#### 搭建 ESP-IDF 开发环境

由于 ESP-IoT-Solution 依赖 ESP-IDF 的基础功能和编译工具，因此首先需要参考 [ESP-IDF 详细安装步骤](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-step-by-step) 完成 ESP-IDF 开发环境的搭建。

请注意，不同 ESP-IoT-Solution 版本 依赖的 ESP-IDF 版本可能不同，请参考下表进行选择。

| ESP-IoT-Solution | 依赖的 ESP-IDF |                  主要变更              |            文档              |        支持状态        |
| :--------------: | :------------: | :----------------------------------: |:------------------------------: | ---------------------- |
|      master      |   >= v4.4 |     支持组件管理器，增加新的芯片支持     | [Docs online](https://docs.espressif.com/projects/esp-iot-solution/zh_CN) | 新功能开发分支         |
|   release/v1.1   |     v4.0.1     | IDF 版本更新，删除已经移动到其它仓库的代码  | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) | 备份，停止维护 |
|   release/v1.0   |     v3.2.2     | 历史版本                               | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | 备份，停止维护  |

> `master` 分支使用 `ESP 组件管理器` 来管理组件，因此每个组件都是一个单独的软件包，每个包可能支持不同版本的 ESP-idf，这些版本将在组件的 `idf_component.yml` 文件中声明。

#### 从 ESP 组件注册表获取组件

如果您只想使用 ESP-IoT-Solution 中的组件，我们建议您从 ESP 组件注册表 [ESP Component Registry](https://components.espressif.com/) 中使用它。

ESP-IoT-Solution 中注册的组件如下:

<center>

|                                                  组件名                                                  |                                                                                              版本                                                                                               |
| :------------------------------------------------------------------------------------------------------: | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------: |
|                 [ble_ota](https://components.espressif.com/components/espressif/ble_ota)                 |                 [![Component Registry](https://components.espressif.com/components/espressif/ble_ota/badge.svg)](https://components.espressif.com/components/espressif/ble_ota)                 |
| [bootloader_support_plus](https://components.espressif.com/components/espressif/bootloader_support_plus) | [![Component Registry](https://components.espressif.com/components/espressif/bootloader_support_plus/badge.svg)](https://components.espressif.com/components/espressif/bootloader_support_plus) |
|                  [button](https://components.espressif.com/components/espressif/button)                  |                  [![Component Registry](https://components.espressif.com/components/espressif/button/badge.svg)](https://components.espressif.com/components/espressif/button)                  |
|         [cmake_utilities](https://components.espressif.com/components/espressif/cmake_utilities)         |         [![Component Registry](https://components.espressif.com/components/espressif/cmake_utilities/badge.svg)](https://components.espressif.com/components/espressif/cmake_utilities)         |
|              [esp_tinyuf2](https://components.espressif.com/components/espressif/esp_tinyuf2)              |              [![Component Registry](https://components.espressif.com/components/espressif/esp_tinyuf2/badge.svg)](https://components.espressif.com/components/espressif/esp_tinyuf2)              |
|            [extended_vfs](https://components.espressif.com/components/espressif/extended_vfs)            |            [![Component Registry](https://components.espressif.com/components/espressif/extended_vfs/badge.svg)](https://components.espressif.com/components/espressif/extended_vfs)            |
|                   [gprof](https://components.espressif.com/components/espressif/gprof)                   |                   [![Component Registry](https://components.espressif.com/components/espressif/gprof/badge.svg)](https://components.espressif.com/components/espressif/gprof)                   |
|                [iot_usbh](https://components.espressif.com/components/espressif/iot_usbh)                |                [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh)                |
|            [iot_usbh_cdc](https://components.espressif.com/components/espressif/iot_usbh_cdc)            |            [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_cdc/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_cdc)            |
|          [iot_usbh_modem](https://components.espressif.com/components/espressif/iot_usbh_modem)          |          [![Component Registry](https://components.espressif.com/components/espressif/iot_usbh_modem/badge.svg)](https://components.espressif.com/components/espressif/iot_usbh_modem)          |
|                    [knob](https://components.espressif.com/components/espressif/knob)                    |                    [![Component Registry](https://components.espressif.com/components/espressif/knob/badge.svg)](https://components.espressif.com/components/espressif/knob)                    |
|           [led_indicator](https://components.espressif.com/components/espressif/led_indicator)           |           [![Component Registry](https://components.espressif.com/components/espressif/led_indicator/badge.svg)](https://components.espressif.com/components/espressif/led_indicator)           |
|        [lightbulb_driver](https://components.espressif.com/components/espressif/lightbulb_driver)        |        [![Component Registry](https://components.espressif.com/components/espressif/lightbulb_driver/badge.svg)](https://components.espressif.com/components/espressif/lightbulb_driver)        |
|        [openai](https://components.espressif.com/components/espressif/openai)        |        [![Component Registry](https://components.espressif.com/components/espressif/openai/badge.svg)](https://components.espressif.com/components/espressif/openai)        |
|               [pwm_audio](https://components.espressif.com/components/espressif/pwm_audio)               |               [![Component Registry](https://components.espressif.com/components/espressif/pwm_audio/badge.svg)](https://components.espressif.com/components/espressif/pwm_audio)               |
|              [usb_stream](https://components.espressif.com/components/espressif/usb_stream)              |              [![Component Registry](https://components.espressif.com/components/espressif/usb_stream/badge.svg)](https://components.espressif.com/components/espressif/usb_stream)              |
|                      [xz](https://components.espressif.com/components/espressif/xz)                      |                      [![Component Registry](https://components.espressif.com/components/espressif/xz/badge.svg)](https://components.espressif.com/components/espressif/xz)                      |

</center>

可以在项目根目录下使用 `idf.py add-dependency` 命令直接将组件从 Component Registry 添加到项目中。例如，执行 `idf.py add-dependency "expressif/usb_stream"` 命令添加 `usb_stream`，该组件将在 `CMake` 步骤中自动下载。

> 请参考 [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html) 查看更多关于组件管理器的细节.

#### 从 ESP-IoT-Solution 仓库获取组件

如果您想为 `ESP-IoT-Solution` 中的组件或示例[贡献代码](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/contribute/index.html)，或者想基于 `ESP-IoT-Solution` 中的示例开发项目，您可以通过以下步骤下载 ESP-IoT-Solution 代码仓库:

* 如果选择 `master` 版本，可使用以下指令获取代码：

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* 如果选择 `release/v1.1` 版本，可使用以下指令获取代码：

    ```
    git clone -b release/v1.1 --recursive https://github.com/espressif/esp-iot-solution
    ```

#### 编译和下载示例

**我们强烈建议**您 [构建您的第一个项目](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#build-your-first-project) 以熟悉 ESP-IDF 并确保环境已经设置正确。

在 ESP-IoT-Solution 和 ESP-IDF 中构建和烧录示例没有区别。 在大多数情况下，您可以按照以下步骤在 ESP-IoT-Solution 中构建和烧录示例：

1. 将当前目录更改为示例目录，例如 `cd examples/usb/host/usb_audio_player`；
2. 运行 `idf.py set-target TARGET` 设置目标芯片。 例如 `idf.py set-target esp32s3` 将目标芯片设置为 `ESP32-S3`；
3. 运行 `idf.py build` 来构建示例；
4. 运行 `idf.py -p PORT flash monitor` 烧录示例，并查看串口输出。 例如 `idf.py -p /dev/ttyUSB0 flash monitor` 将示例烧录到 `/dev/ttyUSB0` 端口，并打开串口监视器。

> 某些示例可能需要额外的步骤来设置 `ESP-IoT-Solution` 环境变量，您可以在 Linux/MacOS 中运行 `export IOT_SOLUTION_PATH=~/esp/esp-iot-solution` 或 `set IOT_SOLUTION_PATH=C:\esp\esp- iot-solution` 在 Windows 中设置环境。

### 其它参考资源

- 最新版的文档：https://docs.espressif.com/projects/esp-iot-solution/zh_CN ，该文档是由本仓库 [docs](./docs) 目录 构建得到；
- ESP-IDF 编程指南 https://docs.espressif.com/projects/esp-idf/zh_CN ，请参考 ESP-IoT-Solution 依赖的版本；
- 可以在 [ESP Component Registry](https://components.espressif.com/)中找到 `ESP-IoT-Solution` 中的组件和其他已注册的组件;
- 可以前往 [esp32.com](https://www.esp32.com/) 论坛 提问，挖掘社区资源；
- 如果你在使用中发现了错误或者需要新的功能，请先查看 [GitHub Issues](https://github.com/espressif/esp-iot-solution/issues)，确保该问题不会被重复提交；
- 如果你有兴趣为 ESP-IoT-Solution 作贡献，请先阅读[贡献指南](./CONTRIBUTING.rst)。