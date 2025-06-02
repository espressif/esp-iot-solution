* [中文版本](README_cn.md)

# USB CDC 4G Module

This example demonstrates the **ESP32-S2**, **ESP32-S3** series SoC as a USB host to dial-up 4G Cat.1 through PPP to access the Internet, with the help of ESP32-SX Wi-Fi softAP function, share the Internet with IoT devices or mobile devices. Realize low-cost "medium-high-speed" Internet access.It is also equipped with a router management interface, which allows you to configure the router online and view the information of connected devices.

In addition, the **ESP32-P4** supports the 4G Cat.4 module (EC20) with faster speeds. It can also enable hotspot sharing by connecting an external ESP32 chip with Wi-Fi capability.

**Features Supported:**

* USB CDC host communication
* Compatible with mainstream 4G module AT commands
* PPP dial-up
* Wi-Fi hotspot
* 4G module status management
* Router management interface
* Status led indicator

![ESP32-S2_USB_4g_moudle](./_static/esp32s2_cdc_4g_moudle.png)

* [Demo video](https://b23.tv/8flUAS)

## Hardware requirement

**Supported ESP Soc:**

* ESP32-S2
* ESP32-S3
* ESP32-P4

> We recommend using ESP modules or chips that integrate 4MB above Flash, and 2MB above PSRAM. The example does not enable PSRAM by default, but users can add to tests by themselves. In theory, increasing the Wi-Fi buffer size can increase the average data throughput rate.

**Hardware wiring:**

Default GPIO as follows:

|        Functions         |  GPIO   |     Notes     |
| :----------------------: | :-----: | :-----------: |
|    **USB D+ (green)**    | **20**  | **Necessary** |
|    **USB D- (white)**    | **19**  | **Necessary** |
|     **GND (black)**      | **GND** | **Necessary** |
|      **+5V (red)**       | **+5V** | **Necessary** |
|   Modem Power Control    |   12    | Not Necessary |
| **Modem Reset Control**  | **13**  | **Necessary** |
| System Status LED (red)  |   15    | Not Necessary |
| Wi-Fi Status LED (blue)  |   17    | Not Necessary |
| Modem Status LED (green) |   16    | Not Necessary |

> User can change GPIO in `menuconfig -> 4G Modem Configuration -> gpio config`

## User Guide

**Wi-Fi SSID and Password:**

User can modify SSID and Password in `menuconfig -> 4G Modem Configuration -> WiFi soft AP `

1. Default Wi-Fi: `esp_4g_router`
2. Default Password: `12345678`

**LED Indicator Status:**

|     Indicator     |    Blink    |                Status                 |
| :---------------: | :---------: | :-----------------------------------: |
| **System (Red)**  | extinguish  |                  NA                   |
|                   | quick blink |           restart 4G Modem            |
|                   |    solid    | internal error(check SIM card please) |
| **Wi-Fi (Blue)**  | extinguish  |                  NA                   |
|                   | slow blink  |     waiting for device connection     |
|                   |    solid    |           device connected            |
| **Modem (Green)** | extinguish  |                  NA                   |
|                   | slow blink  |    waiting for internet connection    |
|                   |    solid    |          internet connected           |

**Router management interface**
You can configure whether to open the router management interface in `4G Modem Configuration → Web router config ` of `menuconfig`, and change the login account and password of the router management interface.

1. The default login account is `esp32`.
2. The default password is `12345678`.
3. Search `192.168.4.1` on the webpage to enter the router background
4. Currently supported features.
    * Support account login authentication
    * Support modify hotspot name, password, invisible or not, channel, bandwidth, security mode
    * Support to view the current connected device information, note the host name, a key to kick out the device
    * Support to view the network status and network time of the device

## How to build example

> You can also download then burn the firmware we have build. Download address: https://esp32.com/viewtopic.php?f=22&t=24468

1. Confirm that the `ESP-IDF` environment is successfully set up, and switch to the `release/v4.4` branch

2. Add the `ESP-IDF` environment variable, the Linux method is as follows, other platforms please refer [Set up the environment variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-4-set-up-the-environment-variables)

    ```bash
    . $HOME/esp/esp-idf/export.sh
    ```

3. Set the IDF build target to `esp32s2` or `esp32s3`

    ```bash
    idf.py set-target esp32s2
    ```

4. Select the Cat.1 module model `Menuconfig → Component config → ESP-MODEM → Choose Modem Board`, if the your module is not in the list, please refer to `Other 4G Cat.1 Module Adaptation Methods` to configure.

    ![choose_modem](./_static/choose_modem.png)

5. Build, download, check log output

    ```bash
    idf.py build flash monitor
    ```

**Log**

```
I (540) 4g_main: ====================================
I (540) 4g_main:      ESP 4G Cat.1 Wi-Fi Router
I (540) 4g_main: ====================================
I (544) modem_board: iot_usbh_modem, version: 1.1.4
I (549) modem_board: Force reset modem board....
I (553) gpio: GPIO[13]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (561) modem_board: Resetting modem using io=13, level=0
I (766) modem_board: Waiting for modem initialize ready
I (5766) USBH_CDC: iot usbh cdc version: 1.1.0
I (5796) esp-modem: --------- Modem PreDefined Info ------------------
I (5797) esp-modem: Model: User Defined
I (5797) esp-modem: Modem itf 3
I (5798) esp-modem: ----------------------------------------------------
I (5805) gpio: GPIO[12]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (5813) gpio: GPIO[13]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
Found NOTIF endpoint: 138
Found IN endpoint: 130
Found OUT endpoint: 1
....
I (7634) modem_board: DTE reconnect, reconnecting ...

I (7639) 4g_main: Modem Board Event: USB connected
I (7643) USBH_CDC: Opened cdc device: 1
I (7647) USBH_CDC: New device connected, address: 1
I (8639) modem_board: reconnect after 5s...
I (9639) modem_board: reconnect after 4s...
I (10639) modem_board: reconnect after 3s...
I (11639) modem_board: reconnect after 2s...
I (12639) modem_board: reconnect after 1s...
I (12639) modem_board: Modem state STAGE_SYNC, Start
I (12649) modem_board: Network Auto reconnecting ...
I (12650) modem_board: Modem state STAGE_SYNC, Success!
W (12650) 4g_main: Modem Board Event: Network disconnected
I (12750) modem_board: Modem state STAGE_CHECK_SIM, Start
I (12751) modem_board: SIM Card Ready
I (12752) modem_board: Modem state STAGE_CHECK_SIM, Success!
I (12752) 4g_main: Modem Board Event: SIM Card Connected
I (12852) modem_board: Modem state STAGE_CHECK_SIGNAL, Start
I (12852) modem_board: Signal quality: rssi=26, ber=99
I (12853) modem_board: Modem state STAGE_CHECK_SIGNAL, Success!
I (12956) modem_board: Modem state STAGE_CHECK_REGIST, Start
I (12956) modem_board: Network registered, Operator: "46000"
I (12957) modem_board: Modem state STAGE_CHECK_REGIST, Success!
I (13061) modem_board: Modem state STAGE_START_PPP, Start
I (13062) modem_board: Modem state STAGE_START_PPP, Success!
I (13162) modem_board: Modem state STAGE_WAIT_IP, Start
W (13162) modem_board: Modem event! 0
I (13742) esp-netif_lwip-ppp: Connected
I (13742) modem_board: IP event! 6
I (13742) modem_board: Modem Connected to PPP Server
I (13742) modem_board: ppp ip: 10.101.18.249, mask: 255.255.255.255, gw: 10.64.64.64
I (13750) modem_board: Main DNS: 211.136.150.86
I (13754) modem_board: Backup DNS: 211.136.150.88
I (13758) modem_board: Modem state STAGE_WAIT_IP, Success!
I (13758) esp-modem-netif: PPP state changed event 0: (NETIF_PPP_ERRORNONE)
I (13770) 4g_main: Modem Board Event: Network connected
I (13758) 4g_router_server: ssid : Can't find in NVS!
I (13780) 4g_router_server: password : Can't find in NVS!
I (13785) 4g_router_server: auth_mode : Can't find in NVS!
I (13790) 4g_router_server: channel : Can't find in NVS!
I (13795) 4g_router_server: hide_ssid : Can't find in NVS!
I (13800) 4g_router_server: bandwidth : Can't find in NVS!
I (13806) 4g_router_server: max_connection : Can't find in NVS!
I (13849) 4g_router_server: Partition size: total: 956561, used: 182477
I (13850) 4g_router_server: Starting server on port: '80'
I (13851) 4g_router_server: Registering URI handlers
I (13855) 4g_router_server: Starting webserver
```

## Debugging method

**1. Debugging mode**

Enable the `4G Modem Configuration -> Dump system task status` option in `menuconfig` to print task detailed information.

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

**2. Performance optimization**

1. If there is a requirement for throughput, please select the `FDD` standard module and operator;
2. Modify `APN` to the name provided by the operator `menuconfig -> 4G Modem Configuration -> Set Modem APN`, for example, when using China Mobile's ordinary 4G card, it can be changed to `cmnet`;
3. Configure ESP32-Sx CPU to 240MHz (`Component config → ESP32S2-specific → CPU frequency`), if it supports dual cores, please turn on both cores at the same time;
4. Add and enable PSRAM (`Component config → ESP32S2-specific → Support for external`), and increase the PSRAM clock frequency (`Component config → ESP32S2-specific → Support for external → SPI RAM config → Set RAM clock speed`) select 80MHz. And open `Try to allocate memories of WiFi and LWIP in SPIRAM firstly.` in this directory;
5. Increase the FreeRTOS Tick frequency `Component config → FreeRTOS → Tick rate` to `1000 Hz`;
6. Optimization of other application layers.

## Performance test

**Test environment:**

* ESP32-S2, CPU 240 MHz
* 4 MB flash, no PSRAM
* ML302-DNLM module
* China Mobile 4G SIM card
* Normal office environment

**Test result:**

| Test item |   Peak   | Average |
| :-------: | :------: | :-----: |
| Download  | 6.4 Mbps | 4 Mbps  |
|  Upload   |  5 Mbps  | 2 Mbps  |

> **4G Cat.1 theoretical peak download rate is 10 Mbps, peak upload rate is 5 Mbps**
> The actual communication rate is affected by the operator's network, test software, Wi-Fi interference, and the number of terminal connections, etc.
