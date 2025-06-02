| Supported Targets | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | -------- | -------- | -------- |

# USB Host CDC Ethernet Example

This example supports ECM devices (CH397A) by default

For devices where ECM is not on the default configuration descriptor 1, please enable `CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK` in menuconfig, such as CH397A.

## Example Output

```
I (386) main_task: Calling app_main()
I (417) USBH_CDC: iot usbh cdc version: 1.1.0
I (417) iot_usbh_ecm: IOT USBH ECM Version: 0.1.0
I (417) iot_eth: IoT ETH Version: 0.1.0
I (421) iot_usbh_ecm: USB ECM network interface init success
I (427) iot_eth.netif_glue: 00:00:00:00:00:00
I (432) iot_eth.netif_glue: ethernet attached to netif
W (798) ENUM: Device has more than 1 configuration
I (798) ECM_4G_MODULE: USB device configuration value set to 2
I (801) iot_usbh_ecm: USB ECM new device detected
*** Device descriptor ***
bLength 18
bDescriptorType 1
bcdUSB 2.00
bDeviceClass 0x0
bDeviceSubClass 0x0
bDeviceProtocol 0x0
bMaxPacketSize0 64
idVendor 0x1a86
idProduct 0x5397
bcdDevice 7.50
iManufacturer 1
iProduct 2
iSerialNumber 3
bNumConfigurations 3
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 80
bNumInterfaces 2
bConfigurationValue 2
iConfiguration 0
bmAttributes 0xa0
bMaxPower 100mA
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 1
        bInterfaceClass 0x2
        bInterfaceSubClass 0x6
        bInterfaceProtocol 0x0
        iInterface 5
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x81   EP 1 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 64
                bInterval 8
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 1
        bAlternateSetting 0
        bNumEndpoints 0
        bInterfaceClass 0xa
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 0
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 1
        bAlternateSetting 1
        bNumEndpoints 2
        bInterfaceClass 0xa
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 0
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
                bEndpointAddress 0x3    EP 3 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
I (939) iot_usbh_ecm: ECM interface found: VID: 0000, PID: 0000, IFNUM: 0
I (946) iot_usbh_ecm: ECM MAC string index: 3
I (952) USBH_CDC: New device connected, address: 1
I (957) iot_usbh_ecm: USB ECM auto detect success
Found NOTIF endpoint: 129
Found IN endpoint: 130
Found OUT endpoint: 3
I (970) iot_usbh_ecm: ECM MAC address: 3C:AB:72:84:17:10
I (975) iot_eth: usb_ecm: MAC address: 3C:AB:72:84:17:10
I (981) iot_usbh_ecm: Setting ETHERNET packet filter
I (986) ECM_4G_MODULE: IOT_ETH_EVENT_START
I (991) iot_eth.netif_glue: eth_action_handle: 0x3fcb0828, 0x3c096c8c, 0, 0x3fcb0bec, 0x3fcb06d0
I (1001) iot_usbh_ecm: Setting interface alternate setting to 1
I (1011) pp: pp rom version: e7ae62f
I (1012) net80211: net80211 rom version: e7ae62f
I (1018) wifi:wifi driver task: 3fcbb398, prio:23, stack:6656, core=0
I (1028) wifi:wifi firmware version: a862fd7
I (1028) wifi:wifi certification version: v7.0
I (1031) wifi:config NVS flash: enabled
I (1035) wifi:config nano formatting: disabled
I (1039) wifi:Init data frame dynamic rx buffer num: 64
I (1044) wifi:Init static rx mgmt buffer num: 5
I (1048) wifi:Init management short buffer num: 32
I (1053) wifi:Init dynamic tx buffer num: 64
I (1057) wifi:Init static tx FG buffer num: 2
I (1061) wifi:Init static rx buffer size: 1600
I (1065) wifi:Init static rx buffer num: 16
I (1069) wifi:Init dynamic rx buffer num: 64
I (1073) wifi_init: rx ba win: 32
I (1077) wifi_init: accept mbox: 6
I (1081) wifi_init: tcpip mbox: 64
I (1085) wifi_init: udp mbox: 64
I (1089) wifi_init: tcp mbox: 64
I (1093) wifi_init: tcp tx win: 65535
I (1097) wifi_init: tcp rx win: 65535
I (1101) wifi_init: tcp mss: 1440
I (1105) wifi_init: WiFi IRAM OP enabled
I (1110) wifi_init: WiFi RX IRAM OP enabled
I (1118) camera wifi: wifi_init_softap finished.SSID:ESP-ECM-4G password:
I (1122) phy_init: phy_version 701,f4f1da3a,Mar  3 2025,15:50:10
I (1186) wifi:mode : softAP (30:30:f9:5a:8e:89)
I (1186) wifi:Total power save buffer number: 32
I (1186) wifi:Init max length of beacon: 752/752
I (1188) wifi:Init max length of beacon: 752/752
I (1193) esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
W (3202) ECM_4G_MODULE: From 8.8.8.8 icmp_seq=1 timeout

I (3210) ECM_4G_MODULE: IOT_ETH_EVENT_CONNECTED
I (3210) iot_eth.netif_glue: eth_action_handle: 0x3fcb0828, 0x3c096c8c, 2, 0x3fcc64c0, 0x3fcb06d0
I (4219) iot_eth.netif_glue: Ethernet Got IP Address
I (4219) iot_eth.netif_glue: ~~~~~~~~~~~
I (4219) iot_eth.netif_glue: ETHIP:192.168.10.54
I (4223) iot_eth.netif_glue: ETHMASK:255.255.255.0
I (4229) iot_eth.netif_glue: ETHGW:192.168.10.1
I (4234) iot_eth.netif_glue: ~~~~~~~~~~~
I (6235) ECM_4G_MODULE: 64 bytes from 8.8.8.8 icmp_seq=1 ttl=118 time=33 ms
```