| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE Health Thermometer Service Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates a GATT server and starts advertising, waiting to be connected by a GATT client.

The health thermometer service exposes temperature and other data related to a thermometer used for healthcare applications.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding health thermometer service and BLE connection management APIs.

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

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_HTS`.
* Select thermometer role which instantiate one device information service of device from `Example Configuration --> Thermometer Role`, default is disable.

In the `BLE Standard Services` menu:

* Select the optional functions of device from `GATT Health Thermometer service`, default is disable.

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
I (319) BTDM_INIT: BT controller compile version [80abacd]
I (319) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (379) system_api: Base MAC address is not set
I (379) system_api: read default base MAC address from EFUSE
I (379) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (389) blecm_nimble: BLE Host Task Started
I (389) blecm_nimble: getting characteristic(0x2a00)
I (399) blecm_nimble: getting characteristic(0x2a01)
I (399) blecm_nimble: getting characteristic(0x2a05)
I (409) NimBLE: GAP procedure initiated: stop advertising.

I (419) NimBLE: GAP procedure initiated: advertise; 
I (419) NimBLE: disc_mode=2
I (419) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (429) NimBLE: 

I (2239) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (2239) NimBLE: GATT procedure initiated: indicate; 
I (2239) NimBLE: att_handle=23

I (25559) blecm_nimble: Read attempted for characteristic UUID = 0x2a1d, attr_handle = 16
I (48799) blecm_nimble: Read attempted for characteristic UUID = 0x2a21, attr_handle = 23
I (68949) blecm_nimble: Write attempt for uuid = 0x2a21, attr_handle = 23, data_len = 2
I (68949) app_main: Measurement Temperature 39041°C
I (68949) NimBLE: GATT procedure initiated: indicate; 
I (68959) NimBLE: att_handle=12

I (69039) app_main: 1970年1月1日  0:1:8
I (69039) app_main: Intermediate Temperature 34252°F
I (69039) NimBLE: GATT procedure initiated: notify; 
I (69049) NimBLE: att_handle=19

I (73039) blecm_nimble: Read attempted for characteristic UUID = 0x2a21, attr_handle = 23
I (79049) app_main: Measurement Temperature 14901°F
I (79049) app_main: Temperature Type 6
I (79049) NimBLE: GATT procedure initiated: indicate; 
I (79059) NimBLE: att_handle=12

I (79149) app_main: Intermediate Temperature 37898°F
I (79149) app_main: Temperature Type 0
I (79149) NimBLE: GATT procedure initiated: notify; 
I (79159) NimBLE: att_handle=19

I (89049) app_main: 1970年1月1日  0:1:28
I (89049) app_main: Measurement Temperature 1674°F
I (89049) NimBLE: GATT procedure initiated: indicate; 
I (89059) NimBLE: att_handle=12

I (89159) app_main: 1970年1月1日  0:1:28
I (89159) app_main: Intermediate Temperature 42280°C
I (89159) NimBLE: GATT procedure initiated: notify; 
I (89169) NimBLE: att_handle=19

I (99049) app_main: Measurement Temperature 6344°F
I (99049) NimBLE: GATT procedure initiated: indicate; 
I (99049) NimBLE: att_handle=12

I (99139) app_main: 1970年1月1日  0:1:38
I (99139) app_main: Intermediate Temperature 2420°C
I (99139) app_main: Temperature Type 3
I (99139) NimBLE: GATT procedure initiated: notify; 
I (99149) NimBLE: att_handle=19

I (109049) app_main: 1970年1月1日  0:1:48
I (109049) app_main: Measurement Temperature 53149°C
I (109049) app_main: Temperature Type 9
I (109059) NimBLE: GATT procedure initiated: indicate; 
I (109059) NimBLE: att_handle=12

I (109149) app_main: 1970年1月1日  0:1:48
I (109149) app_main: Intermediate Temperature 32984°C
I (109149) NimBLE: GATT procedure initiated: notify; 
I (109149) NimBLE: att_handle=19

I (110379) blecm_nimble: Read attempted for characteristic UUID = 0x2a1d, attr_handle = 16
I (119049) app_main: 1970年1月1日  0:1:58
I (119049) app_main: Measurement Temperature 19839°F
I (119049) app_main: Temperature Type 1
I (119059) NimBLE: GATT procedure initiated: indicate; 
I (119059) NimBLE: att_handle=12

I (119159) app_main: 1970年1月1日  0:1:58
I (119159) app_main: Intermediate Temperature 63994°C
I (119159) NimBLE: GATT procedure initiated: notify; 
I (119159) NimBLE: att_handle=19
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
