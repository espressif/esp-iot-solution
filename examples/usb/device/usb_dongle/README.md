## USB 无线适配器方案介绍

该示例程序支持以下功能：

* 支持 Host 主机通过 USB-RNDIS 无线上网
* 支持 Host 主机通过 USB-BTH 进行 BLE 扫描、广播、配对、连接、绑定以及读写数据
* 支持 Host 主机通过 USB-CDC、UART 对 ESP32-S 系列设备进行通信和控制
* 支持多种 system、Wi-Fi 控制命令，使用 FreeRTOS-Plus-CLI 命令行接口，易拓展更多命令
* 支持热插拔

### <span id = "hw">硬件准备</span>

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

### 软件准备

1. 确认 ESP-IDF 环境成功搭建
2. 添加 ESP-IDF 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)
    ```
    . $HOME/esp/esp-idf/export.sh
    ```
3. 设置编译目标为 `esp32s2` 或 `esp32s3`
    ```
    idf.py set-target esp32s2
    ```

### 项目配置

![tinyusb_config](./_static/tinyusb_config.png)

![uart_config](./_static/uart_config.png)

目前 USB-Dongle 支持以下四种组合选项

| USB-RNDIS | USB-BTH | USB-CDC | UART |
| :-------: | :-----: | :-----: | :--: |
|     √     |         |         |  √   |
|     √     |         |    √    |      |
|     √     |    √    |         |  √   |
|           |    √    |         |  √   |

* 项目默认使能 RNDIS、CDC
* UART 默认在 CDC 使能时禁用
* 可以通过 `component config -> TinyUSB Stack` 选择 USB Device
* 同时使能 RNDIS 和 BTH 时，建议禁用 CDC，采用 UART 发送命令，可以通过 `Example Configuration` 进行串口配置

>由于目前硬件限制，EndPoint 不能超过一定数量，故不支持 RNDIS、BTH、CDC 同时使能

### 固件编译&烧录

编译、下载、查看输出

```
idf.py build flash monitor
```

### 使用说明

1. 完成上述[硬件准备](#hw)并成功烧录固件后，将 USB 连接至 PC 端

2. PC 端将会新增一个 USB 网卡以及一个蓝牙设备

3. 可通过以下命令来查看新增 USB 设备
    
    ```
    ifconfig -a
    ```
    
    <img src="./_static/ifconfig.png" alt="ifconfig" style="zoom: 80%;" />
    
    ```
    hciconfig
    ```
    
    ![hciconfig](./_static/hciconfig.png)
    
    ```
    ls /dev/ttyACM*
    ```

    ![ACM](./_static/ACM.png)


4. 通过 USB-CDC 或者 UART 与 ESP 设备进行通信，使用 help 命令来查看目前所支持的所有指令
5. 若使能 USB-RNDIS ，则可通过指令来控制 ESP 设备进行配网操作

    * [通过 sta 命令来连接至对应路由器](./Commands.md#3sta)
    * [通过 startsmart 命令开启 smartconfig 配网](./Commands.md#5smartconfig)

注意！

>当设备已经连上一个路由器，但你需要重新切换路由器时，需要在执行 sta 或者 smartconfig 配网命令后执行以下操作
>
>查看 USB 网卡名称
>
>```
>ifconfig
>```
>
>卸载 USB 网卡
>
>```
>ifconfig <name> down 
>```
>
>装载 USB 网卡
>
>```
>ifconfig <name> up
>```
>

### 命令说明

[Command](./Commands.md)

注意：Wi-Fi 相关命令只有在 USB Network Class 使能时才可以使用 
