# ChangeLog

## v1.1.0 - 2026-04-17

### Enhancements:

- Support SMP out-of-band (OOB) pairing.
- Add optional auto GATT Service Changed trigger based on configurable schema version (`BLE_CONN_MGR_GATT_CHANGED_AUTO`, default disabled).

## v1.0.1 - 2026-04-09

### Bug Fixes:

- Fix missing `ESP_BLE_CONN_EVENT_DATA_RECEIVE` on peripheral GATT writes when `uuid_fn` is NULL.

## v1.0.0 - 2026-03-05

### Enhancements:

- **Multi-Connections Support**: BLE connection manager now supports managing multiple concurrent BLE connections.
  - **Kconfig**: Added `BLE_CONN_MGR_MAX_CONNECTIONS` to set the maximum number of concurrent connections (default: 2; must not exceed the underlying BLE stack limit).
  - **Connection handle semantics**:
    - Valid range: `0x0000`–`0x0EFF`. Use `BLE_CONN_HANDLE_INVALID` (0xFFFF) to denote no connection or invalid handle.
    - Added macros: `BLE_CONN_HANDLE_INVALID`, `BLE_CONN_HANDLE_MAX`.
  - **Connection & discovery APIs**:
    - `esp_ble_conn_connect_to_addr()`: Initiate connection to a peer by address (Central role).
    - `esp_ble_conn_disconnect_by_handle()`: Disconnect by connection handle.
    - `esp_ble_conn_get_conn_handle()`: Get current (default) connection handle; returns `BLE_CONN_HANDLE_INVALID` when not connected.
    - `esp_ble_conn_get_conn_handle_by_addr()`: Get connection handle by peer address.
    - `esp_ble_conn_find_conn_handle_by_peer_addr()`: Find connection handle by peer address.
    - `esp_ble_conn_get_mtu_by_handle()`: Query MTU by connection handle.
    - `esp_ble_conn_get_peer_addr_by_handle()`: Get peer address by connection handle. Replaces deprecated `esp_ble_conn_find_peer_addr_by_conn_handle()` (available in v0.1.x); migrate by calling `esp_ble_conn_get_peer_addr_by_handle(conn_handle, out_addr, NULL)` instead.
    - `esp_ble_conn_get_disconnect_reason_by_handle()`: Get disconnect reason by connection handle.
  - **Per-connection GATT APIs** (accept valid `conn_handle` in range 0x0000–0x0EFF):
    - `esp_ble_conn_notify_by_handle()`, `esp_ble_conn_read_by_handle()`, `esp_ble_conn_write_by_handle()`, `esp_ble_conn_subscribe_by_handle()`.
  - **Per-connection GAP/Security** (APIs now take `conn_handle`):
    - `esp_ble_conn_update_params()`, `esp_ble_conn_mtu_update()`, `esp_ble_conn_security_initiate()`, `esp_ble_conn_passkey_reply()`, `esp_ble_conn_numcmp_reply()`.
  - **Advertising / scan data APIs**:
    - `esp_ble_conn_adv_data_set()`: Set raw advertising data (legacy or extended).
    - `esp_ble_conn_scan_rsp_data_set()`: Set raw scan response data (legacy or extended).
    - `esp_ble_conn_periodic_adv_data_set()`: Set periodic advertising data (when periodic adv is enabled).
  - **LE PHY APIs**:
    - `esp_ble_conn_get_phy()`: Get current TX/RX LE PHY for a connection.
    - `esp_ble_conn_set_preferred_phy()`: Set preferred LE PHY for a connection (tx/rx mask and coded options).
    - Added PHY mask constants: `ESP_BLE_CONN_PHY_MASK_1M`, `_2M`, `_CODED`, `_ALL` (0x07).
    - Added Coded PHY options: `ESP_BLE_CONN_PHY_CODED_OPT_ANY`, `_S2`, `_S8`.
  - **Common BLE enums and types**: Added enums and types for advertising/scan/address/PHY so applications can use a unified API: `esp_ble_conn_adv_conn_mode_t`, `esp_ble_conn_adv_disc_mode_t`, `esp_ble_conn_addr_type_t`, `esp_ble_conn_phy_t`, `esp_ble_conn_scan_filt_policy_t`.
  - **Events**: Event data structures now include `conn_handle` for multi-connection events (connected, disconnected, mtu_update, conn_param_update, enc_change, passkey_action).

## v0.1.6 - 2026-02-11

### Bug Fixes:

- Fix automatic addition of CUD (0x2901)
- Fix characteristic flag field truncation by widening from uint8_t to uint16_t to support extended properties. ([#662](https://github.com/espressif/esp-iot-solution/pull/662)) by [@acouvreur](https://github.com/acouvreur)
- Fix incorrect SM bitfield assignment that prevented security settings from taking effect. ([#661](https://github.com/espressif/esp-iot-solution/pull/661)) by [@acouvreur](https://github.com/acouvreur)

## v0.1.5 - 2026-01-27

### Enhancements:

- Added `esp_ble_conn_l2cap_coc_mem_init()` API to initialize L2CAP CoC SDU memory pool.
- Added `esp_ble_conn_l2cap_coc_mem_release()` API to release L2CAP CoC SDU memory pool.
- Added `esp_ble_conn_l2cap_coc_create_server()` API to create an L2CAP CoC server.
- Added `esp_ble_conn_l2cap_coc_connect()` API to initiate an L2CAP CoC connection.
- Added `esp_ble_conn_l2cap_coc_send()` API to send an SDU over an L2CAP CoC channel.
- Added `esp_ble_conn_l2cap_coc_recv_ready()` API to provide a receive buffer for an L2CAP CoC channel.
- Added `esp_ble_conn_l2cap_coc_accept()` API to accept an incoming L2CAP CoC connection.
- Added `esp_ble_conn_l2cap_coc_disconnect()` API to disconnect an L2CAP CoC channel.
- Added `esp_ble_conn_l2cap_coc_get_chan_info()` API to get L2CAP CoC channel information.
- Added `esp_ble_conn_get_peer_addr()` API to get current peer address.
- Added `esp_ble_conn_get_disconnect_reason()` API to get last disconnect reason.
- Added `esp_ble_conn_register_scan_callback()` API to register scan callback for BLE central role.
- Added `esp_ble_conn_scan_stop()` API to stop ongoing BLE scan.
- Added `esp_ble_conn_parse_adv_data()` API to parse AD type data from raw advertising data.

## v0.1.4 - 2026-01-06

### Enhancements:

- Enhanced advertising configuration to support 128-bit UUIDs.
- Added `ESP_BLE_CONN_EVENT_CCCD_UPDATE` event to report CCCD write status.
- Added `esp_ble_conn_get_conn_handle()` API to query current connection handle.
- Added `esp_ble_conn_get_mtu()` API to query current negotiated MTU.
- Added `esp_ble_conn_update_params()` API to update connection parameters.

## v0.1.3 - 2024-12-09

### Enhancements:

- Support Config UUID Information in BLE broadcasting.

## v0.1.2 - 2024-5-22

### Bug Fixes:

- Fix esp_ble_conn_mgr not report disconnect event

## v0.1.1 - 2023-6-12

### Bug Fixes:

- Add duration to wait for write data completed

## v0.1.0

This is the first release version for BLE connection management component in Espressif Component Registry, more detailed descriptions about the project, please refer to [User_Guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/bluetooth/ble_conn_mgr.html).

Features:
- Roles: Support peripheral and central, client and server
- Operations: Support GAP, GATT, GATT characteristic and services
- Periodic: Support periodic advertising and sync with the advertiser
