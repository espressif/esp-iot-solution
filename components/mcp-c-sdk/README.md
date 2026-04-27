# ESP32 MCP C SDK

[![Component Registry](https://components.espressif.com/components/espressif/mcp-c-sdk/badge.svg)](https://components.espressif.com/components/espressif/mcp-c-sdk)
[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.4%2B-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

**English** | [中文](README_CN.md)

A comprehensive C SDK implementing the **Model Context Protocol (MCP)** for ESP32 devices, providing a standardized way to integrate AI applications with ESP32 devices. This component enables your ESP32 to expose tools and capabilities that can be discovered and used by AI agents and applications.

## 📋 Protocol & Compatibility

- **MCP Protocol Version (default target)**: `2025-11-25`
- **Legacy compatibility**: `2024-11-05` HTTP transport is still available (Kconfig-selected)
- **JSON-RPC Version**: `2.0`
- **Supported Methods**:
  - `initialize` - Initialize MCP session and negotiate capabilities
  - `tools/list` - List available tools with optional cursor-based pagination and optional `params.limit` (capped at 128)
  - `tools/call` - Execute a tool with provided arguments (supports task-augmented mode via `params.task`)
  - `resources/list`, `resources/read` - Resource discovery and reading
  - `prompts/list`, `prompts/get` - Prompt discovery and retrieval
  - `completion/complete` - Argument completion (optional provider)
  - `logging/setLevel` - Configure server-side log level for notifications
  - `tasks/list`, `tasks/get`, `tasks/result`, `tasks/cancel` - Experimental tasks API (async tool execution)
  - `ping` - Health check endpoint (MCP 2025: returns `{}`)
- **Supported Capabilities**:
  - ✅ **Tools**: Full support for tool registration, listing, and execution
  - ✅ **Resources / Prompts / Completions**: Server-side modules and methods
  - ✅ **Tasks (experimental)**: Async tool calls with polling
  - ✅ **Cursor-based Pagination**: Support for paginated tool lists
  - ✅ **Parameter Validation**: Built-in validation with type checking and range constraints
  - ⚠️ **Streamable HTTP** (MCP 2025-11-25): POST is supported; GET is currently 405 when SSE is not enabled
  - ⚠️ **Authorization (resource server)**: OAuth resource-server metadata endpoints are not fully implemented yet

### Capability Matrix (Manager + Engine)

| Area | Inbound (server-side handling) | Outbound (client-side request helpers) |
| --- | --- | --- |
| Lifecycle | `initialize`, `notifications/initialized` gating (protocol 2025) | `esp_mcp_mgr_post_info_init`, auto `notifications/initialized` when required |
| Tools | `tools/list`, `tools/call` (+task hints) | `esp_mcp_mgr_post_tools_list` (`u.list.cursor`; `u.list.limit` if ≥0 adds `params.limit`, `-1` omits it), `esp_mcp_mgr_post_tools_call` |
| Resources | `resources/list`, `resources/read`, `resources/subscribe`, `resources/unsubscribe`, `resources/templates/list`, list/update notifications | `esp_mcp_mgr_post_resources_list`, `esp_mcp_mgr_post_resources_read`, `esp_mcp_mgr_post_resources_subscribe`, `esp_mcp_mgr_post_resources_unsubscribe`, `esp_mcp_mgr_post_resources_templates_list` (cursor + limit) |
| Prompts | `prompts/list`, `prompts/get` | `esp_mcp_mgr_post_prompts_list` (cursor + limit), `esp_mcp_mgr_post_prompts_get` |
| Completion | `completion/complete` | `esp_mcp_mgr_post_completion_complete` |
| Logging | `logging/setLevel`, `notifications/message` | `esp_mcp_mgr_post_logging_set_level` |
| Tasks (exp.) | `tasks/list/get/result/cancel`, task status notifications | `esp_mcp_mgr_post_tasks_list/get/result/cancel` |
| Generic JSON-RPC | `esp_mcp_mgr_req_handle` / manager dispatch | `esp_mcp_mgr_post_request_json`, `esp_mcp_mgr_post_notification_json`, `esp_mcp_mgr_post_ping` |

## 🌟 Features

- **🚀 Clean API**: Intuitive interface for tool registration and management
- **🔧 Dynamic Registration**: Register tools at runtime with flexible property schemas
- **📦 Modular Design**: Standalone component, easy to integrate into existing projects
- **🌐 HTTP Transport**: Built-in HTTP server/client transports for JSON-RPC 2.0
- **📡 Streamable HTTP (MCP 2025-11-25)**: Incremental support; current server path focuses on HTTP POST JSON-RPC
- **🔐 OAuth Resource Server Auth (optional)**: Planned/partial support
- **🔌 Custom Transport**: Support for custom transport implementations through callbacks
- **📊 Type Safety**: Comprehensive data type support (boolean, integer, float, string, array, object)
- **🛡️ Memory Safe**: Automatic memory management and cleanup
- **✅ Parameter Validation**: Built-in parameter validation with range constraints
- **🔒 Thread Safe**: All list operations are protected by mutex for multi-threaded environments
- **🎯 MCP Compliant**: Fully compliant with MCP specification

## 🧱 Architecture & Naming (3 Layers)

This SDK follows a **3-layer unified naming architecture**:

- **Manager/Router layer**: `esp_mcp_mgr_*` (init/start/stop, endpoint routing, transport integration)
- **Transport layer**: `esp_mcp_transport_*` (specific transports like HTTP server/client)
- **Engine layer**: `esp_mcp_*` (protocol semantics + tool scheduling: JSON-RPC, initialize, tools/list, tools/call)

## 📡 MCP Client (Outbound)

When ESP acts as an **MCP client** to call a remote MCP server:

- Use **manager outbound API**: `esp_mcp_mgr_post_info_init()`, `esp_mcp_mgr_post_tools_list()`, `esp_mcp_mgr_post_tools_call()`
- Use **built-in HTTP client transport**: `esp_mcp_transport_http_client` (defined in `esp_mcp_mgr.h`)

The manager layer routes requests through Transport implementations to the Engine layer for protocol handling. For client usage, you can either:
- Use the high-level `esp_mcp_mgr_post_*` functions which build and send complete MCP requests automatically
- Use `esp_mcp_mgr_perform_handle()` directly if you need to send custom JSON-RPC requests (requires building the JSON yourself)

### Client lifecycle note (`initialize` -> `notifications/initialized`)

When using outbound manager helpers:

1. Send `initialize` via `esp_mcp_mgr_post_info_init()`.
2. Wait for the initialize response callback before issuing normal business requests.
3. For negotiated protocol versions newer than `2024-11-05`, manager automatically sends `notifications/initialized` once.
4. If application code manually sends `notifications/initialized` in the initialize callback, manager skips duplicate auto-send.

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

### Resources / Prompts / Completion / Tasks

```c
#include "esp_mcp_resource.h"
#include "esp_mcp_prompt.h"
#include "esp_mcp_completion.h"

// 1) Register a resource (resources/list, resources/read)
static esp_err_t read_device_info(const char *uri, char **out_mime, char **out_text, char **out_blob, void *ctx)
{
    (void)uri;
    (void)ctx;
    *out_mime = strdup("application/json");
    *out_text = strdup("{\"model\":\"esp32\",\"fw\":\"1.0.0\"}");
    *out_blob = NULL;
    if (!*out_mime || !*out_text) {
        if (*out_mime) {
          free(*out_mime);
        }

        if (*out_text) {
          free(*out_text);
        }
        *out_mime = NULL;
        *out_text = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_mcp_resource_t *res = esp_mcp_resource_create(
    "device://info", "device.info", "Device Info",
    "Basic device metadata", "application/json",
    read_device_info, NULL);
ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, res));

// 2) Register a prompt (prompts/list, prompts/get)
static esp_err_t render_prompt(const char *args_json, char **out_desc, char **out_msgs, void *ctx)
{
    (void)args_json;
    (void)ctx;
    *out_desc = strdup("Health-check prompt");
    *out_msgs = strdup("[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"Check system health\"}}]");
    if (!*out_desc || !*out_msgs) {
        if (*out_desc) {
          free(*out_desc);
        }

        if (*out_msgs) {
          free(*out_msgs);
        }
        *out_desc = NULL;
        *out_msgs = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_mcp_prompt_t *prompt = esp_mcp_prompt_create(
    "health.check", "Health Check", "Prompt for health check",
    NULL, render_prompt, NULL);
ESP_ERROR_CHECK(esp_mcp_add_prompt(mcp, prompt));

// 3) Register completion provider (completion/complete)
static esp_err_t complete_arg(const char *ref_type, const char *name_or_uri,
                              const char *arg_name, const char *arg_value,
                              const char *ctx_args_json, cJSON **out_result, void *ctx)
{
    (void)ref_type; (void)name_or_uri; (void)arg_name; (void)arg_value; (void)ctx_args_json; (void)ctx;
    *out_result = cJSON_Parse("{\"completion\":{\"values\":[\"low\",\"medium\",\"high\"],\"hasMore\":false}}");
    return *out_result ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_mcp_completion_provider_t *provider = esp_mcp_completion_provider_create(complete_arg, NULL);
ESP_ERROR_CHECK(esp_mcp_set_completion_provider(mcp, provider));

// 4) Task APIs are built in server-side engine and used by tools/call with params.task=true:
//    tasks/list, tasks/get, tasks/result, tasks/cancel
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

> **Note**: JSON-RPC request `id` supports **number** and **string** types. `null` IDs are invalid for requests.

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

- **HTTP Server transport**: Controlled by `MCP_TRANSPORT_HTTP_SERVER`
  - JSON-RPC 2.0 over HTTP POST
  - MCP endpoint also registers GET and returns 405 when SSE is unavailable
  - Default endpoint: `/mcp`
  - Configurable port (default: 80)
  - Request `id`: supports number/string (null is invalid for requests)
  - Optional Bearer auth: `MCP_HTTP_AUTH_ENABLE` + `MCP_HTTP_AUTH_BEARER_TOKEN`
  - If `CONFIG_MCP_HTTP_AUTH_JWT_VERIFY_ENABLE` is disabled, Bearer token must exactly match `CONFIG_MCP_HTTP_AUTH_BEARER_TOKEN`
  - OAuth protected-resource metadata: `/.well-known/oauth-protected-resource` (`MCP_HTTP_OAUTH_METADATA_ENABLE`)
- **Streamable HTTP (server) — behavior matrix** (implementation as of `esp_mcp_http_server.c`; excludes OAuth flow details):

  For an MCP endpoint registered as `ep_name` (e.g. `mcp`), the server registers:

  | Path | Methods | When registered |
  | --- | --- | --- |
  | `/{ep_name}` | POST, GET, HEAD, DELETE, OPTIONS (if `CONFIG_MCP_HTTP_CORS_ENABLE`) | Always (with HTTP server transport) |
  | `/{ep_name}_sse` | POST, GET, HEAD, DELETE, OPTIONS (if CORS + SSE) | Only if `CONFIG_MCP_HTTP_SSE_ENABLE` |

  - **HEAD** `/{ep_name}` (and `_sse`): **`204 No Content`** with **`Allow`** listing supported methods (includes **`OPTIONS`** when CORS is enabled); runs the same Origin, auth, and **`MCP-Protocol-Version`** rules as other methods (no SSE body).

  - **`CONFIG_MCP_HTTP_CORS_ENABLE`**: `OPTIONS` answers browser preflight; when `Origin` is present and matches `Host` (same check as DNS-rebinding mitigation), successful **POST / GET / DELETE** responses include `Access-Control-Allow-Origin` and `Access-Control-Expose-Headers` listing **`MCP-Session-Id`** and **`MCP-Protocol-Version`** so browser clients can read them. **SSE (`GET` stream)** responses apply the same when CORS is enabled.

  **POST `/{ep_name}` (primary JSON-RPC)**

  - `Content-Type` must include `application/json`.
  - `Accept` negotiation (aligned with common Streamable HTTP hosts; mismatches use **`406 Not Acceptable`** with a short plain-text reason, not `400`):
    - If `CONFIG_MCP_HTTP_SSE_ENABLE` is **off**: omitting `Accept` is OK; if the header is present, it must allow `application/json` (wildcards such as `*/*` count), otherwise **`406`**.
    - If **on**: omitting `Accept` is OK; if present, it must include **`application/json` and/or `text/event-stream`**, otherwise **`406`**. After the body is parsed, **`Accept: text/event-stream` only** (no JSON) is allowed **only** for `initialize` (unified-path SSE upgrade); for any other method it is **`406`**.
  - Typical success: `200` with `application/json` body; response may include `MCP-Session-Id`, `MCP-Protocol-Version`, `Connection: keep-alive`.
  - If the engine returns no serialized response buffer: `202 Accepted` with an empty body (still with session headers when applicable).
  - Request body size is capped (see `MAX_CONTENT_LEN` in the transport source, currently 1 MiB).

  **POST `/{ep_name}_sse` (SSE build only)**

  - “SSE-only” POST: relaxed `Accept` — if the header is **omitted**, JSON responses are assumed; if present, it must include `application/json` or `text/event-stream`.
  - Requires an existing session: `MCP-Session-Id` header or `sessionId` query parameter.
  - When the JSON-RPC handler returns a payload, it is **delivered to the active SSE connection** for that session; the HTTP response is **`202 Accepted`** with an **empty** body. If there is no connected SSE client, **`503`** with plain text `No active SSE client`.

  **GET `/{ep_name}` or `/{ep_name}_sse`**

  - If `CONFIG_MCP_HTTP_SSE_ENABLE` is **off**: **`405`** with body `SSE stream is not enabled`.
  - If **on**: `Accept` must include `text/event-stream`, otherwise **`406`**.
  - Opens a long-lived SSE stream (async handler); supports **`Last-Event-ID`** for replay where configured (`MCP_HTTP_SSE_REPLAY_DEPTH`, etc.). Failure to start async handling → **`503`**.

  **DELETE `/{ep_name}` or `/{ep_name}_sse`**

  - Requires `MCP-Session-Id`. Removes server session state, drops SSE history for that session, triggers disconnect of SSE sockets. **`404`** if the session is unknown; **`200`** on success.

  - Clients may still send `Accept: application/json, text/event-stream` on every request for maximum interoperability with proxies and future behavior.

- **HTTP Client transport**: Controlled by `MCP_TRANSPORT_HTTP_CLIENT`
  - Outbound MCP requests over HTTP POST
  - Sends `Accept: application/json, text/event-stream`
  - Sends `MCP-Protocol-Version` header by default

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

**Q2: How do I disable built-in HTTP transports?**

**A2:** You can disable them via menuconfig:
- `Component config → MCP C SDK → Enable built-in HTTP server transport`
- `Component config → MCP C SDK → Enable built-in HTTP client transport`

**Q3: Can I use multiple transport protocols simultaneously?**

**A3:** Currently, only one transport can be active at a time. You need to choose either the built-in HTTP transport or implement a custom transport.

**Q4: Is the SDK thread-safe?**

**A4:** Yes! All list operations (tools and properties) are protected by mutex. The SDK is designed to be safe for use in multi-threaded environments. Always use the provided API functions instead of directly accessing internal list structures.

**Q5: How do I iterate over tools or properties safely?**

**A5:** All list operations are automatically thread-safe. The SDK uses mutex protection internally for all list access. For advanced use cases, refer to the internal API documentation.

**Q6: What types of request IDs are supported?**

**A6:** Request IDs support both numeric (`number`) and string (`string`) types. `null` request IDs are invalid and will be rejected as `INVALID_REQUEST`.

---

**Made with ❤️ for the ESP32 and AI community**
