## BLE Connection Management Component

[![Component Registry](https://components.espressif.com/components/espressif/ble_conn_mgr/badge.svg)](https://components.espressif.com/components/espressif/ble_conn_mgr)

- [User Guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/bluetooth/ble_conn_mgr.html)

The ``ble_conn_mgr`` is a BLE connection management component that provides a simplified API interface for accessing commonly used BLE functionalities. It supports various scenarios such as peripheral and central.

Any application layer profile, such as GAP, GATT, GATT characteristics, and services, can be executed on top of this layer.

> Note: only Nimble is supported, the support for Bluedroid will be added in later release version.

### GATT cache invalidation (Service Changed)

For NimBLE peripheral/both role, `ble_conn_mgr` can automatically trigger the
GATT Service Changed indication (`0x2A05`) when the configured GATT schema
version changes across boots.

- `BLE_CONN_MGR_GATT_CHANGED_AUTO` (default: disabled): enable auto trigger
  (requires NimBLE GATT Server: `CONFIG_BT_NIMBLE_GATT_SERVER`; when enabled,
  also enables `CONFIG_BT_NIMBLE_GATT_CACHING`).
- `BLE_CONN_MGR_GATT_SCHEMA_VERSION` (default: 1, range 1..999): increase this when your
  server GATT database layout changes.

When enabled, on host sync `ble_conn_mgr` compares the configured schema
version with a persisted NVS value and calls `ble_svc_gatt_changed(0x0001,
0xFFFF)` if changed. The new schema version is written to NVS only after a
Service Changed **indication** is acknowledged on-air (`BLE_GAP_EVENT_NOTIFY_TX`
with `BLE_HS_EDONE`), not when the stack merely queues the indication, so a
reset before any delivery does not permanently suppress a later re-trigger.

### Out-of-band (OOB) pairing (e.g. NFC)

Pairing can use a **side channel** (NFC tag, QR payload, etc.) instead of or in addition to on-air passkey or numeric comparison. NimBLE exposes this through SMP; `ble_conn_mgr` forwards it as `ESP_BLE_CONN_EVENT_PASSKEY_ACTION` with action `ESP_BLE_CONN_SM_ACT_OOB` (legacy LE OOB) or `ESP_BLE_CONN_SM_ACT_OOB_SC` (LE Secure Connections OOB). API details are in `include/esp_ble_conn_mgr.h`.

**Application layer events (`esp_event`)**

OOB uses the **same** events as other pairing methods; there is no separate “OOB-only” event.

1. **Register** the handler on `BLE_CONN_MGR_EVENTS` (for example `esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, …)` or your project’s equivalent on the default loop).

2. **`ESP_BLE_CONN_EVENT_PASSKEY_ACTION`** — whenever SMP needs user input (passkey, numeric comparison, **or OOB**), this event is posted. Read `event_data.passkey_action`:
   - **`conn_handle`**: which link is pairing (same as for INPUT / NUMCMP / DISP).
   - **`action`**: discriminant; for OOB you will see:
     - **`ESP_BLE_CONN_SM_ACT_OOB_SC`**: call `esp_ble_conn_sc_oob_reply()` with local and/or remote `(r, c)` as documented in `esp_ble_conn_mgr.h`.
     - **`ESP_BLE_CONN_SM_ACT_OOB`**: call `esp_ble_conn_oob_legacy_tk_reply()` with the 16-byte TK.
   - Other values (`INPUT`, `DISP`, `NUMCMP`) behave as for classic pairing; use `esp_ble_conn_passkey_reply()` / `esp_ble_conn_numcmp_reply()` / display flow as today.
   - **`passkey`**: used for INPUT / DISP / NUMCMP; **not** used when `action` is `OOB` or `OOB_SC`.

3. **`ESP_BLE_CONN_EVENT_ENC_CHANGE`** — after pairing completes and the link encrypts (or on failure), you still get this event as for non-OOB security; use it to confirm success and `authenticated` / bonding outcome.

**Menuconfig (when `BLE_CONN_MGR_SM` is enabled)**

- **`BLE_CONN_MGR_SM_CAP`**: set **bit3** for Secure Connections (`sm_sc`) if you use LE SC OOB. Set **bit0** so the stack advertises that local OOB data is available (`sm_oob_data_flag`), or call `esp_ble_conn_sm_oob_flag_set(true)` before pairing so both sides agree on OOB. Enable bonding / MITM in the same bitmap if your product requires them.
- **`BLE_CONN_MGR_SM_IO_TYPE`**: choose an IO capability consistent with your UX (OOB is orthogonal to IO cap, but the peer’s pairing method must be supported).

**LE Secure Connections OOB — typical flow**

1. After `esp_ble_conn_init()` / `esp_ble_conn_start()`, ensure SC and OOB capability match the peer (Kconfig or `esp_ble_conn_sm_oob_flag_set()`).
2. **Peripheral (often the device that owns the NFC tag)** calls `esp_ble_conn_sc_oob_data_generate()` to obtain local `r` and `c`. Store them on the tag (or otherwise deliver them to the central out-of-band).
3. Establish the BLE connection as usual (advertising / scanning).
4. Call `esp_ble_conn_security_initiate(conn_handle)` (or your app’s equivalent trigger) to start pairing.
5. On **`ESP_BLE_CONN_EVENT_PASSKEY_ACTION`** with **`ESP_BLE_CONN_SM_ACT_OOB_SC`**, call **`esp_ble_conn_sc_oob_reply()`**: pass **local** `(r,c)` from step 2, and **remote** `(r,c)` read from the side channel if this device is the central (or the inverse pattern for the peripheral, depending on who is SMP initiator and what data each side already has). Use `NULL` only for the side you truly do not have; see API comments for initiator/responder cases.
6. Wait for **`ESP_BLE_CONN_EVENT_ENC_CHANGE`** to confirm encryption (and bonding, if enabled).

**Legacy LE OOB (16-byte TK)**

If the stack requests **`ESP_BLE_CONN_SM_ACT_OOB`**, supply the TK with `esp_ble_conn_oob_legacy_tk_reply()` when you receive the passkey action event.

### Adding the component to your project

To add the `ble_conn_mgr` to your project's dependencies, please use the command `idf.py add-dependency`. During the `CMake` step, the component will be downloaded automatically.

```
idf.py add-dependency "espressif/ble_conn_mgr=*"
```

### Examples

To create a project using the example template, please use the command `idf.py create-project-from-example`.

* BLE SPP Central
```
idf.py create-project-from-example "espressif/ble_conn_mgr=*:spp_client"
```

* BLE SPP Peripheral
```
idf.py create-project-from-example "espressif/ble_conn_mgr=*:spp_server"
```

The example will be downloaded into the current folder. You can navigate to it for building and flashing.

> You can use this command to download other examples as well. Alternatively, you can download examples from the esp-iot-solution repository:
1. [ble_periodic_adv](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_conn_mgr/ble_periodic_adv)
2. [ble_periodic_sync](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_conn_mgr/ble_periodic_sync)
3. [ble_spp](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_conn_mgr/ble_spp)

### Q&A

Q1. I encountered the following problems when using the package manager

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

A1. This is because an older version package manager was used, please run `pip install -U idf-component-manager` in ESP-IDF environment to update.
