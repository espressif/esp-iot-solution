# MCP Server 示例

本示例展示如何在 ESP32 上实现 **Model Context Protocol (MCP)** 服务端，为 AI 应用提供标准化工具接口。示例覆盖设备状态查询、音量控制、亮度调节、主题切换、颜色控制，以及四模块能力（Resource / Prompt / Completion / Tasks）。

## 功能特性

* 设备状态查询（JSON 返回）
* 音量控制（0-100）及范围校验
* 屏幕亮度调节（0-100）
* 主题切换（light/dark）
* HSV 颜色控制（数组参数）
* RGB 颜色控制（对象参数）
* 资源注册与读取（`resources/list`, `resources/read`）
* Prompt 注册与渲染（`prompts/list`, `prompts/get`）
* 参数补全 Provider（`completion/complete`）
* 任务模式工具调用（`tools/call` + `params.task={}`，并可用 `tasks/*` 轮询）
* 完整 JSON-RPC 2.0 支持
* 内置参数校验
* 工具与属性链表的线程安全管理（mutex 保护）
* HTTP 传输支持

## 硬件要求

* 搭载 ESP32/ESP32-S2/ESP32-S3/ESP32-C3/ESP32-C6 SoC 的开发板
* USB 数据线（供电与烧录）
* Wi-Fi 路由器

## 使用方法

在配置和构建前，请先设置正确芯片目标：`idf.py set-target <chip_name>`。

### 配置工程

打开 menuconfig：

```bash
idf.py menuconfig
```

在 `Example Connection Configuration` 菜单下：
* 设置 Wi-Fi SSID
* 设置 Wi-Fi Password

### 编译与烧录

```bash
idf.py -p PORT flash monitor
```

（退出串口监视器请按 `Ctrl-]`）

完整构建流程可参考 [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)。

## 运行输出示例

烧录后，设备联网并启动 MCP 服务端：

```text
I (2543) wifi:connected with MySSID, aid = 1, channel 6, BW20, bssid = xx:xx:xx:xx:xx:xx
I (3542) example_connect: - IPv4 address: 192.168.1.100
I (3552) mcp_server: MCP Server started on port 80
I (3553) mcp_server: MCP endpoint registered: /mcp_server
```

## 服务端测试

服务端启动后，可用 `curl` 或任意 HTTP 客户端测试。

### 1. 获取工具列表

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

### 2. 获取设备状态

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

### 3. 设置音量

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

### 4. 设置亮度

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"self.screen.set_brightness","arguments":{"brightness":60}}}'
```

### 5. 设置主题

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"self.screen.set_theme","arguments":{"theme":"dark"}}}'
```

### 6. 设置 HSV

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"self.screen.set_hsv","arguments":{"HSV":[180,50,80]}}}'
```

### 7. 设置 RGB

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"self.screen.set_rgb","arguments":{"RGB":{"red":255,"green":128,"blue":0}}}}'
```

### 8. 列出资源

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":8,"method":"resources/list","params":{}}'
```

### 9. 读取资源

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":9,"method":"resources/read","params":{"uri":"device://status"}}'
```

### 10. 列出资源模板

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":10,"method":"resources/templates/list","params":{}}'
```

### 11. 订阅资源更新

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":11,"method":"resources/subscribe","params":{"uri":"device://status"}}'
```

### 12. 取消订阅资源更新

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":12,"method":"resources/unsubscribe","params":{"uri":"device://status"}}'
```

### 13. 列出 Prompt

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":13,"method":"prompts/list","params":{}}'
```

### 14. 获取 Prompt

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":14,"method":"prompts/get","params":{"name":"status.summary","arguments":{"device":"screen"}}}'
```

### 15. 参数补全

```bash
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":15,"method":"completion/complete","params":{"ref":{"type":"ref/prompt","name":"status.summary"},"argument":{"name":"device","value":"s"}}}'
```

### 16. 任务模式工具调用与轮询

```bash
# 启动异步任务（返回值在 result.task 中包含任务信息）
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":16,"method":"tools/call","params":{"name":"self.get_device_status","arguments":{},"task":{}}}'

# 查询任务列表
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":17,"method":"tasks/list","params":{}}'

# 按 task id 查询状态和结果
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":18,"method":"tasks/get","params":{"taskId":"<task_id>"}}'

curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":19,"method":"tasks/result","params":{"taskId":"<task_id>"}}'

# 说明：tasks/result 返回的是底层 tools/call 的结果对象（如 content/isError），
# 响应里不会重复回显 taskId。

# 取消任务
curl -X POST http://192.168.1.100/mcp_server \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":20,"method":"tasks/cancel","params":{"taskId":"<task_id>"}}'
```

`notifications/resources/updated` 属于服务端通知，需通过 Streamable HTTP SSE 下发。  
`notifications/resources/list_changed` 在资源注册表变化时触发（例如资源 add/remove）。  
本示例里的 `self.resource.touch_status` 会刻意执行 remove+add，因此会观察到这两类通知。  
资源订阅是会话级（session-scoped）的，POST 与 SSE 需要使用同一个 `MCP-Session-Id` 才能观察该会话的更新通知。  
如果当前未启用 SSE（GET 返回 405），仍可先验证 `resources/subscribe` / `resources/unsubscribe` 的请求响应行为。

### 17. 打开 SSE 流（GET）

```bash
curl -N http://192.168.1.100/mcp_server \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: 2025-11-25"
```

### 17.1 纯 SSE 入口（/mcp_server_sse）

启用 SSE 时，服务端会额外注册一个仅 SSE 的入口：`/mcp_server_sse`。  
该入口要求所有 POST 请求必须带 `MCP-Session-Id`（来自 SSE 响应头），
HTTP 层返回 202/空 body，JSON-RPC 响应通过 SSE 事件下发。

```bash
# 1) 打开 SSE 流并记录响应头里的 MCP-Session-Id
curl -N -D /tmp/mcp_sse.hdr http://192.168.1.100/mcp_server_sse \
  -H "Accept: text/event-stream" \
  -H "MCP-Protocol-Version: 2025-11-25"

# 2) 使用 session id 发送 POST（响应会通过 SSE 返回）
SESSION_ID="$(sed -nE 's/^MCP-Session-Id:[[:space:]]*([^[:space:]\r]+).*/\1/ip' /tmp/mcp_sse.hdr | sed -n '1p' | tr -d '\r')"
curl -i -X POST http://192.168.1.100/mcp_server_sse \
  -H "Content-Type: application/json" \
  -H "MCP-Protocol-Version: 2025-11-25" \
  -H "MCP-Session-Id: ${SESSION_ID}" \
  -d '{"jsonrpc":"2.0","id":1,"method":"ping","params":{}}'
```

### 18. SSE 端到端可观测测试脚本

```bash
# 默认执行
bash test_sse_e2e.sh 192.168.1.100 mcp_server

# 打印每个 JSON-RPC 响应
bash test_sse_e2e.sh 192.168.1.100 mcp_server --verbose

# 脚本退出后保留 SSE 日志文件
bash test_sse_e2e.sh 192.168.1.100 mcp_server --keep-log

# 严格模式：对响应关键字段做断言
bash test_sse_e2e.sh 192.168.1.100 mcp_server --strict
```

> **说明：** 该脚本基于 Streamable HTTP（POST 直接返回 JSON），请对 `/mcp_server` 使用；  
> `/mcp_server_sse` 为纯 SSE 入口，POST 返回 202，响应在 SSE 流中返回。

脚本是示例功能的全链路 smoke test，覆盖：
- `initialize` + `notifications/initialized`
- `ping`（包含字符串 `id`）
- `tools/list`
- 示例里全部 `tools/call`：
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
- 任务模式（`tools/call` + `task={}`）以及 `tasks/get`, `tasks/result`, `tasks/cancel`
- JSON-RPC batch 处理（request + notification + error 混合）
- JSON-RPC 错误包与 `error.data` 校验（如 `method`、`uri`、`expected`）
- SSE 校验 `notifications/resources/updated` 与 `notifications/resources/list_changed`
- SSE heartbeat 注释（`: ping`）与携带 `MCP-Session-Id` + `Last-Event-ID` 的重连流程

`--strict` 会额外校验响应中的关键字段，例如：
- `resources/templates/list` 必须包含 `device://sensors/{id}`
- `prompts/list` 必须包含 `status.summary`
- 任务模式响应必须包含 `taskId`
- batch/error 响应必须包含预期 id/code 与 `error.data`
- 重连后的 SSE 流仍可观测到通知

> **说明：** 严格模式会等待 SSE heartbeat，因此总时长会更长（默认额外约 15-20 秒）。

**说明：** 示例中 endpoint 为 `/mcp_server`，由 `esp_mcp_mgr_register_endpoint()` 注册。你可以按需修改路径。  
启用 SSE 时，会额外提供 `/mcp_server_sse`（纯 SSE 入口，POST 响应通过 SSE 下发）。

## 已注册工具

| 工具名 | 参数 | 说明 |
|--------|------|------|
| `self.get_device_status` | 无 | 获取设备状态（JSON） |
| `self.audio_speaker.set_volume` | `volume` (0-100) | 设置音量 |
| `self.screen.set_brightness` | `brightness` (0-100) | 设置亮度 |
| `self.screen.set_theme` | `theme` (string) | 设置主题 |
| `self.screen.set_hsv` | `HSV` (array[3]) | 设置 HSV |
| `self.screen.set_rgb` | `RGB` (object) | 设置 RGB |
| `self.resource.touch_status` | 无 | 触发 `notifications/resources/updated` 以测试 SSE |
| `self.server.roots_list` | 无 | 向当前会话发送服务端主动 `roots/list` 请求 |
| `self.server.sampling_create` | 无 | 向当前会话发送服务端主动 `sampling/create` 请求 |
| `self.server.elicitation_request` | 无 | 向当前会话发送服务端主动 `elicitation/request` 请求 |

## 已注册 Resource / Prompt / Completion

| 类型 | 名称/URI | 说明 |
|------|----------|------|
| Resource | `device://status` | 返回当前模拟设备状态 JSON |
| Resource Template | `device://sensors/{id}` | 按传感器维度组织的模板 URI |
| Prompt | `status.summary` | 生成设备状态总结 Prompt |
| Completion | 参数 `theme` / `device` | 返回建议补全值 |

## 属性类型示例

```c
// 整型（带范围）
esp_mcp_property_t *property = esp_mcp_property_create_with_range("volume", 0, 100);
esp_mcp_tool_add_property(tool, property);

// 整型（带默认值）
property = esp_mcp_property_create_with_int("param", 10);
esp_mcp_tool_add_property(tool, property);

// 布尔（带默认值）
property = esp_mcp_property_create_with_bool("enabled", true);
esp_mcp_tool_add_property(tool, property);

// 字符串（带默认值）
property = esp_mcp_property_create_with_string("theme", "light");
esp_mcp_tool_add_property(tool, property);

// 数组（JSON 字符串默认值）
property = esp_mcp_property_create_with_array("HSV", "[0, 100, 360]");
esp_mcp_tool_add_property(tool, property);

// 对象（JSON 字符串默认值）
property = esp_mcp_property_create_with_object("RGB", "{\"red\":0,\"green\":120,\"blue\":240}");
esp_mcp_tool_add_property(tool, property);
```

## 故障排查

* **Wi-Fi 连接失败**：检查 `menuconfig` 中的 Wi-Fi 配置，并确保设备在覆盖范围内
* **无法访问 MCP 服务**：确认设备 IP，确保客户端与设备在同一局域网
* **工具调用报错**：检查 JSON-RPC 格式与参数值

## 传输支持

### HTTP 传输

示例默认使用 HTTP 传输，MCP 服务端通过 HTTP POST 接收 JSON-RPC 2.0 请求。

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

**默认 endpoint：** `/mcp_server`  
**默认端口：** 80

### 自定义传输

SDK 支持自定义传输实现，详情见 `components/mcp-c-sdk/README.md`。

## 扩展自定义工具

1. 定义工具回调：

```c
static esp_mcp_value_t my_tool_callback(const esp_mcp_property_list_t* properties)
{
    int value = esp_mcp_property_list_get_property_int(properties, "param");
    ESP_LOGI(TAG, "Tool called with param: %d", value);
    return esp_mcp_value_create_bool(true);
}
```

2. 创建工具：

```c
esp_mcp_tool_t *tool = esp_mcp_tool_create("my_tool", "Tool description", my_tool_callback);
```

3. 添加属性（可选）：

```c
esp_mcp_property_t *property = esp_mcp_property_create_with_int("param", 10);
esp_mcp_tool_add_property(tool, property);
```

4. 注册工具：

```c
esp_mcp_add_tool(mcp, tool);
```

## 技术参考

* [Model Context Protocol Specification](https://modelcontextprotocol.io/)
* [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
* [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
