# ESP-GMP OTA Profile

**English** | [中文](README_CN.md)

OTA lives under `profiles/ota/` (same level as OS / File Transfer). Shared SHA-256 is in `common/`.

Device-side handler writes firmware to the next OTA partition with `esp_ota_write()` (no SPIFFS staging in this profile).

Flow:

1. `OTA_UPLOAD_CONTROL(start)` → `esp_ota_begin()`
2. `OTA_UPLOAD_DATA` → accept/enqueue then respond; a worker runs `esp_ota_write()` and incremental SHA256
3. `OTA_UPLOAD_CONTROL(finish + sha256)` → drain → verify → `esp_ota_end()` → set boot partition → restart (or wait for `esp_gmp_ota_apply()` under manual policy)

`image_size` is the full image size. Every `start` transfers from offset 0; resume is not supported.

SHA256 is for transfer integrity only. Production products must add signed app / secure boot and link authorization.

Protocol: [`SPEC.md`](../../SPEC.md) §12.

## Integration

```c
#include "esp_gmp.h"
#include "esp_gmp_ota.h"
#include "esp_gmp_os.h"   /* optional CAP */

void app_init(void)
{
    ESP_ERROR_CHECK(esp_gmp_init(NULL));
    ESP_ERROR_CHECK(esp_gmp_os_init());
    ESP_ERROR_CHECK(esp_gmp_ota_init(NULL));
    /* Link registration / Flux adapter as in examples/esp_gmp/gmp_ota */
}
```

Host upload APIs: `esp_gmp_ota_host.h`.

## Kconfig

| Option | Description |
|--------|-------------|
| `ESP_GMP_PROFILE_OTA` | Enable OTA profile |
| `ESP_GMP_OTA_DEVICE` / `ESP_GMP_OTA_HOST` | Build device and/or host role |
| `ESP_GMP_OTA_CHUNK_HINT` | Suggested chunk size; `0` = auto |
| `ESP_GMP_OTA_RESTART_DELAY_MS` | Delay before reboot after apply |

## Layout

```text
profiles/ota/
├── include/
│   ├── esp_gmp_ota.h
│   ├── esp_gmp_ota_host.h
│   └── esp_gmp_ota_proto.h
└── src/
    ├── esp_gmp_ota_device.c
    ├── esp_gmp_ota_host.c
    └── esp_gmp_ota_proto.c

common/                     # shared by OTA + FT
├── include/esp_gmp_sha256.h
└── src/esp_gmp_sha256.c
```

See `examples/esp_gmp/gmp_ota`.
