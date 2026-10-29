# Component: BLE Session Manager

A BLE (Bluetooth Low Energy) connection and GATT discovery management component based on the NimBLE protocol stack.

## Overview

The `ble_session_manager` component consists of two modules:

1. **BLE Manager** (`ble_manager`) - Connection management, scanning, and advertising
2. **GATT Discovery** (`gatt_discovery`) - Automatic GATT service, characteristic, and descriptor discovery

Reliable large-data transfer (fragmentation, SACK, sliding window) is **not** part of this component — it is provided by the `gatt_session` module of the [`esp_flux`](../../../../components/esp_flux/README.md) component, which the `ble_spp_client`/`ble_spp_server` examples use together with `ble_session_manager`.

## Features

### BLE Manager Features

- **Multi-connection Support**: Manage multiple concurrent BLE connections (bounded by `BLE_MANAGER_MAX_CONNECTIONS`)
- **Connection Management**: Automatic connection state tracking and management
- **Scanning & Advertising**: Start/stop scanning and advertising with configurable parameters
- **Connection Parameters**: Dynamic connection parameter negotiation (interval, latency, timeout)
- **Auto-reconnect**: Automatic reconnection with configurable retry attempts and intervals
- **RSSI Monitoring**: Real-time signal strength monitoring
- **MTU Management**: MTU size tracking and updates
- **Event Callbacks**: Comprehensive callback system for connection events

### GATT Discovery Features

- **Service Discovery**: Automatic discovery of all GATT services
- **Characteristic Discovery**: Discover all characteristics within services
- **Descriptor Discovery**: Discover all descriptors for characteristics
- **UUID Support**: Support for both UUID16 and UUID128 formats
- **Discovery Caching**: Cache discovered services, characteristics, and descriptors for quick access
- **Flexible Discovery**: Discover all services or specific services by UUID
- **Event Callbacks**: Callbacks for each discovery event (service found, characteristic found, etc.)

## Supported Operations

### Connection Operations
- Start/stop BLE advertising
- Start/stop BLE scanning
- Connect to remote devices
- Disconnect from devices
- Manage multiple concurrent connections
- Enable/disable auto-reconnect

### Discovery Operations
- Discover all GATT services
- Discover services by UUID
- Discover all characteristics
- Discover all descriptors
- Query discovered services/characteristics/descriptors

## Add Component to Your Project

Add it to your project's `main/CMakeLists.txt` (this component currently ships alongside the `esp_flux` examples and is referenced locally via `idf_component.yml`, not yet published to the component registry):

```cmake
idf_component_register(
    ...
    REQUIRES ble_session_manager
)
```

## Quick Start

### 1. Initialize BLE Manager

```c
#include "ble_manager.h"

// Define callbacks
ble_manager_callbacks_t callbacks = {
    .ble_adv_cb = ble_manager_adv_cb,
    .ble_scan_cb = ble_manager_scan_cb,
    .ble_connect_cb = ble_manager_connect_cb,
    .ble_disconnect_cb = ble_manager_disconnect_cb,
    .ble_conn_update_cb = NULL,
    .arg = NULL
};

// Initialize BLE manager
ble_manager_t *manager = ble_manager_init(&callbacks);
```

### 2. Start Scanning or Advertising

```c
// Start scanning for 10 seconds
ble_manager_start_scan(manager, 10000);

// Start advertising
struct ble_gap_adv_params adv_params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
};
ble_manager_start_advertising(manager, &adv_params, 0); // 0 = advertise indefinitely
```

### 3. Discover GATT Services

```c
#include "gatt_discovery.h"

// Start discovery
ble_discovery_callbacks_t disc_callbacks = {
    .discovery_complete_cb = on_discovery_complete,
    .service_found_cb = on_service_found,
    .characteristic_found_cb = on_characteristic_found,
    .descriptor_found_cb = NULL,
    .arg = NULL
};

gatt_discovery_t *discovery = gatt_discovery_start(conn_handle, &disc_callbacks);
gatt_discovery_discover_all_services(discovery);
```

### 4. Reliable Data Transfer

Once a connection and its characteristic handles are known (from discovery, or from the server's own GATT registration), use the `esp_flux` component's `gatt_session` API to send/receive data reliably — see the [`esp_flux` README](../../../../components/esp_flux/README.md#quick-start).

## Configuration

The component can be configured via `menuconfig`:

```bash
idf.py menuconfig
```

Navigate to `Component config` → `BLE Session Manager Configuration` to configure:
- `BLE_MANAGER_MAX_CONNECTIONS` - Maximum number of simultaneous BLE connections

## Examples

Complete examples can be found in `examples/esp_flux/`:
- `ble_spp_client` - BLE SPP client example
- `ble_spp_server` - BLE SPP server example

## API Reference

For detailed API documentation, please refer to the header files:
- [ble_manager.h](include/ble_manager.h) - BLE connection management APIs
- [gatt_discovery.h](include/gatt_discovery.h) - GATT discovery APIs

## License

This component is licensed under the Apache License 2.0. See the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.
