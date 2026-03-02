# MCP Client Example

This example demonstrates using an ESP device as an **MCP client** to call a remote MCP server over HTTP.

## Hardware

- A development board with ESP32/ESP32-S2/ESP32-S3/ESP32-C3/ESP32-C6 SoC
- USB cable for power supply and programming
- Wi-Fi router for network connectivity

## Flow

- Connect Wi-Fi (`protocol_examples_common`)
- Initialize MCP manager with built-in **HTTP client transport**
- Send JSON-RPC requests:
  - `initialize`
  - `tools/list`
  - `tools/call` (example: call `get_device_status` with `{}`)

## How to Use Example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Configure the Project

Open the project configuration menu:

```bash
idf.py menuconfig
```

In the `Example Connection Configuration` menu:

- Set Wi-Fi SSID
- Set Wi-Fi Password

In the `Example Configuration` menu (this example):

- `Remote MCP server host`: MCP server host/IP
- `Remote MCP server port`: MCP server port
- `Remote MCP endpoint name`: HTTP path used as `POST /<endpoint>`

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```bash
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

After flashing the firmware, you should see the device connect to Wi-Fi and then send MCP requests to the remote server:

```
I (...) example_connect: - IPv4 address: 192.168.1.10
I (...) mcp_client: Remote base_url=http://192.168.1.100:80, endpoint=mcp_server
I (...) mcp_client: Response callback: is_error=0, ep_name=mcp_server, resp_json=...
```

## Run Together with `mcp_server` Example

1. Build/flash/run `examples/mcp/mcp_server` and note the device IP address printed in the log
2. In this `mcp_client` example, set:
   - `Remote MCP server host` to the server IP (e.g. `192.168.1.100`)
   - `Remote MCP server port` to the server port (the server example uses port **80** by default)
   - `Remote MCP endpoint name` to match the server endpoint (default: `mcp_server`)
3. Flash/run `mcp_client` and observe the callbacks for `initialize`, `tools/list`, `tools/call`

## Troubleshooting

- **Wi-Fi connection failed**: check Wi-Fi credentials in `menuconfig` and ensure the device is within Wi-Fi range
- **Cannot reach MCP server**: verify `host/port/endpoint` and ensure client/server are on the same network
