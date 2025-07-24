| Supported Targets | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | -------- | -------- | -------- |

# USB Host ECM Example

This example demonstrates how to use [iot_usbh_ecm](https://components.espressif.com/components/espressif/iot_usbh_ecm) to connect to 4G module with ECM protocol.

Supported devices refer to https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_ecm.html

For devices where ECM is not on the default configuration descriptor 1, please enable `CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK` in menuconfig, such as CH397A.

## Usage

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

Once connected to the 4G module and successfully obtains an IP address, the Wi-Fi hotspot sharing network will be enabled.

**Wi‑Fi name and password:**

You can modify Wi‑Fi configuration in `menuconfig` under `4G Modem WiFi Config`.

1. Default Wi‑Fi name: `ESP-USB-4G`
2. No password by default

## Enable AT commands

Users can Enable AT commands by setting `Example Configuration → USB ECM AT Command` in menuconfig. Then, enter the correct interface number, recompile, and run the program. It will periodically print the module's signal quality.

## Example Output

```
I (505) iot_usbh_ecm: USB ECM network interface init success
I (511) iot_eth.netif_glue: ethernet attached to netif
I (517) ECM_4G_MODULE: IOT_ETH_EVENT_START
I (545469) ECM_4G_MODULE: USB device configuration value set to 1
I (545471) USBH_CDC: New device connected, address: 1
I (545471) iot_usbh_ecm: ECM interface found: VID: 19D1, PID: 1003, IFNUM: 0
I (545478) cdc_descriptor: Found NOTIF endpoint: 1
I (545483) cdc_descriptor: Found OUT endpoint: 1
I (545489) cdc_descriptor: Found IN endpoint: 2
I (545495) iot_usbh_ecm: Parsed MAC address: 20:89:84:6A:96:AA
I (545500) iot_usbh_ecm: Setting ETHERNET packet filter
I (545507) iot_usbh_ecm: Setting interface alternate setting to 1
I (545589) iot_usbh_ecm: Notify - network connection changed: Connected
I (545589) iot_eth: Ethernet link up
I (545590) ECM_4G_MODULE: IOT_ETH_EVENT_CONNECTED
I (545595) iot_eth.netif_glue: Set MAC Address: 20:89:84:6A:96:AA
I (545621) iot_usbh_ecm: Notify - link speeds: 425984 kbps ↑, 425984 kbps ↓
I (546605) ECM_4G_MODULE: GOT_IP
I (546605) iot_eth.netif_glue: netif "eth" Got IP Address
I (546605) iot_eth.netif_glue: ~~~~~~~~~~~
I (546608) iot_eth.netif_glue: ETHIP:192.168.10.2
I (546614) iot_eth.netif_glue: ETHMASK:255.255.255.0
I (546619) iot_eth.netif_glue: ETHGW:192.168.10.1
I (546625) iot_eth.netif_glue: Main DNS: 192.168.10.3
I (546630) iot_eth.netif_glue: Backup DNS: 192.168.10.4
I (546636) iot_eth.netif_glue: ~~~~~~~~~~~
```
