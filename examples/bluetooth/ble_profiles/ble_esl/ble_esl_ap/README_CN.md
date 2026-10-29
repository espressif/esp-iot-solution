| 支持的芯片 | ESP32-H4 | ESP32-C6 | ESP32-S31 |
| ---------- | -------- | -------- | --------- |

# BLE ESL 接入点（Profile 示例）

默认编译为 **模式 2**：AP + 现有 Android App（NUS JSONL）。**不修改 App**：命令名、事件、NUS UUID、`max_chunk:192`、`max_line:1024`、`protocol: esl-jsonl-v1`、`device_name: ESP-ESL-AP` 与产品固件保持字节兼容。

本示例基于 iot-solution 的 `esp_ble_eslp_*` 与 `ble_conn_mgr`（不是 ESP-IDF `ble_esl_*`）。请与 [`ble_esl_tag`](../ble_esl_tag/README_CN.md) 配对。

原产品示例 `examples/bluetooth/ble_esl/` 保持不变。

## 环境要求

- ESP-IDF **v6.1 及以上**
- 支持 PAwR 的芯片：**ESP32-H4**、**ESP32-C6** 或 **ESP32-S31**（H4/S31 为 preview：`idf.py --preview`）
- 一块运行 `ble_esl_tag` 的 Tag（H4/C6 接 SSD1680 电子纸）
- 现有 Android ESL App（无需改 APK）

## 使用方法

```bash
cd examples/bluetooth/ble_profiles/ble_esl/ble_esl_ap
idf.py set-target esp32c6    # 或: idf.py --preview set-target esp32h4
idf.py erase-flash
idf.py build flash monitor
```

首次联调请对 AP 和 Tag 都执行 `erase-flash`，避免旧 NVS/绑定残留。

`sdkconfig.defaults` 打开 `CONFIG_BT_NIMBLE_DYNAMIC_SERVICE`。手机网关在 BLE host 启动之后注册 NUS，必须使用动态 GATT；关闭该选项时编译失败。

## App 预期流程

1. 手机连接 **ESP-ESL-AP**（NUS）
2. `provision_tag` → Tag 入网并 PAST 同步
3. `upload_image_*`（4736 字节 1bpp SSD1680 帧）→ `transfer_image_to_tag`
4. `display_image` / `refresh_display`

## 测试说明

本示例尚未提供无硬件的 Host tests。请在支持 PAwR 的 AP、`ble_esl_tag` 与 Android ESL App 上做硬件验证（配网 → 传图 → 显示），并覆盖 AP 重启后的 recovery 以及 Tag 已同步时的多次 `transfer_image_to_tag`。
