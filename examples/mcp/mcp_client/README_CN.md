# MCP Client 示例

本示例演示如何将 ESP 设备作为 **MCP 客户端**，通过 HTTP 调用远端 MCP 服务端。

## 硬件

- 搭载 ESP32/ESP32-S2/ESP32-S3/ESP32-C3/ESP32-C6 SoC 的开发板
- USB 数据线（供电与烧录）
- 用于联网的 Wi-Fi 路由器

## 流程

- 连接 Wi-Fi（`protocol_examples_common`）
- 使用内置 **HTTP 客户端传输**初始化 MCP 管理器
- 按顺序发送 JSON-RPC 请求：
  - `initialize`，并在发起其它 MCP 请求前 **等待其响应返回**
  - `tools/list`（可通过 `nextCursor` 做可选分页）
  - 可选：根据 `tools/list` 结果对首个发现的工具做动态 `tools/call`，参数为 `{}`
  - `tools/call`（若干静态示例工具调用）
  - 可选：`resources/*`、`prompts/*` 演示请求（由 Kconfig 控制）

### `initialize` 与 `notifications/initialized`

示例会请求较新的 MCP 协议版本；`initialize` 返回中 **协商得到的** `protocolVersion` 决定是否必须发送 `notifications/initialized`。

MCP 管理器（`esp_mcp_mgr`）在协商版本 **按字典序新于** `2024-11-05` 时，会 **自动** 发送 `notifications/initialized`（SDK 内为字典序比较）。该流程下 **无需** 再自行调用 `esp_mcp_mgr_post_initialized`。若你在 `initialize` 响应回调里手动发送，管理器 **不会** 再发第二份。

在 `initialize` 响应尚未完成时就发送 `tools/list` 或 `tools/call` 可能破坏会话顺序；本示例会等待挂起的 `initialize` 响应（及回调路径中的后续工作）结束后再继续。

## 使用方法

在配置与构建前，请先使用 `idf.py set-target <chip_name>` 设置正确的芯片目标。

### 配置工程

打开工程配置菜单：

```bash
idf.py menuconfig
```

在 `Example Connection Configuration` 菜单中：

- 设置 Wi-Fi SSID
- 设置 Wi-Fi 密码

在本示例的 `Example Configuration` 菜单中：

- `Remote MCP server host`：MCP 服务端主机名或 IP
- `Remote MCP server port`：MCP 服务端端口
- `Remote MCP endpoint name`：用作 `POST /<endpoint>` 的 HTTP 路径
- `Enable resources/* demo requests`：启用 `resources/list`、`resources/templates/list`、`resources/read`
- `Demo resources/read URI`：启用时 `resources/read` 使用的 URI
- `Enable prompts/* demo requests`：启用 `prompts/list` 与 `prompts/get`
- `Demo prompts/get name`：启用时 `prompts/get` 使用的 prompt 名称
- `Enable tools/list pagination demo`：使用 `nextCursor` 请求后续页
- `Max tools/list pages`：分页循环的上限
- `Enable dynamic tools/call from tools/list result`：对列表中首个发现的工具以 `{}` 调用

### 编译与烧录

编译并烧录到开发板，再通过串口监视器查看输出：

```bash
idf.py -p PORT flash monitor
```

（退出串口监视器：按 ``Ctrl-]``。）

完整的环境搭建与构建步骤见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 示例输出

烧录固件后，设备会先连接 Wi-Fi，再向远端服务发送 MCP 请求，日志中可见类似：

```
I (...) example_connect: - IPv4 address: 192.168.1.10
I (...) mcp_client: Remote base_url=http://192.168.1.100:80, endpoint=mcp_server
I (...) mcp_client: [Success] Endpoint: mcp_server
```

## 与 `mcp_server` 示例联调

1. 编译、烧录并运行 `examples/mcp/mcp_server`，在日志中记下设备 IP。
2. 在本 `mcp_client` 示例中配置：
   - `Remote MCP server host`：服务端 IP（例如 `192.168.1.100`）
   - `Remote MCP server port`：服务端端口（服务端示例默认为 **80**）
   - `Remote MCP endpoint name`：与端点路径一致（默认：`mcp_server`）
3. 烧录并运行 `mcp_client`，观察 `initialize`、`tools/list`、`tools/call` 等回调输出。

## 故障排查

- **Wi-Fi 连接失败**：检查 `menuconfig` 中的 SSID/密码，并确认设备在信号覆盖范围内。
- **无法访问 MCP 服务端**：核对 `host/port/endpoint`，并确认客户端与服务端在同一网络。
