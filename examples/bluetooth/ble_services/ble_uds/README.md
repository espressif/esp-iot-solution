| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE User Data Service Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates a GATT server and starts advertising, waiting to be connected by a GATT client.

The device information service exposes manufacturer and/or vendor information about a device.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding BLE Body Composition service and BLE connection management APIs.

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

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_UDS`.

In the `BLE Standard Services` menu:

* Select the optional functions of device from `GATT User Data Service`, default is disable.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Example Output1

There is this console output when bleprph is connected and characteristic is read:

```
I (330) BLE_INIT: BT controller compile version [9359a4d]
I (340) system_api: Base MAC address is not set
I (340) system_api: read default base MAC address from EFUSE
I (350) BLE_INIT: Bluetooth MAC: 58:cf:79:1e:9e:de

I (350) phy_init: phy_version 1150,7c3c08f,Jan 24 2024,17:32:21
I (420) blecm_nimble: BLE Host Task Started
I (420) blecm_nimble: No characteristic(0x2a00) found
I (420) blecm_nimble: No characteristic(0x2a01) found
I (420) blecm_nimble: No characteristic(0x2a05) found
I (430) NimBLE: GAP procedure initiated: stop advertising.

I (440) NimBLE: GAP procedure initiated: advertise; 
I (440) NimBLE: disc_mode=2
I (440) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (450) NimBLE: 

I (460) main_task: Returned from app_main()

```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
