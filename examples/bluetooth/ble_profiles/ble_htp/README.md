| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- |

# BLE Health Thermometer Profile Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates GATT client and performs passive scan, it then connects to peripheral device if the device advertises connectability and the device advertises support for the health thermometer service (0x1809) as primary service UUID.

At the same time, it also creates an interactive shell and then controlled/interacted with over a serial interface to emulation the HTP behavior.

If the peer does not support a required service, characteristic, or descriptor, then the peer lied when it claimed support for the health thermometer service! When this happens, or if a GATT procedure fails, this function immediately terminates the connection.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding health thermometer profile's characteristic operations and BLE connection management APIs

To test this demo, use any BLE GATT server app that advertises support for the health thermometer service (0x1809) and includes it in the GATT database.

## Using Examples

### Functions Processing

Use the commands as follows can check the health thermometer profile functions.

* htp

  * `-t (Mandatory)`: Get or Set function
  * `-i (Optional)`: Interval

* Read the Value of Temperature Type
```
htp -t 1
```

* Read the Value of Measurement Interval
```
htp -t 2
```

* Set the Value of Measurement Interval to disable or enable periodic indications
```
htp -t 3 -i 0
htp -t 3 -i 10
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

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_HTS`.

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
I (347) BTDM_INIT: BT controller compile version [80abacd]
I (367) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (417) system_api: Base MAC address is not set
I (417) system_api: read default base MAC address from EFUSE
I (417) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (427) esp_nimble: BLE Host Task Started
I (427) NimBLE: GAP procedure initiated: stop advertising.

I (437) NimBLE: GAP procedure initiated: discovery; 
I (447) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=1 
I (457) NimBLE: duration=forever
I (457) NimBLE: 

I (517) NimBLE: GAP procedure initiated: connect; 
I (517) NimBLE: peer_addr_type=0 peer_addr=
I (517) NimBLE: 84:f7:03:09:09:ca
I (527) NimBLE:  scan_itvl=18 scan_window=17 itvl_min=25 itvl_max=26 latency=1 supervision_timeout=20 min_ce_len=3 max_ce_len=4 own_addr_type=0
I (537) NimBLE: 

esp32c3> I (677) NimBLE: GATT procedure initiated: discover all services

I (687) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (697) esp_nimble: received indication; conn_handle=1 attr_handle=23 attr_len=2

E (697) esp_nimble: Incorrect attr_handle 23 with indication received
I (707) NimBLE: GATT procedure initiated: discover all characteristics; 
I (707) NimBLE: start_handle=1 end_handle=5

I (857) NimBLE: GATT procedure initiated: discover all characteristics; 
I (857) NimBLE: start_handle=6 end_handle=9

I (1047) NimBLE: GATT procedure initiated: discover all characteristics; 
I (1047) NimBLE: start_handle=10 end_handle=65535

I (1347) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1347) NimBLE: chr_val_handle=8 end_handle=9

I (1437) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1447) NimBLE: chr_val_handle=12 end_handle=14

I (1537) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1537) NimBLE: chr_val_handle=16 end_handle=17

I (1637) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1637) NimBLE: chr_val_handle=19 end_handle=21

I (1737) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1737) NimBLE: chr_val_handle=23 end_handle=65535

I (1927) esp_nimble: Service discovery complete; rc=0, conn_handle=1
I (1927) app_main: ESP_BLE_CONN_EVENT_DISC_COMPLETE
esp32c3> 
esp32c3> help
help 
  Print the list of registered commands

htp  [-t <01~03>] [-i <0~65535>]
  Health Thermometer Profile
  -t, --type=<01~03>  01 Get Temperature Type Characteristic
                      02 Get Measurement Interval Characteristic
                      03 Set Measurement Interval Characteristic
  -i, --interval=<0~65535>  0: No periodic measurement, Other: Duration of measurement interval

esp32c3> htp -t 1
I (12327) NimBLE: GATT procedure initiated: read; 
I (12327) NimBLE: att_handle=16

I (12397) esp_nimble: characteristic read; conn_handle=1 attr_handle=16 len=1 value=
I (12397) app_htp: Gets the current temperature type value 7
esp32c3> htp -t 2
I (14407) NimBLE: GATT procedure initiated: read; 
I (14407) NimBLE: att_handle=23

I (14477) esp_nimble: characteristic read; conn_handle=1 attr_handle=23 len=2 value=
I (14477) app_htp: Gets the measurement interval value 2068
esp32c3> htp -t 3 -i 0
I (23637) NimBLE: GATT procedure initiated: write; 
I (23647) NimBLE: att_handle=23 len=2

I (23707) esp_htp: Sets the measurement interval value Success!
I (23707) esp_nimble: received indication; conn_handle=1 attr_handle=12 attr_len=5

I (23717) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (23717) esp_htp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (23727) app_htp: Measurement Temperature 62212°C
I (23707) esp_htp: 00 00 
I (23727) app_htp: Sets the measurement interval value 0
I (23797) esp_nimble: received notification; conn_handle=1 attr_handle=19 attr_len=5

I (23807) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (23817) esp_htp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (23817) app_htp: Intermediate Temperature 50762℉
esp32c3> htp -t 3 -i 10
I (27227) NimBLE: GATT procedure initiated: write; 
I (27227) NimBLE: att_handle=23 len=2

I (27307) esp_htp: Sets the measurement interval value Success!
I (27307) esp_nimble: received indication; conn_handle=1 attr_handle=12 attr_len=6

I (27327) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (27327) esp_htp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (27337) app_htp: Temperature Type 7
I (27337) app_htp: Measurement Temperature 7873°C
I (27317) esp_htp: 0a 00 
I (27347) app_htp: Sets the measurement interval value 10
I (27407) esp_nimble: received notification; conn_handle=1 attr_handle=19 attr_len=12

I (27407) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (27417) esp_htp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (27417) app_htp: 1970年1月1日  3:55:41
I (27437) app_htp: Intermediate Temperature 13545°C
esp32c3> 
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
