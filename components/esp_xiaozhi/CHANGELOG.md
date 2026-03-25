# Changelog

## v0.1.0

### Added

- Initial release of Xiaozhi AI Chatbot component
- Support for bidirectional streaming dialogue with xiaozhi.me service
- Real-time voice/text interaction with AI agents
- Support for WebSocket and MQTT+UDP communication protocols
- Audio codec support: OPUS, G.711, and PCM formats
- MCP integration for device control
- Multi-language support: Chinese and English
- Board management API
- Settings management API
- MQTT communication API

### Features

- **esp_xiaozhi_chat**: Main chat API for voice/text interaction
- **esp_xiaozhi_mqtt**: MQTT communication layer
- **esp_xiaozhi_board**: Board management and UUID handling
- **esp_xiaozhi_keystore**: Keystore-based settings management
- **esp_xiaozhi_info**: Device information and OTA update API

### Dependencies

- ESP-IDF >= 5.5
- mcp-c-sdk: Model Context Protocol SDK
- Various ESP-IDF components (nvs_flash, mqtt, esp_http_client, etc.)

