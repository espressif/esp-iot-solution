* [中文版本](README_cn.md)

# USB CDC 4G Module

This example demonstrates the **ESP32-S2**, **ESP32-S3** series SoC as a USB host to dial-up 4G Cat.1 through PPP to access the Internet, with the help of ESP32-SX Wi-Fi softAP function, share the Internet with IoT devices or mobile devices. Realize low-cost "medium-high-speed" Internet access.It is also equipped with a router management interface, which allows you to configure the router online and view the information of connected devices.

In addition, the **ESP32-P4** supports the 4G Cat.4 module (EC20) with faster speeds. It can also enable hotspot sharing by connecting an external ESP32 chip with Wi-Fi capability.

**Implemented features:**

- USB CDC host communication
- Compatible with mainstream 4G module AT commands
- PPP dial‑up
- Wi‑Fi hotspot sharing
- 4G module status management
- Router management interface

![ESP32-S2_USB_4g_moudle](./_static/esp32s2_cdc_4g_moudle.png)

## Hardware preparation

**Supported ESP chips:**

- ESP32-S2
- ESP32-S3
- ESP32-P4

> We recommend using ESP modules or chips that integrate 4 MB or more of Flash and 2 MB or more of PSRAM. PSRAM is disabled by default in this example; you can enable it for your own tests. In theory, increasing buffer sizes can improve average throughput.

**Supported 4G module models:**

See `https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_host/usb_ppp.html`.

**Hardware wiring**

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

## Usage

**Wi‑Fi name and password:**

You can modify Wi‑Fi configuration in `menuconfig` under `4G Modem WiFi Config`.

1. Default Wi‑Fi name: `ESP-USB-4G`
2. No password by default

**Router management interface**

In `menuconfig` under `4G Modem WiFi Config → Open web configuration`, you can enable or disable the router management interface (enabled by default) and change the login account and password.

1. Default login account: `esp32`
2. Default password: `12345678`
3. Visit `192.168.4.1` in a browser to enter the router dashboard
4. Currently supported features:
   - Account login authentication
   - Modify hotspot name, password, hidden or not, channel, bandwidth, security mode
   - View currently connected devices, note hostnames, one‑click kick out
   - View device network status and network uptime

## Build the example

1. Set the correct build target (for example, **ESP32-S3**):

```bash
idf.py set-target esp32s3
```

2. Build, flash, and monitor:

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

## Code debugging

**1. Debugging mode**

Enable `4G Modem Configuration -> Dump system task status` in `menuconfig` to print detailed task information.

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

1. Check module and operator support. If you require higher throughput, choose the `FDD` mode module and operator.
2. Configure ESP32‑Sx CPU to 240 MHz (`Component config → ESP32S2-specific → CPU frequency`). If dual‑core is supported, enable both cores.
3. Add and enable PSRAM (`Component config → ESP32S2-specific → Support for external`) and set PSRAM clock to 80 MHz (`Component config → ESP32S2-specific → Support for external → SPI RAM config → Set RAM clock speed`). Also enable `Try to allocate memories of WiFi and LWIP in SPIRAM firstly.` in the same section.
4. Increase the FreeRTOS Tick rate (`Component config → FreeRTOS → Tick rate`) to `1000 Hz`.
5. Other application‑layer optimizations.

## Performance

**Test environment:**

- ESP32‑S2, CPU 240 MHz
- 4 MB flash, no PSRAM
- ML302‑DNLM module dev board
- China Mobile 4G SIM card
- Normal office environment

**Test result:**

| Test item |   Peak   | Average |
| :-------: | :------: | :-----: |
| Download  | 6.4 Mbps | 4 Mbps  |
|  Upload   |  5 Mbps  | 2 Mbps  |

> 4G Cat.1 theoretical peak download rate is 10 Mbps, peak upload rate is 5 Mbps.
> Actual communication rate is affected by the operator network, test software, Wi‑Fi interference, and number of connected terminals.
