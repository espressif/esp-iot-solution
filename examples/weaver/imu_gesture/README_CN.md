| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | --------- | -------- | -------- |

# IMU 手势示例（基于 BMI270）

[English](README.md)

本示例演示如何使用 ESP Weaver 本地控制协议构建 IMU 手势检测设备，实现通过 Home Assistant 进行本地网络发现和状态上报。

## 支持的手势

| 手势 | 描述 |
|------|------|
| toss | 设备被向上抛 |
| flip | 设备被翻转 |
| shake | 设备被摇晃 |
| rotation | 设备被旋转 |
| push | 设备被推动 |
| circle | 设备画圆 |
| clap_single | 检测到单次拍手 |
| clap_double | 检测到双次拍手 |
| clap_triple | 检测到三次拍手 |

## 硬件

* 带有 ESP32/ESP32-C2/ESP32-C3/ESP32-C5/ESP32-C6/ESP32-C61/ESP32-S3 SoC 的开发板
* BMI270 传感器
* USB 数据线用于供电和编程
* WiFi 路由器用于网络连接

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
I (10573) esp_weaver_local_ctrl: No PoP in NVS. Generating a new one.
I (10573) esp_weaver_local_ctrl: PoP for local control: b1acbcdf
```

PoP 会自动随机生成并持久化到 NVS，后续重启显示 `PoP read from NVS`，PoP 值保持不变。在 Home Assistant 的 ESP-Weaver 集成中添加设备时需要输入此 PoP 码。

### 将设备添加到 Home Assistant

1. 在 Home Assistant 中安装 [ESP-Weaver](https://github.com/espressif/esp_weaver) 集成
2. 设备连接 WiFi 后会被自动发现
3. 输入设备的 PoP 码完成配对
4. 设备实体（手势类型、置信度、方向、角度）将出现在 Home Assistant 中

## 示例输出

程序启动后，您将看到以下日志：

```
I (417) main_task: Calling app_main()
I (427) app_driver: === BMI270 Shake Detection Configuration ===
I (427) app_driver: Selected Board: ESP SPOT C5
I (427) app_driver: GPIO Configuration:
I (427) app_driver:   - Interrupt GPIO: 3
I (437) app_driver:   - I2C SCL GPIO: 26
I (437) app_driver:   - I2C SDA GPIO: 25
I (447) app_driver:   - I2C Type: Hardware I2C
I (447) app_driver: ================================
I (667) BMI2_ESP32: Detected FreeRTOS tick rate: 100 Hz
I (1987) bmi270_api: BMI270 sensor created successfully
I (1997) app_driver: Move the sensor to get shake interrupt...
I (1997) esp_weaver: Weaver initialized: ESP Weaver Device (IMU Gesture), node_id: D0CF13E28F50
I (1997) app_main: Node ID: D0CF13E28F50
I (1997) esp_weaver: Device created: IMU Gesture Sensor (esp.device.imu-gesture)
I (2007) esp_weaver: Device added to node: IMU Gesture Sensor
I (2017) app_main: IMU gesture device created and added to node
I (2017) example_connect: Start example_connect.
I (14017) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:d2cf:13ff:fee2:8f50, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (14457) esp_netif_handlers: example_netif_sta ip: 192.168.1.103, mask: 255.255.255.0, gw: 192.168.1.1
I (14457) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.1.103
I (14457) example_common: Connected to example_netif_sta
I (14467) example_common: - IPv4 address: 192.168.1.103,
I (14467) example_common: - IPv6 address: fe80:0000:0000:0000:d2cf:13ff:fee2:8f50, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (14477) esp_weaver_local_ctrl: Starting local control with HTTP transport and security version: 1
I (14487) mdns_mem: mDNS task will be created from internal RAM
I (14497) esp_weaver_local_ctrl: PoP read from NVS
I (14497) esp_weaver_local_ctrl: PoP for local control: b1acbcdf
I (14507) esp_weaver_local_ctrl: Local control started on port 8080, node_id: D0CF13E28F50
I (14517) app_main: Local control started successfully
I (14517) main_task: Returned from app_main()
```

> 💡 **Node ID**：默认由 MAC 地址自动生成，也可通过 `esp_weaver_config_t.node_id` 字段自定义。
>
> 💡 **PoP (Proof of Possession)**：SEC1 模式下，PoP 会自动随机生成并持久化到 NVS。首次启动显示 `No PoP in NVS. Generating a new one.`，后续重启显示 `PoP read from NVS`，PoP 值保持不变。也可通过 `esp_weaver_local_ctrl_set_pop()` 在启动前手动设置。
>
> 💡 安全模式 (SEC0/SEC1) 可通过 `idf.py menuconfig` → ESP Weaver 配置。

### 手势上报日志示例

检测到手势事件时，它们将通过本地控制上报。串口输出将显示如下日志：

```
I (224603) app_driver: Heavy shake on: 
I (224603) app_driver: Y axis
I (224603) app_imu_gesture: Gesture event reported: shake (confidence: 0%)
I (224603) app_imu_gesture:   Orientation: normal

I (227603) app_driver: Slight shake on: 
I (227603) app_driver: X axis
I (227603) app_imu_gesture: Gesture event reported: shake (confidence: 0%)
I (227603) app_imu_gesture:   Orientation: normal

I (230703) app_driver: Slight shake on: 
I (230703) app_driver: Z axis
I (230703) app_imu_gesture: Gesture event reported: shake (confidence: 0%)
I (230703) app_imu_gesture:   Orientation: normal
```

## 注意

本示例仅演示 shake 手势的检测与上报。要识别更多手势，需要实现相应的手势检测算法。

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
