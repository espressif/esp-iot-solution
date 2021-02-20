[![Documentation Status](https://readthedocs.com/projects/espressif-esp-iot-solution-zh-cn/badge/?version=latest)](https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN/latest/?badge=latest)

## Espressif IoT Solution 概述

* [English Version](./README.md)

ESP-IoT-Solution 包含物联网系统开发中常用的外设驱动和代码框架，可作为 ESP-IDF 的补充组件，方便用户实现更简单的开发.

ESP-IoT-Solution 包含的内容如下:

* 传感器、显示屏、音频设备、输入设备、执行机构等设备驱动；
* 低功耗、安全加密、存储方案等代码框架或说明文档；
* 从实际应用的角度出发，为乐鑫开源解决方案提供了入口指引。

## 文档中心

- 中文：https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN
- English:https://docs.espressif.com/projects/espressif-esp-iot-solution/en

## 版本信息

| ESP-IoT-Solution | 依赖的 ESP-IDF |                  主要变更              |            文档              |        支持状态        |
| :--------------: | :------------: | :----------------------------------: |:------------------------------: | ---------------------- |
|      master      |   release/v4.3 |      代码结构变更，增加支持 ESP32-S2     | [Docs online](https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN) | 新功能开发分支         |
|   release/v1.1   |     v4.0.1     | IDF 版本更新，删除已经移动到其它仓库的代码  | [v1.1 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.1#esp32-iot-solution-overview) | 有限维护 |
|   release/v1.0   |     v3.2.2     | 历史版本                               | [v1.0 Overview](https://github.com/espressif/esp-iot-solution/tree/release/v1.0#esp32-iot-solution-overview) | 备份，停止维护  |


## 快速参考

### 硬件准备

您可以选择任意 ESP32/ESP32-S 系列开发板使用 ESP-IoT-Solution，或者选择[板级支持组件](./examples/common_components/boards)中支持的开发板快速开始。

ESP32/ESP32-S 系列 SoC 采用 40 nm 工艺制成，具有最佳的功耗性能、射频性能、稳定性、通用性和可靠性，适用于各种应用场景和不同功耗需求。

### 环境搭建

#### 安装 ESP-IDF 开发环境


由于 ESP-IoT-Solution 依赖 ESP-IDF 的基础功能和编译工具，因此首先需要参考 [ESP-IDF 详细安装步骤](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-step-by-step) 完成 ESP-IDF 开发环境的搭建。

请注意，不同 ESP-IoT-Solution 版本 依赖的 ESP-IDF 版本可能不同，请参考[版本信息](#版本信息)进行选择。

#### 获取 ESP-IoT-Solution 源码

* 如果选择 `master` 版本，可使用以下指令获取代码：

    ```
    git clone --recursive https://github.com/espressif/esp-iot-solution
    ```

* 如果选择 `release/v1.1` 版本，可使用以下指令获取代码：

    ```
    git clone -b release/v1.1 --recursive https://github.com/espressif/esp-iot-solution
    ```

对于其它版本，请将 `release/v1.1` 替换成目标分支名

#### 使用 ESP-IoT-Solution 组件

您可以参考以下几种方法使用 ESP-IoT-Solution 中的组件：

**方法 1**. 添加 ESP-IoT-Solution 所有组件到工程目录：可直接在工程的 CMakeLists.txt 中添加以下代码：

```
cmake_minimum_required(VERSION 3.5)

include($ENV{IOT_SOLUTION_PATH}/component.cmake)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(empty-project)
```

**方法 2**. 添加 ESP-IoT-Solution 指定组件到工程目录：可直接在工程的 CMakeLists.txt 中添加以下代码：

```
set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS} ${IOT_SOLUTION_PATH}/components/{component_you_choose}")
```

**方法 3**. 复制 ESP-IoT-Solution 指定组件到工程目录：直接将该组件和其依赖的组件，复制粘贴至工程的 components 文件夹。

>注解:
>ESP-IoT-Solution 推荐使用以 CMake 为基础的编译系统（IDF V4.0 及以后版本默认编译系统），如果需要使用 GNU Make 编译系统，可以参考 老版本 GNU Make 。

#### 设置环境变量

添加 `ESP-IDF` 环境变量：

* Windows 在 CMD 窗口运行：

    ```
    %userprofile%\esp\esp-idf\export.bat
    ```

* Linux 和 macOS 在终端运行：

    ```
    . $HOME/esp/esp-idf/export.sh
    ```

添加 `IOT_SOLUTION_PATH` 环境变量：

* Windows 在 CMD 窗口运行：

    ```
    set IOT_SOLUTION_PATH=C:\esp\esp-iot-solution
    ```

* Linux 和 macOS 在终端运行：

    ```
    export IOT_SOLUTION_PATH=~/esp/esp-iot-solution
    ```

> 注解：
>   1. 请将路径替换成实际安装路径；
>   2. 以上方法设置的环境变量，仅对当前终端有效，如终端变更，请重新执行以上步骤。

### 其它参考资源

- 最新版的文档：https://docs.espressif.com/projects/espressif-esp-iot-solution/zh_CN ，该文档是由本仓库 [docs](./docs) 目录 构建得到；
- ESP-IDF 编程指南 https://docs.espressif.com/projects/esp-idf/zh_CN ，请参考 ESP-IoT-Solution 依赖的版本；
- 可以前往 [esp32.com](www.esp32.com) 论坛 提问，挖掘社区资源；
- 如果你在使用中发现了错误或者需要新的功能，请先查看 [GitHub Issues](https://github.com/espressif/esp-iot-solution/issues)，确保该问题不会被重复提交；
- 如果你有兴趣为 ESP-IoT-Solution 作贡献，请先阅读[贡献指南](./CONTRIBUTING.rst)。