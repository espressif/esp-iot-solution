| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE TX Power Service Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates a GATT server and starts advertising, waiting to be connected by a GATT client.

The Tx power service expose the current transmit power level of a device when in a connection.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding TX power service and BLE connection management APIs.

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

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_TPS`.

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
I (316) BTDM_INIT: BT controller compile version [80abacd]
I (316) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (366) system_api: Base MAC address is not set
I (366) system_api: read default base MAC address from EFUSE
I (366) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (376) blecm_nimble: BLE Host Task Started
I (376) blecm_nimble: getting characteristic(0x2a00)
I (386) blecm_nimble: getting characteristic(0x2a01)
I (396) blecm_nimble: getting characteristic(0x2a05)
I (396) NimBLE: GAP procedure initiated: stop advertising.

I (406) NimBLE: GAP procedure initiated: advertise; 
I (406) NimBLE: disc_mode=2
I (416) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (426) NimBLE: 

I (54526) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (54526) app_main: TX Power Level 3dBm
I (54976) blecm_nimble: mtu update event; conn_handle=1 cid=4 mtu=256
I (58366) blecm_nimble: Read attempted for characteristic UUID = 0x2a07, attr_handle = 12
I (61006) blecm_nimble: Read attempted for characteristic UUID = 0x2a07, attr_handle = 12
I (61756) blecm_nimble: Read attempted for characteristic UUID = 0x2a07, attr_handle = 12 
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
