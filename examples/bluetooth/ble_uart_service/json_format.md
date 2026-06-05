# JSONL Envelope Protocol (v1)

Both directions are newline-delimited JSON (one JSON object per line).

---

## PC/daemon → device: permission request

Sent via daemon `POST /request` when OpenCode publishes `permission.asked`.
The daemon envelope uses `op: "permission.request"` and a **non-empty** bridge request id.

```json
{"v":1,"id":"<bridge-request-id>","op":"permission.request","data":{"v":1,"kind":"permission.request","event_id":"evt_...","session_id":"ses_...","permission_id":"perm_...","requires_reply":true,"payload":{"id":"perm_...","sessionID":"ses_...","type":"bash","title":"Run command","metadata":{"command":"git status"}}}}
```

Fields used by the device:
| Path | Used for |
|------|----------|
| `id` | Stored and echoed in the reply |
| `data.kind` | Must equal `"permission.request"` |
| `data.payload.type` | Displayed (e.g. `"bash"`) |
| `data.payload.title` | Displayed (e.g. `"Run command"`) |
| `data.payload.metadata` | Compact metadata; prefers `command`, `path`, then `url`, with first string child as fallback |

### Hardware mapping (ESP-VoCat touch key)

The touch key uses a capacitive touch pad (IO7 on v1.0, IO6 or IO7 on v1.2):

| Action | Decision sent |
|--------|---------------|
| Single click | `"once"` |
| Long press (first threshold) | `"reject"` |
| 30s timeout | `"reject"` (not `"timeout"`) |

`"always"` is not exposed because this firmware exposes only one touch key.

---

## Device → PC: permission reply

The device replies with the **same** bridge request id:

```json
{"v":1,"id":"<bridge-request-id>","ok":true,"data":{"decision":"once","message":"Approved from BLE device"}}
```

```json
{"v":1,"id":"<bridge-request-id>","ok":true,"data":{"decision":"reject","message":"Rejected from BLE device"}}
```

On timeout (no key press within 30s):

```json
{"v":1,"id":"<bridge-request-id>","ok":true,"data":{"decision":"reject","message":"Timed out"}}
```

---

## PC/daemon → device: session status (fire-and-forget)

Sent via daemon `POST /notify` with **empty** bridge request id. The device
**must not reply**.

```json
{"v":1,"id":"","op":"session.status","data":{"v":1,"kind":"session.status","event_id":"evt_...","session_id":"ses_...","requires_reply":false,"payload":{"type":"busy"}}}
```

The device updates its display:
- `"busy"` → show "Busy..." tip
- `"idle"` → clear tip
- `"retry"` → show "Retry..." tip

---

## Error envelopes

On **invalid JSON** (parse failure):

```json
{"v":1,"id":"","ok":false,"error":"bad_json"}
```

On a **malformed or unknown request** with a non-empty `id`:

```json
{"v":1,"id":"<request-id>","ok":false,"error":"bad_request"}
```

```json
{"v":1,"id":"<request-id>","ok":false,"error":"unknown_op"}
```

---

## Legacy maintenance commands

Simple `{"cmd":"..."}` messages are still accepted for local use and must not
interfere with v1 envelope handling:

| Command | Description |
|---------|-------------|
| `{"cmd":"status"}` | Returns ack with device status (heap, uptime, BLE connected/subscribed; legacy `ready` mirrors `connected`) |
| `{"cmd":"name","name":"..."}` | Persists BLE device name in NVS; reboot to apply |
| `{"cmd":"unpair"}` | Clears all bonded peers |

---

## Previous (replaced) protocol

This file previously documented the `hook_event_name`/`hookSpecificOutput`
protocol. That format is **replaced** by the v1 envelope protocol above.
