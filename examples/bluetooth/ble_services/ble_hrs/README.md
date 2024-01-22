| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE Heart Rate Service Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates a GATT server and starts advertising, waiting to be connected by a GATT client.

The heart rate service exposes heart rate and other data related to a heart rate sensor intended for fitness applications.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding heart rate service and BLE connection management APIs.

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

* Select the optional functions of device from `GATT Heart Rate service`, default is disable.

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
I (376) system_api: Base MAC address is not set
I (376) system_api: read default base MAC address from EFUSE
I (376) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (386) blecm_nimble: BLE Host Task Started
I (386) blecm_nimble: getting characteristic(0x2a00)
I (386) blecm_nimble: getting characteristic(0x2a01)
I (396) blecm_nimble: getting characteristic(0x2a05)
I (406) NimBLE: GAP procedure initiated: stop advertising.

I (406) NimBLE: GAP procedure initiated: advertise; 
I (416) NimBLE: disc_mode=2
I (416) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (426) NimBLE: 

I (5066) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (15066) app_main: Heart Rate Value 17
I (15066) app_main: Skin Contact should be detected on Sensor Contact Feature Supported
I (15066) NimBLE: GATT procedure initiated: notify; 
I (15066) NimBLE: att_handle=12

I (25066) app_main: Heart Rate Value 28
I (25066) app_main: Sensor Contact Feature is Supported
I (25066) app_main: Skin Contact is Detected
I (25066) app_main: The Energy Expended feature is supported and accumulated 29520kJ
I (25076) NimBLE: GATT procedure initiated: notify; 
I (25076) NimBLE: att_handle=12

I (35066) app_main: Heart Rate Value 50373
I (35066) app_main: Sensor Contact Feature is Supported
I (35066) app_main: Skin Contact is Detected
I (35066) app_main: The multiple time between two R-Wave detections
I (35076) app_main: cb fa 3e ec 12 3b 2c 5d c9 6d 00 00 00 00 
I (35076) NimBLE: GATT procedure initiated: notify; 
I (35086) NimBLE: att_handle=12

I (45066) app_main: Heart Rate Value 179
I (45066) app_main: Sensor Contact Feature isn't Supported
I (45066) app_main: Skin Contact isn't Detected
I (45066) app_main: The multiple time between two R-Wave detections
I (45076) app_main: cd 14 00 00 00 00 00 00 00 00 00 00 00 00 
I (45076) NimBLE: GATT procedure initiated: notify; 
I (45086) NimBLE: att_handle=12

I (55066) app_main: Heart Rate Value 9821
I (55066) app_main: Sensor Contact Feature is Supported
I (55066) app_main: Skin Contact isn't Detected
I (55066) app_main: The Energy Expended feature is supported and accumulated 31366kJ
I (55076) NimBLE: GATT procedure initiated: notify; 
I (55076) NimBLE: att_handle=12

I (58946) blecm_nimble: Read attempted for characteristic UUID = 0x2a38, attr_handle = 16
I (63526) blecm_nimble: Read attempted for characteristic UUID = 0x2a38, attr_handle = 16
I (65066) app_main: Heart Rate Value 84
I (65066) app_main: Sensor Contact Feature is Supported
I (65066) app_main: Skin Contact is Detected
I (65066) app_main: The Energy Expended feature is supported and accumulated 51049kJ
I (65076) NimBLE: GATT procedure initiated: notify; 
I (65076) NimBLE: att_handle=12
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
