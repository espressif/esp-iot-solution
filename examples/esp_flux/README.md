# BLE SPP Example

This example demonstrates how to combine the `ble_conn_mgr` component (connection management + GATT discovery) with the `esp_flux` component's `gatt_session` (reliable, fragmented data transfer) to implement a BLE Serial Port Profile (SPP)-like application.

## Overview

The BLE SPP example consists of two applications:

1. **ble_spp_server** - BLE SPP server (peripheral) that advertises the SPP service and accepts connections
2. **ble_spp_client** - BLE SPP client (central) that scans for SPP servers and connects to them

Both applications demonstrate:
- Multi-connection management (`ble_conn_mgr`)
- GATT service discovery (`ble_conn_mgr`, client only — the server already knows its own handles)
- Reliable data transmission with automatic fragmentation, SACK, and reassembly (`esp_flux` / `gatt_session`)
- Auto-reconnect via an example FreeRTOS timer (server: re-advertise every 2 s; client: every 5 s, while a connection slot is free, `scan_stop` then `scan_start`)

Once connected (and once connection parameters are negotiated), both the client and the server continuously send a fixed-size test buffer to each other whenever their send session is idle.

## Hardware Requirements

- An ESP32 series SoC with BLE (NimBLE) support (tested with ESP32-H2, ESP32-C2)
- USB cable for programming and power

## Build and Flash

### 1. Set Target Chip

```bash
cd examples/esp_flux/ble_spp_server  # or ble_spp_client
idf.py set-target esp32h2
```

### 2. Build the Project

```bash
idf.py build
```

### 3. Flash and Monitor

```bash
idf.py flash monitor
```

Or flash and monitor separately:

```bash
idf.py flash
idf.py monitor
```

## Usage

### Running the Server

1. Build and flash the `ble_spp_server` example
2. The server will start advertising with device name "ble-spp-server"
3. Wait for clients to connect
4. Once connected and connection parameters are ready, the server will:
   - Receive data from the client and log its size
   - Continuously send a fixed-size test buffer (`SPP_TEST_TRANSFER_SIZE`, ~4.3 KB) whenever its send session is idle

### Running the Client

1. Build and flash the `ble_spp_client` example
2. The client will start scanning for SPP servers
3. When a server is found, it will automatically connect
4. After connection:
   - GATT service discovery is performed
   - Notifications are enabled on the SPP characteristic
   - Once connection parameters are ready, the client also continuously sends the same fixed-size test buffer whenever its send session is idle

### Testing with Multiple Devices

1. Flash `ble_spp_server` on one or more devices
2. Flash `ble_spp_client` on one or more devices
3. The client(s) will automatically discover and connect to available servers
4. Data transmission will occur between all connected devices

## Configuration

### Transfer Size

The test transfer size is a compile-time constant in `main/main.c` on both the client and the server:

```c
#define SPP_TEST_TRANSFER_SIZE  (482 + 491 * 8) // 4411 bytes
```

Change this constant (and rebuild) to test different payload sizes. The send call also sets the sliding window and SACK threshold:

```c
gatt_session_fragment_send(session, send_buffer, SPP_TEST_TRANSFER_SIZE, 0xFF, 80);
//                                                                        ^window ^threshold%
```

### Component Configuration

You can configure the `ble_conn_mgr` component via `menuconfig`:

```bash
idf.py menuconfig
```

Navigate to `Component config` → `BLE Connection Management` to configure:
- `BLE_CONN_MGR_ROLE` - Peripheral (server) or Central (client)
- `BLE_CONN_MGR_MAX_CONNECTIONS` - Maximum number of simultaneous BLE connections

`esp_flux` has no Kconfig options; its protocol parameters (window size defaults, fragment timeout, retransmission, etc.) are compile-time constants in `components/esp_flux/include/esp_flux_transport.h` — see the [esp_flux README](../../components/esp_flux/README.md#configuration).

## Example Output

### Server Output

```
I (1234) spp server: Starting BLE Component Test Example
I (1235) spp server: GATT characteristic handle initialized: 0x0015
I (1236) spp server: Connected successfully to device XX:XX:XX:XX:XX:XX (conn_handle=1)
I (1237) spp server: GATT session created for conn_handle=1, val_handle=0x0015, mtu=23
W (2000) spp server: Session 1 stream 0 received data, Data Size: 4411 bytes (4.31 KB)
I (3000) spp server: Started send to conn_handle=1 (4411 bytes)
```

### Client Output

```
I (1234) spp client: Starting BLE Component Test Example
I (1235) spp client: Connected successfully to device XX:XX:XX:XX:XX:XX (conn_handle=1)
I (1236) spp client: Service discovery completed for connection 1
I (1240) spp client: Started send to conn_handle=1 (4411 bytes)
I (1241) spp client: Session conn_handle: 1 progress: 25% (1102/4411 bytes)
I (1242) spp client: Session conn_handle: 1 progress: 50% (2205/4411 bytes)
...
I (1245) spp client: Session conn_handle 1 stream 0 send completed successfully (4411 bytes)
```

## Code Structure

### Server (`ble_spp_server`)

```
ble_spp_server/
├── main/
│   ├── main.c          # Main application code
│   └── CMakeLists.txt
├── CMakeLists.txt
└── sdkconfig.defaults  # Default configuration
```

Key functions:
- `esp_ble_conn_add_svc()` / `esp_ble_conn_start()` - Register the SPP service and start advertising
- `app_ble_conn_event_handler()` - Handle connect / disconnect / RX, create the GATT session
- `spp_reconnect_timer_cb()` - Re-start advertising after disconnect
- `gatt_data_received_cb()` - Handle received data
- `gatt_session_complete_cb()` - Handle send completion

### Client (`ble_spp_client`)

```
ble_spp_client/
├── main/
│   ├── main.c          # Main application code
│   └── CMakeLists.txt
├── CMakeLists.txt
└── sdkconfig.defaults  # Default configuration
```

Key functions:
- `spp_scan_cb()` - Filter scan results and connect on matching SPP UUID
- `app_ble_conn_event_handler()` - Handle connect / disconnect / discovery, create the GATT session
- `spp_reconnect_timer_cb()` - Every 5 s, while the connected count is below `BLE_CONN_MGR_MAX_CONNECTIONS`, call `esp_ble_conn_scan_stop()` then `esp_ble_conn_scan_start()`. No peer address is saved, and there is no retry limit
- `gatt_data_received_cb()` - Handle received data
- `gatt_session_complete_cb()` - Handle send completion

## Troubleshooting

### Connection Issues

- Ensure both devices are powered on and in range
- Check that the server is advertising (check logs)
- Verify that the client is scanning (check logs)
- Check connection parameters (interval, latency, timeout)

### Data Transmission Issues

- Verify MTU negotiation completed successfully
- Check window size settings (too large may cause issues)
- Monitor heap memory (large data requires sufficient memory)
- Check retransmission timeout settings

### Memory Issues

- Reduce `SPP_TEST_TRANSFER_SIZE` in `main/main.c` if running out of memory
- Reduce `BLE_CONN_MGR_MAX_CONNECTIONS` (via `menuconfig`) if needed
- Monitor free heap size in logs

## Performance Tips

1. **Window Size**: Use window size 4-6 for optimal balance between throughput and stability
2. **MTU Size**: Negotiate larger MTU (up to 517 bytes) for better throughput
3. **Connection Interval**: Adjust based on latency requirements
4. **Data Size**: For best performance, send data in chunks that fit well within MTU

## API Reference

For detailed API documentation, please refer to:
- [esp_ble_conn_mgr.h](../../components/bluetooth/ble_conn_mgr/include/esp_ble_conn_mgr.h) - Connection management
- [esp_flux_transport.h](../../components/esp_flux/include/esp_flux_transport.h) - Transport-agnostic protocol engine
- [flux_gatt_session.h](../../components/esp_flux/include/flux_gatt_session.h) - BLE GATT session (reliable transfer)

## See Also

- [BLE Connection Management Component README](../../components/bluetooth/ble_conn_mgr/README.md)
- [ESP Flux Component README](../../components/esp_flux/README.md)

## License

This example is licensed under the Apache License 2.0. See the LICENSE file for details.
