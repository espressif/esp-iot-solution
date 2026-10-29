# ESP-GMP

**English** | [中文](README_CN.md)

ESP-GMP is a management protocol core for ESP-IDF. Each GMP message is a fixed **10-byte header + payload** frame and is carried by an application-registered transport.

Protocol specification: [`SPEC.md`](SPEC.md).

## Architecture

```text
Application
        ↓
   profiles/{os,ota,file_transfer}   # optional, Kconfig-trimmed
        ↓ esp_gmp_register_handler / link events
   esp_gmp core
        ↓ esp_gmp_transport_t
   esp_flux / UART / socket / …
```

GMP does **not** implement `sub_seq` fragmentation or RX reassembly; those belong to the bound transport (for example Flux).

File Transfer profile API: [`esp_gmp_ft.h`](profiles/file_transfer/include/esp_gmp_ft.h).

## Features

- Transport-agnostic core: framing, CRC (optional), multi-link TX queue, group handler table, multi-subscriber link events
- Optional Flux/GATT adapters (`esp_gmp_flux_*`) for BLE stacks using `esp_flux`
- Optional **OTA** profile: device and/or host roles (`CONFIG_ESP_GMP_OTA_DEVICE` / `OTA_HOST`)
- Optional **File Transfer** profile (`CONFIG_ESP_GMP_PROFILE_FILE_TRANSFER`)
- Optional **OS** profile for runtime capability bits (`CONFIG_ESP_GMP_PROFILE_OS`)
- Product-policy payload cap via Kconfig, with an effective limit of `policy ∩ transport`

## Quick start (preferred: Flux / GATT)

```c
#include "esp_gmp.h"
#include "esp_gmp_flux.h"
#include "flux_gatt_session.h"

static bool on_gmp_packet(void *ctx, const esp_gmp_rx_t *pkt)
{
    (void)ctx;

    if (pkt->op == ESP_GMP_OP_READ_REQ &&
        pkt->group_id == ESP_GMP_GRP_OS &&
        pkt->command_id == ESP_GMP_OS_CAP_QUERY) {
        uint8_t cap[12] = {
            0x02, 0x02, 0x01, 0x03,
            0x00, 0x04, /* max_control_payload = 1024 */
            0x00, 0x00, 0x0F, 0xF0, /* max_gmp_payload example */
            0x01, 0x00,
        };
        esp_gmp_tx_params_t tx = {
            .ver = ESP_GMP_VER,
            .op = ESP_GMP_OP_READ_RSP,
            .group_id = pkt->group_id,
            .sequence = pkt->sequence,
            .command_id = pkt->command_id,
            .flags = 0,
            .status = ESP_GMP_STATUS_OK,
        };
        esp_gmp_send(pkt->link, &tx, cap, sizeof(cap));
    }
    return false; /* false: caller keeps ownership of frame_buf */
}

esp_err_t app_setup_gmp(gatt_session_t *gatt)
{
    ble_session_callbacks_t cbs = { 0 };

    esp_gmp_init();
    esp_gmp_on_packet_register(on_gmp_packet, NULL);

    /* Use gatt_session* as the stable link handle (as in the BLE OTA examples). */
    ESP_ERROR_CHECK(esp_gmp_flux_get_gatt_callbacks(gatt, &cbs, NULL));
    gatt->callbacks = cbs;
    return esp_gmp_flux_link_register(gatt, gatt->flux_session);
}
```

Transport data path:

```text
transport RX → esp_gmp_input
transport TX ← esp_gmp_send
```

For a custom transport, implement `esp_gmp_transport_t` and call `esp_gmp_link_register()` / `esp_gmp_input()` / `esp_gmp_transport_tx_done()` yourself.

## Lifecycle

Use `esp_gmp_link_unregister()` (or `esp_gmp_flux_link_unregister()` when using the Flux adapter) for per-connection teardown. `esp_gmp_deinit()` is only for final component shutdown after all links are unregistered and every transport has delivered its pending `esp_gmp_transport_tx_done()` callbacks; it is ignored while any link is still active.

## Configuration

| Kconfig | Default | Meaning |
|---------|---------|---------|
| `ESP_GMP_MAX_PAYLOAD` | 4096 | Product policy payload cap |
| `ESP_GMP_MAX_LINKS` | 4 | Registered links |
| `ESP_GMP_TX_QUEUE_DEPTH` | 4 | Per-link send queue when the transport is busy |
| `ESP_GMP_FLUX_WINDOW_SIZE` | 6 | Flux window for GMP sends (`0` = Flux default) |
| `ESP_GMP_FLUX_WINDOW_THRESHOLD` | 50 | Flux SACK threshold % |

Effective payload limit: `esp_gmp_max_payload_effective(link)` (policy ∩ transport limit).

## Dependency

```cmake
idf_component_register(
    ...
    REQUIRES esp_gmp esp_flux
)
```

OTA (direct write to the OTA partition): see [`profiles/ota/README.md`](profiles/ota/README.md) / [`profiles/ota/README_CN.md`](profiles/ota/README_CN.md).

BLE OTA examples: [`examples/esp_gmp/gmp_ota`](../../examples/esp_gmp/gmp_ota).
