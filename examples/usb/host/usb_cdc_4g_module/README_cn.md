* [English version](README.md)

# USB CDC 4G Module

该示例程序可实现 ESP32-S2，ESP32-S3 系列 SoC 作为 USB 主机驱动 4G Cat.1 模组 PPP 拨号上网，同时可开启 ESP32-SX Wi-Fi AP 功能，分享互联网给物联网设备或手持设备，实现低成本 “中高速” 互联网接入。同时配有路由器管理界面，可以在线进行路由器配置和查看已连接设备信息。

**已实现功能：**

* USB CDC 主机接口通信
* 兼容主流 4G 模组 AT 指令
* PPP 拨号上网
* Wi-Fi 热点分享
* 4G 模组状态管理
* 路由器管理界面
* 状态指示灯

![ESP32-S2_USB_4g_moudle](./_static/esp32s2_cdc_4g_moudle.png)

* [Demo 视频](https://b23.tv/8flUAS)

## 硬件准备

**已支持 ESP 芯片型号：** 

* ESP32-S2
* ESP32-S3

> 建议使用集成 4MB 及以上 Flash，2MB 及以上 PSRAM 的 ESP 模组或芯片。示例程序默认不开启 PSRAM，用户可自行添加测试，理论上增大缓冲区大小可以提高数据平均吞吐率

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

2. 添加 `ESP-IDF` 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3. 设置编译目标为 `esp32s2` 或 `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

4. 选择 Cat.1 模组型号 `Menuconfig → Component config → ESP-MODEM → Choose Modem Board`，如果所选型号未在该列表，请参照 `其它 4G Cat.1 模组适配方法`，自行配置模组端点信息进行适配

    ![choose_modem](./_static/choose_modem.png)

5. 编译、下载、查看输出

    ```bash
    idf.py build flash monitor
    ```

**Log**

```
I (9634) IOT_USBH: Set Device Configuration = 1
I (9639) IOT_USBH: Set Device Configuration Done
I (9644) IOT_USBH: Pipe init succeed, addr: 81
I (9649) IOT_USBH: Pipe init succeed, addr: 01
I (9654) USB_HCDC: CDC Device Connected
I (9659) esp-modem: --------- Modem PreDefined Info ------------------
I (9666) esp-modem: Model: ML302-DNLM/CNLM
I (9671) esp-modem: Modem itf: IN Addr:0x81, OUT Addr:0x01
I (9677) esp-modem: ----------------------------------------------------
I (9684) gpio: GPIO[12]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (9694) gpio: GPIO[13]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
W (9704) USB_HCDC: rx0 flush -0 = 0
W (9707) modem_board: DTE reconnect, reconnecting ...

W (10713) modem_board: reconnect after 5s...
W (11713) modem_board: reconnect after 4s...
W (12713) modem_board: reconnect after 3s...
W (13713) modem_board: reconnect after 2s...
W (14713) modem_board: reconnect after 1s...
I (14713) modem_board: Modem state STAGE_SYNC, Start
W (14824) modem_board: Network Auto reconnecting ...
I (14824) modem_board: Modem state STAGE_SYNC, Success!
I (14924) modem_board: Modem state STAGE_CHECK_SIM, Start
I (14958) modem_board: SIM Card Ready
I (14958) modem_board: Modem state STAGE_CHECK_SIM, Success!
I (15058) modem_board: Modem state STAGE_CHECK_SIGNAL, Start
I (15081) modem_board: Signal quality: rssi=25, ber=99
I (15081) modem_board: Modem state STAGE_CHECK_SIGNAL, Success!
I (15182) modem_board: Modem state STAGE_CHECK_REGIST, Start
I (15205) modem_board: Network registed, Operator: "46000"
I (15205) modem_board: Modem state STAGE_CHECK_REGIST, Success!
I (15306) modem_board: Modem state STAGE_START_PPP, Start
I (15715) modem_board: Modem state STAGE_START_PPP, Success!
W (15716) modem_board: Modem event! 0
I (15727) esp-netif_lwip-ppp: Connected
I (15727) esp-netif_lwip-ppp: Name Server1: 211.136.150.86
I (15727) esp-netif_lwip-ppp: Name Server2: 0.0.0.0
I (15732) modem_board: IP event! 6
I (15736) modem_board: Modem Connected to PPP Server
I (15742) modem_board: ppp ip: 10.84.162.74, mask: 255.255.255.255, gw: 192.168.0.1
I (15750) modem_board: Main DNS: 211.136.150.86
I (15755) modem_board: Backup DNS: 0.0.0.0
I (15761) pp: pp rom version: e7ae62f
I (15765) net80211: net80211 rom version: e7ae62f
I (15771) wifi:wifi driver task: 3fcb04d8, prio:23, stack:6656, core=0
I (15776) system_api: Base MAC address is not set
I (15781) system_api: read default base MAC address from EFUSE
I (15798) wifi:wifi firmware version: 133d2ca
I (15798) wifi:wifi certification version: v7.0
I (15799) wifi:config NVS flash: enabled
I (15800) wifi:config nano formating: disabled
I (15804) wifi:Init data frame dynamic rx buffer num: 32
I (15809) wifi:Init management frame dynamic rx buffer num: 32
I (15815) wifi:Init management short buffer num: 32
I (15815) modem_board: Modem state STAGE_WAIT_IP, Start
I (15819) wifi:Init dynamic tx buffer num: 32
I (15825) modem_board: Modem state STAGE_WAIT_IP, Success!
I (15829) wifi:Init static tx FG buffer num: 2
I (15840) wifi:Init static rx buffer size: 1600
I (15844) wifi:Init static rx buffer num: 10
I (15848) wifi:Init dynamic rx buffer num: 32
I (15852) wifi_init: tcpip mbox: 32
I (15856) wifi_init: udp mbox: 6
I (15860) wifi_init: tcp mbox: 6
I (15864) wifi_init: tcp tx win: 5744
I (15868) wifi_init: tcp rx win: 5744
I (15873) wifi_init: tcp mss: 1440
I (15877) wifi_init: WiFi IRAM OP enabled
I (15881) wifi_init: WiFi RX IRAM OP enabled
I (15886) wifi_init: LWIP IRAM OP enabled
I (15892) phy_init: phy_version 503,13653eb,Jun  1 2022,17:47:08
I (15931) wifi:mode : softAP (7c:df:a1:e0:91:01)
I (15934) wifi:Total power save buffer number: 16
I (15934) wifi:Init max length of beacon: 752/752
I (15934) wifi:Init max length of beacon: 752/752
I (15939) modem_wifi: Wi-Fi AP started
I (15947) wifi:Total power save buffer number: 16
I (15948) modem_wifi: softap ssid: esp_4g_router password: 12345678
I (15954) modem_wifi: NAT is enabled
```

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
