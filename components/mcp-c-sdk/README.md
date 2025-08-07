# ESP32 MCP C SDK

[![Component Registry](https://components.espressif.com/components/espressif/mcp-c-sdk/badge.svg)](https://components.espressif.com/components/espressif/mcp-c-sdk)
[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.0%2B-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

**English** | [中文](README_CN.md)

A comprehensive C SDK implementing the **Model Context Protocol (MCP)** server for ESP32 devices, providing a standardized way to integrate AI applications with ESP32 devices. This component enables your ESP32 to expose tools and capabilities that can be discovered and used by AI agents and applications.

## 🌟 Features

- **🚀 Clean API**: Intuitive interface for tool registration and management
- **🔧 Dynamic Registration**: Register tools at runtime with flexible property schemas
- **📦 Modular Design**: Standalone component, easy to integrate into existing projects
- **🌐 HTTP Transport**: Built-in HTTP-based JSON-RPC 2.0 for maximum compatibility
- **🔌 Custom Transport**: Support for custom transport implementations through callbacks
- **📊 Type Safety**: Comprehensive data type support (boolean, integer, float, string, array, object)
- **🛡️ Memory Safe**: Automatic memory management and cleanup
- **✅ Parameter Validation**: Built-in parameter validation with range constraints
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
#include "esp_mcp_server.h"
#include "esp_mcp.h"

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
    // Initialize WiFi (using example_connect)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    
    // Create MCP server
    esp_mcp_server_t *mcp_server = NULL;
    ESP_ERROR_CHECK(esp_mcp_server_create(&mcp_server));
    
    // Create property list with volume parameter
    esp_mcp_property_list_t *properties = esp_mcp_property_list_create();
    
    // Add volume property with range validation (0-100)
    esp_mcp_property_list_add_property(properties, 
        esp_mcp_property_create_with_range("volume", 0, 100));
    
    // Register tool with callback
    esp_mcp_server_add_tool_with_callback(
        mcp_server, 
        "audio.set_volume",
        "Set audio speaker volume (0-100)",
        properties, 
        set_volume_callback
    );
    
    // Initialize and start MCP with HTTP transport
    esp_mcp_handle_t mcp_handle = 0;
    esp_mcp_config_t config = MCP_SERVER_DEFAULT_CONFIG();
    config.instance = mcp_server;
    
    ESP_ERROR_CHECK(esp_mcp_init(&config, &mcp_handle));
    ESP_ERROR_CHECK(esp_mcp_start(mcp_handle));
    
    ESP_LOGI(TAG, "MCP server started on port 80");
}
```

## 🔧 Core API

### Server Lifecycle

```c
// Create MCP server instance
esp_err_t esp_mcp_server_create(esp_mcp_server_t **server);

// Destroy MCP server and free all resources
esp_err_t esp_mcp_server_destroy(esp_mcp_server_t *server);

// Initialize MCP with transport configuration
esp_err_t esp_mcp_init(esp_mcp_config_t *config, esp_mcp_handle_t *handle);

// Start MCP server (starts HTTP server)
esp_err_t esp_mcp_start(esp_mcp_handle_t handle);

// Stop MCP server
esp_err_t esp_mcp_stop(esp_mcp_handle_t handle);

// Deinitialize MCP and cleanup resources
esp_err_t esp_mcp_deinit(esp_mcp_handle_t handle);
```

### Tool Registration

```c
// Register tool with callback function
esp_err_t esp_mcp_server_add_tool_with_callback(
    esp_mcp_server_t *server,
    const char *name,
    const char *description,
    esp_mcp_property_list_t *properties,
    esp_mcp_tool_callback_t callback
);
```

### Property Management

```c
// Create property list
esp_mcp_property_list_t* esp_mcp_property_list_create(void);

// Create properties with different types
esp_mcp_property_t* esp_mcp_property_create_with_bool(const char* name, bool default_value);
esp_mcp_property_t* esp_mcp_property_create_with_int(const char* name, int default_value);
esp_mcp_property_t* esp_mcp_property_create_with_float(const char* name, float default_value);
esp_mcp_property_t* esp_mcp_property_create_with_string(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_array(const char* name, const char* default_value);
esp_mcp_property_t* esp_mcp_property_create_with_object(const char* name, const char* default_value);

// Create property with range validation
esp_mcp_property_t* esp_mcp_property_create_with_range(const char* name, int min_value, int max_value);

// Add property to property list
esp_err_t esp_mcp_property_list_add_property(
    esp_mcp_property_list_t* list,
    esp_mcp_property_t* property
);

// Get property values from list
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

- WiFi connection setup
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
idf.py menuconfig  # Configure WiFi credentials
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

### Custom Transport

The SDK supports custom transport implementations through callback functions:

```c
typedef struct {
    uint32_t transport;
    int (*open)(esp_mcp_handle_t handle, esp_mcp_transport_config_t *config);
    int (*read)(esp_mcp_handle_t handle, char *buffer, int len, int timeout_ms);
    int (*write)(esp_mcp_handle_t handle, const char *buffer, int len, int timeout_ms);
    int (*close)(esp_mcp_handle_t handle);
} esp_mcp_transport_funcs_t;

// Set custom transport functions
esp_err_t esp_mcp_transport_set_funcs(esp_mcp_handle_t handle, 
                                     esp_mcp_transport_funcs_t funcs);
```

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

---

**Made with ❤️ for the ESP32 and AI community**
