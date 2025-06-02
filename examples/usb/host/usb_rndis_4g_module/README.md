| Supported Targets | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | -------- | -------- | -------- |

# USB Host RNDIS 4G Module Example

This example demonstrates how to use [iot_usbh_rndis](https://components.espressif.com/components/espressif/iot_usbh_rndis) to connect to 4G module with RNDIS protocol.

## Example Output

```
I (387) main_task: Calling app_main()
I (388) USBH_CDC: iot usbh cdc version: 1.1.0
I (418) usbh_rndis: IOT USBH RNDIS Version: 0.1.0
I (418) iot_eth: IoT ETH Version: 0.1.0
I (419) usbh_rndis: USB RNDIS network interface init success
I (424) iot_eth.netif_glue: 00:00:00:00:00:00
I (429) iot_eth.netif_glue: ethernet attached to netif
I (867) usbh_rndis: USB RNDIS CDC new device found
*** Device descriptor ***
bLength 18
bDescriptorType 1
bcdUSB 2.00
bDeviceClass 0xef
bDeviceSubClass 0x2
bDeviceProtocol 0x1
bMaxPacketSize0 64
idVendor 0x1286
idProduct 0x4e3d
bcdDevice 1.00
iManufacturer 1
iProduct 2
iSerialNumber 3
bNumConfigurations 1
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 222
bNumInterfaces 5
bConfigurationValue 1
iConfiguration 0
bmAttributes 0xc0
bMaxPower 500mA
*** Interface Association Descriptor ***
bLength 8
bDescriptorType 11
bFirstInterface 0
bInterfaceCount 2
bFunctionClass 0xe0
bFunctionSubClass 0x1
bFunctionProtocol 0x3
iFunction 5
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 1
        bInterfaceClass 0xe0
        bInterfaceSubClass 0x1
        bInterfaceProtocol 0x3
        iInterface 5
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x85   EP 5 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 16
                bInterval 16
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 1
        bAlternateSetting 0
        bNumEndpoints 2
        bInterfaceClass 0xa
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 5
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x87   EP 7 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x6    EP 6 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 2
        bAlternateSetting 0
        bNumEndpoints 3
        bInterfaceClass 0xff
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 11
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x8c   EP 12 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 16
                bInterval 16
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x8e   EP 14 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0xd    EP 13 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 3
        bAlternateSetting 0
        bNumEndpoints 3
        bInterfaceClass 0xff
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 8
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x8a   EP 10 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 16
                bInterval 16
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x82   EP 2 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x1    EP 1 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 4
        bAlternateSetting 0
        bNumEndpoints 3
        bInterfaceClass 0xff
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 11
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x8b   EP 11 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 16
                bInterval 16
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x84   EP 4 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x3    EP 3 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
I (1175) usbh_rndis: RNDIS device found, VID: 4742, PID: 20029, ITF: 0
I (1182) USBH_CDC: New device connected, address: 1
I (1188) usbh_rndis: RNDIS auto detect success
Found NOTIF endpoint: 133
Found IN endpoint: 135
Found OUT endpoint: 6
I (1200) RNDIS_4G_MODULE: IOT_ETH_EVENT_START
I (1204) iot_eth.netif_glue: eth_action_handle: 0x3fcb0830, 0x3c09717c, 0, 0x3fcb47e8, 0x3fcb06c8
I (1214) usbh_rndis: USB RNDIS CDC notification
I (1219) usbh_rndis: MessageLength 52
I (1223) usbh_rndis: Max transfer packets: 1
I (1228) usbh_rndis: Max transfer size: 1580
I (1234) usbh_rndis: InformationBufferLength 112
I (1238) usbh_rndis: rndis query OID_GEN_SUPPORTED_LIST success,oid num :28
W (1246) usbh_rndis: Ignore rndis query iod:00010101
W (1252) usbh_rndis: Ignore rndis query iod:00010102
W (1257) usbh_rndis: Ignore rndis query iod:00010103
W (1263) usbh_rndis: Ignore rndis query iod:00010104
I (1237) usbh_rndis: USB RNDIS CDC notification
I (1274) usbh_rndis: InformationBufferLength 4
I (1280) usbh_rndis: InformationBufferLength 4
I (1284) usbh_rndis: USB RNDIS CDC notification
W (1284) usbh_rndis: Ignore rndis query iod:0001010a
I (1281) pp: pp rom version: e7ae62f
W (1295) usbh_rndis: Ignore rndis query iod:0001010b
I (1299) net80211: net80211 rom version: e7ae62f
I (1300) usbh_rndis: USB RNDIS CDC notification
W (1305) usbh_rndis: Ignore rndis query iod:0001010c
I (1317) wifi:W (1321) usbh_rndis: Ignore rndis query iod:0001010d
W (1328) usbh_rndis: Ignore rndis query iod:00010116
W (1334) usbh_rndis: Ignore rndis query iod:0001010e
wifi driver task: 3fcb8ba8, prio:23, stack:6656, core=0
I (1340) usbh_rndis: InformationBufferLength 4
I (1349) usbh_rndis: USB RNDIS CDC notification
I (1359) wifi:wifi firmware version: a862fd7
I (1360) wifi:wifi certification version: v7.0
I (1363) wifi:config NVS flash: enabled
I (1366) wifi:config nano formatting: disabled
I (1370) wifi:Init data frame dynamic rx buffer num: 64
I (1375) wifi:Init static rx mgmt buffer num: 5
I (1379) wifi:Init management short buffer num: 32
I (1384) wifi:Init dynamic tx buffer num: 64
I (1388) wifi:Init static tx FG buffer num: 2
I (1392) wifi:Init static rx buffer size: 1600
I (1396) wifi:Init static rx buffer num: 16
I (1400) wifi:Init dynamic rx buffer num: 64
I (1404) usbh_rndis: USB RNDIS CDC notification
I (1409) usbh_rndis: InformationBufferLength 4
I (1410) wifi_init: rx ba win: 32
I (1415) usbh_rndis: InformationBufferLength 4
W (1424) usbh_rndis: Ignore rndis query iod:00020101
W (1429) usbh_rndis: Ignore rndis query iod:00020102
W (1435) usbh_rndis: Ignore rndis query iod:00020103
W (1441) usbh_rndis: Ignore rndis query iod:00020104
W (1446) usbh_rndis: Ignore rndis query iod:00020105
I (1429) usbh_rndis: USB RNDIS CDC notification
I (1457) wifi_init: accept mbox: 6
I (1457) usbh_rndis: InformationBufferLength 6
I (1461) usbh_rndis: USB RNDIS CDC notification
I (1471) wifi_init: tcpip mbox: 64
I (1472) usbh_rndis: InformationBufferLength 6
W (1481) usbh_rndis: Ignore rndis query iod:01010103
W (1486) usbh_rndis: Ignore rndis query iod:01010105
I (1477) usbh_rndis: USB RNDIS CDC notification
I (1497) wifi_init: udp mbox: 64
I (1497) usbh_rndis: InformationBufferLength 4
W (1506) usbh_rndis: Ignore rndis query iod:01020101
W (1512) usbh_rndis: Ignore rndis query iod:01020102
W (1517) usbh_rndis: Ignore rndis query iod:01020103
I (1509) usbh_rndis: USB RNDIS CDC notification
I (1528) wifi_init: tcp mbox: 64
I (1528) usbh_rndis: resp->Status: 0
I (1536) usbh_rndis: rndis set OID_GEN_CURRENT_PACKET_FILTER success
I (1541) usbh_rndis: USB RNDIS CDC notification
I (1549) wifi_init: tcp tx win: 65535
I (1549) usbh_rndis: resp->Status: 0
I (1557) usbh_rndis: rndis set OID_802_3_MULTICAST_LIST success
I (1564) iot_eth: usb_rndis: MAC address: 00:0C:29:A3:9B:6D
I (1570) usbh_rndis: RNDIS connected success
I (1575) RNDIS_4G_MODULE: IOT_ETH_EVENT_CONNECTED
I (1580) iot_eth.netif_glue: eth_action_handle: 0x3fcb0830, 0x3c09717c, 2, 0x3fcb714c, 0x3fcb06c8
I (1557) usbh_rndis: USB RNDIS CDC notification
I (1595) wifi_init: tcp rx win: 65535
I (1600) wifi_init: tcp mss: 1440
I (1604) wifi_init: WiFi IRAM OP enabled
I (1608) wifi_init: WiFi RX IRAM OP enabled
I (1616) camera wifi: wifi_init_softap finished.SSID:ESP-RNDIS-4G password:
I (1621) phy_init: phy_version 701,f4f1da3a,Mar  3 2025,15:50:10
I (1677) wifi:mode : softAP (30:30:f9:5a:8e:89)
I (1678) wifi:Total power save buffer number: 32
I (1678) wifi:Init max length of beacon: 752/752
I (1680) wifi:Init max length of beacon: 752/752
I (1685) esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
I (2598) iot_eth.netif_glue: Ethernet Got IP Address
I (2598) iot_eth.netif_glue: ~~~~~~~~~~~
I (2598) iot_eth.netif_glue: ETHIP:192.168.0.100
I (2602) iot_eth.netif_glue: ETHMASK:255.255.255.0
I (2608) iot_eth.netif_glue: ETHGW:192.168.0.1
I (2613) iot_eth.netif_glue: ~~~~~~~~~~~
W (3694) RNDIS_4G_MODULE: From 8.8.8.8 icmp_seq=1 timeout

I (6855) RNDIS_4G_MODULE: 64 bytes from 8.8.8.8 icmp_seq=1 ttl=110 time=160 ms

```