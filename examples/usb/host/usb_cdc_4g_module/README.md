## USB CDC 4G Module 示例程序说明

该示例程序可实现 ESP32-S 系列 SoC (已支持 ESP32-S2，ESP32-S3) 作为 USB 主机驱动 4G Cat.1 模组拨号上网，同时可开启 ESP32-S Wi-Fi AP 功能，分享互联网给物联网设备或手持设备，实现低成本 “中高速” 互联网接入。

**已实现功能：**

* USB CDC 主机接口通信
* 兼容主流 4G 模组 AT 指令
* PPP 拨号上网
* Wi-Fi 热点分享
* 4G 模组状态管理
* 状态指示灯

![ESP32S2_USB_4g_moudle](./_static/esp32s2_cdc_4g_moudle.png)

* [Demo 视频](https://b23.tv/8flUAS)

**本代码目前仅用于 USB CDC 功能测试，不建议基于此开发量产产品，原因如下：**

1. 本代码缺少完整的枚举过程、错误处理等机制；
2. 本代码基于的 ESP-IDF 的 USB Host 底层驱动存在潜在的 bug 和 API 变更的风险；

## 硬件准备

**已支持 ESP 芯片型号：** 
* ESP32-S2
* ESP32-S3

> 建议使用集成 4MB 及以上 Flash，2MB 及以上 PSRAM 的 ESP 模组或芯片。示例程序默认不开启 PSRAM，用户可自行添加测试，理论上增大缓冲区大小可以提高数据平均吞吐率

**已测试 4G Cat.1 模组型号：** 
* 中移 ML302
* 合宙 Air724UG
* 移远 EC600N
* SIMCom A7600C1

> 以上模组有不同子型号，不同型号支持的通信制式可能略有区别，不同制式支持的运营商不同，上下行速率也不同。例如 LTE-FDD 5(UL)/10(DL), LTE-TDD 1(UL)/8(DL)

**其它 4G Cat.1 模组适配方法：**

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

**硬件接线**

默认 GPIO 配置如下，用户也可在 `menuconfig -> 4G Modem Configuration -> gpio config` 中配置。 

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

## 编译示例代码

1. 确认 `ESP-IDF` 环境成功搭建，并按照说明文件切换到指定 commit [idf_usb_support_patch](../../../usb/idf_usb_support_patch/readme.md)

2. 确认已经完整下载 `ESP-IOT-SOLUTION` 仓库，并切换到 `usb/add_usb_solutions` 分支

    ```bash
    git clone -b usb/add_usb_solutions --recursive https://github.com/espressif/esp-iot-solution
    ```

3. 添加 `ESP-IDF` 环境变量，Linux 方法如下，其它平台请查阅 [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)

    ```bash
    $HOME/esp/esp-idf/export.sh
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

8. 错误处理：
    如果编译时输出 `fatal error: usb.h: No such file or directory`，请将 `esp-idf/components/usb/CMakeLists.txt` 按照以下修改:

    ```
    idf_component_register(SRCS "hcd.c"
                        INCLUDE_DIRS "private_include"
                        PRIV_INCLUDE_DIRS ""
                        PRIV_REQUIRES hal driver)
    ```

## 使用说明

**Wi-Fi 名称和密码：**

可在 `menuconfig` 的 `4G Modem Configuration → WiFi soft AP ` 中修改 Wi-Fi 配置信息

1. 默认 Wi-Fi 名为 `esp_4g_router`
2. 默认密码为 `12345678`

**指示灯说明：**

|        指示灯         | 闪烁 |              说明              |
| :-------------------: | :--: | :----------------------------: |
|  **系统指示灯 (红)**  | 熄灭 |               无               |
|                       | 慢闪 |            电量不足            |
|                       | 快闪 |         重启 Modem 中          |
|                       | 常量 | 内部错误 (请检查 SIM 卡后重启) |
| **Wi-Fi 指示灯 (蓝)** | 熄灭 |               无               |
|                       | 慢闪 |          等待设备连接          |
|                       | 常亮 |           设备已连接           |
| **Modem 指示灯 (绿)** | 熄灭 |               无               |
|                       | 慢闪 |         等待互联网连接         |
|                       | 常量 |          互联网已连接          |

**调试信息说明：**

1. 可在 `menuconfig` 打开 `4G Modem Configuration -> Dump system task status` 选项打印 task 详细信息，也可打开 `Component config → USB Host CDC ->Trace internal memory status ` 选项打印 usb 内部 buffer 使用信息。

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
2. 系统调试

	```
    E (60772) esp-netif_lwip-ppp: pppos_input_tcpip failed with -1
	```
	以上错误信息可能在调试中出现，原因是 RAM 空间较小导致通信时多次尝试申请内存，可添加并开启 PSRAM 解决该问题 (配置为 80M 时钟，并选择将 Wi-Fi 和 LWIP 优先申请到 PSRAM 中)

## 性能参数

* ESP32-S2 ，CPU 240Mhz
* 4MB flash，无 PSRAM
* ML302-DNLM 模组开发板
* 中国移动 4G 上网卡
* 办公室正常使用环境

| 测试项 |   峰值   |   平均   |
| :----: | :------: | :------: |
|  下载  |  6.4 Mbps  |  4 Mbps  |
|  上传  | 5 Mbps | 2 Mbps |

> **4G Cat.1 理论峰值下载速率 10 Mbps，峰值上传速率 5 Mbps**
> 实际通信速率受运营商网络、测试软件、Wi-Fi 干扰情况、终端连接数影响，以实际使用为准

**性能优化**

1. 检查模组和运营商支持情况，如果对吞吐率有要求，请选择 `FDD` 制式模组和运营商；
2. 将 `APN` 修改为运营商提供的名称 `menuconfig -> 4G Modem Configuration -> Set Modem APN`， 例如，当使用中国移动普通 4G 卡可改为 `cmnet`；
3. 将 ESP32-Sx CPU 配置为 240MHz（`Component config → ESP32S2-specific → CPU frequency`），如支持双核请同时打开双核；
4. ESP32-Sx 添加并使能 PSRAM（`Component config → ESP32S2-specific → Support for external`），并提高 PSRAM 时钟频率 (`Component config → ESP32S2-specific → Support for external → SPI RAM config → Set RAM clock speed`) 选择 80MHz。并在该目录下打开 `Try to allocate memories of WiFi and LWIP in SPIRAM firstly.`；
5. 将 FreeRTOS Tick 频率 `Component config → FreeRTOS → Tick rate` 提高到 `1000 Hz`；
6. 其它应用层优化。
