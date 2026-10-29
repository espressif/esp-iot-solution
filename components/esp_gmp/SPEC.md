# ESP-GMP Specification

This document describes the ESP General Management Protocol (ESP-GMP) as
implemented by `components/esp_gmp`.

ESP-GMP is a transport-agnostic management protocol for ESP-IDF. It defines a
small binary message header, request/response pairing, standard command groups,
and the OTA upload payloads used by the examples. ESP-GMP is not wire-compatible
with Zephyr SMP or MCUmgr.

## 1. Scope

ESP-GMP defines:

- A fixed 10-byte GMP frame header.
- Request and response opcodes.
- A 16-bit sequence number used to pair responses with requests.
- Standard groups for OS capability query, OTA upload, and file transfer.
- A transport binding model based on `esp_gmp_transport_t`.

ESP-GMP does not define:

- A general payload serialization format such as CBOR or JSON.
- End-to-end encryption or authentication.
- BLE ATT fragmentation. Large GMP frames are carried by the registered
  transport, for example `gatt_session`/Flux in the BLE OTA examples.

## 2. Byte Order

All multi-byte integer fields on the wire are big-endian.

## 3. GMP Frame

Every GMP message is one complete frame:

```text
Offset  Size  Field
0       1     ver_op
1       1     group_id
2       2     sequence
4       1     reserved0
5       1     command_id
6       1     flags
7       1     status_rsv
8       2     packet_length
10      N     payload
```

Frame length must be exactly `10 + packet_length`.

### 3.1 `ver_op`

`ver_op` packs version and opcode:

```text
bits 7..4: version
bits 3..0: opcode
```

Current version is `0x1`.

### 3.2 `sequence`

`sequence` is a 16-bit value chosen by the requester. A response must reuse the
same sequence number as the request it answers. Multiple requests may be in
flight if the application can match responses by sequence.

### 3.3 `reserved0`

`reserved0` must be `0x00` in v1. Receivers should reject non-zero values with
`NOT_SUPPORTED` when the frame is otherwise parseable.

### 3.4 `status_rsv`

For requests, `status_rsv` must be `0x00`.

For responses, `status_rsv` contains the response status code.

### 3.5 `packet_length`

`packet_length` is the byte length of `payload`, not including the 10-byte GMP
header. The field is 16-bit, so the protocol maximum payload length is 65535
bytes. Implementations may apply a smaller policy or transport limit.

## 4. Opcodes

| Value | Symbol | Direction | Meaning |
|---:|---|---|---|
| `0x00` | `READ_REQ` | requester to responder | Read/query request |
| `0x01` | `READ_RSP` | responder to requester | Response to `READ_REQ` |
| `0x02` | `WRITE_REQ` | requester to responder | Write/action request |
| `0x03` | `WRITE_RSP` | responder to requester | Response to `WRITE_REQ` |
| `0x04`-`0x0F` | Reserved | - | Not supported in v1 |

## 5. Groups and Commands

### 5.1 Group IDs

| Value | Symbol | Meaning |
|---:|---|---|
| `0x00` | `GRP_OS` | OS and capability commands |
| `0x01` | `GRP_OTA` | OTA upload commands |
| `0x08` | `GRP_FILE_TRANSFER` | File transfer commands |
| `0x02`-`0x07`, `0x09`-`0xFF` | Reserved | Not defined by v1 |

### 5.2 `GRP_OS` Commands

| Value | Symbol | Opcode | Meaning |
|---:|---|---|---|
| `0x01` | `OS_CAP_QUERY` | `READ_REQ` / `READ_RSP` | Query receiver capabilities |

### 5.3 `GRP_OTA` Commands

| Value | Symbol | Opcode | Meaning |
|---:|---|---|---|
| `0x01` | `OTA_UPLOAD_DATA` | `WRITE_REQ` / `WRITE_RSP` | Upload one firmware data chunk |
| `0x02` | `OTA_UPLOAD_CONTROL` | `WRITE_REQ` / `WRITE_RSP` | Start, finish, abort, or erase upload session |
| `0x03` | `OTA_UPLOAD_QUERY` | `READ_REQ` / `READ_RSP` | Query OTA upload state |

### 5.4 `GRP_FILE_TRANSFER` Commands

File transfer uses `group_id = GRP_FILE_TRANSFER` (`0x08`). Command IDs and
payload layouts for `TRANSFER_META`, `DATA_BLOCK`, `FINAL_CONFIRM`, and `ABORT`
are defined in
`profiles/file_transfer/include/esp_gmp_ft_proto.h`. That header (and the golden
frames in `test_ft`) is the frozen wire source of truth — this SPEC does not
redefine those bytes.

Responses must use the same `group_id`, `command_id`, and `sequence` as the
request they answer.

## 6. Flags

| Mask | Symbol | Meaning |
|---:|---|---|
| `0x01` | `CRC16_TAIL` | Payload ends with a 2-byte CRC16-CCITT-FALSE over the preceding payload bytes |
| `0x02` | `ABORT` | Reserved for transaction abort semantics |
| `0x04` | `FRAGMENTED` | Reserved; not supported by the current implementation |
| `0xF8` | Reserved | Must be zero |

When `CRC16_TAIL` is set, `packet_length` includes the trailing 2-byte CRC. The
CRC algorithm is CRC16-CCITT-FALSE: polynomial `0x1021`, initial value `0xFFFF`,
no input or output reflection.

The current `esp_gmp` implementation rejects incoming requests with
`FRAGMENTED` set. GMP-level fragmentation is not implemented.

## 7. Status Codes

| Value | Symbol | Meaning |
|---:|---|---|
| `0x00` | `OK` | Request completed successfully |
| `0x01` | `UNKNOWN_COMMAND` | Group or command is not handled |
| `0x02` | `BAD_STATE` | Command is not valid in the current state |
| `0x03` | `BAD_LENGTH` | Frame or payload length is invalid |
| `0x05` | `CRC_ERROR` | CRC16 validation failed |
| `0x06` | `BUSY` | Receiver is busy; requester may retry |
| `0x07` | `NOT_SUPPORTED` | Version, opcode, flags, or reserved fields are not supported |
| `0x08` | `NO_SESSION` | No matching OTA/session state exists |
| `0x09` | `SESSION_IN_USE` | Another session is active |
| `0xFF` | `INTERNAL` | Unclassified internal error |

## 8. Validation Rules

A v1 receiver should reject a request when:

- `version != 0x1`.
- `opcode` is not `READ_REQ` or `WRITE_REQ`.
- `reserved0 != 0`.
- `status_rsv != 0`.
- `FRAGMENTED` or any reserved flag bit is set.
- Frame length is not exactly `10 + packet_length`.
- `packet_length` exceeds the link's effective maximum payload.
- `CRC16_TAIL` is set and CRC validation fails.

If the frame is parseable and the opcode is a request, the receiver may return
the matching response opcode with a non-OK status. Malformed frames that cannot
be parsed may be dropped silently.

## 9. Transport Binding

ESP-GMP core is transport-agnostic. A link is registered with:

```c
esp_err_t esp_gmp_link_register(esp_gmp_link_t link,
                                const esp_gmp_transport_t *ops,
                                void *transport_ctx);
```

The transport provides:

```c
typedef struct {
    esp_err_t (*send)(void *ctx, const uint8_t *data, size_t len);
    size_t (*max_payload)(void *ctx);
    bool (*can_send)(void *ctx);
} esp_gmp_transport_t;
```

`send()` receives one complete GMP frame. When `send()` returns `ESP_OK`, the
transport has accepted the frame pointer and GMP keeps it in the in-flight set.
The transport must later notify GMP when the frame buffer is no longer in use,
even if the transport copied the bytes synchronously and completed immediately:

```c
void esp_gmp_transport_tx_done(esp_gmp_link_t link, const uint8_t *data);
```

`esp_gmp_transport_tx_done()` is a transport-completion hook. It releases GMP's
TX frame buffer and drains the per-link TX queue. Application-level success or
failure is reported by GMP responses received through `esp_gmp_input()` and the
registered packet callback.

## 10. BLE GATT Binding

BLE products should expose ESP-GMP as a dedicated primary service.

| Logical name | UUID | Properties | Direction |
|---|---|---|---|
| GMP Service | `a0eeffc0-504f-4b53-b62f-0a0000000001` | Primary service | - |
| GMP RX | `a0eeffc0-504f-4b53-b62f-0a0000000002` | Write or Write Without Response | Central to Peripheral |
| GMP TX | `a0eeffc0-504f-4b53-b62f-0a0000000003` | Notify | Peripheral to Central |

The current BLE OTA examples carry complete GMP frames over `gatt_session`,
which delegates reliable fragmentation and reassembly to Flux. In this binding:

- A received complete `gatt_session` message is passed to
  `esp_gmp_input(link, data, len)`.
- `esp_gmp_transport_t.send()` calls `gatt_session_fragment_send()`.
- `gatt_session` `session_complete_cb` calls
  `esp_gmp_transport_tx_done(link, data)`.

## 11. OS Capability Query

`OS_CAP_QUERY` is sent as:

- `op = READ_REQ`
- `group_id = GRP_OS`
- `command_id = OS_CAP_QUERY`
- empty payload

The response uses `READ_RSP` and `status = OK`. The current example returns a
12-byte capability payload:

```text
Offset  Size  Field
0       1     schema_major
1       1     schema_minor
2       1     role
3       1     transport
4       2     max_control_payload
6       4     max_gmp_payload
10      1     ota_supported
11      1     reserved
```

The BLE OTA client currently consumes `max_gmp_payload` at offset 6 and derives
its OTA data chunk size from it. Other fields are informational in the current
example.

## 12. OTA Upload Protocol

OTA commands use `group_id = GRP_OTA`.

### 12.1 Upload Control Request

`OTA_UPLOAD_CONTROL` requests use `WRITE_REQ`.

Payload for `START`, `ABORT`, and `ERASE` is 8 bytes:

```text
Offset  Size  Field
0       1     action
1       1     session_id
2       2     flags
4       4     image_size
```

Payload for `FINISH` is 40 bytes:

```text
Offset  Size  Field
0       1     action
1       1     session_id
2       2     flags
4       4     image_size
8       32    image_sha256
```

`START` always begins the transfer at offset 0 of the image; there is no
partial/resume upload support. `image_size` is the full firmware size and the
first `OTA_UPLOAD_DATA` chunk must use `image_offset = 0` (see 12.3).

Actions:

| Value | Symbol | Meaning |
|---:|---|---|
| `0` | `START` | Open an OTA upload session |
| `1` | `FINISH` | Complete upload and verify SHA256 |
| `2` | `ABORT` | Abort current upload session |
| `3` | `ERASE` | Reset OTA session state |

Control flags:

| Mask | Symbol | Meaning |
|---:|---|---|
| `0x0001` | `SHA256` | `FINISH` payload includes SHA256 digest |

`FINISH` must set `SHA256` and include the 32-byte digest. `ABORT` and `ERASE`
must use zero `image_size` and zero flags.

### 12.2 Upload Control Response

The `START` response payload is 4 bytes:

```text
Offset  Size  Field
0       1     session_id
1       2     chunk_hint
3       1     reserved
```

`chunk_hint` is the receiver's suggested firmware data bytes per
`OTA_UPLOAD_DATA` message. The client should also honor its own maximum GMP
payload and alignment constraints.

Other control responses may use an empty payload with status in the GMP header.

### 12.3 Upload Data Request

`OTA_UPLOAD_DATA` requests use `WRITE_REQ`.

Payload:

```text
Offset  Size  Field
0       1     session_id
1       1     flags
2       2     data_len
4       4     image_offset
8       N     firmware_data
```

`data_len` must be non-zero and `payload_length` must equal `8 + data_len`.
`image_offset` here is the running offset of this chunk within the image
being transferred; it must start at `0` for the first chunk after `START`
and increase contiguously (see 12.1).

Data flags:

| Mask | Symbol | Meaning |
|---:|---|---|
| `0x01` | `LAST_CHUNK` | This data chunk reaches the end of the image |

Each data request is answered by `WRITE_RSP` with the same sequence and
`command_id`. A successful DATA response means the receiver accepted the chunk
into the current upload session, and may still write it to flash asynchronously.
It is not a durable-flash confirmation. The final durable result is reported by
`FINISH` after the receiver drains queued data, verifies SHA256, completes
`esp_ota_end()`, and selects the boot partition. The current client allows two
data requests in flight and matches responses by sequence.

### 12.4 Upload Query

`OTA_UPLOAD_QUERY` requests use `READ_REQ`; the request payload may be empty.

Response payload is 10 bytes:

```text
Offset  Size  Field
0       1     session_state
1       1     active_session_id
2       4     bytes_received
6       4     bytes_expected
```

Session states:

| Value | Symbol |
|---:|---|
| `0` | `IDLE` |
| `1` | `OPEN` |
| `2` | `CLOSING` |

`bytes_received` reports image bytes accepted by the receiver for the current
session. It may be ahead of durable flash writes while asynchronous DATA handling
is draining.

## 13. Current Implementation Notes

- `components/esp_gmp/src/esp_gmp_frame.c` is the source of truth for the v1
  GMP frame layout.
- `components/esp_gmp/include/esp_gmp_types.h` defines public constants.
- `components/esp_gmp/profiles/ota/include/esp_gmp_ota_proto.h` defines OTA payload
  sizes and field meanings.
- GMP core owns TX frame buffers allocated by `esp_gmp_send()`. An asynchronous
  transport must call `esp_gmp_transport_tx_done()` with the same data pointer
  it received in `send()`. Synchronous-copy transports must also arrange this
  notification after `send()` has returned.
- GMP core does not parse OTA payloads. OTA semantics live in
  `components/esp_gmp/profiles/ota` (SHA-256 helpers in `common/`).
- OTA SHA256 verifies transfer integrity only. ESP-GMP does not define firmware
  authenticity, image signing, BLE authorization, or secure boot policy.
