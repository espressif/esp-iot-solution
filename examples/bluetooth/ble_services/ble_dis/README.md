| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE Device Information Service Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates a GATT server and starts advertising, waiting to be connected by a GATT client.

The device information service exposes manufacturer and/or vendor information about a device.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding device information service and BLE connection management APIs.

To test this demo, any BLE scanner app can be used.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Hardware Required

* A development board with ESP32/ESP32-C3/ESP32-C2/ESP32-S3 SoC
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the project

Open the project configuration menu: 

```bash
idf.py menuconfig
```

In the `Example Configuration` menu:

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_DIS`.

In the `BLE Standard Services` menu:

* Select the optional functions of device from `GATT Device Information service`, default is disable.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Example Output

There is this console output when bleprph is connected and characteristic is read:

```
I (150) BTDM_INIT: BT controller compile version [80abacd]
I (150) coexist: coexist rom version 9387209
I (160) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (200) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (200) blecm_nimble: BLE Host Task Started
I (200) blecm_nimble: getting characteristic(0x2a00)
I (210) blecm_nimble: getting characteristic(0x2a01)
I (210) blecm_nimble: getting characteristic(0x2a05)
I (220) NimBLE: GAP procedure initiated: stop advertising.

I (220) NimBLE: GAP procedure initiated: advertise; 
I (230) NimBLE: disc_mode=2
I (230) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (240) NimBLE: 

I (27280) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (27280) app_main: Model name ESP
I (27280) app_main: Serial Number 84:f7:03:08:21:1a
I (27290) app_main: Firmware revision v4.3.5-dirty
I (27290) app_main: Hardware revision v4.3.5-dirty
I (27300) app_main: Software revision v4.3.5-dirty
I (27300) app_main: Manufacturer name Manufacturer
I (27310) app_main: System ID System
I (27310) app_main: PnP ID Vendor ID Source 0x1, Vendor ID 0x00, Product ID 0x00, Product Version 0x01
I (27880) blecm_nimble: mtu update event; conn_handle=1 cid=4 mtu=256
I (30970) blecm_nimble: Read attempted for characteristic UUID = 0x2a23, attr_handle = 12
I (32140) blecm_nimble: Read attempted for characteristic UUID = 0x2a24, attr_handle = 15
I (33820) blecm_nimble: Read attempted for characteristic UUID = 0x2a25, attr_handle = 18
I (37870) blecm_nimble: Read attempted for characteristic UUID = 0x2a26, attr_handle = 21
I (38890) blecm_nimble: Read attempted for characteristic UUID = 0x2a27, attr_handle = 24
I (42790) blecm_nimble: Read attempted for characteristic UUID = 0x2a28, attr_handle = 27
I (44560) blecm_nimble: Read attempted for characteristic UUID = 0x2a29, attr_handle = 30
I (47470) blecm_nimble: Read attempted for characteristic UUID = 0x2a50, attr_handle = 33
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
