# GMP OTA BLE 示例

[English](README.md) | **中文**

GMP over Flux 的完整 OTA 演示：

- **Server（设备）**：收到固件后 **直写 OTA 分区** 并重启（框架内无 SPIFFS）
- **Client（主机）**：用 **SPIFFS 管理待升级固件**，连接后推送给 Server

## 目录

```text
examples/esp_gmp/gmp_ota/
├── common_components/gmp_ota_common/
├── ble_gmp_ota_server/     # 外设，双 OTA 分区
└── ble_gmp_ota_client/     # 中心，SPIFFS 存 ota_image.bin
```

## 硬件

- **Server**：4 MB Flash（双 OTA 槽，每个 OTA 槽 `0x1A0000` 字节）
- **Client**：4 MB Flash（factory + SPIFFS `storage` 约 2 MB 固件区）

待上传镜像必须不大于 Server OTA 槽大小。Client SPIFFS 可以容纳更大的文件，但 Server 会在 `START` 阶段拒绝超过 OTA 槽容量的镜像。

## 构建 Server

```bash
cd examples/esp_gmp/gmp_ota/ble_gmp_ota_server
idf.py set-target esp32
idf.py build flash monitor
```

## 构建 Client

```bash
cp ble_gmp_ota_server/build/ble_gmp_ota_server.bin \
   ble_gmp_ota_client/spiffs/ota_image.bin

cd ble_gmp_ota_client
idf.py set-target esp32
idf.py build flash monitor
```

Client 从 `/spiffs/ota_image.bin` 读取固件，扫描 `ESP-GMP-OTA` 后自动上传。

如果未放置 `ble_gmp_ota_client/spiffs/ota_image.bin`，Client 仍可构建和烧录；运行时会提示缺少固件并跳过 OTA 上传。

## 安全边界

本示例只演示 GMP-over-Flux OTA 数据通路，不验证固件来源，不启用 BLE 访问控制，也不替代 signed app 或 secure boot。请仅在受控实验环境中使用；生产产品需要自行增加链路鉴权、业务授权和固件可信校验。

Client 示例默认挂载 SPIFFS 时不会自动格式化失败分区（`format_if_mount_failed=false`）；挂载失败会报错退出。如需更新 `ota_image.bin`，请按 `ble_gmp_ota_client/spiffs/README.md` 重新生成并烧录 SPIFFS。

## 组件 API

设备侧 OTA 处理器：`esp_gmp_ota_init()`（见 `components/esp_gmp/profiles/ota/`）。

协议：[`SPEC.md`](../../../components/esp_gmp/SPEC.md)。

## 测试建议

基础协议和 OTA payload 解析由 `components/esp_gmp/test` 覆盖。BLE OTA 示例建议在 HIL 或手工集成测试中覆盖：`OS_CAP_QUERY`、小镜像上传、断链取消、FINISH 超时边界，以及缺少 `ota_image.bin` 时运行时跳过 OTA 的日志路径。
