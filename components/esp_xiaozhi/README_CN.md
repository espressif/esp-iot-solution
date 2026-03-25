# ESP32 小智 AI 聊天机器人组件

[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.5%2B-blue)](https://github.com/espressif/esp-idf)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](https://opensource.org/licenses/Apache-2.0)

[English](README.md) | **中文**

小智是一个双向流式对话组件，连接到 [xiaozhi.me](https://xiaozhi.me) 服务。它支持与使用 Qwen 和 DeepSeek 等大语言模型的 AI 代理进行实时语音/文本交互。

该组件非常适合语音助手和智能语音问答系统等用例。它具有低延迟和轻量级设计，适合在 ESP32 等嵌入式设备上运行的应用。

## 功能特性

- **双向流式传输**：与 AI 代理进行实时语音和文本交互
- **多种通信协议**：支持 WebSocket 和 MQTT+UDP 协议
- **音频编解码支持**：OPUS、G.711 和 PCM 音频格式
- **MCP 集成**：设备端 MCP 用于设备控制（扬声器、LED、舵机、GPIO 等）
- **多语言支持**：中文和英文
- **离线唤醒词**：提供上报唤醒词的 API（如 `esp_xiaozhi_chat_send_wake_word`），与 ESP-SR 的集成由应用层实现
- **设备信息**：自动获取设备信息和 OTA 更新支持

## 架构

小智使用流式 ASR（自动语音识别）+ LLM（大语言模型）+ TTS（文本转语音）架构进行语音交互：

1. **音频输入**：从麦克风捕获音频
2. **ASR**：实时将语音转换为文本
3. **LLM**：处理文本并生成响应
4. **TTS**：将文本响应转换为语音
5. **音频输出**：通过扬声器播放音频

该组件与 MCP（模型上下文协议）集成，以实现设备控制功能。

## 快速开始

### 添加组件

该组件包含在 esp-iot-solution 仓库中。要在项目中使用它：

```cmake
# 在项目的 CMakeLists.txt 中
idf_component_register(
    ...
    REQUIRES esp_xiaozhi
    ...
)
```

或者如果使用 IDF Component Manager，请在 `idf_component.yml` 中添加：

```yaml
dependencies:
  espressif/esp_xiaozhi:
    override_path: "../../components/esp_xiaozhi"
```

### 基本用法

```c
#include "esp_xiaozhi_chat.h"
#include "esp_xiaozhi_info.h"

// 从服务器获取设备信息
esp_xiaozhi_chat_info_t info = {0};
esp_xiaozhi_chat_get_info(&info);
// 使用 info 进行配置...
esp_xiaozhi_chat_free_info(&info);

// 初始化聊天配置
esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
config.audio_callback = audio_callback;      // 接收音频数据的回调
config.event_callback = event_callback;       // 聊天事件的回调
config.mcp_engine = mcp_engine;              // 可选：用于设备控制的 MCP 引擎

esp_xiaozhi_chat_handle_t chat_handle;
esp_xiaozhi_chat_init(&config, &chat_handle);

// 启动聊天会话
esp_xiaozhi_chat_start(chat_handle);

// 打开音频通道（可选，可在检测到唤醒词时调用）
esp_xiaozhi_chat_open_audio_channel(chat_handle, NULL, NULL, 0);

// 发送音频数据
esp_xiaozhi_chat_send_audio_data(chat_handle, audio_data, data_len);

// 发送唤醒词检测（如果使用离线唤醒词）
esp_xiaozhi_chat_send_wake_word(chat_handle, "你好小智");

// 发送开始监听
esp_xiaozhi_chat_send_start_listening(chat_handle, 0);

// 停止聊天会话
esp_xiaozhi_chat_stop(chat_handle);
esp_xiaozhi_chat_deinit(chat_handle);
```

### 完整示例（包含事件处理）

```c
#include "esp_xiaozhi_chat.h"
#include "esp_event.h"

// 音频回调 - 当从服务器接收到音频数据时调用
static void audio_callback(uint8_t *data, int len, void *ctx)
{
    // 通过扬声器播放音频数据
    audio_feeder_feed_data(data, len);
}

// 事件回调 - 当聊天事件发生时调用
static void event_callback(esp_xiaozhi_chat_event_t event, void *event_data, void *ctx)
{
    switch (event) {
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED:
        ESP_LOGI("APP", "语音开始");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STOPPED:
        ESP_LOGI("APP", "语音停止");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT:
        ESP_LOGI("APP", "收到文本: %s", (char *)event_data);
        break;
    default:
        break;
    }
}

// 连接事件处理器
static void chat_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_XIAOZHI_CHAT_EVENT_CONNECTED:
        ESP_LOGI("APP", "已连接到服务器");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED:
        ESP_LOGI("APP", "已断开与服务器的连接");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED:
        ESP_LOGI("APP", "音频通道已打开");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED:
        ESP_LOGI("APP", "音频通道已关闭");
        break;
    default:
        break;
    }
}

void app_main(void)
{
    // 获取设备信息
    esp_xiaozhi_chat_info_t info = {0};
    esp_xiaozhi_chat_get_info(&info);
    esp_xiaozhi_chat_free_info(&info);

    // 初始化 MCP 引擎（可选）
    esp_mcp_t *mcp = NULL;
    esp_mcp_create(&mcp);
    // 添加用于设备控制的 MCP 工具...

    // 初始化聊天
    esp_xiaozhi_chat_config_t config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    config.audio_callback = audio_callback;
    config.event_callback = event_callback;
    config.mcp_engine = mcp;

    esp_xiaozhi_chat_handle_t chat_handle;
    esp_xiaozhi_chat_init(&config, &chat_handle);

    // 注册事件处理器
    esp_event_handler_register(ESP_XIAOZHI_CHAT_EVENTS, ESP_EVENT_ANY_ID,
                                chat_event_handler, NULL);

    // 启动聊天会话
    esp_xiaozhi_chat_start(chat_handle);

    // 您的应用程序逻辑...
}
```

### MCP 集成示例

该组件支持 MCP（模型上下文协议）用于设备控制。以下是集成 MCP 工具的方法：

```c
#include "esp_mcp_engine.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"

// 创建 MCP 引擎
esp_mcp_t *mcp = NULL;
esp_mcp_create(&mcp);

// 创建设备控制工具
esp_mcp_tool_t *tool = esp_mcp_tool_create(
    "self.audio_speaker.set_volume",
    "设置音频扬声器音量 (0-100)",
    set_volume_callback
);

// 向工具添加属性
esp_mcp_property_t *property = esp_mcp_property_create_with_range("volume", 0, 100);
esp_mcp_tool_add_property(tool, property);

// 将工具添加到 MCP 引擎
esp_mcp_add_tool(mcp, tool);

// 将 MCP 引擎传递给聊天配置
config.mcp_engine = mcp;
```

## 示例

请参阅 `examples/ai/xiaozhi_chat` 示例以获取完整实现，包括：
- 音频录制和播放
- 唤醒词检测集成
- MCP 工具实现
- 事件处理
- 设备信息获取

## API 参考

请参阅组件头文件中的 API 文档：
- `esp_xiaozhi_chat.h` - 主聊天 API
- `esp_xiaozhi_info.h` - 设备信息 API

### 主要函数

#### 聊天管理
- `esp_xiaozhi_chat_init()` - 初始化聊天会话
- `esp_xiaozhi_chat_deinit()` - 反初始化聊天会话
- `esp_xiaozhi_chat_start()` - 启动聊天会话
- `esp_xiaozhi_chat_stop()` - 停止聊天会话

#### 音频通道控制
- `esp_xiaozhi_chat_open_audio_channel()` - 打开音频通道
- `esp_xiaozhi_chat_close_audio_channel()` - 关闭音频通道
- `esp_xiaozhi_chat_send_audio_data()` - 向服务器发送音频数据

#### 交互控制
- `esp_xiaozhi_chat_send_wake_word()` - 发送唤醒词检测
- `esp_xiaozhi_chat_send_start_listening()` - 启动监听模式
- `esp_xiaozhi_chat_send_stop_listening()` - 停止监听模式
- `esp_xiaozhi_chat_send_abort_speaking()` - 中止当前语音输出

#### 设备信息
- `esp_xiaozhi_chat_get_info()` - 从服务器获取设备信息
- `esp_xiaozhi_chat_free_info()` - 释放设备信息结构

### 事件

该组件通过事件回调和 ESP 事件系统提供各种事件：

**事件回调事件（最小协议层，参见 docs/PROTOCOL_API_DRAFT.md）：**
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT` - 文本消息（STT/TTS 句子）；`event_data` = `esp_xiaozhi_chat_text_data_t *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_EMOJI` - LLM 表情；`event_data` = `const char *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE` - TTS 协议状态（start/stop/sentence_start）；`event_data` = `esp_xiaozhi_chat_tts_state_t *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_SYSTEM_CMD` - 系统命令（如 "reboot"）；`event_data` = `const char *`；是否执行由应用决定
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR` - 传输/协议错误；`event_data` = `esp_xiaozhi_chat_error_info_t *`
- `ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED` / `CHAT_SPEECH_STOPPED` - 兼容保留；新代码建议使用 `CHAT_TTS_STATE`

**ESP 事件系统事件：**
- `ESP_XIAOZHI_CHAT_EVENT_CONNECTED` - 已连接到服务器
- `ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED` - 已断开与服务器的连接
- `ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED` - 音频通道已打开
- `ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED` - 音频通道已关闭

## 依赖项

- ESP-IDF >= 5.5
- mcp-c-sdk - 模型上下文协议 SDK
- ESP-SR - 语音识别（可选，用于唤醒词）
- 各种 ESP-IDF 组件：
  - `nvs_flash` - 非易失性存储
  - `mqtt` - MQTT 客户端
  - `esp_http_client` - HTTP 客户端
  - `json` - JSON 解析
  - `mbedtls` - TLS/SSL 支持
  - `app_update` - OTA 更新支持
  - `spi_flash` - Flash 操作

## 配置

该组件可以通过 `ESP_XIAOZHI_CHAT_DEFAULT_CONFIG()` 宏或手动设置配置结构字段进行配置：

- `audio_type` - 音频格式（PCM、OPUS 或 G.711）
- `audio_callback` - 接收音频数据的回调
- `event_callback` - 聊天事件的回调
- `audio_callback_ctx` - 传递给音频回调的上下文
- `event_callback_ctx` - 传递给事件回调的上下文
- `mcp_engine` - MCP 引擎实例（可选）
- `has_mqtt_config` - 若服务器提供 MQTT 则为 true（来自 `esp_xiaozhi_chat_get_info`）；两者都有时优先 MQTT
- `has_websocket_config` - 若服务器提供 WebSocket 则为 true（来自 `esp_xiaozhi_chat_get_info`）

## 许可证

Apache-2.0
