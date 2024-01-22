| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- |

# BLE Alert Notification Profile Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates GATT client and performs passive scan, it then connects to peripheral device if the device advertises connectability and the device advertises support for the Alert Notification service (0x1811) as primary service UUID.

At the same time, it also creates an interactive shell and then controlled/interacted with over a serial interface to emulation the ANP behavior.

It performs three GATT operations against the specified peer:

* Reads the ANS Supported New Alert Category characteristic.
* After the read operation is completed, writes the ANS Alert Notification Control Point characteristic.
* After the write operation is completed, subscribes to notifications for the ANS Unread Alert Status characteristic.

If the peer does not support a required service, characteristic, or descriptor, then the peer lied when it claimed support for the alert notification service! When this happens, or if a GATT procedure fails, this function immediately terminates the connection.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding alert notification profile's characteristic operations and BLE connection management APIs

To test this demo, use any BLE GATT server app that advertises support for the Alert Notification service (0x1811) and includes it in the GATT database.

## Using Examples

### Functions Processing

Use the commands as follows can check the alert notification profile functions.

* anp

  * `-t (Mandatory)`: Get or Set function
  * `-c (Mandatory)`: Category ID
  * `-o (Mandatory)`: Enable, Disable and Recovery optional

* New Alert

  * Read the Value of Supported New Alert Category
```
anp -t 1 -c 1 -o 0
anp -t 1 -c 255 -o 0
```

  * Enable the specific category for New Alert to Notify when New Alert Count Changes
```
anp -t 2 -c 1 -o 0
```

  * Disable the specific category for New Alert to Notify when New Alert Count Changes
```
anp -t 2 -c 1 -o 1
```

  * Recovery from Connection Loss for New Alerts
```
anp -t 2 -c 1 -o 2
```

* Unread Alert

  * Read the Value of Supported Unread Alert Category
```
anp -t 3 -c 1 -o 0
anp -t 3 -c 255 -o 0
```

  * Enable the specific category for Unread Alert to Notify when Unread Alert Status Changes
```
anp -t 4 -c 1 -o 0
```

  * Disable the specific category for Unread Alert to Notify when Unread Alert Status Changes
```
anp -t 4 -c 1 -o 1
```

  * Recovery from Connection Loss for Unread Alert
```
anp -t 4 -c 1 -o 2
```

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Hardware Required

* A development board with ESP32/ESP32-C3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

Open the project configuration menu: 

```bash
idf.py menuconfig
```

In the `Example Configuration` menu:

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_ANS`.

### Build and Flash

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://idf.espressif.com/) for full steps to configure and use ESP-IDF to build projects.

## Example Output

This is the console output on successful connection:

```
This is an example of profile component.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the console environment.
I (348) BTDM_INIT: BT controller compile version [80abacd]
I (368) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (418) system_api: Base MAC address is not set
I (418) system_api: read default base MAC address from EFUSE
I (418) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (428) esp_nimble: BLE Host Task Started
I (428) NimBLE: GAP procedure initiated: stop advertising.

I (438) NimBLE: GAP procedure initiated: discovery; 
I (448) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=1 
I (458) NimBLE: duration=forever
I (458) NimBLE: 

I (468) NimBLE: GAP procedure initiated: connect; 
I (468) NimBLE: peer_addr_type=0 peer_addr=
I (468) NimBLE: 84:f7:03:09:09:ca
I (478) NimBLE:  scan_itvl=18 scan_window=17 itvl_min=25 itvl_max=26 latency=1 supervision_timeout=20 min_ce_len=3 max_ce_len=4 own_addr_type=0
I (488) NimBLE: 

esp32c3> I (638) NimBLE: GATT procedure initiated: discover all services

I (638) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (698) NimBLE: GATT procedure initiated: discover all characteristics; 
I (698) NimBLE: start_handle=1 end_handle=5

I (858) NimBLE: GATT procedure initiated: discover all characteristics; 
I (858) NimBLE: start_handle=6 end_handle=9

I (1048) NimBLE: GATT procedure initiated: discover all characteristics; 
I (1058) NimBLE: start_handle=10 end_handle=65535

I (1348) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1348) NimBLE: chr_val_handle=8 end_handle=9

I (1438) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1448) NimBLE: chr_val_handle=12 end_handle=13

I (1538) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1538) NimBLE: chr_val_handle=15 end_handle=17

I (1638) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1638) NimBLE: chr_val_handle=19 end_handle=20

I (1738) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1738) NimBLE: chr_val_handle=22 end_handle=24

I (1828) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1838) NimBLE: chr_val_handle=26 end_handle=65535

I (2028) esp_nimble: Service discovery complete; rc=0, conn_handle=1
I (2028) app_main: ESP_BLE_CONN_EVENT_DISC_COMPLETE
esp32c3> 
esp32c3> help
help 
  Print the list of registered commands

anp  [-t <01~04>] [-c <01~255>] [-o <00~02>]
  Alert Notification Client
  -t, --type=<01~04>  01 Get supported new alert category
                      02 Set supported new alert category
                      03 Get supported unread alert status category
                      04 Set supported unread alert status category
  -c, --category=<01~255>  Category ID
  -o, --option=<00~02>  0: Enable, 1: Disable, 2: Recover

esp32c3> anp -t 2 -c 3 -o 0
I (27848) NimBLE: GATT procedure initiated: write; 
I (27848) NimBLE: att_handle=26 len=2

I (27928) esp_anp: Configure Alert Notification Control Point Success!
I (27928) esp_anp: 00 03 
I (27928) esp_anp: Request the New Alert in the server to notify immediately on current enabled categories 3
I (27938) NimBLE: GATT procedure initiated: write; 
I (27958) NimBLE: att_handle=26 len=2

I (28028) esp_nimble: received notification; conn_handle=1 attr_handle=15 attr_len=2

I (28028) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (28038) esp_anp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (28038) esp_anp: Receive Notification of New Alert on cat_id 3 which count change to 0
I (28048) app_anp: Get the current message counts 3 from category 0 of supported new alert
I (28028) esp_anp: Configure Alert Notification Control Point Success!
I (28068) esp_anp: 04 03 
I (28068) app_anp: 4.06 Request Category 3 of Supported New Alert Category to Notify
esp32c3> I (43338) esp_nimble: received notification; conn_handle=1 attr_handle=15 attr_len=2

I (43338) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (43348) esp_anp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (43348) esp_anp: Receive Notification of New Alert on cat_id 3 which count change to 1
I (43358) app_anp: Get the current message counts 3 from category 1 of supported new alert
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
