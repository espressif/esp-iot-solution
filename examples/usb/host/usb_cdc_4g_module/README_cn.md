* [English version](README.md)

# USB CDC 4G Module

该示例程序可实现 ESP32-S2，ESP32-S3 系列 SoC 作为 USB 主机驱动 4G Cat.1 模组 PPP 拨号上网，同时可开启 ESP32-S Wi-Fi AP 功能，分享互联网给物联网设备或手持设备，实现低成本 “中高速” 互联网接入。同时配有路由器管理界面，可以在线进行路由器配置和查看已连接设备信息。

**已实现功能：**

* USB CDC 主机接口通信
* 兼容主流 4G 模组 AT 指令
* PPP 拨号上网
* Wi-Fi 热点分享
* 4G 模组状态管理
* 路由器管理界面
* 状态指示灯

![ESP32S2_USB_4g_moudle](./_static/esp32s2_cdc_4g_moudle.png)

* [Demo 视频](https://b23.tv/8flUAS)

## 硬件准备

**已支持 ESP 芯片型号：** 

* ESP32-S2
* ESP32-S3

> 建议使用集成 4MB 及以上 Flash，2MB 及以上 PSRAM 的 ESP 模组或芯片。示例程序默认不开启 PSRAM，用户可自行添加测试，理论上增大缓冲区大小可以提高数据平均吞吐率

**已支持 4G Cat.1 模组型号：** 

* 中移 ML302-DNLM/CNLM
* 合宙 Air724UG-NFM
* 移远 EC600N-CNLA-N05
* 移远 EC600N-CNLC-N06
* SIMCom A7600C1

> 以上模组有不同子型号，不同型号支持的通信制式可能略有区别，不同制式支持的运营商不同，上下行速率也不同。例如 LTE-FDD 5(UL)/10(DL), LTE-TDD 1(UL)/8(DL)

**硬件接线**

默认 GPIO 配置如下：

|           功能           |  GPIO   |        说明         |
| :----------------------: | :-----: | :-----------------: |
|    **USB D+ (green)**    | **20**  |      **必要**       |
|    **USB D- (white)**    | **19**  |      **必要**       |
|     **GND (black)**      | **GND** |      **必要**       |
|      **+5V (red)**       | **+5V** |      **必要**       |
|   Modem Power Control    |   12    | 4G 模组自动开机模式 |
| **Modem Reset Control**  | **13**  |      **必要**       |
| System Status LED (red)  |   15    |       非必要        |
| Wi-Fi Status LED (blue)  |   17    |       非必要        |
| Modem Status LED (green) |   16    |       非必要        |

> 用户也可在 `menuconfig -> 4G Modem Configuration -> gpio config` 中配置 GPIO

## 使用说明

**Wi-Fi 名称和密码：**

可在 `menuconfig` 的 `4G Modem Configuration → WiFi soft AP ` 中修改 Wi-Fi 配置信息

1. 默认 Wi-Fi 名为 `esp_4g_router`
2. 默认密码为 `12345678`

**指示灯说明：**

|        指示灯         | 闪烁 |              说明              |
| :-------------------: | :--: | :----------------------------: |
|  **系统指示灯 (红)**  | 熄灭 |               无               |
|                       | 快闪 |         重启 Modem 中          |
|                       | 常量 | 内部错误 (请检查 SIM 卡后重启) |
| **Wi-Fi 指示灯 (蓝)** | 熄灭 |               无               |
|                       | 慢闪 |          等待设备连接          |
|                       | 常亮 |           设备已连接           |
| **Modem 指示灯 (绿)** | 熄灭 |               无               |
|                       | 慢闪 |         等待互联网连接         |
|                       | 常量 |          互联网已连接          |

**路由器管理界面**
可在`menuconfig` 的 `4G Modem Configuration → Web router config `中配置是否开启路由器管理界面，以及修改路由器管理界面的登录账户和密码。

1. 默认登录账户为 `esp32`
2. 默认密码为 `12345678`
3. 在网页搜索 `192.168.4.1` 进入路由器后台
4. 目前支持功能：
    * 支持账户登录验证
    * 支持修改热点名称，密码，是否隐身，信道，带宽，安全模式
    * 支持查看当前连接设备信息，备注主机名，一键踢出设备
    * 支持设备联网状态，联网时间的查看

## 编译示例代码

> 也可以下载试用我们已经编译的固件，直接烧录即可，固件地址：https://esp32.com/viewtopic.php?f=22&t=24468

1. 确认 `ESP-IDF` 环境成功搭建，并切换到 `release/v4.4` 分支

2. 确认已经完整下载 `ESP-IOT-SOLUTION` 仓库，并切换到 `usb/add_usb_solutions` 分支

    ```bash
    git clone -b usb/add_usb_solutions --recursive https://github.com/espressif/esp-iot-solution
    ```

3. 添加 `ESP-IDF` 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

4. 添加 `ESP-IOT-SOLUTION` 环境变量，Linux 方法如下:

    ```bash
    export IOT_SOLUTION_PATH=$HOME/esp/esp-iot-solution
    ```

5. 设置编译目标为 `esp32s2` 或 `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

6. 选择 Cat.1 模组型号 `Menuconfig → Component config → ESP-MODEM → Choose Modem Board`，如果所选型号未在该列表，请参照 `其它 4G Cat.1 模组适配方法`，自行配置模组端点信息进行适配

    ![choose_modem](./_static/choose_modem.png)

7. 编译、下载、查看输出

    ```bash
    idf.py build flash monitor
    ```

**Log**

```
I (0) cpu_start: Starting scheduler on APP CPU.
W (385) main: Force reset 4g board
I (389) gpio: GPIO[13]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (3915) main: ====================================
I (3915) main:      ESP 4G Cat.1 Wi-Fi Router
I (3915) main: ====================================
I (3920) gpio: GPIO[15]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (3929) gpio: GPIO[17]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (3939) gpio: GPIO[16]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (3998) USB_HCDC: usb driver install succeed
I (3998) USB_HCDC: Waitting Device Connection
I (4028) USB_HCDC: USB Port=1 init succeed
I (4028) USB_HCDC: Waitting USB Connection
I (4028) USB_HCDC: Port power: ON
I (4281) USB_HCDC: line 263 HCD_PORT_EVENT_CONNECTION
I (4281) USB_HCDC: Resetting Port
I (4341) USB_HCDC: Port reset succeed
I (4341) USB_HCDC: Getting Port Speed
I (4341) USB_HCDC: Port speed = 1
I (4343) USB_HCDC: USB Speed: full-speed
I (4347) USB_HCDC: Set Device Addr = 1
I (4352) USB_HCDC: Set Device Addr Done
I (4356) USB_HCDC: Set Device Configuration = 1
I (4362) USB_HCDC: Set Device Configuration Done
I (4367) USB_HCDC: Creating bulk in pipe
I (4371) USB_HCDC: Creating bulk out pipe
I (4378) USB_HCDC: Device Connected
I (4380) gpio: GPIO[12]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (4390) gpio: GPIO[13]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (7375) modem_board: Modem PPP Started
I (7376) modem_board: re-start ppp, retry=1
I (7386) esp-netif_lwip-ppp: Connected
I (7386) esp-netif_lwip-ppp: Name Server1: 114.114.114.114
I (7387) esp-netif_lwip-ppp: Name Server2: 0.0.0.0
I (7391) modem_board: IP event! 6
I (7395) modem_board: Modem Connected to PPP Server
I (7401) modem_board: ppp ip: 10.250.188.169, mask: 255.255.255.255, gw: 192.168.0.1
I (7409) modem_board: Main DNS: 114.114.114.114
I (7415) modem_board: Backup DNS: 0.0.0.0
I (7420) main: ap dns addr(auto): 114.114.114.114
I (7425) pp: pp rom version: e7ae62f
I (7429) net80211: net80211 rom version: e7ae62f
I (7435) wifi:wifi driver task: 600fee20, prio:23, stack:6656, core=0
I (7440) system_api: Base MAC address is not set
I (7446) system_api: read default base MAC address from EFUSE
I (7462) wifi:wifi firmware version: cca42a5
I (7462) wifi:wifi certification version: v7.0
I (7462) wifi:config NVS flash: enabled
I (7464) wifi:config nano formating: disabled
I (7468) wifi:Init data frame dynamic rx buffer num: 32
I (7473) wifi:Init management frame dynamic rx buffer num: 32
I (7478) wifi:Init management short buffer num: 32
I (7483) wifi:Init dynamic tx buffer num: 32
I (7487) wifi:Init static tx FG buffer num: 2
I (7491) wifi:Init static rx buffer size: 1600
I (7495) wifi:Init static rx buffer num: 10
I (7499) wifi:Init dynamic rx buffer num: 32
I (7503) wifi_init: tcpip mbox: 32
I (7507) wifi_init: udp mbox: 6
I (7511) wifi_init: tcp mbox: 6
I (7515) wifi_init: tcp tx win: 5744
I (7519) wifi_init: tcp rx win: 5744
I (7523) wifi_init: tcp mss: 1440
I (7527) wifi_init: WiFi IRAM OP enabled
I (7532) wifi_init: WiFi RX IRAM OP enabled
I (7537) wifi_init: LWIP IRAM OP enabled
I (7542) phy_init: phy_version 302,291a31f,Oct 22 2021,19:22:08
I (7596) wifi:mode : softAP (7c:df:a1:e0:32:cd)
I (7598) wifi:Total power save buffer number: 16
I (7598) wifi:Init max length of beacon: 752/752
I (7598) wifi:Init max length of beacon: 752/752
I (7607) wifi:Total power save buffer number: 16
I (7607) modem_wifi: softap ssid: esp_4g_router password: 12345678
I (7613) modem_wifi: NAT is enabled
```

## 其它 4G Cat.1 模组适配方法

1. 确认 4G 模组是否支持 USB Fullspeed 通信模式
2. 确认 4G 模组是否支持 USB PPP 拨号上网；
3. 确认 4G SIM 卡为激活状态，并开启了上网功能；
4. 确认已按照**硬件接线**连接必要信号线；
5. 确认 4G 模组 USB PPP 接口输入端点 （IN）和 输出端点（OUT） 地址，并在 `menuconfig` 中修改以下选项： 

   * 选择自定义 4G Modem 开发板：
   ```
   Component config → ESP-MODEM → Choose Modem Board → User Defined
                                
   ```
   * 配置自定义 4G Modem 开发板端点地址：
   ```
   Component config → ESP-MODEM → USB CDC endpoint address config
                                        → Modem USB CDC IN endpoint address
                                        → Modem USB CDC OUT endpoint address
   ```

6. 控制台输出 log，确认 `AT` 指令能够执行；

> 不同 Cat.1 芯片平台支持的 AT 基础指令大致相同，但可能存在部分特殊指令，需要自行支持


## 代码调试

**1. 调试模式**

   可在 `menuconfig` 打开 `4G Modem Configuration -> Dump system task status` 选项打印 task 详细信息，也可打开 `Component config → USB Host CDC ->Trace internal memory status ` 选项打印 usb 内部 buffer 使用信息。

	```
    I (79530) main: Task dump
    I (79531) main: Load    Stack left      Name    PRI
    I (79531) main: 3.24    1080    main    1
    I (79532) main: 95.25   1248    IDLE    0
    I (79536) main: 0.03    1508    bulk-out        6
    I (79541) main: 0.03    1540    port    9
    I (79546) main: 0.01    1752    Tmr Svc         1
    I (79550) main: 0.04    2696    tiT     18
    I (79554) main: 1.21    1352    usb_event       4
    I (79559) main: 0.01    1532    bulk-in         5
    I (79564) main: 0.05    3540    esp_timer       22
    I (79569) main: 0.13    4632    wifi    23
    I (79573) main: 0.00    1532    dflt    8
    I (79577) main: 0.00    1092    sys_evt         20
    I (79582) main: Free heap=37088 bigst=16384, internal=36968 bigst=16384
    I (79589) main: ..............
    
    I (79593) USB_HCDC: USBH CDC Transfer Buffer Dump:
    I (79599) USB_HCDC: usb transfer Buffer size, out = 3000, in = 1500
    I (79606) USB_HCDC: usb transfer Max packet size, out = 46, in = 48
    
    I (79613) USB_HCDC: USBH CDC Ringbuffer Dump:
    I (79618) USB_HCDC: usb ringbuffer size, out = 15360, in = 15360
    I (79625) USB_HCDC: usb ringbuffer High water mark, out = 46, in = 48
	```

**2. 性能优化**

1. 检查模组和运营商支持情况，如果对吞吐率有要求，请选择 `FDD` 制式模组和运营商；
2. 将 `APN` 修改为运营商提供的名称 `menuconfig -> 4G Modem Configuration -> Set Modem APN`， 例如，当使用中国移动普通 4G 卡可改为 `cmnet`；
3. 将 ESP32-Sx CPU 配置为 240MHz（`Component config → ESP32S2-specific → CPU frequency`），如支持双核请同时打开双核；
4. ESP32-Sx 添加并使能 PSRAM（`Component config → ESP32S2-specific → Support for external`），并提高 PSRAM 时钟频率 (`Component config → ESP32S2-specific → Support for external → SPI RAM config → Set RAM clock speed`) 选择 80MHz。并在该目录下打开 `Try to allocate memories of WiFi and LWIP in SPIRAM firstly.`；
5. 将 FreeRTOS Tick 频率 `Component config → FreeRTOS → Tick rate` 提高到 `1000 Hz`；
6. 其它应用层优化。


## 性能参数

**测试环境：**

* ESP32-S2 ，CPU 240Mhz
* 4MB flash，无 PSRAM
* ML302-DNLM 模组开发板
* 中国移动 4G 上网卡
* 办公室正常使用环境

**测试结果：**

| 测试项 |   峰值   |   平均   |
| :----: | :------: | :------: |
|  下载  |  6.4 Mbps  |  4 Mbps  |
|  上传  | 5 Mbps | 2 Mbps |

> **4G Cat.1 理论峰值下载速率 10 Mbps，峰值上传速率 5 Mbps**
> 实际通信速率受运营商网络、测试软件、Wi-Fi 干扰情况、终端连接数影响，以实际使用为准
