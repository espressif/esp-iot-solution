| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- |

# BLE SPP peripheral example

  In Bluetooth classic (BR/EDR) systems, a Serial Port Profile (SPP) is an adopted profile defined by the Bluetooth Special Interest Group (SIG) used to emulate a serial port connection over a Bluetooth wireless connection. For BLE systems, an adopted SPP profile over BLE is not defined, thus emulation of a serial port must be implemented as a vendor-specific custom profile.

  This reference design consists of two Demos, the BLE SPP server and BLE SPP client that run on their respective endpoints. These devices connect and exchange data wirelessly with each other. This capability creates a virtual serial link over the air. Each byte input can be sent and received by both the server and client. The SPP server is implemented as the [spp_server](../spp_server) demo while the SPP client is implemented as the [spp_client](../spp_client) demo. Espressif designed the BLE SPP applications to use the UART transport layer but you could adapt this design to work with other serial protocols, such as SPI.

  This vendor-specific custom profile is implemented in [app_main.c](../spp_client/main/app_main.c) and [app_main.c](../spp_server/main/app_main.c).

## Using Examples

### Initialization

  Both the server and client will first initialize the UART and BLE. The server demo will set up the serial port service with standard GATT and GAP services in the attribute server. The client demo will scan the BLE broadcast over the air to find the SPP server.

### Functions Processing

  The spp server has two characteristic and one event processing functions:

```c
  esp_spp_chr_cb(const uint8_t *inbuf, uint16_t inlen, uint8_t **outbuf, uint16_t *outlen, void *priv_data);
  app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
```

  The SPP client has one main event processing functions:

```c
  app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
```

  These are some queues and tasks used by SPP application:

  Queues:

  * spp_uart_queue       - Uart data messages received from the Uart

  Tasks:

  * `ble_server_uart_task`            - process Uart

### Packet Structure

  After the Uart received data, the data will be posted to Uart task. Then, in the UART_DATA event, the raw data may be retrieved. The max length is 120 bytes every time.
  If you run the BLE SPP demo with two ESP32 chips, the MTU size will be exchanged for 200 bytes after the ble connection is established, so every packet can be send directly.
  If you only run the ble_spp_server demo, and it was connected by a phone, the MTU size may be less than 123 bytes. In such a case the data will be split into fragments and send in turn.
  In every packet, we add 4 bytes to indicate that this is a fragment packet. The first two bytes contain "##" if this is a fragment packet, the third byte is the total number of the packets, the fourth byte is the current number of this packet.
  The phone APP need to check the structure of the packet if it want to communicate with the ble_spp_server demo.

### Sending Data Wirelessly

  The client will be sending WriteNoRsp packets to the server. The server side sends data through notifications. When the Uart receives data, the Uart task places it in the buffer.

### Receiving Data Wirelessly

   The server will receive this data in the BLE_GATT_ACCESS_OP_WRITE_CHR event.

### GATT Server Attribute Table

  characteristic|UUID|Permissions
  :-:|:-:|:-:
  SPP_DATA_RECV_CHAR|0xABF1|READ&WRITE_NR
  SPP_DATA_NOTIFY_CHAR|0xABF2|READ&NOTIFY
  SPP_COMMAND_CHAR|0xABF3|READ&WRITE_NR
  SPP_STATUS_CHAR|0xABF4|READ & NOTIFY

This example creates GATT client and performs passive scan, it then connects to peripheral device if the device advertises connectability and the write characteristic.

It performs three GATT operations against the specified peer:

* Discover all services,characteristics and descriptors.

* After the discovery is completed, take UART input from user and write characteristic.

## How to use example

Before project configuration and build, be sure to set the correct chip target using:

```bash
idf.py set-target <chip_name>
```

### Hardware Required

* A development board with ESP32/ESP32-C3/ESP32-C2/ESP32-S3 SoC
* A USB cable for Power supply and programming

See [Development Boards](https://www.espressif.com/en/products/devkits) for more information about it.

### Configure the Project

Open the project configuration menu: 

```bash
idf.py menuconfig
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

This is the console output on successful connection:

```
I (320) BTDM_INIT: BT controller compile version [80abacd]
I (320) phy_init: phy_version 950,11a46e9,Oct 21 2022,08:56:12
I (370) system_api: Base MAC address is not set
I (370) system_api: read default base MAC address from EFUSE
I (370) BTDM_INIT: Bluetooth MAC: 84:f7:03:08:21:1a

I (380) blecm_nimble: BLE Host Task Started
I (380) blecm_nimble: getting characteristic(0x2a00)
I (390) blecm_nimble: getting characteristic(0x2a01)
I (390) blecm_nimble: getting characteristic(0x2a05)
I (400) app_main: Callback for read
I (410) NimBLE: GAP procedure initiated: stop advertising.

I (410) NimBLE: GAP procedure initiated: advertise; 
I (420) NimBLE: disc_mode=2
I (420) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=256 adv_itvl_max=256
I (430) NimBLE: 

I (4100) app_main: ESP_BLE_CONN_EVENT_CONNECTED

I (7000) blecm_nimble: Write attempt for uuid = 0xabf1, attr_handle = 12, data_len = 1
I (7000) app_main: Callback for write
I (7560) blecm_nimble: Write attempt for uuid = 0xabf1, attr_handle = 12, data_len = 1
I (7560) app_main: Callback for write
I (7910) blecm_nimble: Write attempt for uuid = 0xabf1, attr_handle = 12, data_len = 1
I (7910) app_main: Callback for write
I (8080) blecm_nimble: Write attempt for uuid = 0xabf1, attr_handle = 12, data_len = 1
I (8080) app_main: Callback for write
I (10770) NimBLE: GATT procedure initiated: notify; 
I (10770) NimBLE: att_handle=12

I (10770) app_main: Notification sent successfully
I (10840) blecm_nimble: Read attempted for characteristic UUID = 0xabf1, attr_handle = 12
I (10840) app_main: Callback for read
I (11540) NimBLE: GATT procedure initiated: notify; 
I (11540) NimBLE: att_handle=12

I (11540) app_main: Notification sent successfully
I (11620) blecm_nimble: Read attempted for characteristic UUID = 0xabf1, attr_handle = 12
I (11620) app_main: Callback for read

```
