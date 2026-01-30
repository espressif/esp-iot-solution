# ChangeLog

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
