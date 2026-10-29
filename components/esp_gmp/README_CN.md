# ESP-GMP

[English](README.md) | **中文**

ESP-GMP 是面向 ESP-IDF 的管理协议核心。每条 GMP 消息为固定的 **10 字节头 + payload** 帧，由应用注册的传输层承载。

协议规范见 [`SPEC.md`](SPEC.md)。

## 架构

```text
应用层（OTA、OS_CAP_QUERY、…）
        ↓ esp_gmp_send / on_packet
   esp_gmp（本组件）
        ↓ esp_gmp_transport_t
   esp_flux / UART / socket / …
```

GMP **不**实现 `sub_seq` 分片或 RX 重组；这些由绑定的传输层负责（例如 Flux）。

## 功能特性

- 传输无关核心：组帧、可选 CRC、多 link TX 队列
- 可选 Flux/GATT 适配（`esp_gmp_flux_*`），对接使用 `esp_flux` 的 BLE 路径
- 可选 OTA 子模块：将固件直写下一 OTA 分区，并做 SHA256 校验
- 通过 Kconfig 配置产品侧 payload 上限，有效上限为 `策略 ∩ 传输`

## 快速开始（推荐：Flux / GATT）

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
            0x00, 0x00, 0x0F, 0xF0, /* max_gmp_payload 示例 */
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
    return false; /* false：由调用方继续持有 frame_buf */
}

esp_err_t app_setup_gmp(gatt_session_t *gatt)
{
    ble_session_callbacks_t cbs = { 0 };

    esp_gmp_init();
    esp_gmp_on_packet_register(on_gmp_packet, NULL);

    /* 与 BLE OTA 示例一致：使用 gatt_session* 作为稳定 link 句柄 */
    ESP_ERROR_CHECK(esp_gmp_flux_get_gatt_callbacks(gatt, &cbs, NULL));
    gatt->callbacks = cbs;
    return esp_gmp_flux_link_register(gatt, gatt->flux_session);
}
```

数据通路：

```text
transport RX → esp_gmp_input
transport TX ← esp_gmp_send
```

若使用自定义传输，需自行实现 `esp_gmp_transport_t`，并调用 `esp_gmp_link_register()` / `esp_gmp_input()` / `esp_gmp_transport_tx_done()`。

## 生命周期

按连接拆除时调用 `esp_gmp_link_unregister()`（使用 Flux 适配时调用 `esp_gmp_flux_link_unregister()`）。`esp_gmp_deinit()` 仅用于最终关停：所有 link 已注销，且传输层已送达全部挂起的 `esp_gmp_transport_tx_done()`；若仍有活跃 link，该调用会被忽略。

## 配置

| Kconfig | 默认值 | 含义 |
|---------|--------|------|
| `ESP_GMP_MAX_PAYLOAD` | 4096 | 产品策略 payload 上限 |
| `ESP_GMP_MAX_LINKS` | 4 | 可注册 link 数 |
| `ESP_GMP_TX_QUEUE_DEPTH` | 4 | 传输繁忙时每 link 发送队列深度 |
| `ESP_GMP_FLUX_WINDOW_SIZE` | 6 | GMP 发送使用的 Flux 窗口（`0` = Flux 默认） |
| `ESP_GMP_FLUX_WINDOW_THRESHOLD` | 50 | Flux SACK 阈值百分比 |

有效 payload 上限：`esp_gmp_max_payload_effective(link)`（策略 ∩ 传输上限）。

## 依赖

```cmake
idf_component_register(
    ...
    REQUIRES esp_gmp esp_flux
)
```

OTA（直写 OTA 分区）见 [`profiles/ota/README_CN.md`](profiles/ota/README_CN.md) / [`profiles/ota/README.md`](profiles/ota/README.md)。

BLE OTA 示例：[`examples/esp_gmp/gmp_ota`](../../examples/esp_gmp/gmp_ota)。
