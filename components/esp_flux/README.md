# Component: ESP Flux

A transport-agnostic reliable data transfer protocol for embedded systems, with a built-in BLE (NimBLE GATT) adapter.

## Overview

The `esp_flux` component consists of two layers:

1. **Flux Transport** (`flux_transport`) - The protocol engine: fragmentation/reassembly, selective acknowledgment (SACK), sliding-window flow control, and retransmission. It only depends on a small `flux_transport_ops_t` interface (`send` + `get_mtu`), so it can run over any unreliable, MTU-bound transport (BLE GATT, Wi-Fi, UART, ...).
2. **GATT Session** (`gatt_session`) - A thin BLE (NimBLE) adapter that implements `flux_transport_ops_t` on top of GATT write/notify/indicate, and bridges GAP/GATT RX events back into the protocol engine.

Connection management (scanning, advertising, auto-reconnect) and GATT service discovery are **not** part of this component — see the `ble_session_manager` component used by the examples in `examples/esp_flux/`.

## Features

### Flux Transport (protocol engine)

- **Fragmentation & Reassembly**: Automatically splits data larger than MTU into fragments and reassembles them on the receiver side
- **Selective Acknowledgment (SACK)**: Bitmap-based acknowledgment for efficient loss detection and retransmission
- **Sliding Window Flow Control**: Configurable window size for concurrent in-flight fragments
- **Automatic Retransmission**: Per-fragment retry with configurable timeout and retry count
- **Multi-stream**: Up to `FLUX_MAX_CONCURRENT_SENDS` (2) concurrent send streams and `FLUX_MAX_CONCURRENT_RECVS` (2) concurrent receive streams per session, demultiplexed via a wire-level `stream_id`
- **Progress & Error Callbacks**: Reports transfer progress and terminal errors (timeout, max retries exceeded)
- **Callback-based Receive Delivery**: complete reassembled data is delivered via `data_received_cb`

### GATT Session (BLE adapter)

- **Role Support**: Works as both Central (Master, GATT client writes) and Peripheral (Slave, GATT indications/notifications)
- **MTU Adaptation**: Recomputes max per-packet payload after MTU negotiation via `gatt_session_set_mtu()`
- **MBUF Backpressure**: Detects NimBLE mbuf pool pressure and backs off before retrying sends
- **Multi-connection**: Sessions are tracked in a linked list, one `gatt_session_t` per `conn_handle`

## Packet Format

All packets share a 1-byte control field as their first byte:

```
bit:  7 6 5 4 3 2 1 0
      +---------+---+-----+
      | proto   |sid| type|
      | ver (2) |(3)|  (3)|
      +---------+---+-----+
```

- **type** (bits 0-2): packet type, see below
- **stream_id** (bits 3-5): identifies which concurrent stream this packet belongs to (demultiplexing)
- **protocol_ver** (bits 6-7): protocol version

### Packet Types

- `0x1` - **SINGLE**: Data that fits in one packet (no fragmentation needed)
- `0x2` - **FRAGMENT**: One fragment of a fragmented transfer
- `0x3` - **SACK**: Selective acknowledgment (optionally carries a bitmap)
- `0x4` - **START**: First packet of a fragmented transfer, carries transfer metadata

### Packet Structures

#### SINGLE / FRAGMENT Packet

```
+---------+---------+---------+------------+
| Control | Seq Num | Size    | Data       |
| (1)     | (1)     | (2)     | (variable) |
+---------+---------+---------+------------+
```

- **Control** (1 byte): type (SINGLE=0x1 or FRAGMENT=0x2), stream_id, protocol_ver
- **Seq Num** (1 byte): fragment sequence number (`0` for a SINGLE packet)
- **Size** (2 bytes, little-endian): payload size
- **Total header size**: 4 bytes

#### START Packet

```
+---------+----------+---------+----------+---------+---------+---------+---------+------------+
| Control | Reserved | Seq Num | Window   | Window  | Size    | Total   | Total   | Data       |
| (1)     | (1)      | (1)     | Size (1) | Thresh% | (2)     | Frags   | Size    | (variable) |
|         |          |         |          | (1)     |         | (2)     | (4)     |            |
+---------+----------+---------+----------+---------+---------+---------+---------+------------+
```

- **Control** (1 byte): type = `0x4`, stream_id, protocol_ver
- **Reserved** (1 byte): reserved for future use
- **Seq Num** (1 byte): always `0` for the START packet
- **Window Size** (1 byte): sliding window size negotiated for this transfer
- **Window Threshold %** (1 byte): window-occupancy percentage that triggers an immediate SACK
- **Size** (2 bytes, little-endian): payload size carried in this START packet (first chunk of data)
- **Total Fragments** (2 bytes, little-endian): total fragment count for this transfer
- **Total Size** (4 bytes, little-endian): total data size for this transfer
- **Total header size**: 13 bytes

#### SACK Packet

```
+---------+---------+---------+------------------+
| Control | Ack Num | Size    | Bitmap           |
| (1)     | (1)     | (2)     | (0 or 16 bytes)  |
+---------+---------+---------+------------------+
```

- **Control** (1 byte): type = `0x3`, stream_id, protocol_ver
- **Ack Num** (1 byte): last continuously-received sequence number
- **Size** (2 bytes, little-endian): bitmap size in bytes (`0` or `FLUX_SACK_BITMAP_SIZE` = 16)
- **Bitmap** (0 or 16 bytes): one bit per fragment, `1` = lost, `0` = received; `Size == 0` means a plain cumulative ACK (acknowledges everything up to Ack Num)
- **Total header size**: 4 bytes (excluding bitmap)

### Packet Size Calculation

Over BLE GATT, the actual on-air size also includes the ATT header:

- ATT header: 3 bytes (opcode + attribute handle)
- Max SINGLE/FRAGMENT payload: `MTU - 3 (ATT) - 4 (header) = MTU - 7`
- Max START payload: `MTU - 3 (ATT) - 13 (header) = MTU - 16`

## Configuration

`esp_flux` has **no Kconfig options** — all protocol tunables are compile-time constants in `include/esp_flux_transport.h`:

| Constant | Default | Meaning |
|---|---|---|
| `FLUX_MAX_CONCURRENT_SENDS` | 2 | Max concurrent send streams per session |
| `FLUX_MAX_CONCURRENT_RECVS` | 2 | Max concurrent receive streams per session |
| `FLUX_MAX_FRAGMENTS` | 128 | Max fragments per transfer (bounded by the SACK bitmap width) |
| `FLUX_MAX_REASSEMBLY_SIZE` | 64 KB | Max size of one reassembled message |
| `FLUX_FRAGMENT_TIMEOUT_MS` | 10000 | Overall timeout for one fragmented transfer |
| `FLUX_ACK_MIN_INTERVAL_MS` | 100 | Minimum interval between two SACKs |
| `FLUX_MAX_RETRANSMIT_RETRIES` | 3 | Max retransmissions per fragment |
| `FLUX_RETRANSMIT_TIMEOUT_MS` | 500 | Per-fragment retransmission timeout |

To change these, edit `esp_flux_transport.h` (or override via component-level `EXTRA_COMPONENT_DIRS`/local copy) and rebuild.

## Add Component to Your Project

Please use the component manager command `add-dependency` to add `esp_flux` to your project's dependency, during the `CMake` step the component will be downloaded automatically:

```bash
idf.py add-dependency "espressif/esp_flux=*"
```

Or if you're using this component locally, add it to your project's `main/CMakeLists.txt`:

```cmake
idf_component_register(
    ...
    REQUIRES esp_flux
)
```

## Quick Start

This example assumes a GATT connection already exists (`conn_handle`, characteristic value handles, and negotiated MTU) — see `ble_session_manager` / `gatt_discovery` in `examples/esp_flux/` for connection setup and service discovery.

### 1. Create a GATT Session

```c
#include "gatt_session.h"

static void on_data_received(gatt_session_t *session, uint8_t stream_id,
                              const uint8_t *data, uint32_t size)
{
    // `data` ownership is transferred here; free it when done.
    free((void *)data);
}

static void on_send_complete(gatt_session_t *session, uint8_t stream_id,
                              esp_err_t status, const uint8_t *data, uint32_t size)
{
    // `data` is the buffer passed to gatt_session_fragment_send(); free it here.
}

ble_session_callbacks_t callbacks = {
    .data_received_cb = on_data_received,
    .session_complete_cb = on_send_complete,
    .progress_cb = NULL,
    .error_cb = NULL,
    .arg = NULL,
};

gatt_session_t *session = gatt_session_create(
    conn_handle, &callbacks, write_val_handle, read_val_handle,
    mtu_size, GATT_SESSION_ROLE_MASTER);
```

### 2. Send Data

`gatt_session_fragment_send()` automatically sends a `SINGLE` packet when the data fits in one MTU, or fragments it (`START` + `FRAGMENT` packets) otherwise:

```c
uint8_t *data = malloc(4096);
// ... fill data ...

// window_size = 6 concurrent fragments in flight, SACK at 80% window occupancy
esp_err_t err = gatt_session_fragment_send(session, data, 4096, 6, 80);
```

### 3. Receive Data

Received data is delivered via `data_received_cb` once a stream completes reassembly (see step 1).

### 4. Destroy the Session

```c
gatt_session_destroy(session);
```

## Performance Optimization

- **Window Size**: 4-6 gives a good balance between throughput and retransmission cost on typical BLE links
- **MTU Size**: Negotiate a larger MTU (up to 517 bytes) to reduce per-fragment overhead
- **Window Threshold %**: Lower values trigger SACKs sooner, trading a bit of overhead for faster loss recovery

## Examples

Complete examples can be found in `examples/esp_flux/`:
- `ble_spp_client` - BLE SPP client example
- `ble_spp_server` - BLE SPP server example

## API Reference

For detailed API documentation, please refer to the header files:
- [esp_flux_transport.h](include/esp_flux_transport.h) - Transport-agnostic protocol engine API
- [flux_gatt_session.h](include/flux_gatt_session.h) - BLE (NimBLE) GATT adapter API

## License

This component is licensed under the Apache License 2.0. See the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.
