# ESP32 MCP C SDK

[![Component Registry](https://components.espressif.com/components/espressif/mcp-c-sdk/badge.svg)](https://components.espressif.com/components/espressif/mcp-c-sdk)
[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.4%2B-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

**English** | [中文](README_CN.md)

A comprehensive C SDK implementing the **Model Context Protocol (MCP)** for ESP32 devices, providing a standardized way to integrate AI applications with ESP32 devices. This component enables your ESP32 to expose tools and capabilities that can be discovered and used by AI agents and applications.

## 📋 Protocol & Compatibility

- **MCP Protocol Version**: `2024-11-05`
- **JSON-RPC Version**: `2.0`
- **Supported Methods**:
  - `initialize` - Initialize MCP session and negotiate capabilities
  - `tools/list` - List available tools with optional cursor-based pagination
  - `tools/call` - Execute a tool with provided arguments
  - `ping` - Health check endpoint
- **Supported Capabilities**:
  - ✅ **Tools**: Full support for tool registration, listing, and execution
  - ✅ **Experimental Features**: Support for experimental MCP features
  - ✅ **Cursor-based Pagination**: Support for paginated tool lists
  - ✅ **Parameter Validation**: Built-in validation with type checking and range constraints
  - ⚠️ **Prompts**: Not currently supported
  - ⚠️ **Resources**: Not currently supported

## 🌟 Features

- **🚀 Clean API**: Intuitive interface for tool registration and management
- **🔧 Dynamic Registration**: Register tools at runtime with flexible property schemas
- **📦 Modular Design**: Standalone component, easy to integrate into existing projects
- **🌐 HTTP Transport**: Built-in HTTP-based JSON-RPC 2.0 for maximum compatibility
- **🔌 Custom Transport**: Support for custom transport implementations through callbacks
- **📊 Type Safety**: Comprehensive data type support (boolean, integer, float, string, array, object)
- **🛡️ Memory Safe**: Automatic memory management and cleanup
- **✅ Parameter Validation**: Built-in parameter validation with range constraints
- **🔒 Thread Safe**: All list operations are protected by mutex for multi-threaded environments
- **🎯 MCP Compliant**: Fully compliant with MCP specification

## 📦 Installation

### Using ESP Component Registry (Recommended)

```bash
idf.py add-dependency "espressif/mcp-c-sdk=*"
```

### Manual Installation

```bash
cd your_project/components
git clone https://github.com/espressif/esp-iot-solution.git
cd esp-iot-solution/components
cp -r mcp-c-sdk your_project/components/
```

## 🚀 Quick Start

```c
#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"
// Transport manager (esp_mcp_mgr_*) lives in esp_mcp_mgr.h
// MCP engine (esp_mcp_*) lives in esp_mcp_engine.h

// Tool callback function
static esp_mcp_value_t set_volume_callback(const esp_mcp_property_list_t* properties)
{
    // Get volume parameter from properties
    int volume = esp_mcp_property_list_get_property_int(properties, "volume");
    
    if (volume < 0 || volume > 100) {
        ESP_LOGE(TAG, "Invalid volume value: %d", volume);
        return esp_mcp_value_create_bool(false);
    }
    
    // Set device volume
    current_volume = volume;
    ESP_LOGI(TAG, "Volume set to: %d", current_volume);
    
    return esp_mcp_value_create_bool(true);
}

void app_main(void)
{
    // Initialize Wi-Fi (using example_connect)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    
    // Create MCP server
    esp_mcp_t *mcp = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    
    // Create tool with callback
    esp_mcp_tool_t *tool = esp_mcp_tool_create(
        "audio.set_volume",
        "Set audio speaker volume (0-100)",
        set_volume_callback
    );
    
    // Add volume property with range validation (0-100)
    esp_mcp_tool_add_property(tool, 
        esp_mcp_property_create_with_range("volume", 0, 100));
    
    // Register tool to server
    ESP_ERROR_CHECK(esp_mcp_add_tool(mcp, tool));
    
    // Initialize and start MCP with HTTP transport
    esp_mcp_mgr_handle_t mcp_handle = 0;
    esp_mcp_mgr_config_t config = MCP_SERVER_DEFAULT_CONFIG();
    config.instance = mcp;
    
    ESP_ERROR_CHECK(esp_mcp_mgr_init(config, &mcp_handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(mcp_handle));
    
    ESP_LOGI(TAG, "MCP server started on port 80");
}
```

## 🔧 Core API

### Server Lifecycle

```c
// Create MCP server instance
esp_err_t esp_mcp_create(esp_mcp_t **server);

// Initialize MCP with transport configuration
esp_err_t esp_mcp_mgr_init(esp_mcp_mgr_config_t *config, esp_mcp_mgr_handle_t *handle);

// Start MCP server (starts HTTP server)
esp_err_t esp_mcp_mgr_start(esp_mcp_mgr_handle_t handle);

// Stop MCP server
esp_err_t esp_mcp_mgr_stop(esp_mcp_mgr_handle_t handle);

// Deinitialize MCP and cleanup resources
esp_err_t esp_mcp_mgr_deinit(esp_mcp_mgr_handle_t handle);

// Destroy MCP server and free all resources
esp_err_t esp_mcp_destroy(esp_mcp_t *mcp);
```

### Tool Registration

```c
// Create a tool
esp_mcp_tool_t *esp_mcp_tool_create(
    const char *name,
    const char *description,
    esp_mcp_tool_callback_t callback
);

// Add property to a tool
esp_err_t esp_mcp_tool_add_property(
    esp_mcp_tool_t *tool,
    esp_mcp_property_t *property
);

// Add tool to server
esp_err_t esp_mcp_add_tool(
    esp_mcp_t *mcp,
    esp_mcp_tool_t *tool
);

// Remove tool from server
esp_err_t esp_mcp_remove_tool(
    esp_mcp_t *mcp,
    esp_mcp_tool_t *tool
);
```

### Property Management

```c
// Create properties with different types
esp_mcp_property_t* esp_mcp_property_create_with_bool(const char* name, bool default_value);
esp_mcp_property_t* esp_mcp_property_create_with_int(const char* name, int default_value);
esp_mcp_property_t* esp_mcp_property_create_with_float(const char* name, float default_value);
esp_mcp_property_t* esp_mcp_property_create_with_string(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_array(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_object(const char* name, const char* default_value);

// Create property with range validation
esp_mcp_property_t* esp_mcp_property_create_with_range(const char* name, int min_value, int max_value);

// Create property with default value and range
esp_mcp_property_t* esp_mcp_property_create_with_int_and_range(
    const char* name, 
    int default_value, 
    int min_value, 
    int max_value
);

// Destroy a property
esp_err_t esp_mcp_property_destroy(esp_mcp_property_t* property);

// Get property values from list (thread-safe)
int esp_mcp_property_list_get_property_int(const esp_mcp_property_list_t* list, const char* name);
float esp_mcp_property_list_get_property_float(const esp_mcp_property_list_t* list, const char* name);
bool esp_mcp_property_list_get_property_bool(const esp_mcp_property_list_t* list, const char* name);
const char* esp_mcp_property_list_get_property_string(const esp_mcp_property_list_t* list, const char* name);
const char* esp_mcp_property_list_get_property_array(const esp_mcp_property_list_t* list, const char* name);
const char* esp_mcp_property_list_get_property_object(const esp_mcp_property_list_t* list, const char* name);
```

### Value Creation

```c
// Create MCP values with different types
esp_mcp_value_t esp_mcp_value_create_bool(bool value);
esp_mcp_value_t esp_mcp_value_create_int(int value);
esp_mcp_value_t esp_mcp_value_create_float(float value);
esp_mcp_value_t esp_mcp_value_create_string(const char* value);
```

## 📊 Examples

The component includes a complete example in `examples/mcp/mcp_server/` demonstrating:

- Wi-Fi connection setup
- MCP server initialization and configuration
- Tool registration with various parameter types
- Property validation (range constraints)
- Different data types (boolean, integer, string, array, object)
- Device status reporting
- Setting device parameters

### Running the Example

```bash
cd examples/mcp/mcp_server
idf.py set-target esp32
idf.py menuconfig  # Configure Wi-Fi credentials
idf.py build flash monitor
```

### Example Tools

The example implements several tools:

1. **get_device_status** - Get complete device status (audio, screen, etc.)
2. **audio.set_volume** - Set audio volume (0-100) with range validation
3. **screen.set_brightness** - Set screen brightness (0-100)
4. **screen.set_theme** - Set screen theme (light/dark)
5. **screen.set_hsv** - Set screen color in HSV format (array parameter)
6. **screen.set_rgb** - Set screen color in RGB format (object parameter)

## 🧪 Testing

Test your MCP server using any MCP-compatible client or with `curl`:

> **Note**: The request `id` field must be a **number** type. String or null IDs are not supported.

### List Available Tools

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

### Call a Tool

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

### Get Device Status

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

## 🔌 Transport Support

### Built-in Transport

- **HTTP**: Included by default (can be disabled via menuconfig)
  - JSON-RPC 2.0 over HTTP POST
  - Default endpoint: `/mcp`
  - Configurable port (default: 80)
  - **Request ID**: Only numeric IDs are supported (string or null IDs will be rejected)

### Custom Transport

The SDK supports custom transport implementations via the transport vtable `esp_mcp_transport_t`
provided in `esp_mcp_mgr_config_t.transport`.

## 📖 Documentation

- [User Guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/mcp/mcp-c-sdk.html)
- [API Reference](https://docs.espressif.com/projects/esp-iot-solution/en/latest/api-reference/mcp/index.html)

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📄 License

This project is licensed under the Apache License 2.0 - see the [LICENSE](license.txt) file for details.

## 🔗 Related Links

- [Model Context Protocol Specification](https://modelcontextprotocol.io/)
- [ESP-IDF](https://github.com/espressif/esp-idf)
- [ESP-IoT-Solution](https://github.com/espressif/esp-iot-solution)

## 🔒 Thread Safety

All list operations (tools and properties) are thread-safe and protected by mutex:

- **Tool List Operations**: All operations on tool lists are protected by mutex
  - Adding tools - Thread-safe
  - Finding tools - Thread-safe
  - All list operations - Thread-safe

- **Property List Operations**: All operations on property lists are protected by mutex
  - `esp_mcp_property_list_add_property()` - Thread-safe
  - All getter functions - Thread-safe
  - All list operations - Thread-safe

- **Thread Safety**: All list operations automatically use mutex protection. Direct access to internal list structures is not recommended.

## ❓ Q&A

**Q1: I encountered the following problems when using the package manager**

```
Executing action: create-project-from-example
CMakeLists.txt not found in project directory /home/username
```

**A1:** This is because an older version package manager was used. Please run `pip install -U idf-component-manager` in ESP-IDF environment to update.

**Q2: How do I disable HTTP transport?**

**A2:** You can disable HTTP transport via menuconfig: `Component config → MCP C SDK → Enable HTTP Transport`

**Q3: Can I use multiple transport protocols simultaneously?**

**A3:** Currently, only one transport can be active at a time. You need to choose either the built-in HTTP transport or implement a custom transport.

**Q4: Is the SDK thread-safe?**

**A4:** Yes! All list operations (tools and properties) are protected by mutex. The SDK is designed to be safe for use in multi-threaded environments. Always use the provided API functions instead of directly accessing internal list structures.

**Q5: How do I iterate over tools or properties safely?**

**A5:** All list operations are automatically thread-safe. The SDK uses mutex protection internally for all list access. For advanced use cases, refer to the internal API documentation.

**Q6: What types of request IDs are supported?**

**A6:** Only numeric (number) request IDs are supported. String IDs or null IDs in JSON-RPC requests will be rejected with an `INVALID_REQUEST` error. This is a limitation of the current implementation to simplify ID handling.

---

**Made with ❤️ for the ESP32 and AI community**
