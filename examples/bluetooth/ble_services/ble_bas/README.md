| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE Battery Service Example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This example creates a GATT server and starts advertising, waiting to be connected by a GATT client.

At the same time, it also creates an interactive shell that can be controlled/interacted with over a serial interface to emulate the behavior of BAS.

The battery service exposes the Battery Level and other information for a battery in the context of the battery’s electrical connection to a device.

It uses Bluetooth controller based on BLE connection management.

This example aims at understanding battery service and BLE connection management APIs.

To test this demo, any BLE scanner app can be used.

## Using Examples

### Functions Processing

Use the commands as follows can check the battery services functions.

* bas

  * `-t (Mandatory)`: Get or Set function
  * `-c (Mandatory)`: Characteristic ID

* functions

| Supported Functions | Commands | Description |
| ----------------- | ----- | -------- |
| Battery Level | bas -t 1 -c 1 | Read the value |
| Battery Level | bas -t 0 -c 1 | Update the value and notify to the client if the Notify Properties is enabled |
| Battery Level Status | bas -t 1 -c 2 | Read the value |
| Battery Level Status | bas -t 0 -c 2 | Update/Notify the value and indicate to the client if the Indicate Properties is enabled |
| Estimated Service Date | bas -t 1 -c 3 | Read the value |
| Estimated Service Date | bas -t 0 -c 3 | Update/Notify the value and indicate to the client if the Indicate Properties is enabled |
| Battery Critical Status | bas -t 1 -c 4 | Read the value |
| Battery Critical Status | bas -t 0 -c 4 | Update/Indicate the value to the client |
| Battery Energy Status | bas -t 1 -c 5 | Read the value |
| Battery Energy Status | bas -t 0 -c 5 | Update/Notify the value and indicate to the client if the Indicate Properties is enabled |
| Battery Time Status | bas -t 1 -c 6 | Read the value |
| Battery Time Status | bas -t 0 -c 6 | Update/Notify the value and indicate to the client if the Indicate Properties is enabled |
| Battery Health Status | bas -t 1 -c 7 | Read the value |
| Battery Health Status | bas -t 0 -c 7 | Update/Notify the value and indicate to the client if the Indicate Properties is enabled |
| Battery Health Information | bas -t 1 -c 8 | Read the value |
| Battery Health Information | bas -t 0 -c 8 | Update/Indicate the value to the client |
| Battery Information | bas -t 1 -c 9 | Read the value |
| Battery Information | bas -t 0 -c 9 | Update/Indicate the value to the client |

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

* Select advertisement name of device from `Example Configuration --> Advertisement name`, default is `BLE_BAS`.

In the `BLE Standard Services` menu:

* Select the optional functions of device from `GATT Battery service`, default is disable.

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
This is an example of profile component.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the console environment.
I (346) BTDM_INIT: BT controller compile version [80abacd]
I (366) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (416) system_api: Base MAC address is not set
I (426) system_api: read default base MAC address from EFUSE
I (426) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (436) blecm_nimble: BLE Host Task Started
I (436) blecm_nimble: getting characteristic(0x2a00)
I (446) blecm_nimble: getting characteristic(0x2a01)
I (456) blecm_nimble: getting characteristic(0x2a05)
I (456) NimBLE: GAP procedure initiated: stop advertising.

I (456) NimBLE: GAP procedure initiated: advertise; 
I (466) NimBLE: disc_mode=2
I (466) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (486) NimBLE: 

esp32c3> I (234206) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (234646) blecm_nimble: mtu update event; conn_handle=1 cid=4 mtu=256
I (238236) blecm_nimble: Read attempted for characteristic UUID = 0x2a19, attr_handle = 12
esp32c3> 
esp32c3> help
help 
  Print the list of registered commands

bas  [-t <00~01>] [-c <01~09>]
  Battery Service
  -t, --type=<00~01>  00 Set, 01 Get
  -c, --characteristic=<01~09>  01 Battery Level
                                02 Battery Level Status
                                03 Estimated Service Date
                                04 Battery Critical Status
                                05 Battery Energy Status
                                06 Battery Time Status
                                07 Battery Health Status
                                08 Battery Health Information
                                09 Battery Information


esp32c3> bas -t 0 -c 1
I (256746) NimBLE: GATT procedure initiated: notify; 
I (256746) NimBLE: att_handle=12

esp32c3> bas -t 0 -c 1
I (260536) NimBLE: GATT procedure initiated: notify; 
I (260536) NimBLE: att_handle=12

esp32c3> bas -t 0 -c 1
I (264576) NimBLE: GATT procedure initiated: notify; 
I (264576) NimBLE: att_handle=12

esp32c3> bas -t 0 -c 1
I (267086) NimBLE: GATT procedure initiated: notify; 
I (267086) NimBLE: att_handle=12

esp32c3> I (271386) blecm_nimble: Read attempted for characteristic UUID = 0x2a19, attr_handle = 12
I (272196) blecm_nimble: Read attempted for characteristic UUID = 0x2a19, attr_handle = 12
I (272496) blecm_nimble: Read attempted for characteristic UUID = 0x2a19, attr_handle = 12

```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub. We will get back to you soon.
