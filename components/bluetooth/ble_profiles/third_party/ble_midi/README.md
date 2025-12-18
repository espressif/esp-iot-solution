## MIDI over BLE Profile Component

[![Component Registry](https://components.espressif.com/components/espressif/ble_midi/badge.svg)](https://components.espressif.com/components/espressif/ble_midi)

`ble_midi` is a Bluetooth® Low Energy (BLE) MIDI Profile component for ESP32‑series SoCs. It provides a simple C API to send and receive MIDI over BLE with low latency.

### Features

- **BLE‑MIDI GATT Service**
  - Service UUID: `03B80E5A‑EDE8‑4B33‑A751‑6CE34EC4C700`
  - Characteristic (MIDI I/O) UUID: `7772E5DB‑3868‑4112‑A1A9‑F2669D106BF3`
  - Properties: Write Without Response, Notify
- **MIDI I/O**
  - Send/receive BLE‑MIDI Event Packets (BEP)
  - 13‑bit timestamps (6‑bit high in BEP header + 7‑bit per message)
  - Multi‑message aggregation via `esp_ble_midi_send_multi()`
  - SysEx (0xF0…0xF7) automatic fragmentation and reassembly
  - Real‑time messages (0xF8…0xFF) interleaving during SysEx
- **High‑level APIs**
  - Raw BEP: `esp_ble_midi_send()`, `esp_ble_midi_send_raw_midi()`, `esp_ble_midi_register_rx_cb()`
  - Parsed events (built‑in decoder): `esp_ble_midi_register_event_cb()` (Note On/Off, Control Change, Pitch Bend, SysEx, etc.)
- **Service lifecycle**
  - `esp_ble_midi_profile_init()` / `esp_ble_midi_profile_deinit()` to initialize/deinitialize the MIDI profile (init must be called before using other MIDI APIs)
  - `esp_ble_midi_svc_init()` / `esp_ble_midi_svc_deinit()` to register/unregister the BLE‑MIDI GATT service

### Add to Your Project

Use the Component Manager to add the component (dependencies are fetched automatically during CMake configure):

```bash
idf.py add-dependency "espressif/ble_midi=*"
```

### Examples

Create the example from a template:

```bash
idf.py create-project-from-example "espressif/ble_profiles=*:ble_midi"
```

You can also browse the example sources at `examples/bluetooth/ble_profiles/ble_midi`.  
The example demonstrates:

- Registering & publishing the BLE‑MIDI GATT service (including the 128‑bit MIDI Service UUID in Extended Advertising)
- Sending/receiving BLE‑MIDI Event Packets (BEP) via notifications
- Aggregating multiple MIDI messages using `esp_ble_midi_send_multi()`

### Q&A

- **How do I make the device discoverable as a BLE‑MIDI peripheral?**  
  The example configures Extended Advertising and includes the BLE‑MIDI Service UUID so hosts (e.g., iOS/macOS) can discover it in their MIDI device lists.

- **How do I stop and clean up the service?**  
  Call `esp_ble_midi_svc_deinit()` to remove the GATT service, then use `esp_ble_conn_stop()` / `esp_ble_conn_deinit()` as needed to shut down the BLE stack cleanly.
