## BLE MIDI Central Example (MIDI In / host)

This example runs as a **BLE Central** and acts as **MIDI In** (receives MIDI stream from a peripheral).

It pairs with `ble_midi_peripheral`:

1. Scans for the BLE-MIDI service UUID (`0x07` AD type) and/or an optional peer name (menuconfig).
2. Connects and performs GATT discovery.
3. Enables notifications on the MIDI I/O characteristic.
4. Prints raw BLE-MIDI Event Packets and parsed MIDI messages (same callbacks as the peripheral).

### Build and flash

1. `idf.py set-target <chip>`
2. `idf.py build`
3. `idf.py -p <PORT> flash monitor`

Flash `ble_midi_peripheral` on another board (or phone as central is not covered here), then run this example. After connection, the peripheral’s scale demo should appear in the central’s log.

### Configuration

- **EXAMPLE_LOCAL_NAME**: Local GAP name (default `BLE_MIDI_Central`).
- **EXAMPLE_PEER_NAME**: Optional advertiser name (default empty = UUID only). Use `BLE_MIDI` to match the default peripheral name.

### Notes

- Uses `esp_ble_midi_svc_init()` so the connection manager registers the same BLE-MIDI GATT layout for discovery and notification routing (no MIDI GATT server is required on the central for this flow).
- MTU exchange requests 185 bytes after connection, consistent with the peripheral example.
- **Heap ownership:** For `ESP_BLE_CONN_EVENT_DATA_RECEIVE`, `esp_event` holds a copy of the posted payload; cast `event_data` to `esp_ble_conn_data_t *` and **do not** `free(event_data->data)` — it points into that copy. Copy the bytes before returning if another task needs them.
