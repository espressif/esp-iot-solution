# ESP32 Xiaozhi AI Chatbot Component

[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.5%2B-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

**English** | [中文](README_CN.md)

Xiaozhi is a bidirectional streaming dialogue component that connects to the [xiaozhi.me](https://xiaozhi.me) service. It supports real-time voice/text interaction with AI agents using large language models like Qwen and DeepSeek.

This component is ideal for use cases such as voice assistants and intelligent voice Q&A systems. It features low latency and a lightweight design, making it suitable for applications running on embedded devices such as the ESP32.

## Features

- **Bidirectional Streaming**: Real-time voice and text interaction with AI agents
- **Multiple Communication Protocols**: Supports WebSocket and MQTT+UDP; both can coexist. When the server provides both (from `esp_xiaozhi_chat_get_info`), MQTT is preferred
- **Audio Codec Support**: OPUS, G.711, and PCM audio formats
- **MCP Integration**: Device-side MCP for device control (speaker, LED, servo, GPIO, etc.)
- **Multi-language Support**: Chinese and English
- **Offline Wake Word**: API to report wake word (e.g. `esp_xiaozhi_chat_send_wake_word`); integration with ESP-SR is application-level
- **Device Information**: Automatic device information retrieval and OTA update support

## Architecture

Xiaozhi uses a streaming ASR (Automatic Speech Recognition) + LLM (Large Language Model) + TTS (Text-to-Speech) architecture for voice interaction:

1. **Audio Input**: Captures audio from microphone
2. **ASR**: Converts speech to text in real-time
3. **LLM**: Processes text and generates responses
4. **TTS**: Converts text responses to speech
5. **Audio Output**: Plays audio through speaker

The component integrates with the MCP (Model Context Protocol) to enable device control capabilities.

## Quick Start

### Adding the Component

The component is included in the esp-iot-solution repository. To use it in your project:

```cmake
# In your project's CMakeLists.txt
idf_component_register(
    ...
    REQUIRES esp_xiaozhi
    ...
)
```

Or if using IDF Component Manager, add to your `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_xiaozhi:
    override_path: "../../components/esp_xiaozhi"
```

### Basic Usage

```c
#include "esp_xiaozhi_chat.h"
#include "esp_xiaozhi_info.h"

// Get device information from server
esp_xiaozhi_chat_info_t info = {0};
esp_xiaozhi_chat_get_info(&info);
// Use info for configuration...
esp_xiaozhi_chat_free_info(&info);

// Initialize chat configuration
esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
config.audio_callback = audio_callback;      // Callback for receiving audio data
config.event_callback = event_callback;      // Callback for chat events
config.mcp_engine = mcp_engine;              // Optional: MCP engine for device control
// Prefer MQTT when server supports both (set from get_info result)
config.has_mqtt_config = info.has_mqtt_config;
config.has_websocket_config = info.has_websocket_config;

esp_xiaozhi_chat_handle_t chat_handle;
esp_xiaozhi_chat_init(&config, &chat_handle);

// Start chat session
esp_xiaozhi_chat_start(chat_handle);

// Open audio channel (optional, can be called when wake word detected)
esp_xiaozhi_chat_open_audio_channel(chat_handle, NULL, NULL, 0);

// Send audio data
esp_xiaozhi_chat_send_audio_data(chat_handle, audio_data, data_len);

// Send wake word detected (if using offline wake word)
esp_xiaozhi_chat_send_wake_word(chat_handle, "你好小智");

// Send start listening
esp_xiaozhi_chat_send_start_listening(chat_handle, 0);

// Stop chat session
esp_xiaozhi_chat_stop(chat_handle);
esp_xiaozhi_chat_deinit(chat_handle);
```

### Complete Example with Event Handling

```c
#include "esp_xiaozhi_chat.h"
#include "esp_event.h"

// Audio callback - called when audio data is received from server
static void audio_callback(uint8_t *data, int len, void *ctx)
{
    // Play audio data through speaker
    audio_feeder_feed_data(data, len);
}

// Event callback - called when chat events occur
static void event_callback(esp_xiaozhi_chat_event_t event, void *event_data, void *ctx)
{
    switch (event) {
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED:
        ESP_LOGI("APP", "Speech started");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STOPPED:
        ESP_LOGI("APP", "Speech stopped");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT:
        ESP_LOGI("APP", "Text received: %s", (char *)event_data);
        break;
    default:
        break;
    }
}

// Event handler for connection events
static void chat_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_XIAOZHI_CHAT_EVENT_CONNECTED:
        ESP_LOGI("APP", "Connected to server");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED:
        ESP_LOGI("APP", "Disconnected from server");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED:
        ESP_LOGI("APP", "Audio channel opened");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED:
        ESP_LOGI("APP", "Audio channel closed");
        break;
    default:
        break;
    }
}

void app_main(void)
{
    // Get device information
    esp_xiaozhi_chat_info_t info = {0};
    esp_xiaozhi_chat_get_info(&info);
    esp_xiaozhi_chat_free_info(&info);

    // Initialize MCP engine (optional)
    esp_mcp_t *mcp = NULL;
    esp_mcp_create(&mcp);
    // Add MCP tools for device control...

    // Initialize chat
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = audio_callback;
    config.event_callback = event_callback;
    config.mcp_engine = mcp;

    esp_xiaozhi_chat_handle_t chat_handle;
    esp_xiaozhi_chat_init(&config, &chat_handle);

    // Register event handler
    esp_event_handler_register(ESP_XIAOZHI_CHAT_EVENTS, ESP_EVENT_ANY_ID,
                                chat_event_handler, NULL);

    // Start chat session
    esp_xiaozhi_chat_start(chat_handle);

    // Your application logic here...
}
```

### MCP Integration Example

The component supports MCP (Model Context Protocol) for device control. Here's how to integrate MCP tools:

```c
#include "esp_mcp_engine.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"

// Create MCP engine
esp_mcp_t *mcp = NULL;
esp_mcp_create(&mcp);

// Create a tool for device control
esp_mcp_tool_t *tool = esp_mcp_tool_create(
    "self.audio_speaker.set_volume",
    "Set audio speaker volume (0-100)",
    set_volume_callback
);

// Add property to the tool
esp_mcp_property_t *property = esp_mcp_property_create_with_range("volume", 0, 100);
esp_mcp_tool_add_property(tool, property);

// Add tool to MCP engine
esp_mcp_add_tool(mcp, tool);

// Pass MCP engine to chat config
config.mcp_engine = mcp;
```

## Examples

See the `examples/ai/xiaozhi_chat` example for a complete implementation including:
- Audio recording and playback
- Wake word detection integration
- MCP tool implementation
- Event handling
- Device information retrieval

## API Reference

See the API documentation in the component header files:
- `esp_xiaozhi_chat.h` - Main chat API
- `esp_xiaozhi_info.h` - Device information API

### Key Functions

#### Chat Management
- `esp_xiaozhi_chat_init()` - Initialize chat session
- `esp_xiaozhi_chat_deinit()` - Deinitialize chat session
- `esp_xiaozhi_chat_start()` - Start chat session
- `esp_xiaozhi_chat_stop()` - Stop chat session

#### Audio Channel Control
- `esp_xiaozhi_chat_open_audio_channel()` - Open audio channel
- `esp_xiaozhi_chat_close_audio_channel()` - Close audio channel
- `esp_xiaozhi_chat_send_audio_data()` - Send audio data to server

#### Interaction Control
- `esp_xiaozhi_chat_send_wake_word()` - Send wake word detection
- `esp_xiaozhi_chat_send_start_listening()` - Start listening mode
- `esp_xiaozhi_chat_send_stop_listening()` - Stop listening mode
- `esp_xiaozhi_chat_send_abort_speaking()` - Abort current speech output

#### Device Information
- `esp_xiaozhi_chat_get_info()` - Get device information from server
- `esp_xiaozhi_chat_free_info()` - Free device information structure

### Events

The component follows a **minimal protocol layer** design: it only reports protocol facts (connection, session, messages); the application handles state machine, UI, and system commands. See **docs/PROTOCOL_API_DRAFT.md** for the full keep/deprecate/add contract.

**Event Callback Events (recommended / stable):**
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT` - Text message (STT/TTS sentence); `event_data` = `esp_xiaozhi_chat_text_data_t *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_EMOJI` - LLM emotion; `event_data` = `const char *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE` - TTS protocol state (start/stop/sentence_start); `event_data` = `esp_xiaozhi_chat_tts_state_t *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_SYSTEM_CMD` - System command (e.g. `"reboot"`); `event_data` = `const char *`; app decides whether to execute
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR` - Transport/protocol error; `event_data` = `esp_xiaozhi_chat_error_info_t *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED` / `CHAT_SPEECH_STOPPED` - Still emitted for compatibility; prefer `CHAT_TTS_STATE` for new code

**ESP Event System Events:**
- `ESP_XIAOZHI_CHAT_EVENT_CONNECTED` - Connected to server
- `ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED` - Disconnected from server
- `ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED` - Audio channel opened
- `ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED` - Audio channel closed

## Dependencies

- ESP-IDF >= 5.5
- mcp-c-sdk - Model Context Protocol SDK
- ESP-SR - Speech recognition (optional, for wake word)
- Various ESP-IDF components:
  - `nvs_flash` - Non-volatile storage
  - `mqtt` - MQTT client
  - `esp_http_client` - HTTP client
  - `json` - JSON parsing
  - `mbedtls` - TLS/SSL support
  - `app_update` - OTA update support
  - `spi_flash` - Flash operations

## Configuration

The component can be configured through `ESP_XIAOZHI_CHAT_DEFAULT_CONFIG()` macro or by manually setting the configuration structure fields:

- `audio_type` - Audio format (PCM, OPUS, or G.711)
- `audio_callback` - Callback for receiving audio data
- `event_callback` - Callback for chat events
- `audio_callback_ctx` - Context passed to the audio callback
- `event_callback_ctx` - Context passed to the event callback
- `mcp_engine` - MCP engine instance (optional)
- `has_mqtt_config` - True if server provides MQTT (from `esp_xiaozhi_chat_get_info`); prefer MQTT when both are available
- `has_websocket_config` - True if server provides WebSocket (from `esp_xiaozhi_chat_get_info`)

## License

Apache-2.0
