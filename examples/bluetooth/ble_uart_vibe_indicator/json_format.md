# Signal Light Control Protocol (v1)

Newline-delimited JSON over ESP-BLE-UART. Each request must carry a non-empty `id`; the device always replies on the TX characteristic.

## Envelope

| Direction | Format |
|-----------|--------|
| Host → device | `{"v":1,"id":"<req-id>","op":"command","data":{...}}` |
| Device → host (success) | `{"v":1,"id":"<req-id>","ok":true,"data":{...}}` |
| Device → host (failure) | `{"v":1,"id":"<req-id>","ok":false,"error":"<code>","data":{...}}` |

| Field | Description |
|-------|-------------|
| `v` | Protocol version. Must be `1`. |
| `id` | Non-empty request id. Empty or missing → `id_not_specified`. |
| `op` | Must be `"command"`. |
| `data` | Command payload object. |

Firmware maps `light_id` `0` / `1` / `2` to red / yellow / green GPIO within each indicator group.

### `light_action`

| Value | Meaning |
|-------|---------|
| `0` | Off |
| `1` | On |
| `2` | Slow blink (~1 Hz, `CONFIG_VIBE_INDICATOR_SLOW_BLINK_HALF_MS`) |
| `3` | Fast blink (~3 Hz, `CONFIG_VIBE_INDICATOR_FAST_BLINK_HALF_MS`) |

---

## Query indicator count

**Request**

```json
{"v":1,"id":"req-001","op":"command","data":{"cmd":"query","type":"indicator_count"}}
```

**Success `data`**

```json
{"count":3}
```

`count` comes from `CONFIG_VIBE_INDICATOR_CHANNEL_COUNT`.

---

## Control lamps

**Request** — `payload` carries one or more lamp updates:

```json
{"v":1,"id":"req-002","op":"command","data":{"cmd":"control","payload":[{"indicator_id":0,"light_id":0,"light_action":0},{"indicator_id":1,"light_id":2,"light_action":2}]}}
```

| Field | Type | Description |
|-------|------|-------------|
| `cmd` | string | `"control"` |
| `payload` | array | Non-empty list of lamp commands |
| `payload[].indicator_id` | int | Group id, `0` .. `count - 1` |
| `payload[].light_id` | int | `0` / `1` / `2` |
| `payload[].light_action` | int | `0` .. `3` |

All items are validated before any GPIO change. On success the device echoes the request `payload`:

```json
{"v":1,"id":"req-002","ok":true,"data":{"payload":[{"indicator_id":0,"light_id":0,"light_action":0},{"indicator_id":1,"light_id":2,"light_action":2}]}}
```

---

## Error codes

### Protocol

| `error` | When |
|---------|------|
| `bad_json` | Line is not valid JSON |
| `bad_request` | Missing/invalid envelope fields |
| `id_not_specified` | `id` empty or missing |
| `unknown_op` | `op` is not `command` |

### Application

| `error` | When |
|---------|------|
| `unsupported_command` | Unknown `data.cmd` or query `type` |
| `invalid_parameter` | Bad `payload` or out-of-range values |

Application errors may include `data.message`:

```json
{"v":1,"id":"req-002","ok":false,"error":"invalid_parameter","data":{"message":"light_id out of range, expected 0-2"}}
```
