# BLE Object Transfer Profile (OTP)

This component implements the BLE Object Transfer Profile based on the Bluetooth **Object Transfer Service (OTS)**. Object data is transferred over **L2CAP CoC (Connection-Oriented Channel)**. Both Client and Server roles are supported.

## Overview

- **Roles**: OTP Client / OTP Server
- **OACP (Object Action Control Point)**: Create, Delete, Read, Write, Execute, Abort, Checksum
- **OLCP (Object List Control Point)**: Object list discovery and selection (First/Last/Next/Previous, Go To, by index/ID, directory listing, filter, sort, request number of objects, etc.)
- **Transfer**: Object read/write over L2CAP CoC with configurable PSM and MTU
- **Write modes**: Overwrite, Truncate, Append, Patch
- **Resume**: Checksum-based and Current Size-based resume for partial transfers

## Dependencies

- **BLE OTS**: This component depends on `ble_ots`, which is selected automatically via Kconfig
- **BLE connection manager**: Depends on `esp_ble_conn_mgr` for connections and L2CAP CoC

## Configuration

In `idf.py menuconfig`:

- **Component config → Bluetooth → BLE Profile: Object Transfer → Enable Object Transfer Profile**: Enable the OTP component

Enabling OTP will automatically select BLE OTS.

## Quick Start

### 1. Initialization

```c
#include "esp_otp.h"

esp_ble_otp_config_t config = {
    .role = BLE_OTP_ROLE_CLIENT,           // or BLE_OTP_ROLE_SERVER
    .psm = BLE_OTP_PSM_DEFAULT,            // optional, default 0x0025
    .l2cap_coc_mtu = BLE_OTP_L2CAP_COC_MTU_DEFAULT,  // optional, default 512
    .auto_discover_ots = true,              // Client: auto-discover OTS after connect
};
esp_ble_otp_init(&config);
```

### 2. Register event handler

The application must listen to `BLE_OTP_EVENTS` and advance the flow or handle transfer data according to events:

```c
esp_event_handler_register(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, otp_event_handler, NULL);
```

Common events:

| Event | Description |
|-------|-------------|
| `BLE_OTP_EVENT_OTS_DISCOVERED` | OTS service discovery completed (Client) |
| `BLE_OTP_EVENT_OTS_DISCOVERY_FAILED` | OTS discovery failed |
| `BLE_OTP_EVENT_OBJECT_SELECTED` | Object selected via OLCP |
| `BLE_OTP_EVENT_OBJECT_CHANGED` | Object changed notification |
| `BLE_OTP_EVENT_OACP_RESPONSE` | OACP response (includes rsp_code) |
| `BLE_OTP_EVENT_OLCP_RESPONSE` | OLCP response |
| `BLE_OTP_EVENT_TRANSFER_CHANNEL_CONNECTED` | L2CAP CoC connected; can start send/receive |
| `BLE_OTP_EVENT_TRANSFER_DATA_RECEIVED` | Object data received (may be chunked when SDU > MTU; use `chunk_offset` + `total_len` to reassemble) |
| `BLE_OTP_EVENT_TRANSFER_DATA_SENT` | Data chunk sent |
| `BLE_OTP_EVENT_TRANSFER_EOF` | Object transfer end (EOF) |
| `BLE_OTP_EVENT_TRANSFER_COMPLETE` | Read/Write procedure completed successfully |
| `BLE_OTP_EVENT_TRANSFER_ERROR` | Transfer error |
| `BLE_OTP_EVENT_OACP_ABORTED` | OACP operation aborted |

## Usage flow summary

### Client

1. **Discover OTS**  
   If `auto_discover_ots == true`, discovery is started automatically after connection; otherwise call `esp_ble_otp_client_discover_ots(conn_handle)`.

2. **Read Feature**  
   Call `esp_ble_otp_client_read_feature()` to get OTS Feature and confirm server OACP/OLCP capabilities.

3. **Object selection (OLCP)**  
   When the session is idle, use OLCP to select an object, e.g.:
   - `esp_ble_otp_client_select_first()` / `select_last()` / `select_next()` / `select_previous()`
   - `esp_ble_otp_client_select_by_id()` / `select_by_index()`
   - `esp_ble_otp_client_select_directory_listing()` to select the directory listing object

4. **Read metadata**  
   After selection, call `esp_ble_otp_client_read_object_info()` before Read/Write/Delete so that metadata is valid.

5. **Object operations (OACP)**  
   - **Read**: `esp_ble_otp_client_read_object(conn_handle, offset, length)`  
     Establishes L2CAP CoC; receive data in `TRANSFER_DATA_RECEIVED`; `TRANSFER_COMPLETE` indicates read finished.
   - **Write**: `esp_ble_otp_client_write_object(conn_handle, offset, length, mode)`  
     After CoC is up, call `esp_ble_otp_client_send_data()` after `TRANSFER_CHANNEL_CONNECTED`.
   - **Delete**: `esp_ble_otp_client_delete_object(conn_handle)` (object must support delete property)
   - **Execute**: `esp_ble_otp_client_execute_object()` (if supported)
   - **Abort**: `esp_ble_otp_client_abort(conn_handle)`

6. **Directory listing**  
   Use `esp_ble_otp_client_read_directory_listing(conn_handle)` to select the directory listing object, read current size, and OACP Read full contents in one go.

7. **Resume**  
   - **Read resume**: `esp_ble_otp_client_resume_read_checksum()` to verify received data, then call `esp_ble_otp_client_read_object()` with remaining offset.
   - **Write resume**: `esp_ble_otp_client_resume_write_current_size()` or `esp_ble_otp_client_resume_write_checksum()`, then continue writing.

### Server

1. **Set Feature**  
   `esp_ble_otp_server_set_feature(&feature)` to declare supported OACP/OLCP capabilities.

2. **Register OACP/OLCP callbacks**  
   `esp_ble_otp_server_register_oacp_callback()` and `esp_ble_otp_server_register_olcp_callback()`; in the callbacks handle requests and call `esp_ble_otp_server_send_oacp_response()` / `esp_ble_otp_server_send_olcp_response()`.

3. **Transfer data**  
   After OACP Read/Write and L2CAP CoC establishment, handle send/receive via `BLE_OTP_EVENT_TRANSFER_*` events. For Read, the server calls `esp_ble_otp_server_send_data()` to send object data; for Write, receive in `TRANSFER_DATA_RECEIVED` and write to the object.

## API usage rules (summary)

- OTS must be discovered before any OACP/OLCP; feature bits determine which operations are available.
- Before Read/Write/Delete, select an object via OLCP and call `esp_ble_otp_client_read_object_info()` so metadata is valid.
- Execute requires the object to support the execute property; write commit can also be done via Execute.
- When Abort completes, `BLE_OTP_EVENT_OACP_ABORTED` is reported.

Common error codes:

- `ESP_ERR_INVALID_STATE`: Prerequisites not met (e.g. OTS not discovered, no object selected, metadata not valid)
- `ESP_ERR_NOT_SUPPORTED`: Feature not supported or object properties do not allow the operation
- `ESP_ERR_INVALID_ARG`: Invalid parameters
- `ESP_FAIL`: Stack or internal error

## Examples

- **ble_otp_client**: OTP Client example; after connect it runs discover OTS → select first object → read object → append write → read again → delete, etc.
- **ble_otp_server**: OTP Server example; exposes OTS and L2CAP CoC object transfer.

Paths in this repository:

- `examples/bluetooth/ble_profiles/ble_otp/ble_otp_client`
- `examples/bluetooth/ble_profiles/ble_otp/ble_otp_server`

Add the component and create an example project with the component manager:

```bash
idf.py add-dependency "espressif/ble_otp=*"
idf.py create-project-from-example "espressif/ble_otp=*:ble_otp_client"
# or
idf.py create-project-from-example "espressif/ble_otp=*:ble_otp_server"
```

## References

- [User Guide — esp-iot-solution](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/bluetooth/ble_profiles.html)
- Bluetooth specifications: Object Transfer Service (OTS) and Object Transfer Profile (OTP)
