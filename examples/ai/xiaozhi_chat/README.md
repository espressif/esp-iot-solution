# Xiaozhi Chat Example

**English** | [中文](README_CN.md)

## Overview

This example demonstrates a voice AI chatbot on ESP32 using the **esp_xiaozhi** component. It connects to the [xiaozhi.me](https://xiaozhi.me) service for real-time voice interaction based on streaming ASR + LLM + TTS, and uses the **Model Context Protocol (MCP)** for device-side control (e.g. volume, display, GPIO).

The example uses the esp-iot-solution **esp_xiaozhi** component for chat session, WebSocket/MQTT+UDP transport, and MCP integration. Application-level features such as offline wake word (ESP-SR), audio I/O, display, and board support are implemented in the example.

## Features

* Bidirectional streaming voice chat with [xiaozhi.me](https://xiaozhi.me) (Qwen/DeepSeek, etc.)
* **Transport**: WebSocket or MQTT+UDP (configurable; MQTT preferred when server supports both)
* **Audio**: OPUS codec (component also supports G.711 and PCM)
* **MCP**: Device-side tools for volume, brightness, theme, color (RGB/HSV), etc.
* Offline wake word support via [ESP-SR](https://github.com/espressif/esp-sr) (when enabled)
* OLED/LCD display with emoji support
* Multi-language (Chinese, English)
* Device information and OTA from server

## Hardware

* ESP32-S3 (or supported target) development board
* Microphone and speaker (or board with audio codec)
* Wi-Fi connectivity
* Optional: OLED/LCD for display, compatible board definition in `main/boards`

See the [esp_xiaozhi component README](../../../components/esp_xiaozhi/README.md) for API and architecture details.

## How to Use Example

### Configure the Project

Set the chip target and open the configuration menu:

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

Configure under the relevant menus:

* **Wi-Fi**: Set SSID and password for station mode
* **Xiaozhi Chat Application**: Choose transport (Auto prefer MQTT, MQTT, or WebSocket) when both are available
* **Board**: Select or add a board in `main/boards` if using a predefined board

### Build and Flash

This example uses **esp_board_manager** for board device management (audio, display, etc.). You must generate the board config before building. See `managed_components/espressif__esp_board_manager/README.md`.

```bash
export IDF_EXTRA_ACTIONS_PATH=./managed_components/espressif__esp_board_manager

# Board config: run after first clone or when changing board (-l list boards, -b set board by name or index)
idf.py gen-bmgr-config -l
idf.py gen-bmgr-config -b <board_name_or_index>

# Build, flash and open serial monitor (replace PORT with your port, e.g. /dev/ttyUSB0 or COM3)
idf.py -p PORT build flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

The device will connect to Wi-Fi, obtain device info from the server, and start the chat session. Use the configured wake word or UI to start voice interaction.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps.

## Example Output

After flashing, the device connects to Wi-Fi and the Xiaozhi service. Serial output will show connection status, MCP tool registration, and chat/audio events. For example:

```
I (xxxx) wifi: connected ...
I (xxxx) esp_xiaozhi: Chat session started
I (xxxx) esp_xiaozhi_chat_app: MCP tools registered
```

## Configuration

* **Transport**: In `menuconfig` → `Xiaozhi Chat Application` → choose Auto (prefer MQTT), MQTT, or WebSocket.
* **Server**: The example uses [xiaozhi.me](https://xiaozhi.me) by default; device info and OTA URL come from the component/server configuration.

## Troubleshooting

* **Build fails**: Ensure the ESP-IDF environment is activated; set `IOT_SOLUTION_PATH` when building from esp-iot-solution; when using board config, run `idf.py gen-bmgr-config` first and set `IDF_EXTRA_ACTIONS_PATH` per esp_board_manager docs if required.
* **Wi-Fi connection failed**: Check SSID/password in `menuconfig`.
* **Cannot connect to xiaozhi.me**: Ensure network allows HTTPS and that device time/date are set if required.
* **No audio**: Check board selection and microphone/speaker wiring; ensure codec is configured for your board.

## Related Documentation

* [esp_xiaozhi component](../../../components/esp_xiaozhi/README.md) – Chat API, transport, MCP, and configuration
* [xiaozhi.me](https://xiaozhi.me) – Service and registration
* [MCP Server example](../../mcp/mcp_server/README.md) – Standalone MCP server on ESP32
