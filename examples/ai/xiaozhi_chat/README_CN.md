# 小智聊天示例

[English](README.md) | **中文**

## 概述

本示例基于 **esp_xiaozhi** 组件在 ESP32 上实现语音 AI 聊天机器人，连接 [xiaozhi.me](https://xiaozhi.me) 服务，实现基于流式 ASR + LLM + TTS 的实时语音交互，并通过 **模型上下文协议（MCP）** 进行设备端控制（如音量、显示、GPIO 等）。

示例使用 esp-iot-solution 的 **esp_xiaozhi** 组件完成会话管理、WebSocket/MQTT+UDP 传输及 MCP 集成；离线唤醒词（ESP-SR）、音频输入输出、显示与开发板支持等由示例层实现。

## 功能

* 与 [xiaozhi.me](https://xiaozhi.me) 双向流式语音对话（Qwen/DeepSeek 等）
* **传输方式**：WebSocket 或 MQTT+UDP（可配置；服务器同时支持时优先 MQTT）
* **音频**：OPUS 编解码（组件还支持 G.711、PCM）
* **MCP**：设备端工具（音量、亮度、主题、RGB/HSV 颜色等）
* 可选离线唤醒词 [ESP-SR](https://github.com/espressif/esp-sr)
* OLED/LCD 显示与表情显示
* 多语言（中文、英文）
* 设备信息与服务器 OTA

## 硬件

* 支持 ESP32-S3（或其它已支持目标）的开发板
* 麦克风与扬声器（或带音频编解码的开发板）
* Wi-Fi 网络
* 可选：OLED/LCD 显示屏；`main/boards` 中兼容的开发板定义

API 与架构说明见 [esp_xiaozhi 组件 README](../../../components/esp_xiaozhi/README_CN.md)。

## 使用方法

### 配置工程

设置目标芯片并打开配置菜单：

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

在对应菜单中配置：

* **Wi-Fi**：配置 Station 模式下的 SSID 与密码
* **Xiaozhi Chat Application**：在同时支持 MQTT/WebSocket 时选择传输方式（自动优先 MQTT、仅 MQTT、仅 WebSocket）
* **Board**：若使用预定义开发板，在 `main/boards` 中选择或添加板型

### 编译与烧录

本示例使用 **esp_board_manager** 管理开发板设备（音频、显示等）。编译前须先完成板级配置生成；详见 `managed_components/espressif__esp_board_manager/README_CN.md`。

```bash
export IDF_EXTRA_ACTIONS_PATH=./managed_components/espressif__esp_board_manager

# 板级配置：首次编译或更换板型后执行（-l 列出可用板型，-b 指定板型名称或序号）
idf.py gen-bmgr-config -l
idf.py gen-bmgr-config -b <板型名称或序号>

# 编译、烧录并打开串口监视器（将 PORT 替换为实际串口，如 /dev/ttyUSB0 或 COM3）
idf.py -p PORT build flash monitor
```

（退出串口监视器请键入 `Ctrl-]`。）

设备将连接 Wi-Fi、从服务器获取设备信息并启动聊天会话；可通过配置的唤醒词或 UI 开始语音交互。

完整步骤见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)。

## 示例输出

烧录后设备会连接 Wi-Fi 与小智服务，串口会打印连接状态、MCP 工具注册及聊天/音频事件，例如：

```
I (xxxx) wifi: connected ...
I (xxxx) esp_xiaozhi: Chat session started
I (xxxx) esp_xiaozhi_chat_app: MCP tools registered
```

## 配置说明

* **传输方式**：在 `menuconfig` → `Xiaozhi Chat Application` 中选择自动（优先 MQTT）、MQTT 或 WebSocket。
* **服务器**：示例默认使用 [xiaozhi.me](https://xiaozhi.me)；设备信息与 OTA 地址由组件/服务器配置决定。

## 常见问题

* **编译失败**：确认已激活 ESP-IDF 环境；在 esp-iot-solution 中构建时设置 `IOT_SOLUTION_PATH`；使用板级配置时先执行 `idf.py gen-bmgr-config`，并参考 esp_board_manager 文档设置 `IDF_EXTRA_ACTIONS_PATH`（若需）。
* **Wi-Fi 连接失败**：在 `menuconfig` 中检查 SSID/密码。
* **无法连接 xiaozhi.me**：确认网络可访问 HTTPS，必要时校准设备时间。
* **无声音**：检查开发板选择、麦克风/扬声器接线及编解码配置。

## 相关文档

* [esp_xiaozhi 组件](../../../components/esp_xiaozhi/README_CN.md) – 聊天 API、传输、MCP 与配置
* [xiaozhi.me](https://xiaozhi.me) – 服务与注册
* [MCP Server 示例](../../mcp/mcp_server/README.md) – ESP32 独立 MCP 服务器
