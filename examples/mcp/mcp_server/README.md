# MCP Server Example

This example demonstrates how to implement a **Model Context Protocol (MCP)** server on ESP32, providing standardized tool interfaces for AI applications. The example showcases device status queries, volume control, brightness adjustment, theme switching, and color control through JSON-RPC 2.0 protocol.

## Features

* Device status query with JSON response
* Volume control (0-100) with range validation
* Screen brightness adjustment (0-100)
* Theme switching (light/dark)
* HSV color control (array format)
* RGB color control (object format)
* Resource registration and read (`resources/list`, `resources/read`)
* Prompt registration and rendering (`prompts/list`, `prompts/get`)
* Completion provider (`completion/complete`)
* Task-mode tool execution (`tools/call` with `params.task={}`, and `tasks/*` polling)
* Full JSON-RPC 2.0 support
* Built-in parameter validation
* Thread-safe tool and property list management with mutex protection
* HTTP transport support

## Hardware

* A development board with ESP32/ESP32-S2/ESP32-S3/ESP32-C3/ESP32-C6 SoC.
* USB cable for power supply and programming
* Wi-Fi router for network connectivity

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Configure the Project

Open the project configuration menu:

```bash
idf.py menuconfig
```

In the `Example Connection Configuration` menu:
* Set Wi-Fi SSID
* Set Wi-Fi Password

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

After flashing the firmware, you will see the device connect to Wi-Fi and start the MCP server:

```
I (2543) wifi:connected with MySSID, aid = 1, channel 6, BW20, bssid = xx:xx:xx:xx:xx:xx
I (3542) example_connect: - IPv4 address: 192.168.1.100
I (3552) mcp_server: MCP Server started on port 80
I (3553) mcp_server: MCP endpoint registered: /mcp_server
```

## Testing the MCP Server

Once the server is running, you can test it using curl or any HTTP client.

### 1. Get Available Tools List

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/list",
    "params": {}
  }'
```

Response will include all registered tools with their descriptions and input schemas.

### 2. Get Device Status

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "tools/call",
    "params": {
      "name": "self.get_device_status",
      "arguments": {}
    }
  }'
```

### 3. Set Volume

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "tools/call",
    "params": {
      "name": "self.audio_speaker.set_volume",
      "arguments": {
        "volume": 75
      }
    }
  }'
```

### 4. Set Screen Brightness

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"self.screen.set_brightness","arguments":{"brightness":60}}}'
```

### 5. Set Screen Theme

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"self.screen.set_theme","arguments":{"theme":"dark"}}}'
```

### 6. Set HSV Color

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"self.screen.set_hsv","arguments":{"HSV":[180,50,80]}}}'
```

### 7. Set RGB Color

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"self.screen.set_rgb","arguments":{"RGB":{"red":255,"green":128,"blue":0}}}}'
```

### 8. List Resources

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":8,"method":"resources/list","params":{}}'
```

### 9. Read Resource

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":9,"method":"resources/read","params":{"uri":"device://status"}}'
```

### 10. List Resource Templates

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":10,"method":"resources/templates/list","params":{}}'
```

### 11. Subscribe Resource Updates

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":11,"method":"resources/subscribe","params":{"uri":"device://status"}}'
```

### 12. Unsubscribe Resource Updates

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":12,"method":"resources/unsubscribe","params":{"uri":"device://status"}}'
```

### 13. List Prompts

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":13,"method":"prompts/list","params":{}}'
```

### 14. Get Prompt

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":14,"method":"prompts/get","params":{"name":"status.summary","arguments":{"device":"screen"}}}'
```

### 15. Completion

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":15,"method":"completion/complete","params":{"ref":{"type":"ref/prompt","name":"status.summary"},"argument":{"name":"device","value":"s"}}}'
```

### 16. Task-mode Tool Call + Polling

```bash
# Start async task (returns task metadata in result.task)
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":16,"method":"tools/call","params":{"name":"self.get_device_status","arguments":{},"task":{}}}'

# Query task list
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":17,"method":"tasks/list","params":{}}'

# Query task status/result by task id
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":18,"method":"tasks/get","params":{"taskId":"<task_id>"}}'

curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":19,"method":"tasks/result","params":{"taskId":"<task_id>"}}'

# Note: tasks/result returns the underlying tool result object (e.g. content/isError),
# so it does not repeat taskId in the response payload.

# Cancel task
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":20,"method":"tasks/cancel","params":{"taskId":"<task_id>"}}'
```

`notifications/resources/updated` are server notifications delivered over Streamable HTTP SSE.  
`notifications/resources/list_changed` is emitted when the resource registry changes (for example, resource add/remove).  
In this example, `self.resource.touch_status` intentionally triggers remove+add to produce both notifications.  
Resource subscription is session-scoped. Use the same `MCP-Session-Id` on POST and SSE requests to observe updates for that session.  
When SSE is not enabled (GET returns 405), you can still test `resources/subscribe` / `resources/unsubscribe` request-response behavior.

### 17. Open SSE stream (GET)

```bash
curl -N http://192.168.1.100/mcp_server \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: 2025-11-25"
```

### 17.1 SSE-only endpoint (/mcp_server_sse)

When SSE is enabled, the server also registers a SSE-only endpoint: `/mcp_server_sse`.  
All POST requests must include `MCP-Session-Id` (from the SSE response headers).  
The HTTP layer returns 202/empty body, and JSON-RPC responses are delivered via SSE events.

```bash
# 1) Open SSE stream and capture MCP-Session-Id from response headers
curl -N -D /tmp/mcp_sse.hdr http://192.168.1.100/mcp_server_sse \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: 2025-11-25"

# 2) Use session id for POST (response arrives via SSE)
SESSION_ID="$(sed -nE 's/^MCP-Session-Id:[[:space:]]*([^[:space:]\r]+).*/\1/ip' /tmp/mcp_sse.hdr | sed -n '1p' | tr -d '\r')"
curl -i -X POST http://192.168.1.100/mcp_server_sse \
  -H "Content-Type: application/json" \
  -H "MCP-Protocol-Version: 2025-11-25" \
  -H "MCP-Session-Id: ${SESSION_ID}" \
  -d '{"jsonrpc":"2.0","id":1,"method":"ping","params":{}}'
```

### 18. End-to-end SSE observable test script

```bash
# default run
bash test_sse_e2e.sh 192.168.1.100 mcp_server

# print each JSON-RPC response
bash test_sse_e2e.sh 192.168.1.100 mcp_server --verbose

# keep SSE log file after script exits
bash test_sse_e2e.sh 192.168.1.100 mcp_server --keep-log

# strict mode: assert key fields in responses
bash test_sse_e2e.sh 192.168.1.100 mcp_server --strict
```

> **Note:** This script targets Streamable HTTP (POST returns JSON). Use it with `/mcp_server`.  
> `/mcp_server_sse` is SSE-only; POST returns 202 and responses arrive on the SSE stream.

This script is a full smoke test for the example and covers:
- `initialize` + `notifications/initialized`
- `ping` (including string `id`)
- `tools/list`
- `tools/call` for all example tools:
  - `self.get_device_status`
  - `self.audio_speaker.set_volume`
  - `self.screen.set_brightness`
  - `self.screen.set_theme`
  - `self.screen.set_rgb`
  - `self.screen.set_hsv`
  - `self.resource.touch_status`
- `resources/list`, `resources/read`, `resources/templates/list`, `resources/subscribe`, `resources/unsubscribe`
- `prompts/list`, `prompts/get`
- `completion/complete`
- task mode (`tools/call` with `task={}`), `tasks/get`, `tasks/result`, `tasks/cancel`
- JSON-RPC batch request handling (mixed request + notification + error)
- JSON-RPC error envelope checks with `error.data` (e.g. `method`, `uri`, `expected`)
- SSE checks for `notifications/resources/updated` and `notifications/resources/list_changed`
- SSE heartbeat comment (`: ping`) and reconnect flow with `MCP-Session-Id` + `Last-Event-ID`

`--strict` additionally validates key fields in response payloads, such as:
- `resources/templates/list` must include `device://sensors/{id}`
- `prompts/list` must include `status.summary`
- task mode response must include `taskId`
- batch/error responses must include expected IDs/codes and `error.data`
- reconnect stream must still observe notifications

> **Note:** strict mode waits for SSE heartbeat, so runtime is longer (about 15-20s extra by default).

**Note:** The endpoint name (`/mcp_server`) is set via `esp_mcp_mgr_register_endpoint()` in the example code. You can customize it to any path you prefer.  
When SSE is enabled, `/mcp_server_sse` is also provided as the SSE-only endpoint (POST responses delivered over SSE).

## Registered Tools

| Tool Name | Parameters | Description |
|-----------|------------|-------------|
| `self.get_device_status` | None | Get device status in JSON format |
| `self.audio_speaker.set_volume` | `volume` (0-100) | Set audio volume |
| `self.screen.set_brightness` | `brightness` (0-100) | Set screen brightness |
| `self.screen.set_theme` | `theme` (string) | Set screen theme |
| `self.screen.set_hsv` | `HSV` (array[3]) | Set HSV color |
| `self.screen.set_rgb` | `RGB` (object) | Set RGB color |
| `self.resource.touch_status` | None | Trigger `notifications/resources/updated` for SSE testing |
| `self.server.roots_list` | None | Emit server-initiated `roots/list` request to current session |
| `self.server.sampling_create` | None | Emit server-initiated `sampling/create` request to current session |
| `self.server.elicitation_request` | None | Emit server-initiated `elicitation/request` request to current session |

## Registered Resource / Prompt / Completion

| Type | Name/URI | Description |
|------|----------|-------------|
| Resource | `device://status` | Returns current simulated device status as JSON |
| Resource Template | `device://sensors/{id}` | Template URI for per-sensor data |
| Prompt | `status.summary` | Generates a user prompt for status summarization |
| Completion | argument `theme` / `device` | Returns suggested completion values |

## Property Types

The example demonstrates different property types supported by the MCP SDK:

```c
// Integer with range validation (min, max)
esp_mcp_property_t *property = esp_mcp_property_create_with_range("volume", 0, 100);
esp_mcp_tool_add_property(tool, property);

// Integer with default value
property = esp_mcp_property_create_with_int("param", 10);
esp_mcp_tool_add_property(tool, property);

// Boolean with default value
property = esp_mcp_property_create_with_bool("enabled", true);
esp_mcp_tool_add_property(tool, property);

// String with default value
property = esp_mcp_property_create_with_string("theme", "light");
esp_mcp_tool_add_property(tool, property);

// Array with default value (JSON array string)
property = esp_mcp_property_create_with_array("HSV", "[0, 100, 360]");
esp_mcp_tool_add_property(tool, property);

// Object with default value (JSON object string)
property = esp_mcp_property_create_with_object("RGB", "{\"red\":0,\"green\":120,\"blue\":240}");
esp_mcp_tool_add_property(tool, property);
```

**Note:** Properties are added to tools using `esp_mcp_tool_add_property()`, and the tool is then registered with the server using `esp_mcp_add_tool()`.

## Troubleshooting

* **Wi-Fi Connection Failed**: Check Wi-Fi credentials in `menuconfig` and ensure the device is within Wi-Fi range
* **Cannot Access MCP Server**: Verify device IP address and ensure client is on the same network
* **Tool Call Returns Error**: Validate JSON-RPC format and parameter values

## Transport Support

### HTTP Transport

The example uses HTTP transport by default. The MCP server runs as an HTTP server that accepts JSON-RPC 2.0 requests via HTTP POST.

**Configuration:**

```c
httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
esp_mcp_mgr_config_t mcp_mgr_config = {
    .transport = esp_mcp_transport_http_server,
    .config = &http_config,
    .instance = mcp,
};
ESP_ERROR_CHECK(esp_mcp_mgr_init(mcp_mgr_config, &mcp_mgr_handle));
ESP_ERROR_CHECK(esp_mcp_mgr_start(mcp_mgr_handle));
ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(mcp_mgr_handle, "mcp_server", NULL));
```

**Default endpoint:** `/mcp_server` (configurable via `esp_mcp_mgr_register_endpoint()`)

**Default port:** 80 (configurable via `httpd_config_t`)

### Custom Transport

The MCP SDK supports custom transport implementations. Refer to the [MCP SDK documentation](../../../components/mcp-c-sdk/README.md) for details on implementing custom transports.

## Adding Custom Tools

To add a new tool, follow these steps:

1. **Define a callback function** that handles the tool execution:

```c
static esp_mcp_value_t my_tool_callback(const esp_mcp_property_list_t* properties)
{
    int value = esp_mcp_property_list_get_property_int(properties, "param");
    ESP_LOGI(TAG, "Tool called with param: %d", value);
    return esp_mcp_value_create_bool(true);
}
```

2. **Create the tool** with name, description, and callback:

```c
esp_mcp_tool_t *tool = esp_mcp_tool_create("my_tool", "Tool description", my_tool_callback);
```

3. **Add properties** to the tool (optional):

```c
esp_mcp_property_t *property = esp_mcp_property_create_with_int("param", 10);
esp_mcp_tool_add_property(tool, property);
```

4. **Register the tool** with the server:

```c
esp_mcp_add_tool(mcp, tool);
```

**Complete example:**

```c
// Define callback
static esp_mcp_value_t my_tool_callback(const esp_mcp_property_list_t* properties)
{
    int value = esp_mcp_property_list_get_property_int(properties, "param");
    ESP_LOGI(TAG, "Tool called with param: %d", value);
    return esp_mcp_value_create_bool(true);
}

// In app_main(), after creating the MCP engine:
esp_mcp_tool_t *tool = esp_mcp_tool_create("my_tool", "My custom tool description", my_tool_callback);
esp_mcp_property_t *property = esp_mcp_property_create_with_int("param", 10);
esp_mcp_tool_add_property(tool, property);
esp_mcp_add_tool(mcp, tool);
```

## Technical References

* [Model Context Protocol Specification](https://modelcontextprotocol.io/)
* [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
* [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
