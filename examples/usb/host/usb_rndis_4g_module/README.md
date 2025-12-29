| Supported Targets | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | -------- | -------- | -------- |

# USB Host RNDIS 4G Module Example

This example demonstrates how to use [iot_usbh_rndis](https://components.espressif.com/components/espressif/iot_usbh_rndis) to connect to 4G module with RNDIS protocol.

Supported devices refer to https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/usb/usb_host/usb_rndis.html

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

Users can Enable AT commands by setting `Example Configuration` in menuconfig. Then, enter the correct interface number, recompile, and run the program. It will periodically print the module's signal quality.

## Example Output

```
I (502) usbh_rndis: USB RNDIS network interface init success
I (507) iot_eth.netif_glue: ethernet attached to netif
I (513) RNDIS_4G_MODULE: IOT_ETH_EVENT_START
I (884) USBH_CDC: New device connected, address: 1
I (884) usbh_rndis: RNDIS device found: VID: 2C7C, PID: 0903, IFNUM: 0
I (885) cdc_descriptor: Found NOTIF endpoint: 7
I (890) cdc_descriptor: Found OUT endpoint: 12
I (896) cdc_descriptor: Found IN endpoint: 3
I (902) usbh_rndis: Max transfer packets: 10
I (905) usbh_rndis: Max transfer size: 3200
I (911) usbh_rndis: rndis query OID_GEN_SUPPORTED_LIST success,oid num :0
I (918) usbh_rndis: rndis set OID_GEN_CURRENT_PACKET_FILTER success
I (925) usbh_rndis: rndis set OID_802_3_MULTICAST_LIST success
I (931) usbh_rndis: RNDIS connected success
I (936) iot_eth: Ethernet link up
I (940) RNDIS_4G_MODULE: IOT_ETH_EVENT_CONNECTED
I (945) iot_eth.netif_glue: Set MAC Address: 00:00:00:00:00:00
I (5940) usbh_rndis: No events for 5s, sending keepalive
I (5941) usbh_rndis: RNDIS line change: 1
I (5941) iot_eth: Ethernet link up
I (5944) RNDIS_4G_MODULE: IOT_ETH_EVENT_CONNECTED
I (5949) iot_eth.netif_glue: Set MAC Address: 00:00:00:00:00:00
I (9454) RNDIS_4G_MODULE: GOT_IP
I (9454) iot_eth.netif_glue: netif "eth" Got IP Address
I (9454) iot_eth.netif_glue: ~~~~~~~~~~~
I (9457) iot_eth.netif_glue: ETHIP:192.168.43.100
I (9462) iot_eth.netif_glue: ETHMASK:255.255.255.0
I (9468) iot_eth.netif_glue: ETHGW:192.168.43.1
I (9473) iot_eth.netif_glue: Main DNS: 192.168.43.2
I (9479) iot_eth.netif_glue: Backup DNS: 192.168.43.3
I (9484) iot_eth.netif_glue: ~~~~~~~~~~~
```
