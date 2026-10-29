# Network Provisioner 示例

同一固件可同时编入 SoftAP(HTTP)+Wi-Fi、BLE+Wi-Fi、BLE+Thread（按芯片能力与 Kconfig 勾选）。
上电后串口菜单选 1/2/3 执行，可连续多跑几种；也可用 menuconfig 关掉不需要的 `EXAMPLE_SUPPORT_*`。

安全等级 **Sec0 / Sec1 / Sec2 在运行时按对端 `proto-ver` 的 `sec_ver` 自动识别**：
- Sec0：无需凭据
- Sec1：提示输入 PoP（可直接回车使用 Kconfig 默认值；输入掩码为 `*`）
- Sec2：提示输入 username / password（password 掩码；默认值仅用于联调）

**注意：** Kconfig 中的 PoP / Sec2 默认值仅用于对接官方示例的实验室场景，请勿带固定密钥发布产品固件。
Thread Active Operational Dataset 建议在串口运行时输入，不要把真实网络密钥写进固件。
BLE+Thread 配网端只需 NimBLE；IEEE 802.15.4 仅设备侧需要（如 ESP32-H2 / C6）。

```bash
cd examples/network_provisioner
idf.py set-target esp32
idf.py menuconfig
idf.py build flash monitor
```

设备侧请用官方 `espressif/network_provisioning` 的 `wifi_prov` / `thread_prov`，并与 SoftAP 名、BLE 前缀对齐。
