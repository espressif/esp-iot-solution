* [English version](README.md)

# USB CDC 4G Module

该示例程序可实现 **ESP32-S2**，**ESP32-S3** 系列 SoC 作为 USB 主机驱动 4G Cat.1 模组 PPP 拨号上网，同时可开启 ESP32-SX Wi-Fi AP 功能，分享互联网给物联网设备或手持设备，实现低成本 “中高速” 互联网接入。同时配有路由器管理界面，可以在线进行路由器配置和查看已连接设备信息。

此外对于 **ESP32-P4** 支持了 4G Cat.4 模组（EC20），速率更快。并可通过外挂 ESP32 带 Wi-Fi 版本芯片实现热点共享。

**已实现功能：**

* USB CDC 主机接口通信
* 兼容主流 4G 模组 AT 指令
* PPP 拨号上网
* Wi-Fi 热点分享
* 4G 模组状态管理
* 路由器管理界面

![ESP32-S2_USB_4g_moudle](./_static/esp32s2_cdc_4g_moudle.png)

## 硬件准备

**已支持 ESP 芯片型号：**

* ESP32-S2
* ESP32-S3
* ESP32-P4

> 建议使用集成 4MB 及以上 Flash，2MB 及以上 PSRAM 的 ESP 模组或芯片。示例程序默认不开启 PSRAM，用户可自行添加测试，理论上增大缓冲区大小可以提高数据平均吞吐率

**已支持 4G 模组型号：**

参考 https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_ppp.html

**硬件接线**

```
┌─────────────┐          ┌─────────────────┐
│             ┼──────────┼5V               │
│  4G Module  ┼──────────┼GND              │
│             │          │    ESP32-xx     │
│             │          │                 │
│             ┼──────────┼USB D+           │
│             ┼──────────┼USB D-           │
│             │          │                 │
└─────────────┘          │                 │
                         └─────────────────┘
```

## 使用说明

**Wi-Fi 名称和密码：**

可在 `menuconfig` 的 `4G Modem WiFi Config` 中修改 Wi-Fi 配置信息

1. 默认 Wi-Fi 名为 `ESP-USB-4G`
2. 默认密码为空

**路由器管理界面**

可在 `menuconfig` 的 `4G Modem WiFi Config → Open web configuration` 配置是否开启路由器管理界面（默认开启），以及修改路由器管理界面的登录账户和密码。

1. 默认登录账户为 `esp32`
2. 默认密码为 `12345678`
3. 在网页搜索 `192.168.4.1` 进入路由器后台
4. 目前支持功能：
    * 支持账户登录验证
    * 支持修改热点名称，密码，是否隐身，信道，带宽，安全模式
    * 支持查看当前连接设备信息，备注主机名，一键踢出设备
    * 支持设备联网状态，联网时间的查看

## 编译示例代码

1. 设置正确的编译目标，以**ESP32-S3** 为例

    ```bash
    idf.py set-target esp32s3
    ```

2. 编译、下载、查看输出

    ```bash
    idf.py build flash monitor
    ```

**Log**

```
I (471) USBH_CDC: iot usbh cdc version: 3.0.0
I (501) PPP_4G_main: ====================================
I (501) PPP_4G_main:      ESP 4G Cat.1 Wi-Fi Router
I (501) PPP_4G_main: ====================================
I (505) modem_board: iot_usbh_modem, version: 2.0.0
I (509) iot_eth: IoT ETH Version: 0.1.0
I (513) esp-modem-dte: USB PPP network interface init success
I (519) iot_eth.netif_glue: ethernet attached to netif
I (523) PPP_4G_main: modem board installed
I (882) USBH_CDC: New device connected, address: 1
I (882) USBH_CDC: Detect hub device, skip
I (1053) USBH_CDC: New device connected, address: 2
I (1054) esp-modem-dte: USB CDC device connected, VID: 0x2c7c, PID: 0x0903
I (1054) esp-modem-dte: No matched USB Modem found, ignore this connection
I (1225) USBH_CDC: New device connected, address: 3
I (1226) esp-modem-dte: USB CDC device connected, VID: 0x2cb7, PID: 0x0d01
I (1226) esp-modem-dte: Matched USB Modem: "Fibocom, LE270-CN", AT Interface: 2, DATA Interface: -1
I (1234) cdc_descriptor: Found NOTIF endpoint: 3
I (1239) cdc_descriptor: Found OUT endpoint: 2
I (1243) cdc_descriptor: Found IN endpoint: 4
I (1247) modem_board: DTE connected
I (1250) modem_board: Handling stage = STAGE_SYNC
I (1258) modem_board: Modem manufacturer ID: Fibocom Wireless Inc.
I (1262) modem_board: Modem module ID: LE270-CN
I (1266) modem_board: Modem revision ID: 12007.6005.00.02.03.04
I (1270) modem_board: Modem auto connect enabled, starting PPP...
I (1376) modem_board: Handling stage = STAGE_START_PPP
I (1376) modem_board: Check SIM card state...
I (1377) modem_board: Check signal quality...
I (1381) modem_board: PDP context: "+CGDCONT: 1,"IPV4V6","cmiot","10.48.54.63,2409:8d1e:123:7d31:1870:1b56:8ecb:4a14",0,0"
I (1388) modem_board: Check network registration...
I (1394) modem_board: PPP Dial up...
I (2200) iot_eth: Ethernet link up
I (2200) PPP_4G_main: IOT_ETH_EVENT_START
W (2200) iot_eth: Driver does not support get_addr
I (2200) PPP_4G_main: IOT_ETH_EVENT_CONNECTED
I (2208) iot_eth.netif_glue: netif "ppp" Got IP Address
I (2208) esp-netif_lwip-ppp: Connected
I (2209) iot_eth.netif_glue: ~~~~~~~~~~~
I (2216) iot_eth.netif_glue: ETHIP:10.48.54.63
I (2220) iot_eth.netif_glue: ETHMASK:255.255.255.255
I (2225) iot_eth.netif_glue: ETHGW:10.64.64.64
I (2229) iot_eth.netif_glue: Main DNS: 211.136.150.86
I (2234) iot_eth.netif_glue: Backup DNS: 211.136.150.88
I (2239) iot_eth.netif_glue: ~~~~~~~~~~~
I (2242) PPP_4G_main: GOT_IP
......
```

## 代码调试

**1. 调试模式**

   可在 `menuconfig` 打开 `4G Modem Configuration -> Dump system task status` 选项打印 task 详细信息。

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
	```

**2. 性能优化**

1. 检查模组和运营商支持情况，如果对吞吐率有要求，请选择 `FDD` 制式模组和运营商；
2. 将 ESP32-Sx CPU 配置为 240MHz（`Component config → ESP32S2-specific → CPU frequency`），如支持双核请同时打开双核；
3. ESP32-Sx 添加并使能 PSRAM（`Component config → ESP32S2-specific → Support for external`），并提高 PSRAM 时钟频率 (`Component config → ESP32S2-specific → Support for external → SPI RAM config → Set RAM clock speed`) 选择 80MHz。并在该目录下打开 `Try to allocate memories of WiFi and LWIP in SPIRAM firstly.`；
4. 将 FreeRTOS Tick 频率 `Component config → FreeRTOS → Tick rate` 提高到 `1000 Hz`；
5. 其它应用层优化。


## 性能参数

**测试环境：**

* ESP32-S2 ，CPU 240Mhz
* 4MB flash，无 PSRAM
* ML302-DNLM 模组开发板
* 中国移动 4G 上网卡
* 办公室正常使用环境

**测试结果：**

| 测试项 |   峰值   |  平均  |
| :----: | :------: | :----: |
|  下载  | 6.4 Mbps | 4 Mbps |
|  上传  |  5 Mbps  | 2 Mbps |

> **4G Cat.1 理论峰值下载速率 10 Mbps，峰值上传速率 5 Mbps**
> 实际通信速率受运营商网络、测试软件、Wi-Fi 干扰情况、终端连接数影响，以实际使用为准
