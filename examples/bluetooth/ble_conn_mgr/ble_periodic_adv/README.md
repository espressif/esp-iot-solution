| Supported Targets | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- | -------- | -------- | -------- |

# BLE Periodic Advertiser Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example starts periodic advertising with non resolvable private address.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding periodic advertisement and related BLE connection management APIs.

To test this demo, any BLE Periodic Sync app can be used.

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Hardware Required

* A development board with ESP32-C3/ESP32-C2/ESP32-S3 SoC
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the project

Open the project configuration menu: 

```bash
idf.py menuconfig
```

In the `Example Configuration` menu:

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `ESP_PERIODIC_ADV`.
* Select set extended advertisement data of device from `Example Configuration --> Enable Set Extended Adv Data`, default is `n`.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Example Output

There is this console output when periodic_adv is started:

```
I (316) BTDM_INIT: BT controller compile version [80abacd]
I (316) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (366) system_api: Base MAC address is not set
I (366) system_api: read default base MAC address from EFUSE
I (376) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (376) blecm_nimble: BLE Host Task Started
I (386) blecm_nimble: getting characteristic(0x2a00)
I (386) blecm_nimble: getting characteristic(0x2a01)
I (396) blecm_nimble: getting characteristic(0x2a05)
I (406) blecm_nimble: Instance 1 started (periodic)
I (406) blecm_nimble: Instance 1 started (extended)
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
