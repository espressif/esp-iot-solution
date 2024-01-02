| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE Alert Notification Service Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates a GATT server and starts advertising, waiting to be connected by a GATT client.

At the same time, it also creates an interactive shell that can be controlled/interacted with over a serial interface to emulate the behavior of ANS.

The alert notification service exposes alert information in a device. This information includes the following:

* Type of alert occurring in a device
* Additional text information such as caller ID or sender ID
* Count of new alerts
* Count of unread alert items

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding alert notification service and BLE connection management APIs.

To test this demo, any BLE scanner app can be used.

## Using Examples

### Functions Processing

Use the commands as follows can check the alert notification services functions.

* ans

  * `-t (Mandatory)`: Get or Set function
  * `-c (Mandatory)`: Category ID
  * `-o (Mandatory)`: Enable or Disable optional

* New Alert

  * Read the Value of Supported New Alert Category
```
ans -t 1 -c 1 -o 0
```

  * Enable the specific category for New Alert to Notify when New Alert Count Changes
```
ans -t 2 -c 1 -o 0
```

  * Disable the specific category for New Alert to Notify when New Alert Count Changes
```
ans -t 2 -c 1 -o 1
```

* Unread Alert

  * Read the Value of Supported Unread Alert Category
```
ans -t 3 -c 1 -o 0
```

  * Enable the specific category for Unread Alert to Notify when Unread Alert Status Changes
```
ans -t 4 -c 1 -o 0
```

  * Disable the specific category for Unread Alert to Notify when Unread Alert Status Changes
```
ans -t 4 -c 1 -o 1
```

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

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_ANS`.

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
I (367) BTDM_INIT: BT controller compile version [80abacd]
I (367) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (417) system_api: Base MAC address is not set
I (417) system_api: read default base MAC address from EFUSE
I (417) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (437) blecm_nimble: BLE Host Task Started
I (437) blecm_nimble: No characteristic(0x2a00) found
I (437) blecm_nimble: No characteristic(0x2a01) found
I (457) blecm_nimble: No characteristic(0x2a05) found
I (457) NimBLE: GAP procedure initiated: stop advertising.

I (467) NimBLE: GAP procedure initiated: advertise; 
I (467) NimBLE: disc_mode=2
I (467) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (477) NimBLE: 

esp32c3> help
help 
  Print the list of registered commands

ans  [-t <01~04>] [-c <00~07>] [-o <00~01>]
  Alert Notification Service
  -t, --type=<01~04>  01 Get supported new alert category
                      02 Set supported new alert category
                      03 Get supported unread alert status category
                      04 Set supported unread alert status category
  -c, --category=<00~07>  Category ID
  -o, --option=<00~01>  1: Enable, 0: Disable

esp32c3> I (6447) app_main: ESP_BLE_CONN_EVENT_CONNECTED
esp32c3> 
esp32c3> ans -t 1 -c 3 -o 0
I (15227) app_ans: Category 3 of Supported New Alert Category is Disabled
esp32c3> I (29907) blecm_nimble: Write attempt for uuid = 0x2a44, attr_handle = 26, data_len = 2
I (30007) blecm_nimble: Write attempt for uuid = 0x2a44, attr_handle = 26, data_len = 2
I (30007) NimBLE: GATT procedure initiated: notify; 
I (30017) NimBLE: att_handle=15

esp32c3> 
esp32c3> ans -t 1 -c 3 -o 0
I (33817) app_ans: Category 3 of Supported New Alert Category is Enabled
esp32c3> ans -t 2 -c 3 -o 0
I (50387) NimBLE: GATT procedure initiated: notify; 
I (50387) NimBLE: att_handle=15

I (50387) app_ans: Notify New Alert of Supported Category ID 3
esp32c3> ans -t 2 -c 3 -o 1
I (58047) NimBLE: GATT procedure initiated: notify; 
I (58047) NimBLE: att_handle=15

I (58047) app_ans: Notify New Alert of Supported Category ID 3
esp32c3> ans -t 3 -c 3 -o 0
I (97907) app_ans: Category 3 of Supported Unread Alert Status Category is Disabled
esp32c3> I (101637) blecm_nimble: Write attempt for uuid = 0x2a44, attr_handle = 26, data_len = 2
I (101737) blecm_nimble: Write attempt for uuid = 0x2a44, attr_handle = 26, data_len = 2
I (101737) NimBLE: GATT procedure initiated: notify; 
I (101747) NimBLE: att_handle=22

esp32c3> 
esp32c3> ans -t 3 -c 3 -o 0
I (105757) app_ans: Category 3 of Supported Unread Alert Status Category is Enabled
esp32c3> ans -t 4 -c 3 -o 0
I (125137) NimBLE: GATT procedure initiated: notify; 
I (125137) NimBLE: att_handle=22

I (125137) app_ans: Notify Unread Alert Status of Supported Category ID 3
esp32c3> ans -t 4 -c 3 -o 1
I (127517) NimBLE: GATT procedure initiated: notify; 
I (127517) NimBLE: att_handle=22

I (127517) app_ans: Notify Unread Alert Status of Supported Category ID 3
esp32c3>
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
