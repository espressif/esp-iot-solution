# BLE SPP Example

This example demonstrates how to combine the `ble_session_manager` component (connection management + GATT discovery) with the `esp_flux` component's `gatt_session` (reliable, fragmented data transfer) to implement a BLE Serial Port Profile (SPP)-like application.

## Overview

The BLE SPP example consists of two applications:

1. **ble_spp_server** - BLE SPP server (peripheral) that advertises the SPP service and accepts connections
2. **ble_spp_client** - BLE SPP client (central) that scans for SPP servers and connects to them

Both applications demonstrate:
- Multi-connection management (`ble_session_manager` / `ble_manager`)
- GATT service discovery (`ble_session_manager` / `gatt_discovery`, client only — the server already knows its own handles)
- Reliable data transmission with automatic fragmentation, SACK, and reassembly (`esp_flux` / `gatt_session`)
- Auto-reconnect functionality

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

You can configure the `ble_session_manager` component via `menuconfig`:

```bash
idf.py menuconfig
```

Navigate to `Component config` → `BLE Session Manager Configuration` to configure:
- `BLE_MANAGER_MAX_CONNECTIONS` - Maximum number of simultaneous BLE connections

`esp_flux` has no Kconfig options; its protocol parameters (window size defaults, fragment timeout, retransmission, etc.) are compile-time constants in `components/esp_flux/include/esp_flux_transport.h` — see the [esp_flux README](../../components/esp_flux/README.md#configuration).

## Example Output

### Server Output

```
I (1234) spp server: Starting BLE Component Test Example
I (1235) spp server: BLE Host Task Started
I (1236) spp server: BLE Host Synced
I (1237) spp server: GATT characteristic handle initialized: 0x0015
I (1238) spp server: Connected successfully to device XX:XX:XX:XX:XX:XX (conn_handle=1, first time)
I (1239) spp server: Auto reconnect enabled for device XX:XX:XX:XX:XX:XX
I (1240) spp server: GATT session created for conn_handle=1, val_handle=0x0015, mtu=23
W (2000) spp server: Session 1 stream 0 received data, Data Size: 4411 bytes (4.31 KB)
I (3000) spp server: Started send to conn_handle=1 (4411 bytes)
```

### Client Output

```
I (1234) spp client: Starting BLE Component Test Example
I (1235) spp client: BLE Host Task Started
I (1236) spp client: BLE Host Synced
I (1237) spp client: Connected successfully to device XX:XX:XX:XX:XX:XX (first time)
I (1238) spp client: Auto reconnect enabled for device XX:XX:XX:XX:XX:XX
I (1239) spp client: Service discovery completed for connection 1
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
- `ble_spp_server_advertise()` - Start advertising SPP service
- `gatt_svr_init()` - Initialize GATT server with SPP service
- `ble_manager_connect_cb()` - Handle client connections, create the GATT session
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
- `ble_manager_scan_cb()` - Handle scan results, connect on matching SPP UUID
- `ble_manager_connect_cb()` - Handle connection events, start GATT discovery
- `gatt_discovery_complete_cb()` - Handle discovery completion, create the GATT session
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
- Reduce `BLE_MANAGER_MAX_CONNECTIONS` (via `menuconfig`) if needed
- Monitor free heap size in logs

## Performance Tips

1. **Window Size**: Use window size 4-6 for optimal balance between throughput and stability
2. **MTU Size**: Negotiate larger MTU (up to 517 bytes) for better throughput
3. **Connection Interval**: Adjust based on latency requirements
4. **Data Size**: For best performance, send data in chunks that fit well within MTU

## API Reference

For detailed API documentation, please refer to:
- [ble_manager.h](common_components/ble_session_manager/include/ble_manager.h) - Connection management
- [gatt_discovery.h](common_components/ble_session_manager/include/gatt_discovery.h) - GATT service discovery
- [esp_flux_transport.h](../../components/esp_flux/include/esp_flux_transport.h) - Transport-agnostic protocol engine
- [flux_gatt_session.h](../../components/esp_flux/include/flux_gatt_session.h) - BLE GATT session (reliable transfer)

## See Also

- [BLE Session Manager Component README](common_components/ble_session_manager/README.md)
- [ESP Flux Component README](../../components/esp_flux/README.md)

## License

This example is licensed under the Apache License 2.0. See the LICENSE file for details.
