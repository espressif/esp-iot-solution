| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- |

# BLE Heart Rate Profile Example 

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates GATT client and performs passive scan, it then connects to peripheral device if the device advertises connectability and the device advertises support for the heart rate service (0x180D) as primary service UUID.

At the same time, it also creates an interactive shell and then controlled/interacted with over a serial interface to emulation the HRP behavior.

If the peer does not support a required service, characteristic, or descriptor, then the peer lied when it claimed support for the heart rate service! When this happens, or if a GATT procedure fails, this function immediately terminates the connection.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding heart rate profile's characteristic operations and BLE connection management APIs

To test this demo, use any BLE GATT server app that advertises support for the heart rate service (0x180D) and includes it in the GATT database.

## Using Examples

### Functions Processing

Use the commands as follows can check the heart rate profile functions.

* hrp

  * `-t (Mandatory)`: Get or Set function
  * `-c (Optional)`: Command

* Read the Value of Body Sensor Location

```
hrp -t 1
```
> The command is valid when the Body Sensor Location characteristic is static in a connection.

* Reset or energy expended

```
hrp -t 2 -c 1
hrp -t 2 -c 10
```
> The command is valid when the server supports the Energy Expended feature.

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

I (437) esp_nimble: BLE Host Task Started
I (437) NimBLE: GAP procedure initiated: stop advertising.

I (437) NimBLE: GAP procedure initiated: discovery; 
I (457) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=1 
I (457) NimBLE: duration=forever
I (467) NimBLE: 

                                                                                                                                     I (537) NimBLE: GAP procedure initiated: connect; 
I (537) NimBLE: peer_addr_type=0 peer_addr=
I (537) NimBLE: 84:f7:03:09:09:ca
I (547) NimBLE:  scan_itvl=18 scan_window=17 itvl_min=25 itvl_max=26 latency=1 supervision_timeout=20 min_ce_len=3 max_ce_len=4 own_addr_type=0
I (557) NimBLE: 

esp32c3> I (697) NimBLE: GATT procedure initiated: discover all services

I (707) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (757) NimBLE: GATT procedure initiated: discover all characteristics; 
I (757) NimBLE: start_handle=1 end_handle=5

I (917) NimBLE: GATT procedure initiated: discover all characteristics; 
I (927) NimBLE: start_handle=6 end_handle=9

I (1117) NimBLE: GATT procedure initiated: discover all characteristics; 
I (1117) NimBLE: start_handle=10 end_handle=65535

I (1347) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1347) NimBLE: chr_val_handle=8 end_handle=9

I (1437) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1447) NimBLE: chr_val_handle=12 end_handle=14

I (1537) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1537) NimBLE: chr_val_handle=16 end_handle=17

I (1637) NimBLE: GATT procedure initiated: discover all descriptors; 
I (1637) NimBLE: chr_val_handle=19 end_handle=65535

I (1827) esp_nimble: Service discovery complete; rc=0, conn_handle=1
I (1837) app_main: ESP_BLE_CONN_EVENT_DISC_COMPLETE
esp32c3> 
esp32c3> heI (4337) esp_nimble: received notification; conn_handle=1 attr_handle=12 attr_len=17

I (4337) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (4347) esp_hrp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

W (4347) esp_hrp: Sensor Contact Feature is Supported
W (4357) esp_hrp: Skin Contact is Detected
I (4357) app_hrp: Heart Rate Value 12098
W (4357) app_hrp: Sensor Contact Feature is Supported
W (4367) app_hrp: Skin Contact is Detected
I (4367) app_hrp: The multiple time between two R-Wave detections
I (4377) app_hrp: ec 75 00 00 00 00 00 00 00 00 00 00 00 00 
esp32c3> help
help 
  Print the list of registered commands

hrp  [-t <01~02>] [-c <0~255>]
  Heart Rate Profile
  -t, --type=<01~02>  01 Gets Body Sensor Location Characteristic
                      02 Set Heart Rate Control Point Characteristic
  -c, --cmd=<0~255>  1: Reset Energy Expended, Other: Reserved for Future Use

esp32c3> hrp -I (14347) esp_nimble: received notification; conn_handle=1 attr_handle=12 attr_len=4

I (14347) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (14357) esp_hrp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

W (14357) esp_hrp: Sensor Contact Feature is Supported
W (14367) esp_hrp: Skin Contact isn't Detected
I (14367) app_hrp: Heart Rate Value 38
W (14367) app_hrp: Sensor Contact Feature is Supported
W (14377) app_hrp: Skin Contact isn't Detected
I (14377) app_hrp: The Energy Expended feature is supported and accumulated 27125kJ
esp32c3> hrp -t 1
I (16517) NimBLE: GATT procedure initiated: read; 
I (16527) NimBLE: att_handle=16

I (16587) esp_nimble: characteristic read; conn_handle=1 attr_handle=16 len=1 value=
I (16587) app_hrp: Gets the Value of Body Sensor Location 4
esp32c3> hrp -t 2 -c 1I (24357) esp_nimble: received notification; conn_handle=1 attr_handle=12 attr_len=17

I (24357) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (24367) esp_hrp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

W (24367) esp_hrp: Sensor Contact Feature is Supported
W (24377) esp_hrp: Skin Contact is Detected
I (24377) app_hrp: Heart Rate Value 17386
W (24377) app_hrp: Sensor Contact Feature is Supported
W (24387) app_hrp: Skin Contact is Detected
I (24387) app_hrp: The multiple time between two R-Wave detections
I (24397) app_hrp: c0 76 e2 ae 00 00 00 00 00 00 00 00 00 00 
esp32c3> hrp -t 2 -c 1
E (25127) app_hrp: Invalid option
Command returned non-zero error code: 0x102 (ESP_ERR_INVALID_ARG)
esp32c3> hrp -t 2 -c 10
E (29447) app_hrp: Invalid option
Command returned non-zero error code: 0x102 (ESP_ERR_INVALID_ARG)
esp32c3> I (34367) esp_nimble: received notification; conn_handle=1 attr_handle=12 attr_len=19

I (34367) app_main: ESP_BLE_CONN_EVENT_DATA_RECEIVE

I (34377) esp_hrp: ESP_BLE_CONN_EVENT_DATA_RECEIVE

W (34377) esp_hrp: Sensor Contact Feature is Supported
W (34387) esp_hrp: Skin Contact is Detected
I (34387) app_hrp: Heart Rate Value 30605
W (34387) app_hrp: Sensor Contact Feature is Supported
W (34397) app_hrp: Skin Contact is Detected
I (34397) app_hrp: The Energy Expended feature is supported and accumulated 58749kJ
I (34407) app_hrp: The multiple time between two R-Wave detections
I (34427) app_hrp: e2 b6 8b fd 8f bc 00 00 00 00 00 00 00 00 
esp32c3> 
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.
