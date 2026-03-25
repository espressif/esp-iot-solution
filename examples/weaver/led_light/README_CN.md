| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | --------- | -------- | -------- |

# LED 灯示例

[English](README.md)

本示例演示如何使用 ESP Weaver 本地控制协议构建智能灯设备，实现通过 Home Assistant 进行本地网络发现和控制。

## 功能

* 电源开/关控制
* 亮度调节 (0-100%)
* HSV (色相、饱和度、明度) 颜色控制 (色相: 0-360°, 饱和度: 0-100%)

## 硬件

* 带有 ESP32/ESP32-C2/ESP32-C3/ESP32-C5/ESP32-C6/ESP32-C61/ESP32-S3 SoC 的开发板
* RGB LED（WS2812 或 3 引脚 RGB 模块）
* USB 数据线用于供电和编程
* WiFi 路由器用于网络连接
* 按钮使用开发板上的 BOOT 按钮

> 💡 LED 类型和 GPIO 配置可以通过 `idf.py menuconfig` → Example Configuration 进行更改。

## 如何使用示例

在项目配置和编译之前，请确保使用 `idf.py set-target <chip_name>` 设置正确的芯片目标。

### 配置项目

打开项目配置菜单：

```bash
idf.py menuconfig
```

在 `Example Connection Configuration` 菜单中：
* 设置 Wi-Fi SSID
* 设置 Wi-Fi 密码

在 `ESP Weaver` 菜单中（可选）：
* 设置安全版本（SEC0 或 SEC1）

### 编译和烧录

编译项目并烧录到开发板，然后运行监视工具查看串口输出：

```bash
idf.py -p PORT flash monitor
```

（要退出串口监视器，请输入 ``Ctrl-]``。）

请参阅[快速入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)了解配置和使用 ESP-IDF 构建项目的完整步骤。

### 获取 PoP 码

设备启动后，串口日志中会显示 Local Control PoP 码：

```
I (10592) esp_weaver_local_ctrl: No PoP in NVS. Generating a new one.
I (10592) esp_weaver_local_ctrl: PoP for local control: 8f820513
```

PoP 会自动随机生成并持久化到 NVS，后续重启将显示 `PoP read from NVS`，PoP 值保持不变。在 Home Assistant 的 ESP-Weaver 集成中添加设备时需要输入此 PoP 码。

### 将设备添加到 Home Assistant

1. 在 Home Assistant 中安装 [ESP-Weaver](https://github.com/espressif/esp_weaver) 集成
2. 设备连接 WiFi 后会被自动发现
3. 输入设备的 PoP 码完成配对
4. 设备实体（电源、亮度、颜色）将出现在 Home Assistant 中

## 示例输出

程序启动后，您将看到以下日志：

```
I (10502) esp_netif_handlers: example_netif_sta ip: 192.168.30.11, mask: 255.255.255.0, gw: 192.168.30.1
I (10502) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.30.11
I (10532) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:6255:f9ff:fef9:19ec, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (10532) example_common: Connected to example_netif_sta
I (10532) example_common: - IPv4 address: 192.168.30.11,
I (10542) example_common: - IPv6 address: fe80:0000:0000:0000:6255:f9ff:fef9:19ec, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (10552) esp_weaver: Weaver initialized: ESP Weaver Device (Lightbulb), node_id: 6055F9F919EC
I (10562) app_main: Node ID: 6055F9F919EC
I (10562) esp_weaver: Device created: Light (esp.device.lightbulb)
I (10572) esp_weaver: Device added to node: Light
I (10572) esp_weaver_local_ctrl: Starting local control with HTTP transport and security version: 1
I (10582) mdns_mem: mDNS task will be created from internal RAM
I (10592) esp_weaver_local_ctrl: No PoP in NVS. Generating a new one.
I (10592) esp_weaver_local_ctrl: PoP for local control: 8f820513
I (10602) esp_weaver_local_ctrl: Local control started on port 8080, node_id: 6055F9F919EC
I (10612) app_main: Local control started successfully
```

> 💡 **Node ID**：默认由 MAC 地址自动生成，也可通过 `esp_weaver_config_t.node_id` 字段自定义。
>
> 💡 **PoP (Proof of Possession)**：SEC1 模式下，PoP 会自动随机生成并持久化到 NVS。首次启动显示 `No PoP in NVS. Generating a new one.`，后续重启显示 `PoP read from NVS`，PoP 值保持不变。也可通过 `esp_weaver_local_ctrl_set_pop()` 在启动前手动设置。
>
> 💡 安全模式 (SEC0/SEC1) 可通过 `idf.py menuconfig` → ESP Weaver 配置。

### 控制日志

通过本地控制控制灯时，设备串口输出将显示如下日志：

```
I (37132) app_light: Light received 1 params in write
I (37132) app_light: Light.Power = false

I (38062) app_light: Light received 1 params in write
I (38062) app_light: Light.Power = true

I (40532) app_light: Light received 1 params in write
I (40532) app_light: Light.Brightness = 63

I (42272) app_light: Light received 1 params in write
I (42272) app_light: Light.Hue = 141

I (42342) app_light: Light received 1 params in write
I (42342) app_light: Light.Saturation = 38
```

## 故障排除

* **WiFi 连接失败**：检查 `menuconfig` 中的 WiFi 凭据，确保设备在 WiFi 范围内
* **设备未被发现**：确保 Home Assistant 和 ESP 设备在同一局域网内，且已安装 [ESP-Weaver](https://github.com/espressif/esp_weaver) 集成
* **PoP 码被拒绝**：验证 PoP 码是否与串口日志中显示的一致

如有任何技术问题，请在 GitHub 上提交 [issue](https://github.com/espressif/esp-iot-solution/issues)。

## 技术参考

* [ESP Weaver 组件](../../../components/esp_weaver/README_CN.md)
* [ESP-Weaver Home Assistant 集成](https://github.com/espressif/esp_weaver)
* [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html)
* [esp_local_ctrl 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/protocols/esp_local_ctrl.html)
