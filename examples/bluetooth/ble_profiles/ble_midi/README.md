## BLE MIDI Example

This example demonstrates a minimal MIDI over BLE GATT server using the BLE-MIDI profile.

It:
- Initializes BLE connection manager
- Adds the BLE-MIDI service and IO characteristic
- Logs incoming BLE-MIDI packets
- Sends a sample scale periodically after connection

### Build and flash
1. `idf.py set-target <chip>`
2. `idf.py build`
3. `idf.py -p <PORT> flash monitor`

### Developer Notes
- Advertising: uses Extended Advertising (if supported) to include 128-bit BLE-MIDI Service UUID (`0x07` AD type).
- Connection: prefer low connection interval (7.5–15 ms) and MTU 185 for lower latency and higher throughput.
- BLE‑MIDI Event Packet:
  - Packet header: `0x80 | (ts >> 7)`; per-message timestamp byte: `0x80 | (ts & 0x7F)`, where `ts` is 13‑bit ms tick.
  - Multi-event: multiple messages can be aggregated in one notification; each message has its own timestamp byte.
  - SysEx: split across packets based on MTU; reassembled until `0xF7`.
- Flow control: notifications are paced by stack; avoid flooding and batch multiple events when possible.
  - `esp_ble_conn_notify()` internally blocks on a semaphore until the stack accepts the notification; this prevents overruns.
  - For higher throughput, prefer batching with `esp_ble_midi_send_multi()`.

### Quick start (API usage)
Minimal steps in your app:

```c
// 1) Initialize BLE connection manager and MIDI service
ESP_ERROR_CHECK(esp_ble_conn_init(&config));
ESP_ERROR_CHECK(esp_ble_conn_set_mtu(185));
ESP_ERROR_CHECK(esp_ble_midi_svc_init());
ESP_ERROR_CHECK(esp_ble_midi_profile_init());  // Initialize MIDI profile (required before using other MIDI APIs)

// 2) (Optional) Register callbacks
ESP_ERROR_CHECK(esp_ble_midi_register_rx_cb(midi_rx_cb));      // raw BEP payload
ESP_ERROR_CHECK(esp_ble_midi_register_event_cb(midi_evt_cb));  // parsed MIDI messages

// 3) Start advertising
ESP_ERROR_CHECK(esp_ble_conn_start());
```

Obtain a 13‑bit timestamp for BLE‑MIDI:

```c
uint16_t ts = esp_ble_midi_get_timestamp_ms(); // 0..0x1FFF, 1 ms tick
```

#### Send helpers
Typical messages (status + data) are wrapped for you:

```c
uint16_t ts = esp_ble_midi_get_timestamp_ms();
// Note On, channel 1 (0), note C4 (60), velocity 100
ESP_ERROR_CHECK(esp_ble_midi_send_note_on(0, 60, 100, ts));

ts = esp_ble_midi_get_timestamp_ms();
// Note Off, channel 1, note C4, velocity 64
ESP_ERROR_CHECK(esp_ble_midi_send_note_off(0, 60, 64, ts));

ts = esp_ble_midi_get_timestamp_ms();
// Control Change (mod wheel), channel 1, CC#1=64
ESP_ERROR_CHECK(esp_ble_midi_send_cc(0, 1, 64, ts));

ts = esp_ble_midi_get_timestamp_ms();
// Pitch Bend (center=8192)
ESP_ERROR_CHECK(esp_ble_midi_send_pitch_bend(0, 9000, ts));
```

Send arbitrary MIDI bytes (1–3 bytes typical):

```c
uint8_t prog_change[] = { 0xC0 | 0, 40 }; // Program Change, channel 1, program 40
ESP_ERROR_CHECK(esp_ble_midi_send_raw_midi(prog_change, sizeof(prog_change),
                                           esp_ble_midi_get_timestamp_ms()));
```

#### Aggregate multiple messages
Batch messages into one notification to reduce overhead:

```c
const uint8_t note_on[]  = { 0x90 | 0, 60, 100 };
const uint8_t note_off[] = { 0x80 | 0, 60,  64  };
const uint8_t *msgs[] = { note_on, note_off };
uint8_t lens[]       = { 3,       3        };
uint16_t tss[]       = { esp_ble_midi_get_timestamp_ms(),
                         esp_ble_midi_get_timestamp_ms() };

/* Simple usage: pass NULL for first_unsent_index if not needed */
ESP_ERROR_CHECK(esp_ble_midi_send_multi(msgs, lens, tss, 2, NULL));

/* With error handling: track which messages were sent on failure */
size_t first_unsent = 0;
esp_err_t ret = esp_ble_midi_send_multi(msgs, lens, tss, 2, &first_unsent);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send all messages. First unsent index: %zu", first_unsent);
    /* Messages 0 to (first_unsent - 1) were successfully sent */
}
```

#### SysEx (automatic fragmentation)
Provide a complete 0xF0..0xF7 buffer; it will be split across packets based on MTU:

```c
const uint8_t sysex_msg[] = {
    0xF0, 0x7D, 0x01, 0x02, 0x03, /* vendor specific ... */ 0xF7
};
ESP_ERROR_CHECK(esp_ble_midi_send_sysex(sysex_msg, sizeof(sysex_msg),
                                        esp_ble_midi_get_timestamp_ms()));
```

#### Receive callbacks
Raw BEP payload (for diagnostics), and parsed MIDI events (recommended):

```c
static void midi_rx_cb(const uint8_t *data, uint16_t len)
{
    // Raw BLE-MIDI Event Packet payload (hex dump, etc.)
}

static void midi_evt_cb(uint16_t ts_ms, esp_ble_midi_event_type_t event_type, const uint8_t *msg, uint16_t msg_len)
{
    if (event_type == ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW) {
        // SysEx buffer overflow detected (msg is NULL, msg_len is 0)
        // The internal state has been reset
        return;
    }
    // One complete MIDI message:
    // - Channel Voice/System Common: status+data
    // - SysEx: full 0xF0..0xF7 sequence (reassembled)
    // - Real-time (0xF8..0xFF): 1-byte events
}
```

##### Aggregation helper
Use `esp_ble_midi_send_multi()` to pack multiple MIDI messages into one BLE notification:

```c
const uint8_t *msgs[] = { note_on, cc1, pitchbend };
uint8_t lens[] = { 3, 3, 3 };
uint16_t ts[] = { esp_ble_midi_get_timestamp_ms(), esp_ble_midi_get_timestamp_ms(), esp_ble_midi_get_timestamp_ms() };
/* Pass NULL for first_unsent_index if error handling is not needed */
ESP_ERROR_CHECK(esp_ble_midi_send_multi(msgs, lens, ts, 3, NULL));
```

#### Connection Parameters
- This example sets preferred MTU to 185 via `esp_ble_conn_set_mtu(185)`.
- For low latency, set connection interval to 7.5–15 ms and latency 0–1.
  - **Central**: The central (host) has final authority over connection parameters. Some hosts (e.g., Windows BLE MIDI stacks) may ignore peripheral-requested updates. The peripheral can request parameter changes, but the host may refuse or adjust them based on its own policies.
  - **Peripherals** (default here): Peripherals can request updates using `esp_ble_conn_update_params(conn_handle, {...})`. This example uses it to request 7.5 ms / latency 0 / timeout 4 s on connect. However, the central usually decides the final values. If you control the central, set the connection interval there for best results.

#### Interoperability Notes
- iOS/macOS: Pairs with CoreMIDI BLE; ensure connection interval ≤ 15 ms for responsive play.
- Windows: Use “Bluetooth MIDI” in supported DAWs; some adapters require pairing before listing.
- Android: Requires Android 6.0+ and app support for BLE-MIDI; latency varies by device.
- DAWs and synth apps may throttle notifications; prefer batching short messages where possible.
