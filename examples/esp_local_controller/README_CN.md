# ESP Local Controller 示例

使用 `esp_local_controller` + `protocomm_ext`，对接 IDF
[`esp_local_ctrl`](https://github.com/espressif/esp-idf/tree/master/examples/protocols/esp_local_ctrl) 设备端。

串口菜单可选 HTTP / BLE / Console；安全等级由对端 version JSON 的 `sec_ver` 自动识别。

HTTP 会话成功后可以选择保持打开（最多 4 路）。再选 HTTP 连接下一台设备时，已经保持的会话不会断开。菜单 4 回到其中一路继续控制，菜单 5 全部关闭。再次输入已经保持的设备地址会直接恢复那一路，不会再占一个槽。同一台设备仍然只接受一个控制器。已保持的设备按协议、主机和端口匹配（`http` 默认 80，`https` 默认 443）。IPv6 地址写成方括号，例如 `http://[2001:db8::1]`。BLE 需要 NimBLE central、observer 和 GATT client，并且仍是单连接。

## 构建

```bash
cd examples/esp_local_controller
idf.py set-target esp32c3
idf.py menuconfig
idf.py build flash monitor
```

请在 menuconfig 中配置 HTTP 模式用的 Wi-Fi、设备 URL，以及 Sec1/Sec2 测试默认值（仅实验室）。
