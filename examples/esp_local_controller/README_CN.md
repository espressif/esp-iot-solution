# ESP Local Controller 示例

使用 `esp_local_controller` + `protocomm_ext`，对接 IDF
[`esp_local_ctrl`](https://github.com/espressif/esp-idf/tree/master/examples/protocols/esp_local_ctrl) 设备端。

串口菜单可选 HTTP / BLE / Console；安全等级由对端 version JSON 的 `sec_ver` 自动识别。

## 构建

```bash
cd examples/esp_local_controller
idf.py set-target esp32c3
idf.py menuconfig
idf.py build flash monitor
```

请在 menuconfig 中配置 HTTP 模式用的 Wi-Fi、设备 URL，以及 Sec1/Sec2 测试默认值（仅实验室）。
