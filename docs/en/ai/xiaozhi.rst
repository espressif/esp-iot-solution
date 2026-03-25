Xiaozhi AI Chatbot
===================
:link_to_translation:`en:[English]`

==================  =============
Supported chips     ESP32-S3
==================  =============

Xiaozhi is a bidirectional streaming dialogue component that connects to the `xiaozhi.me <https://xiaozhi.me>`_ service. It supports real-time voice/text interaction with AI agents using large language models like Qwen and DeepSeek.

This component is ideal for use cases such as voice assistants and intelligent voice Q&A systems. It features low latency and a lightweight design, making it suitable for applications running on embedded devices such as the ESP32.

Features
--------

- **Bidirectional Streaming**: Real-time voice and text interaction with AI agents
- **Multiple Communication Protocols**: Supports WebSocket and MQTT+UDP protocols
- **Audio Codec Support**: OPUS, G.711, and PCM audio formats
- **MCP Integration**: Device-side MCP for device control (speaker, LED, servo, GPIO, etc.)
- **Multi-language Support**: Chinese and English
- **Offline Wake Word**: API to report wake word (e.g. `esp_xiaozhi_chat_send_wake_word`); ESP-SR integration is application-level

Architecture
------------

Xiaozhi uses a streaming ASR (Automatic Speech Recognition) + LLM (Large Language Model) + TTS (Text-to-Speech) architecture for voice interaction:

1. **Audio Input**: Captures audio from microphone
2. **ASR**: Converts speech to text in real-time
3. **LLM**: Processes text and generates responses
4. **TTS**: Converts text responses to speech
5. **Audio Output**: Plays audio through speaker

The component integrates with the MCP (Model Context Protocol) to enable device control capabilities.

Examples
--------

1. Xiaozhi App Example: :example:`ai/xiaozhi_chat`. A complete voice assistant application demonstrating:
   - Voice interaction with AI agents
   - Device control via MCP protocol
   - Multi-language support
   - Display support

API Reference
-------------

.. include-build-file:: inc/esp_xiaozhi_chat.inc
.. include-build-file:: inc/esp_xiaozhi_info.inc
