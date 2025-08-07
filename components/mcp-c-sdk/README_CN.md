# ESP32 MCP C SDK

[![Component Registry](https://components.espressif.com/components/espressif/mcp-c-sdk/badge.svg)](https://components.espressif.com/components/espressif/mcp-c-sdk)
[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.0%2B-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

[English](README.md) | **中文**

一个为 ESP32 设备实现的完整的 **模型上下文协议 (MCP)** 服务器 C SDK，为 AI 应用程序与 ESP32 设备的集成提供标准化方式。该组件使您的 ESP32 能够暴露工具和功能，供 AI 代理和应用程序发现和使用。

## 🌟 特性

- **🚀 简洁 API**：直观的工具注册和管理接口
- **🔧 动态注册**：运行时注册工具，支持灵活的参数模式
- **📦 模块化设计**：独立组件，易于集成到现有项目
- **🌐 HTTP 传输**：内置基于 HTTP 的 JSON-RPC 2.0，最大兼容性
- **🔌 自定义传输**：通过回调函数支持自定义传输实现
- **📊 类型安全**：全面的数据类型支持（布尔、整数、浮点、字符串、数组、对象）
- **🛡️ 内存安全**：自动内存管理和清理
- **✅ 参数验证**：内置参数验证和范围约束
- **🎯 MCP 兼容**：完全符合 MCP 规范

## 📦 安装

### 使用 ESP Component Registry（推荐）

```bash
idf.py add-dependency "espressif/mcp-c-sdk=*"
```

### 手动安装

```bash
cd your_project/components
git clone https://github.com/espressif/esp-iot-solution.git
cd esp-iot-solution/components
cp -r mcp-c-sdk your_project/components/
```

## 🚀 快速开始

```c
#include "esp_mcp_server.h"
#include "esp_mcp.h"

// 工具回调函数
static esp_mcp_value_t set_volume_callback(const esp_mcp_property_list_t* properties)
{
    // 从属性列表获取音量参数
    int volume = esp_mcp_property_list_get_property_int(properties, "volume");
    
    if (volume < 0 || volume > 100) {
        ESP_LOGE(TAG, "无效的音量值: %d", volume);
        return esp_mcp_value_create_bool(false);
    }
    
    // 设置设备音量
    current_volume = volume;
    ESP_LOGI(TAG, "音量设置为: %d", current_volume);
    
    return esp_mcp_value_create_bool(true);
}

void app_main(void)
{
    // 初始化 WiFi (使用 example_connect)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    
    // 创建 MCP 服务器
    esp_mcp_server_t *mcp_server = NULL;
    ESP_ERROR_CHECK(esp_mcp_server_create(&mcp_server));
    
    // 创建带音量参数的属性列表
    esp_mcp_property_list_t *properties = esp_mcp_property_list_create();
    
    // 添加带范围验证的音量属性（0-100）
    esp_mcp_property_list_add_property(properties, 
        esp_mcp_property_create_with_range("volume", 0, 100));
    
    // 注册带回调的工具
    esp_mcp_server_add_tool_with_callback(
        mcp_server, 
        "audio.set_volume",
        "设置音频扬声器音量（0-100）",
        properties, 
        set_volume_callback
    );
    
    // 初始化并启动 MCP（使用 HTTP 传输）
    esp_mcp_handle_t mcp_handle = 0;
    esp_mcp_config_t config = MCP_SERVER_DEFAULT_CONFIG();
    config.instance = mcp_server;
    
    ESP_ERROR_CHECK(esp_mcp_init(&config, &mcp_handle));
    ESP_ERROR_CHECK(esp_mcp_start(mcp_handle));
    
    ESP_LOGI(TAG, "MCP 服务器已在端口 80 启动");
}
```

## 🔧 核心 API

### 服务器生命周期

```c
// 创建 MCP 服务器实例
esp_err_t esp_mcp_server_create(esp_mcp_server_t **server);

// 销毁 MCP 服务器并释放所有资源
esp_err_t esp_mcp_server_destroy(esp_mcp_server_t *server);

// 使用传输配置初始化 MCP
esp_err_t esp_mcp_init(esp_mcp_config_t *config, esp_mcp_handle_t *handle);

// 启动 MCP 服务器（启动 HTTP 服务器）
esp_err_t esp_mcp_start(esp_mcp_handle_t handle);

// 停止 MCP 服务器
esp_err_t esp_mcp_stop(esp_mcp_handle_t handle);

// 清理 MCP 并释放资源
esp_err_t esp_mcp_deinit(esp_mcp_handle_t handle);
```

### 工具注册

```c
// 注册带回调函数的工具
esp_err_t esp_mcp_server_add_tool_with_callback(
    esp_mcp_server_t *server,
    const char *name,
    const char *description,
    esp_mcp_property_list_t *properties,
    esp_mcp_tool_callback_t callback
);
```

### 属性管理

```c
// 创建属性列表
esp_mcp_property_list_t* esp_mcp_property_list_create(void);

// 创建不同类型的属性
esp_mcp_property_t* esp_mcp_property_create_with_bool(const char* name, bool default_value);
esp_mcp_property_t* esp_mcp_property_create_with_int(const char* name, int default_value);
esp_mcp_property_t* esp_mcp_property_create_with_float(const char* name, float default_value);
esp_mcp_property_t* esp_mcp_property_create_with_string(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_array(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_object(const char* name, const char* default_value);

// 创建带范围验证的属性
esp_mcp_property_t* esp_mcp_property_create_with_range(const char* name, int min_value, int max_value);

// 将属性添加到属性列表
esp_err_t esp_mcp_property_list_add_property(
    esp_mcp_property_list_t* list,
    esp_mcp_property_t* property
);

// 从列表获取属性值
int esp_mcp_property_list_get_property_int(const esp_mcp_property_list_t* list, const char* name);
float esp_mcp_property_list_get_property_float(const esp_mcp_property_list_t* list, const char* name);
bool esp_mcp_property_list_get_property_bool(const esp_mcp_property_list_t* list, const char* name);
const char* esp_mcp_property_list_get_property_string(const esp_mcp_property_list_t* list, const char* name);
const char* esp_mcp_property_list_get_property_array(const esp_mcp_property_list_t* list, const char* name);
const char* esp_mcp_property_list_get_property_object(const esp_mcp_property_list_t* list, const char* name);
```

### 值创建

```c
// 创建不同类型的 MCP 值
esp_mcp_value_t esp_mcp_value_create_bool(bool value);
esp_mcp_value_t esp_mcp_value_create_int(int value);
esp_mcp_value_t esp_mcp_value_create_float(float value);
esp_mcp_value_t esp_mcp_value_create_string(const char* value);
```

## 📊 示例

组件在 `examples/mcp/mcp_server/` 中包含完整示例，演示：

- WiFi 连接设置
- MCP 服务器初始化和配置
- 各种参数类型的工具注册
- 属性验证（范围约束）
- 不同数据类型（布尔、整数、字符串、数组、对象）
- 设备状态报告
- 设置设备参数

### 运行示例

```bash
cd examples/mcp/mcp_server
idf.py set-target esp32
idf.py menuconfig  # 配置 WiFi 凭据
idf.py build flash monitor
```

### 示例工具

示例实现了几个工具：

1. **get_device_status** - 获取完整的设备状态（音频、屏幕等）
2. **audio.set_volume** - 设置音频音量（0-100），带范围验证
3. **screen.set_brightness** - 设置屏幕亮度（0-100）
4. **screen.set_theme** - 设置屏幕主题（亮色/暗色）
5. **screen.set_hsv** - 以 HSV 格式设置屏幕颜色（数组参数）
6. **screen.set_rgb** - 以 RGB 格式设置屏幕颜色（对象参数）

## 🧪 测试

使用任何 MCP 兼容客户端或 `curl` 测试您的 MCP 服务器：

### 列出可用工具

```bash
curl -X POST http://your-esp32-ip/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/list",
    "params": {}
  }'
```

### 调用工具

```bash
curl -X POST http://your-esp32-ip/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "tools/call",
    "params": {
      "name": "audio.set_volume",
      "arguments": {
        "volume": 75
      }
    }
  }'
```

### 获取设备状态

```bash
curl -X POST http://your-esp32-ip/mcp \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "tools/call",
    "params": {
      "name": "self.get_device_status",
      "arguments": {}
    }
  }'
```

## 🔌 传输支持

### 内置传输

- **HTTP**：默认包含（可通过 menuconfig 禁用）
  - 基于 HTTP POST 的 JSON-RPC 2.0
  - 默认端点：`/mcp`
  - 可配置端口（默认：80）

### 自定义传输

SDK 通过回调函数支持自定义传输实现：

```c
typedef struct {
    uint32_t transport;
    int (*open)(esp_mcp_handle_t handle, esp_mcp_transport_config_t *config);
    int (*read)(esp_mcp_handle_t handle, char *buffer, int len, int timeout_ms);
    int (*write)(esp_mcp_handle_t handle, const char *buffer, int len, int timeout_ms);
    int (*close)(esp_mcp_handle_t handle);
} esp_mcp_transport_funcs_t;

// 设置自定义传输函数
esp_err_t esp_mcp_transport_set_funcs(esp_mcp_handle_t handle, 
                                     esp_mcp_transport_funcs_t funcs);
```

## 📖 文档

- [用户指南](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/mcp/mcp-c-sdk.html)
- [API 参考](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/api-reference/mcp/index.html)

## 🤝 贡献

欢迎贡献！请随时提交 Pull Request。

## 📄 许可证

本项目采用 Apache License 2.0 许可证 - 查看 [LICENSE](license.txt) 文件了解详情。

## 🔗 相关链接

- [模型上下文协议规范](https://modelcontextprotocol.io/)
- [ESP-IDF](https://github.com/espressif/esp-idf)
- [ESP-IoT-Solution](https://github.com/espressif/esp-iot-solution)

## ❓ 常见问题

**Q1：使用包管理器时遇到以下问题**

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

**A1：** 这是因为使用了旧版本的包管理器。请在 ESP-IDF 环境中运行 `pip install -U idf-component-manager` 来更新。

**Q2：如何禁用 HTTP 传输？**

**A2：** 您可以通过 menuconfig 禁用 HTTP 传输：`Component config → MCP C SDK → Enable HTTP Transport`

**Q3：我可以同时使用多个传输协议吗？**

**A3：** 目前一次只能激活一个传输。您需要选择内置的 HTTP 传输或实现自定义传输。

---

**为 ESP32 和 AI 社区用 ❤️ 制作**

