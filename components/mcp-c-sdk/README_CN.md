# ESP32 MCP C SDK

[![Component Registry](https://components.espressif.com/components/espressif/mcp-c-sdk/badge.svg)](https://components.espressif.com/components/espressif/mcp-c-sdk)
[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.4%2B-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

[English](README.md) | **中文**

一个为 ESP32 设备实现的完整的 **模型上下文协议 (MCP)** C SDK，为 AI 应用程序与 ESP32 设备的集成提供标准化方式。该组件使您的 ESP32 能够暴露工具和功能，供 AI 代理和应用程序发现和使用。

## 📋 协议与兼容性

- **MCP 协议版本**: `2024-11-05`
- **JSON-RPC 版本**: `2.0`
- **支持的方法**:
  - `initialize` - 初始化 MCP 会话并协商能力
  - `tools/list` - 列出可用工具，支持基于游标的分页
  - `tools/call` - 使用提供的参数执行工具
  - `ping` - 健康检查端点
- **支持的能力**:
  - ✅ **工具 (Tools)**: 完整支持工具注册、列表和执行
  - ✅ **实验性功能 (Experimental)**: 支持实验性 MCP 功能
  - ✅ **游标分页 (Cursor Pagination)**: 支持分页工具列表
  - ✅ **参数验证 (Parameter Validation)**: 内置类型检查和范围约束验证
  - ⚠️ **提示 (Prompts)**: 当前不支持
  - ⚠️ **资源 (Resources)**: 当前不支持

## 🌟 特性

- **🚀 简洁 API**：直观的工具注册和管理接口
- **🔧 动态注册**：运行时注册工具，支持灵活的参数模式
- **📦 模块化设计**：独立组件，易于集成到现有项目
- **🌐 HTTP 传输**：内置基于 HTTP 的 JSON-RPC 2.0，最大兼容性
- **🔌 自定义传输**：通过回调函数支持自定义传输实现
- **📊 类型安全**：全面的数据类型支持（布尔、整数、浮点、字符串、数组、对象）
- **🛡️ 内存安全**：自动内存管理和清理
- **✅ 参数验证**：内置参数验证和范围约束
- **🔒 线程安全**：所有链表操作都有 mutex 保护，适用于多线程环境
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
#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"
// 传输/管理层（esp_mcp_mgr_*）在 esp_mcp_mgr.h
// 协议引擎层（esp_mcp_*）在 esp_mcp_engine.h

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
    // 初始化 Wi-Fi (使用 example_connect)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    
    // 创建 MCP 服务器
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    
    // 创建带回调的工具
    esp_mcp_tool_t *tool = esp_mcp_tool_create(
        "audio.set_volume",
        "设置音频扬声器音量（0-100）",
        set_volume_callback
    );
    
    // 添加带范围验证的音量属性（0-100）
    esp_mcp_tool_add_property(tool, 
        esp_mcp_property_create_with_range("volume", 0, 100));
    
    // 注册工具到服务器
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));
    
    // 初始化并启动 MCP（使用 HTTP 传输）
    esp_mcp_mgr_handle_t mcp_handle = 0;
    esp_mcp_mgr_config_t config = MCP_SERVER_DEFAULT_CONFIG();
    config.instance = mcp;
    
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &mcp_handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(mcp_handle));
    
    ESP_LOGI(TAG, "MCP 服务器已在端口 80 启动");
}
```

## 🔧 核心 API

### 服务器生命周期

```c
// 创建 MCP 服务器实例
esp_err_t esp_mcp_create(esp_mcp_t **server);

// 使用传输配置初始化 MCP
esp_err_t esp_mcp_mgr_init(esp_mcp_mgr_config_t *config, esp_mcp_mgr_handle_t *handle);

// 启动 MCP 服务器（启动 HTTP 服务器）
esp_err_t esp_mcp_mgr_start(esp_mcp_mgr_handle_t handle);

// 停止 MCP 服务器
esp_err_t esp_mcp_mgr_stop(esp_mcp_mgr_handle_t handle);

// 清理 MCP 并释放资源
esp_err_t esp_mcp_mgr_deinit(esp_mcp_mgr_handle_t handle);

// 销毁 MCP 服务器并释放所有资源
esp_err_t esp_mcp_destroy(esp_mcp_t *mcp);
```

### 工具注册

```c
// 创建工具
esp_mcp_tool_t *esp_mcp_tool_create(
    const char *name,
    const char *description,
    esp_mcp_tool_callback_t callback
);

// 向工具添加属性
esp_err_t esp_mcp_tool_add_property(
    esp_mcp_tool_t *tool,
    esp_mcp_property_t *property
);

// 向服务器添加工具
esp_err_t esp_mcp_add_tool(
    esp_mcp_t *mcp,
    esp_mcp_tool_t *tool
);

// 从服务器移除工具
esp_err_t esp_mcp_remove_tool(
    esp_mcp_t *mcp,
    esp_mcp_tool_t *tool
);
```

### 属性管理

```c
// 创建不同类型的属性
esp_mcp_property_t* esp_mcp_property_create_with_bool(const char* name, bool default_value);
esp_mcp_property_t* esp_mcp_property_create_with_int(const char* name, int default_value);
esp_mcp_property_t* esp_mcp_property_create_with_float(const char* name, float default_value);
esp_mcp_property_t* esp_mcp_property_create_with_string(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_array(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_object(const char* name, const char* default_value);

// 创建带范围验证的属性
esp_mcp_property_t* esp_mcp_property_create_with_range(const char* name, int min_value, int max_value);

// 创建带默认值和范围的属性
esp_mcp_property_t* esp_mcp_property_create_with_int_and_range(
    const char* name, 
    int default_value, 
    int min_value, 
    int max_value
);

// 销毁属性
esp_err_t esp_mcp_property_destroy(esp_mcp_property_t* property);

// 从列表获取属性值（线程安全）
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

- Wi-Fi 连接设置
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
idf.py menuconfig  # 配置 Wi-Fi 凭据
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

> **注意**：请求中的 `id` 字段必须是 **数字** 类型。不支持字符串或 null 类型的 ID。

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
  - **请求 ID**：仅支持数字类型的 ID（字符串或 null 类型的 ID 将被拒绝）

### 自定义传输

SDK 通过 `esp_mcp_mgr_config_t.transport` 提供的传输函数表 `esp_mcp_transport_t` 支持自定义传输实现。

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

## 🔒 线程安全

所有链表操作（工具和属性）都是线程安全的，并通过 mutex 保护：

- **工具列表操作**：所有工具列表操作都受 mutex 保护
  - 添加工具 - 线程安全
  - 查找工具 - 线程安全
  - 所有列表操作 - 线程安全

- **属性列表操作**：所有属性列表操作都受 mutex 保护
  - `esp_mcp_property_list_add_property()` - 线程安全
  - 所有 getter 函数 - 线程安全
  - 所有列表操作 - 线程安全

- **线程安全**：所有列表操作自动使用 mutex 保护。不建议直接访问内部链表结构。

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

**Q4：SDK 是线程安全的吗？**

**A4：** 是的！所有链表操作（工具和属性）都受 mutex 保护。SDK 设计为可在多线程环境中安全使用。请始终使用提供的 API 函数，而不是直接访问内部链表结构。

**Q5：如何安全地遍历工具或属性？**

**A5：** 所有列表操作都是自动线程安全的。SDK 内部对所有列表访问都使用 mutex 保护。对于高级用例，请参考内部 API 文档。

**Q6：支持哪些类型的请求 ID？**

**A6：** 仅支持数字（number）类型的请求 ID。JSON-RPC 请求中的字符串 ID 或 null ID 将被拒绝，并返回 `INVALID_REQUEST` 错误。这是当前实现的限制，以简化 ID 处理。

---

**为 ESP32 和 AI 社区用 ❤️ 制作**

