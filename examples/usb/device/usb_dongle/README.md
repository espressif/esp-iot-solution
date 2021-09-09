# ESP32-S 系列 USB 无线适配器方案介绍

## 1.概述

本示例程序演示 ESP32-S 系列芯片如何实现 USB Dongle 设备功能，支持以下功能：

* 支持 Host 主机通过 USB-RNDIS 无线上网
* 支持 Host 主机通过 USB-BTH 进行 BLE 扫描、广播、配对、连接、绑定以及读写数据
* 支持 Host 主机通过 USB-CDC、UART 对 ESP32-S 系列设备进行通信和控制
* 支持多种 system、Wi-Fi 控制命令，使用 FreeRTOS-Plus-CLI 命令行接口，易拓展更多命令
* 支持热插拔

## 2. 如何使用示例
### 2.1 硬件准备

支持 USB-OTG 的 ESP 开发板。

* ESP32-S2

* ESP32-S3

### <span id = "connect">2.2 引脚分配</span>

只有具有 USB-OTG 外设的 ESP 芯片才需要引脚分配。 如果您的电路板没有连接到 USB-OTG 专用 GPIO 的 USB 连接器，您可能需要自己动手制作电缆并将 **D+** 和 **D-** 连接到下面列出的引脚

```
ESP BOARD          USB CONNECTOR (type A)
                          --
                         | || VCC
[USBPHY_DM_NUM]  ------> | || D-
[USBPHY_DP_NUM]  ------> | || D+
                         | || GND
                          --
```

|             | USB_DP | USB_DM |
| ----------- | ------ | ------ |
| ESP32-S2/S3 | GPIO20 | GPIO19 |

* ESP32-S2-Saola

<img src="./_static/ESP32-S2.jpg" alt="ESP32-S2" style="zoom: 15%;" />

* ESP32-S3 DevKitC

<img src="./_static/ESP32-S3.jpg" alt="ESP32-S3" style="zoom:25%;" />

### 2.3 软件准备

1. 确认 ESP-IDF 环境成功搭建。
2. 添加 ESP-IDF 环境变量，Linux 方法如下，其它平台请查阅 [设置环境变量](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-set-up-env)。
    ```
    >. $HOME/esp/esp-idf/export.sh
    >
    ```
3. 设置编译目标为 `esp32s2` 或 `esp32s3`。
    ```
    >idf.py set-target esp32s2
    >
    ```

### 2.4 软件工程配置

![tinyusb_config](./_static/tinyusb_config.png)

![uart_config](./_static/uart_config.png)

目前 USB-Dongle 支持以下四种组合选项：

| USB-RNDIS | USB-BTH | USB-CDC | UART |
| :-------: | :-----: | :-----: | :--: |
|     √     |         |         |  √   |
|     √     |         |    √    |      |
|     √     |    √    |         |  √   |
|           |    √    |         |  √   |

* 本软件默认使能 RNDIS、CDC。
* UART 默认在 CDC 使能时禁用。
* 可以通过 `component config -> TinyUSB Stack` 选择 USB 设备。
* 同时使能 RNDIS 和 BTH 时，建议禁用 CDC，采用 UART 发送命令，可以通过 `Example Configuration` 进行串口配置。

>由于目前硬件限制，EndPoint 不能超过一定数量，故不支持 RNDIS、BTH、CDC 同时使能。

### 2.5 固件编译&烧录

您可以通过以下命令编译、下载固件，并查看输出。

```
>idf.py -p (PORT) build flash monitor
>
```

 * ``(PORT)`` 需要替换为实际的串口名称。
 * 使用命令 `Ctrl-]` 可以退出串口监控状态。


### 2.6 使用说明

1. 完成上述[硬件连接](#connect)并成功烧录固件后，将 USB 连接至 PC 端

2. PC 端将会新增一个 USB 网卡以及一个蓝牙设备

    * 显示网络设备

    ```
    >ifconfig -a
    >
    ```

    <img src="./_static/ifconfig.png" alt="ifconfig" style="zoom: 80%;" />

    * 显示蓝牙设备

    ```
    >hciconfig
    >
    ```

    ![hciconfig](./_static/hciconfig.png)

    * 显示 USB-CDC 设备

    ```
    >ls /dev/ttyACM*
    >
    ```

    ![ACM](./_static/ACM.png)

3. 通过 USB-CDC 或者 UART 与 ESP 设备进行通信，使用 help 命令来查看目前所支持的所有指令

4. 若使能 USB-RNDIS，则可通过指令来控制 ESP 设备进行配网操作

* [通过 sta 命令来连接至对应路由器](./Commands.md#3sta)
* [通过 startsmart 命令开启 smartconfig 配网](./Commands.md#5smartconfig)

## 3. 配置连接 Wi-Fi 网络

本示例提供了两种连接 Wi-Fi 网络的方法。

### [方法 1. 通过 `sta` 命令连接至 Wi-Fi 路由器](./Commands.md#3sta)

**命令示例**

```
sta -s <ssid> -p [<password>]
```

**说明**

* `password` 为选填参数。

* 当设备已经连接到一个路由器，但您需要切换连接到其他路由器时，则需要重新加载网卡设备，在执行配网命令后执行以下操作：

    >查询 USB Ethernet name
    >
    >```
    >>ifconfig
    >>
    >```
    >
    >卸载 USB 网卡
    >
    >```
    >>ifconfig <USB Ethernet name> down
    >>
    >```
    >
    >装载 USB 网卡 
    >
    >```
    >>ifconfig <USB Ethernet name> up
    >>
    >```
    >
    >* `USB Ethernet name` 为查询出来的网卡名称

### [方法 2. 通过 smartconfig 连接至 Wi-Fi 路由器](./Commands.md#5smartconfig)

(1) 硬件准备

从手机应用市场下载 ESPTOUCH APP：[Android source code](https://github.com/EspressifApp/EsptouchForAndroid) [iOS source code](https://github.com/EspressifApp/EsptouchForIOS)。

(2) 将手机连接到目标 Wi-Fi AP（2.4GHz）。

(3) 手机打开 ESPTOUCH app 输入 Wi-Fi 密码。

(4) Host 通过 USB ACM port 发送以下命令给 ESP 设备，开始配网。

**命令示例**

```
smartconfig 1
```

**说明**

* 当设备已经连接到一个路由器，但您需要切换连接到其他路由器时，则需要重新加载网卡设备，在执行配网命令后执行以下操作：

    >查询 USB Ethernet name
    >
    >```
    >>ifconfig
    >>
    >```
    >
    >卸载 USB 网卡
    >
    >```
    >>ifconfig <USB Ethernet name> down
    >>
    >```
    >
    >装载 USB 网卡 
    >
    >```
    >>ifconfig <USB Ethernet name> up
    >>
    >```
    >
    >* `USB Ethernet name` 为查询出来的网卡名称

## 4. 命令说明

[Command](./Commands.md)

注意：Wi-Fi 相关命令只有在 USB Network Class 使能时才可以使用. 
