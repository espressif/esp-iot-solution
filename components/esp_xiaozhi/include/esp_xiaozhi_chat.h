/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_mcp_engine.h"
#include "esp_xiaozhi_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Audio packet for Xiaozhi chat; also used as audio params when passed to esp_xiaozhi_chat_open_audio_channel().
 *        For packet use: set sample_rate, frame_duration, timestamp, payload, payload_size (format/channels ignored).
 *        For open_audio_channel use: set format (NULL = "opus"), sample_rate (0 = 16000, otherwise 8000-48000),
 *        channels (0 = 1, otherwise 1-2), frame_duration (0 = 60, otherwise 10-120); payload/timestamp/payload_size ignored.
 */
typedef struct {
    const char *format;                             /*!< Audio format for hello, e.g. "opus", "pcm". NULL means "opus". Packet use: ignore */
    int sample_rate;                                /*!< Sample rate (Hz). For hello: 0 means 16000 */
    int channels;                                   /*!< Channel count for hello. 0 means 1. Packet use: ignore */
    int frame_duration;                             /*!< Frame duration (ms). For hello: 0 means 60 */
    uint32_t timestamp;                             /*!< Timestamp (packet use only) */
    uint8_t *payload;                               /*!< Payload (packet use only) */
    size_t payload_size;                            /*!< Payload size (packet use only) */
} esp_xiaozhi_chat_audio_t;

/**
* @brief  Handle for a Xiaozhi chat session.
*/
typedef uint32_t esp_xiaozhi_chat_handle_t;

/**
 * @brief  Event bits for ESP event system (app may register for these).
 *         These are the only event bits exposed to the app; do not add internal sync flags here.
 */
#define ESP_XIAOZHI_CHAT_EVENT_CONNECTED                (1 << 0)
#define ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED             (1 << 1)
#define ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED     (1 << 2)
#define ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED     (1 << 3)
#define ESP_XIAOZHI_CHAT_EVENT_AUDIO_DATA_INCOMING      (1 << 4)
#define ESP_XIAOZHI_CHAT_EVENT_SERVER_GOODBYE           (1 << 5)

/**
 * @brief  TTS state kind for protocol-layer notification (app decides device state)
 */
typedef enum {
    ESP_XIAOZHI_CHAT_TTS_STATE_START,          /*!< TTS playback started */
    ESP_XIAOZHI_CHAT_TTS_STATE_STOP,           /*!< TTS playback stopped */
    ESP_XIAOZHI_CHAT_TTS_STATE_SENTENCE_START, /*!< TTS sentence started; text is valid */
} esp_xiaozhi_chat_tts_state_kind_t;

/**
 * @brief  TTS state payload for ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE
 *         Pointers valid only during the event callback.
 */
typedef struct {
    esp_xiaozhi_chat_tts_state_kind_t state;  /*!< TTS state kind (start / stop / sentence_start) */
    const char *text;  /*!< Non-NULL only when state is SENTENCE_START */
} esp_xiaozhi_chat_tts_state_t;

/**
 * @brief  Error info for ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR (protocol layer only)
 *         Pointers valid only during the event callback.
 */
typedef struct {
    esp_err_t code;       /*!< Error code */
    const char *source;   /*!< Hint e.g. "transport", "hello_timeout", "udp" */
} esp_xiaozhi_chat_error_info_t;

/**
 * @brief  Events that can occur during a Xiaozhi chat session (minimal protocol API)
 *
 *  Component only reports protocol facts; app handles state machine, UI, and system commands.
 */
typedef enum {
    ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED = 0,   /*!< Emitted on TTS start; prefer CHAT_TTS_STATE for new code */
    ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STOPPED,      /*!< Emitted on TTS stop; prefer CHAT_TTS_STATE for new code */
    ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR,               /*!< event_data = esp_xiaozhi_chat_error_info_t * */
    ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT,                /*!< event_data = esp_xiaozhi_chat_text_data_t * (STT/TTS sentence) */
    ESP_XIAOZHI_CHAT_EVENT_CHAT_EMOJI,               /*!< event_data = const char * (LLM emotion) */
    ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE,           /*!< event_data = esp_xiaozhi_chat_tts_state_t * (protocol TTS state) */
    ESP_XIAOZHI_CHAT_EVENT_CHAT_SYSTEM_CMD,          /*!< event_data = const char * (e.g. "reboot"); app decides whether to execute */
    ESP_XIAOZHI_CHAT_EVENT_CHAT_JSON,                /*!< event_data = esp_xiaozhi_chat_json_t *; complete message, including unknown fields */
} esp_xiaozhi_chat_event_t;

/**
 * @brief  Supported audio formats for Xiaozhi chat
 */
typedef enum {
    ESP_XIAOZHI_CHAT_AUDIO_TYPE_OPUS,     /*!< OPUS compressed audio format */
} esp_xiaozhi_chat_audio_type_t;

/**
 * @brief  Device state for Xiaozhi chat
 */
typedef enum {
    ESP_XIAOZHI_CHAT_DEVICE_STATE_UNKNOWN,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_STARTING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_WIFI_CONFIGURING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_IDLE,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_CONNECTING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_LISTENING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_SPEAKING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_UPGRADING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_ACTIVATING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_AUDIO_TESTING,
    ESP_XIAOZHI_CHAT_DEVICE_STATE_FATAL_ERROR
} esp_xiaozhi_chat_device_state_t;

/**
 * @brief  Listening mode for Xiaozhi chat
 */
typedef enum {
    ESP_XIAOZHI_CHAT_LISTENING_MODE_REALTIME,
    ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO,
    ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL,
    ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO_STOP,
    ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL_STOP,
    ESP_XIAOZHI_CHAT_LISTENING_MODE_UNKNOWN,
} esp_xiaozhi_chat_listening_mode_t;

/**
 * @brief  Reasons for aborting speaking
 *
 */
typedef enum {
    ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED,
    ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_STOP_LISTENING,
    ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_UNKNOWN,
} esp_xiaozhi_chat_abort_speaking_reason_t;

/**
 * @brief  Text role enumeration for chat messages
 */
typedef enum {
    ESP_XIAOZHI_CHAT_TEXT_ROLE_USER,      /*!< User message role */
    ESP_XIAOZHI_CHAT_TEXT_ROLE_ASSISTANT, /*!< Assistant message role */
} esp_xiaozhi_chat_text_role_t;

/**
 * @brief  Text data structure for chat messages
 */
typedef struct {
    esp_xiaozhi_chat_text_role_t role; /*!< Role of the message (user or assistant) */
    const char *text;              /*!< Text content of the message */
} esp_xiaozhi_chat_text_data_t;

/**
 * @brief  Callback for receiving audio data during chat
 *
 * The @p data buffer is owned by the chat module and is only valid for the duration of
 * this callback. Implementations must consume or copy the data before returning and must
 * not store the pointer for asynchronous use.
 *
 * @param data  Pointer to the audio data buffer, valid only during this callback
 * @param len   Length of the audio data in bytes
 * @param ctx   User-defined context passed to the callback
 */
typedef void (*esp_xiaozhi_chat_audio_callback_t)(const uint8_t *data, int len, void *ctx);

/**
 * @brief  Complete incoming JSON payload for ESP_XIAOZHI_CHAT_EVENT_CHAT_JSON
 *
 *         All pointers are borrowed for the duration of the event callback and
 *         must be copied before asynchronous use.
 */
typedef struct {
    const char *type;   /*!< Message "type" field */
    const char *json;   /*!< Complete NUL-terminated incoming JSON string */
    size_t length;      /*!< Length of the JSON string in bytes, excluding terminating NUL */
} esp_xiaozhi_chat_json_t;

/**
 * @brief  Callback for receiving incoming audio packets with negotiated parameters and wire timestamp
 *
 *         The @p packet and its payload buffer are borrowed for the duration of this callback.
 *         Callbacks run on the receive task; defer blocking work and destruction to an application task.
 *
 * @param[in] packet  Pointer to the received audio packet metadata and payload
 * @param[in] ctx     User-defined context passed to the callback
 */
typedef void (*esp_xiaozhi_chat_packet_callback_t)(const esp_xiaozhi_chat_audio_t *packet, void *ctx);

/**
 * @brief  Callback for receiving chat events
 *
 * @param event       Chat event type
 * @param event_data  Optional output data associated with the event
 * @param ctx         User-defined context passed to the callback
 */
typedef void (*esp_xiaozhi_chat_event_callback_t)(esp_xiaozhi_chat_event_t event, void *event_data, void *ctx);

/**
 * @brief  Configuration structure for initializing a Xiaozhi chat session
 */
typedef struct {
    esp_xiaozhi_chat_audio_type_t audio_type;                /*!< Type of audio input/output to use */
    esp_xiaozhi_chat_audio_callback_t audio_callback;            /*!< Callback function for handling audio data */
    esp_xiaozhi_chat_event_callback_t event_callback;            /*!< Callback function for handling Xiaozhi events */
    void *audio_callback_ctx;        /*!< Context pointer passed to the audio callback */
    void *event_callback_ctx;        /*!< Context pointer passed to the event callback */
    esp_mcp_t *mcp_engine;           /*!< MCP engine instance provided by the caller */
    bool owns_mcp_engine;            /*!< Whether chat takes ownership of mcp_engine and destroys it in deinit */
    bool has_mqtt_config;           /*!< True if server provides MQTT config (from get_info). When both MQTT and WebSocket supported, prefer MQTT */
    bool has_websocket_config;      /*!< True if server provides WebSocket config (from get_info) */
    esp_xiaozhi_chat_packet_callback_t packet_callback; /*!< Optional metadata-preserving audio callback */
    void *packet_callback_ctx;      /*!< Context pointer passed to the packet callback */
} esp_xiaozhi_chat_config_t;

/**
 * @brief  Default configuration initializer for esp_xiaozhi_chat_config_t
 */
#define ESP_XIAOZHI_CHAT_DEFAULT_CONFIG() { \
    .audio_type = ESP_XIAOZHI_CHAT_AUDIO_TYPE_OPUS, \
    .audio_callback = NULL, \
    .event_callback = NULL, \
    .audio_callback_ctx = NULL, \
    .event_callback_ctx = NULL, \
    .mcp_engine = NULL, \
    .owns_mcp_engine = false, \
    .has_mqtt_config = false, \
    .has_websocket_config = false, \
    .packet_callback = NULL, \
    .packet_callback_ctx = NULL, \
}

/**
 * @brief  Send a complete protocol JSON object over the active control transport
 *
 *         Can be used to send MCP or application protocol messages even before a
 *         dialogue hello handshake completes.
 *
 * @param[in] chat_hd  Handle to the chat instance
 * @param[in] json     NUL-terminated JSON object string containing a string "type" field
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid handle, malformed JSON, missing "type", or oversized payload
 *       - ESP_ERR_INVALID_STATE  Transport is not started or not connected
 *       - Other                  Error from transport send
 */
esp_err_t esp_xiaozhi_chat_send_json(esp_xiaozhi_chat_handle_t chat_hd, const char *json);

/**
 * @brief  Wrap a JSON-RPC payload in a Xiaozhi MCP envelope and send it
 *
 * @param[in] chat_hd       Handle to the chat instance
 * @param[in] payload_json  NUL-terminated JSON-RPC object string to wrap under "payload"
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid handle, malformed JSON object, or oversized payload
 *       - ESP_ERR_INVALID_STATE  Transport is not started or not connected
 *       - ESP_ERR_NO_MEM         Failed to allocate outgoing message buffer
 *       - Other                  Error from transport send
 */
esp_err_t esp_xiaozhi_chat_send_mcp(esp_xiaozhi_chat_handle_t chat_hd, const char *payload_json);

/**
 * @brief  Send an audio packet while preserving its timestamp
 *
 * @param[in] chat_hd  Handle to the chat instance
 * @param[in] packet   Audio packet containing payload, payload_size, and timestamp
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid handle, packet, payload, or payload_size
 *       - ESP_ERR_INVALID_STATE  Audio channel is not ready
 *       - Other                  Error from transport send
 */
esp_err_t esp_xiaozhi_chat_send_audio_packet(esp_xiaozhi_chat_handle_t chat_hd, const esp_xiaozhi_chat_audio_t *packet);

/**
 * @brief  Copy negotiated audio parameters from the chat session
 *
 *         Fills @p params with negotiated format, sample_rate, channels, and frame_duration
 *         (payload is set to NULL). The @c format string pointer is borrowed from the chat
 *         instance and remains valid until the next channel negotiation or deinit; copy it
 *         for longer-lived use.
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * @param[out] params   Pointer to structure receiving the negotiated parameters
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid handle or params pointer
 */
esp_err_t esp_xiaozhi_chat_get_audio_params(esp_xiaozhi_chat_handle_t chat_hd, esp_xiaozhi_chat_audio_t *params);

/**
 * @brief  Query audio channel readiness, including errors and the 120-second inbound timeout
 *
 * @param[in] handle  Handle to the chat instance
 *
 * @return
 *       - true   Audio channel is open and healthy
 *       - false  Invalid handle, channel closed, transport disconnected, error occurred, or timed out
 */
bool esp_xiaozhi_chat_is_audio_channel_opened(esp_xiaozhi_chat_handle_t handle);

/**
 * @brief  Get independent MQTT+UDP probe health status
 *
 * @param[in]  handle  Handle to the chat instance
 * @param[out] info    Pointer to health info structure to populate
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid handle or info pointer
 *       - ESP_ERR_NOT_SUPPORTED  Session uses WebSocket instead of MQTT+UDP
 */
esp_err_t esp_xiaozhi_chat_get_udp_health(esp_xiaozhi_chat_handle_t handle, esp_xiaozhi_udp_health_info_t *info);

/**
 * @brief  Resolve vision explain configuration for one MCP tool invocation
 *
 *         Modern 2026-07-28 requests read only the current request's
 *         `me.xiaozhi/vision` extension. Legacy requests read the endpoint stored
 *         from the last initialize capabilities. Never falls back from a modern
 *         request to the legacy endpoint. Call from an MCP tool callback or
 *         request validator; output strings are copied to caller-owned buffers.
 *
 * @param[in]  handle      Handle to the chat instance
 * @param[in]  request     Optional request context for the current tool invocation (NULL selects legacy)
 * @param[out] url         Output buffer for the vision explain URL (cleared on error)
 * @param[in]  url_size    Size of @p url buffer in bytes, including terminating NUL
 * @param[out] token       Optional output buffer for the bearer token (NULL skips copy)
 * @param[in]  token_size  Size of @p token buffer in bytes, including terminating NUL (must be > 0 when @p token is non-NULL)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid arguments or invalid vision URL/token format
 *       - ESP_ERR_INVALID_SIZE   Output @p url or @p token buffer is too small
 *       - ESP_ERR_INVALID_STATE  Legacy vision URL is not set
 *       - ESP_ERR_NOT_SUPPORTED  CONFIG_XIAOZHI_VISION_ENABLE is disabled (legacy path)
 */
esp_err_t esp_xiaozhi_chat_get_request_vision(esp_xiaozhi_chat_handle_t handle,
                                              const esp_mcp_request_context_t *request,
                                              char *url, size_t url_size, char *token, size_t token_size);

/** @cond **/
ESP_EVENT_DECLARE_BASE(ESP_XIAOZHI_CHAT_EVENTS);
/** @endcond **/

/**
 * @brief  Instance the chat module
 *
 * The current implementation supports only one chat instance at a time.
 *
 * @param[in]   config   Pointer to the chat configuration structure
 * @param[out]  chat_hd  Pointer to the chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_ERR_INVALID_STATE Another chat instance is already active
 */
esp_err_t esp_xiaozhi_chat_init(esp_xiaozhi_chat_config_t *config, esp_xiaozhi_chat_handle_t *chat_hd);

/**
 * @brief  Deinitialize the chat module
 *
 * This function releases chat-owned resources. If the chat session is still running, it
 * will stop runtime resources first. The MCP engine is destroyed only when
 * `config.owns_mcp_engine` was set to true during init.
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 */
esp_err_t esp_xiaozhi_chat_deinit(esp_xiaozhi_chat_handle_t chat_hd);

/**
 * @brief  Start the chat session
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - ESP_ERR_NOT_FOUND     Required transport configuration is missing
 *       - Other    Error from transport start (MQTT or WebSocket)
 */
esp_err_t esp_xiaozhi_chat_start(esp_xiaozhi_chat_handle_t chat_hd);

/**
 * @brief  Stop the chat session
 *
 * Stops the active chat runtime, including audio channel and MCP manager resources, but
 * does not destroy the configured MCP engine.
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 */
esp_err_t esp_xiaozhi_chat_stop(esp_xiaozhi_chat_handle_t chat_hd);

/**
 * @brief  Open audio channel
 *
 * @param[in]  chat_hd     Handle to the chat instance
 * @param[in]  audio       Optional audio params for the generated hello (format, sample_rate, channels, frame_duration). Used only when message is NULL. NULL or zero fields mean defaults: "opus", 16000, 1, 60. Non-zero values must be within valid protocol ranges.
 * @param[in]  message     Optional message to send when opening the channel. If NULL, a default hello message will be generated
 * @param[in]  message_len Length of the message buffer. If 0 with message NULL, a default hello is generated; if message is non-NULL, message_len must be > 0 (otherwise returns ESP_ERR_INVALID_ARG)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle, invalid audio params, or invalid message/message_len combination
 *       - ESP_ERR_NO_MEM        Failed to allocate hello message buffer
 *       - ESP_ERR_INVALID_SIZE   Hello message buffer too small
 *       - Other    Error from get_hello_message, transport_send_text, or audio_open
 */
esp_err_t esp_xiaozhi_chat_open_audio_channel(esp_xiaozhi_chat_handle_t chat_hd, const esp_xiaozhi_chat_audio_t *audio,
                                              char *message, size_t message_len);

/**
 * @brief  Close audio channel
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 */
esp_err_t esp_xiaozhi_chat_close_audio_channel(esp_xiaozhi_chat_handle_t chat_hd);

/**
 * @brief  Send audio data to the chat session
 *
 * @param[in]  chat_hd   Handle to the chat instance
 * @param[in]  data      Pointer to the audio data buffer
 * @param[in]  data_len  Length of the audio data in bytes
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle, data, or data_len is 0
 *       - ESP_ERR_INVALID_STATE   Transport not ready for binary (e.g. audio channel not open)
 *       - Other    Error from transport_send_binary
 */
esp_err_t esp_xiaozhi_chat_send_audio_data(esp_xiaozhi_chat_handle_t chat_hd, const char *data, size_t data_len);

/**
 * @brief  Send wake word detected
 *
 * @param[in]  chat_hd    Handle to the chat instance
 * @param[in]  wake_word  Pointer to the wake word (non-empty string)
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle or wake_word
 *       - ESP_ERR_INVALID_STATE   No session (open audio channel first)
 *       - ESP_ERR_NO_MEM        Failed to create JSON
 *       - Other    Error from transport_send_text
 */
esp_err_t esp_xiaozhi_chat_send_wake_word(esp_xiaozhi_chat_handle_t chat_hd, const char *wake_word);

/**
 * @brief  Send start listening
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * @param[in]  mode    Listening mode
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - ESP_ERR_INVALID_STATE   No session (open audio channel first)
 *       - ESP_ERR_NO_MEM        Failed to create JSON
 *       - Other    Error from transport_send_text
 */
esp_err_t esp_xiaozhi_chat_send_start_listening(esp_xiaozhi_chat_handle_t chat_hd, int mode);

/**
 * @brief  Send stop listening
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - ESP_ERR_INVALID_STATE   No session (open audio channel first)
 *       - ESP_ERR_NO_MEM        Failed to create JSON
 *       - Other    Error from transport_send_text
 */
esp_err_t esp_xiaozhi_chat_send_stop_listening(esp_xiaozhi_chat_handle_t chat_hd);

/**
 * @brief  Send abort speaking
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * @param[in]  reason   Reason for aborting speaking
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid handle
 *       - ESP_ERR_INVALID_STATE   No session (open audio channel first)
 *       - ESP_ERR_NO_MEM        Failed to create JSON
 *       - Other    Error from transport_send_text
 */
esp_err_t esp_xiaozhi_chat_send_abort_speaking(esp_xiaozhi_chat_handle_t chat_hd, esp_xiaozhi_chat_abort_speaking_reason_t reason);

/**
 * @brief  Copy this chat's vision explain endpoint
 *
 *         Xiaozhi sends the endpoint in initialize.params.capabilities.vision.
 *         The URL is an HTTP upload target, not the OTA host. Each chat handle
 *         keeps its own copy, so two agents do not share it. Empty until that
 *         message arrives. Not thread-safe. Not ISR-safe.
 *
 * @param[in]   chat_hd     Chat handle returned by esp_xiaozhi_chat_init()
 * @param[out]  url         Explain URL buffer; cleared when it is too small
 * @param[in]   url_size    Size of url, including the terminating NUL
 * @param[out]  token       Optional bearer token buffer. NULL skips the copy
 * @param[in]   token_size  Size of token, including the terminating NUL. Must be
 *                          non-zero when token is not NULL
 *
 * @return
 *       - ESP_OK                  On success; url and token are complete copies
 *       - ESP_ERR_INVALID_ARG     If chat_hd is 0, url is NULL, url_size is 0,
 *                                 or token is not NULL and token_size is 0
 *       - ESP_ERR_NOT_SUPPORTED   If CONFIG_XIAOZHI_VISION_ENABLE is disabled
 *       - ESP_ERR_INVALID_STATE   If this chat has not received a vision URL
 *       - ESP_ERR_INVALID_SIZE    If url or token buffer is too small
 */
esp_err_t esp_xiaozhi_chat_get_vision_explain(esp_xiaozhi_chat_handle_t chat_hd, char *url, size_t url_size,
                                              char *token, size_t token_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
