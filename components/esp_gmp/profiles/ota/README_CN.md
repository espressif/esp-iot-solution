# ESP-GMP OTA Profile

**English** | [中文](README_CN.md)

OTA 位于 `profiles/ota/`（与 OS / File Transfer 同级）。共享 SHA-256 在 `common/`。

设备侧将固件经 `esp_ota_write()` 写入下一 OTA 分区（本 profile 无 SPIFFS 暂存）。

流程：

1. `OTA_UPLOAD_CONTROL(start)` → `esp_ota_begin()`
2. `OTA_UPLOAD_DATA` → 入队并应答；后台 worker 执行 `esp_ota_write()` 与增量 SHA256
3. `OTA_UPLOAD_CONTROL(finish + sha256)` → 排空 → 校验 → `esp_ota_end()` → 设启动分区 → 重启（或手动策略下等待 `esp_gmp_ota_apply()`）

`image_size` 为完整镜像大小；每次 `start` 从偏移 0 开始，不支持断点续传。

SHA256 仅保证传输完整性。量产需叠加签名固件 / secure boot 与链路鉴权。

协议：[`SPEC.md`](../../SPEC.md) §12。

## 集成

```c
#include "esp_gmp.h"
#include "esp_gmp_ota.h"
#include "esp_gmp_os.h"   /* 可选 CAP */

void app_init(void)
{
    ESP_ERROR_CHECK(esp_gmp_init(NULL));
    ESP_ERROR_CHECK(esp_gmp_os_init());
    ESP_ERROR_CHECK(esp_gmp_ota_init(NULL));
}
```

Host 上传 API：`esp_gmp_ota_host.h`。

## 目录

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

common/                     # OTA + FT 共用
├── include/esp_gmp_sha256.h
└── src/esp_gmp_sha256.c
```

示例见 `examples/esp_gmp/gmp_ota`。
