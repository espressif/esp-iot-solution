# ESP Weaver

[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.5-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

[English](README.md) | **中文**

ESP Weaver 是一个用于构建智能家居设备的设备端 SDK，支持 Home Assistant 本地发现和控制。基于 [ESP-IDF](https://github.com/espressif/esp-idf) 构建，提供简单的设备/参数模型，通过 esp_local_ctrl + mDNS 实现局域网内通信，无需依赖云服务。

> **注意：** ESP Weaver **不是** ESP RainMaker 的直接替换品。虽然它使用兼容的本地控制协议和类似的设备/参数模型，但 API 命名空间（`esp_weaver_*`）、类型和功能集均有所不同。云端功能（MQTT、OTA、定时任务、场景等）被有意排除。每个进程仅支持一个 weaver 节点（单例模式）。

[ESP-Weaver](https://github.com/espressif/esp_weaver) 是乐鑫开发的对应 Home Assistant 自定义集成组件，通过 esp_local_ctrl 协议实现对 ESP 设备的本地发现与控制。

## 📋 架构与工作原理

```
┌─────────────────┐    mDNS Discovery    ┌─────────────────┐
│                 │ ◄─────────────────── │                 │
│  Home Assistant │                      │   ESP Device    │
│  (ESP-Weaver)   │ ───────────────────► │  (ESP Weaver)   │
│                 │    Local Control     │                 │
└─────────────────┘                      └─────────────────┘
         │                                        │
         │             Local Network              │
         └────────────────────────────────────────┘
```

1. ESP 设备启动后连接 WiFi 并通过 mDNS 广播服务
2. Home Assistant 中的 ESP-Weaver 集成发现设备
3. 用户输入设备的 PoP（拥有证明）码完成配对
4. 通过 Local Control 协议进行双向通信

## 🌟 功能特性

- **低延迟本地控制**：基于 mDNS 服务发现与 esp_local_ctrl 协议，实现局域网内毫秒级响应
- **离线可用**：设备与 Home Assistant 在局域网内直接通信，无需互联网连接即可正常控制
- **端到端加密**：采用 Proof of Possession (PoP) 进行设备认证，Security1 协议基于 Curve25519 密钥交换与 AES-CTR 加密
- **零配置发现**：设备联网后通过 mDNS 自动广播服务，无需手动配置即可被 Home Assistant 发现
- **简洁设备模型**：直观的设备/参数模型用于智能家居集成，支持参数实时更新

## 📦 安装

### 使用 ESP Component Registry（推荐）

```bash
idf.py add-dependency "espressif/esp_weaver=*"
```

### 手动安装

克隆仓库并将组件添加到您的项目中：

```bash
git clone https://github.com/espressif/esp-iot-solution.git
cp -r esp-iot-solution/components/esp_weaver your_project/components/
```

## 🚀 快速开始

```c
#include <esp_weaver.h>
#include <esp_weaver_standard_types.h>
#include <esp_weaver_standard_params.h>

void app_main(void)
{
    // 初始化 weaver 节点（单例）
    esp_weaver_config_t config = ESP_WEAVER_CONFIG_DEFAULT();
    esp_weaver_node_t *node = esp_weaver_node_init(&config, "我的设备", "Lightbulb");

    // 创建灯泡设备
    esp_weaver_device_t *dev = esp_weaver_device_create("Light", ESP_WEAVER_DEVICE_LIGHTBULB, NULL);
    esp_weaver_device_add_bulk_cb(dev, my_write_cb);

    // 添加参数
    esp_weaver_param_t *power = esp_weaver_param_create(ESP_WEAVER_DEF_POWER_NAME, ESP_WEAVER_PARAM_POWER,
                                    esp_weaver_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    esp_weaver_device_add_param(dev, power);
    esp_weaver_device_assign_primary_param(dev, power);

    esp_weaver_param_t *brightness = esp_weaver_param_create(ESP_WEAVER_DEF_BRIGHTNESS_NAME, ESP_WEAVER_PARAM_BRIGHTNESS,
                                         esp_weaver_int(50), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    esp_weaver_param_add_bounds(brightness, esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(1));
    esp_weaver_device_add_param(dev, brightness);

    esp_weaver_node_add_device(node, dev);

    // 启动本地控制（所有设置来自 Kconfig）
    esp_weaver_local_ctrl_start();
}
```

## 🔧 核心 API

### 初始化

```c
// 初始化 weaver 节点（返回节点句柄，失败返回 NULL）
esp_weaver_node_t *esp_weaver_node_init(const esp_weaver_config_t *config, const char *name, const char *type);

// 反初始化并释放资源
esp_err_t esp_weaver_node_deinit(const esp_weaver_node_t *node);

// 获取节点 ID
const char *esp_weaver_get_node_id(void);
```

### 设备管理

```c
// 创建新设备
esp_weaver_device_t *esp_weaver_device_create(const char *dev_name, const char *type, void *priv_data);

// 删除设备（需先从节点移除）
esp_err_t esp_weaver_device_delete(esp_weaver_device_t *device);

// 向设备添加参数
esp_err_t esp_weaver_device_add_param(esp_weaver_device_t *device, esp_weaver_param_t *param);

// 添加批量写入回调（一次处理多个参数）
esp_err_t esp_weaver_device_add_bulk_cb(esp_weaver_device_t *device, esp_weaver_device_bulk_write_cb_t write_cb);

// 指定设备的主参数
esp_err_t esp_weaver_device_assign_primary_param(esp_weaver_device_t *device, esp_weaver_param_t *param);

// 将设备添加到节点 / 从节点移除设备
esp_err_t esp_weaver_node_add_device(const esp_weaver_node_t *node, esp_weaver_device_t *device);
esp_err_t esp_weaver_node_remove_device(const esp_weaver_node_t *node, esp_weaver_device_t *device);

// 获取设备名称 / 私有数据
const char *esp_weaver_device_get_name(const esp_weaver_device_t *device);
void *esp_weaver_device_get_priv_data(const esp_weaver_device_t *device);

// 按名称或类型获取参数
esp_weaver_param_t *esp_weaver_device_get_param_by_name(const esp_weaver_device_t *device, const char *param_name);
esp_weaver_param_t *esp_weaver_device_get_param_by_type(const esp_weaver_device_t *device, const char *param_type);
```

### 参数管理

```c
// 创建新参数
esp_weaver_param_t *esp_weaver_param_create(const char *param_name, const char *type, esp_weaver_param_val_t val, uint8_t properties);

// 删除参数（仅用于未添加到设备的参数）
esp_err_t esp_weaver_param_delete(esp_weaver_param_t *param);

// 添加参数的 UI 类型
esp_err_t esp_weaver_param_add_ui_type(esp_weaver_param_t *param, const char *ui_type);

// 添加值边界
esp_err_t esp_weaver_param_add_bounds(esp_weaver_param_t *param, esp_weaver_param_val_t min, esp_weaver_param_val_t max, esp_weaver_param_val_t step);

// 获取参数名称 / 值
const char *esp_weaver_param_get_name(const esp_weaver_param_t *param);
const esp_weaver_param_val_t *esp_weaver_param_get_val(const esp_weaver_param_t *param);

// 更新参数值（标记待上报）
esp_err_t esp_weaver_param_update(esp_weaver_param_t *param, esp_weaver_param_val_t val);

// 更新并推送给客户端
esp_err_t esp_weaver_param_update_and_report(esp_weaver_param_t *param, esp_weaver_param_val_t val);
```

### 本地控制

```c
// 启动本地控制（所有设置来自 Kconfig）
esp_err_t esp_weaver_local_ctrl_start(void);

// 停止本地控制服务
esp_err_t esp_weaver_local_ctrl_stop(void);

// 设置自定义拥有证明（可选，在启动前调用）
esp_err_t esp_weaver_local_ctrl_set_pop(const char *pop);

// 获取当前拥有证明
const char *esp_weaver_local_ctrl_get_pop(void);

// 设置自定义 mDNS TXT 记录（在启动后调用）
esp_err_t esp_weaver_local_ctrl_set_txt(const char *key, const char *value);

// 推送参数变化给连接的客户端
esp_err_t esp_weaver_local_ctrl_send_params(void);
```

## 📊 标准类型

### 设备类型

- `ESP_WEAVER_DEVICE_LIGHTBULB` - 智能灯泡

### 参数类型

- `ESP_WEAVER_PARAM_NAME` - 设备名称
- `ESP_WEAVER_PARAM_POWER` - 电源开关
- `ESP_WEAVER_PARAM_BRIGHTNESS` - 亮度等级
- `ESP_WEAVER_PARAM_HUE` - 色相
- `ESP_WEAVER_PARAM_SATURATION` - 饱和度
- `ESP_WEAVER_PARAM_CCT` - 相关色温

### UI 类型

- `ESP_WEAVER_UI_TOGGLE` - 开关切换
- `ESP_WEAVER_UI_SLIDER` - 滑块控制
- `ESP_WEAVER_UI_HUE_SLIDER` - 色相滑块
- `ESP_WEAVER_UI_TEXT` - 文本显示

### 值辅助宏

```c
esp_weaver_bool(x)    // 创建布尔值
esp_weaver_int(x)     // 创建整数值
esp_weaver_float(x)   // 创建浮点值
esp_weaver_str(x)     // 创建字符串值
esp_weaver_obj(x)     // 创建 JSON 对象值
esp_weaver_array(x)   // 创建 JSON 数组值
```

## 🔒 安全与 PoP

组件支持两种安全模式：

- **SEC0**：无安全性（用于开发/测试）
- **SEC1**：Curve25519 密钥交换 + AES-CTR 加密，带拥有证明 (PoP)

设备启动后，串口日志中会显示 PoP 码：

```
I (10592) esp_weaver_local_ctrl: No PoP in NVS. Generating a new one.
I (10592) esp_weaver_local_ctrl: PoP for local control: 8f820513
```

PoP 会自动随机生成并持久化到 NVS。后续重启会复用同一 PoP。也可以在启动本地控制前通过 `esp_weaver_local_ctrl_set_pop()` 以编程方式设置自定义 PoP。

在 Home Assistant 的 ESP-Weaver 集成中添加设备时需要输入此 PoP 码。

### 配置

安全性可以通过 `idf.py menuconfig` → ESP Weaver 进行配置：

- **本地控制 HTTP 端口**：默认 8080
- **安全版本**：SEC0（无安全性）或 SEC1（带 PoP）

## 🏠 Home Assistant 配置

1. 在 Home Assistant 中安装 [ESP-Weaver](https://github.com/espressif/esp_weaver) 集成
2. 设备连接 WiFi 后会被自动发现
3. 输入设备的 PoP 码完成配对
4. 设备实体将出现在 Home Assistant 中

## 📊 示例

请查看 [examples/weaver](../../examples/weaver) 目录获取完整示例：

- [led_light](../../examples/weaver/led_light) - RGB LED 灯控制

## 🧪 测试

组件在 `test_apps/` 中包含完整的测试套件，包含 18 个测试用例，涵盖：

- 初始化 / 反初始化 API
- 设备 API（创建、删除、添加参数、回调）
- 参数 API（创建、删除、设置 UI、设置边界、更新）
- 集成测试（完整流程）
- 内存泄漏测试

```bash
cd components/esp_weaver/test_apps
idf.py set-target esp32c6
idf.py build flash monitor
```

详见 [test_apps/README.md](test_apps/README.md)。

## 📋 IDF 版本兼容性

| ESP-IDF 版本 | 支持状态 |
|:------------ |:--------:|
| v5.5 | ✅ 支持 |

## 🔧 硬件

本仓库示例已在以下乐鑫官方开发板上完成功能验证：

| 芯片 | 开发板型号 | 模组型号 | 板载 LED |
|------|-----------|---------|---------|
| ESP32 | [ESP32_DevKitc_V4](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32/esp32-devkitc/user_guide.html) | ESP32-WROVER-E | 否（需外接） |
| ESP32-C2 | [ESP8684-DevKitM-1 V1.1](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32c2/esp8684-devkitm-1/user_guide.html) | ESP8684-MINI-1 V1.0 | 是 |
| ESP32-C3 | [ESP32-C3-DevKitM-1 V1.0](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32c3/esp32-c3-devkitm-1/index.html) | ESP32-C3-MINI-1 V1.1 | 是 |
| ESP32-C5 | [ESP32-C5-DevKitC-1 V1.2](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32c5/esp32-c5-devkitc-1/user_guide.html) | ESP32-C5-WROOM-1 V1.3 | 是 |
| ESP32-C6 | [ESP32-C6-DevKitC-1 V1.2](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html) | ESP32-C6-WROOM-1 | 是 |
| ESP32-C61 | [ESP32-C61-DevKitC-1 V1.0](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32c61/esp32-c61-devkitc-1/user_guide_v1.0.html) | ESP32-C61-WROOM-1 V1.1 | 是 |
| ESP32-S3 | [ESP32-S3-DevKitC-1 v1.1](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html) | ESP32-S3-WROOM-1 V1.3 | 是 |

## 🤝 贡献

欢迎贡献！请随时提交 Pull Request。

## ❓ 常见问题

**Q1：ESP Weaver 和 ESP-RainMaker 有什么区别？**

**A1：** ESP Weaver 使用与 ESP-RainMaker 本地控制兼容的协议，但它**不是**直接替换品。所有云端相关功能（MQTT、OTA、定时任务、场景、设备认领等）均已移除。API 命名空间为 `esp_weaver_*`（而非 `esp_rmaker_*`），部分功能如单参数读回调也未包含。ESP Weaver 专注于通过 esp_local_ctrl + mDNS 实现 Home Assistant 本地发现与控制。

**Q2：需要互联网连接吗？**

**A2：** 不需要。ESP Weaver 完全在局域网内运行。ESP 设备和 Home Assistant 只需要在同一个局域网内即可。

**Q3：如何更改 PoP 码？**

**A3：** 可以在启动本地控制前通过 `esp_weaver_local_ctrl_set_pop()` 以编程方式设置自定义 PoP。如果未设置自定义 PoP，首次启动时会自动随机生成并持久化到 NVS。

**Q4：应该使用哪个 Home Assistant 集成？**

**A4：** 安装 [ESP-Weaver](https://github.com/espressif/esp_weaver) 自定义集成。它会通过 mDNS 自动发现局域网内的 ESP Weaver 设备。

## 🔗 相关链接

- [ESP-Weaver](https://github.com/espressif/esp_weaver) - Home Assistant 自定义集成
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html)
- [esp_local_ctrl 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/protocols/esp_local_ctrl.html)
- [Home Assistant 官方文档](https://www.home-assistant.io/docs/)
- [ESP-IoT-Solution](https://github.com/espressif/esp-iot-solution)

## 📄 许可证

本项目采用 Apache License 2.0 许可证 - 查看 [LICENSE](license.txt) 文件了解详情。
