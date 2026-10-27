# Changelog

## v0.1.3~1 - 2026-10-23

### Bundled example updates

- Update the bundled `examples/ai/xiaozhi_chat` example for ESP Board Manager v0.7.1 and `esp_codec_dev` v2.0.0-beta3.
- Migrate touch and ADC audio configuration to current Board Manager APIs and generated board defaults; require ESP-IDF >=5.5.3 for the example.
- No changes to the `esp_xiaozhi` component API or runtime behavior relative to v0.1.3.

## v0.1.3

### Fixed

- **MCP session lifecycle**: Do not clear MCP manager session state on server hello, goodbye, or audio channel close. Clearing it dropped tool state that the server still uses for the same MCP session.
- **MCP request context**: Handle inbound MCP with a NULL request context. Stamp the current audio `session_id` only on the reply.
- **Vision explain endpoint**: Store `initialize.params.capabilities.vision` on the chat handle through `esp_xiaozhi_vision_t`, and copy it out with `esp_xiaozhi_chat_get_vision_explain()`. Each agent keeps its own endpoint.
- **Vision endpoint refresh**: A re-initialize without a usable vision URL clears the previous endpoint. Oversized URLs or tokens are rejected with `ESP_ERR_INVALID_SIZE` instead of being truncated on save or copy-out.
- **MCP message logging**: Log only the MCP method and payload length. The full payload is no longer printed because `initialize` carries the vision bearer token.

## v0.1.2

### Fixed

- **MCP session lifecycle**: Clear MCP manager session state on server hello, goodbye, and audio channel close so MCP runtime state stays aligned with the audio session lifecycle.
- **MCP request context**: Set and reset per-request MCP context with the current audio `session_id` while handling inbound MCP messages.
- **MCP response logging**: Avoid warning logs when MCP request handling returns `ESP_OK` without a response payload.
- **Board JSON heap field**: Export `minimum_free_heap_size` as a string in board JSON to avoid a type/format mismatch when serializing `esp_get_minimum_free_heap_size()`.
- **MQTT endpoint parsing**: Added `esp_xiaozhi_mqtt_parse_endpoint()` to correctly parse broker URLs (`mqtt://`, `tcp://`, `mqtts://`, `ssl://`), optional ports, and bracketed IPv6 hosts; infer TCP vs SSL transport and attach the certificate bundle only for SSL connections.

## v0.1.1

This release updates `esp_xiaozhi` for build compatibility with both **ESP-IDF 5.5** and **ESP-IDF 6.0+**, while keeping the existing WebSocket and MQTT+UDP Xiaozhi chat APIs compatible with v0.1.0.

### Changed

- **Component version**: Bumped the component manifest version to **`0.1.1`**.
- **ESP-IDF version range**: Relaxed the manifest requirement from `>=5.5,<6` to **`>=5.5`** so IDF 6.0+ can resolve the component.
- **MCP dependency**: Updated the required `espressif/mcp-c-sdk` version to **`^2.0.1`**, which provides IDF 5.5 / IDF 6 compatible JSON and Mbed TLS handling.
- **JSON dependency**: Switched `PRIV_REQUIRES` from built-in `json` to managed **`cjson`**, and added **`espressif/cjson`** to the component manifest for all supported IDF versions.
- **MQTT dependency**: Added **`espressif/mqtt`** to the component manifest so ESP-MQTT resolves on IDF 6, where it is no longer a built-in component (managed `mqtt` is also used on IDF 5.5).
- **UDP audio payload**: Added Kconfig `XIAOZHI_UDP_MAX_AUDIO_PAYLOAD_SIZE` (default 1400) and enforce the limit when sending MQTT+UDP audio to reduce IP fragmentation risk.
- **UDP send behavior**: Added UDP send socket timeout and backpressure delay when the send path is congested.
- **Test app build wiring**: Updated `components/esp_xiaozhi/test_apps` to use the correct IDF test component path for IDF 5.5 vs IDF 6.0+, and to include the local `mcp-c-sdk` component for repository-local validation.

### Fixed

- **String copies (GCC 15)**: Replaced `strncpy`-based copies in keystore, board JSON export, and related paths with bounded `memcpy` / `snprintf` to avoid `-Wstringop-truncation` warnings.
- **IDF 6 Mbed TLS compatibility**: Replaced direct usage of `mbedtls/aes.h`, which is no longer a public header in IDF 6 / Mbed TLS 4.x.
- **MQTT+UDP AES-CTR audio encryption**: Added an internal AES-CTR wrapper:
  - IDF `< 6.0`: keeps the existing `mbedtls_aes_*` implementation.
  - IDF `>= 6.0`: uses the public PSA cipher API for AES-CTR encryption/decryption.
- **Repeated AES key setup**: Ensured the IDF 6 PSA key is destroyed before importing a replacement key, avoiding leaked volatile PSA keys when server hello provides a new UDP key.

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

