# BLE Electronic Shelf Label Profile

ESL Profile for Tag and Access Point roles.

## Tag role (`CONFIG_BLE_ESL_PROFILE`)

- Register ESL Service and Device Information Service
- Optional OTP Object Server for image slots (`CONFIG_BLE_ESL_OTS_SUPPORT`)
- Shared lifecycle SM: Unassociated / Configuring / Updating / Synchronized / Unsynchronized
- 60-minute Synchronized and Unsynchronized timeouts (`CONFIG_BLE_ESL_PROFILE_TIMEOUT_MIN`)
- Association writes and lifecycle ECP commands
- Default Display Image / Timed / Refresh, LED Control / Timed, Read Sensor
  (events `DISPLAY_IMAGE` / `REFRESH_DISPLAY` / `LED_CONTROL` / `SENSOR_READ`)
- Optional PAST receive and encrypted PAwR Ping/Display/LED response (EAD)
- Events on `BLE_ESLP_EVENTS`

## AP role (`CONFIG_BLE_ESL_PROFILE_AP`)

- Configure a connected Tag over GATT
- Read Tag info characteristics (`esp_ble_eslp_ap_read_info`)
- Typed commands (`ping` / `display_image` / `led_control` / ...) with ECP<->PAwR transport selection
- 30-second ECP procedure timeout (`BLE_ESLP_AP_EVENT_ECP_TIMEOUT`)
- OTP Object Client with `esp_ble_eslp_ap_transfer_image()`
- Track remote Tag lifecycle with the same state machine + timeouts
- Start or stop local PAwR advertising and PAST sync transfer
- Queue per-subevent commands encrypted with AP Sync Key (EAD)
- Decrypt PAwR responses with per-Tag Response Key
- Events on `BLE_ESLP_AP_EVENTS` (including `TAG_STATE_CHANGED` / `TAG_TIMEOUT` / `TAG_INFO`)

## Shared state machine

`esp_eslp_state.c` / `esp_eslp_state.h` implement one reusable SM used by:

- Tag: local ESL state
- AP: per-Tag tracking in `esp_eslp_ap_lifecycle.c`

## Directory layout

| Path | Contents |
|---|---|
| `include/` | Public APIs: `esp_eslp_tag.h`, `esp_eslp_ap.h`, `esp_eslp_state.h` |
| `private/` | Internal headers (`*_priv.h`), not for application use |
| `src/` | Implementation |

## Encrypted Advertising Data (EAD)

Synchronized-state PAwR packets use CSS Encrypted Data (AD type `0x31`) wrapping
an ESL AD (type `0x34`):

- AP -> Tag: encrypt with **AP Sync Key Material**
- Tag -> AP: encrypt with **ESL Response Key Material**

Requires `CONFIG_BT_NIMBLE_ENC_ADV_DATA` (selected by the ESL Profile Kconfig).

## Dependencies

- `ble_services`
- `ble_conn_mgr`
- `bt` / NimBLE (`ble_ead` / AES-CCM)
- `ble_otp` (when Tag OTS support and/or AP role is enabled)

## Examples

- `examples/bluetooth/ble_profiles/ble_esl/ble_esl_tag`
- `examples/bluetooth/ble_profiles/ble_esl/ble_esl_ap`

## Configuration

`Component config -> BLE Profile: Electronic Shelf Label`
