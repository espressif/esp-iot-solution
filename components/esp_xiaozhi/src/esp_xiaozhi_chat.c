/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <ctype.h>

#include <esp_check.h>
#include <esp_log.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>
#include <mbedtls/aes.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <esp_event.h>
#include <esp_heap_caps.h>

#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"

#include "esp_xiaozhi_mcp.h"
#include "esp_xiaozhi_transport.h"
#include "esp_xiaozhi_chat.h"

#define ESP_XIAOZHI_CHAT_MAX_STRING_SIZE 256
#define ESP_XIAOZHI_CHAT_AES_NONCE_SIZE 16
#define ESP_XIAOZHI_CHAT_AES_KEY_SIZE 16

#define ESP_XIAOZHI_CHAT_CLOSE_MUTEX_RETRY_MS   1000
#define ESP_XIAOZHI_CHAT_CLOSE_MUTEX_RETRY_CNT 3
#define ESP_XIAOZHI_CHAT_TASK_EXIT_WAIT_MS 2000
#define ESP_XIAOZHI_CHAT_UDP_RECV_TIMEOUT_MS 200
#define ESP_XIAOZHI_CHAT_UDP_MIN_PORT 1
#define ESP_XIAOZHI_CHAT_UDP_MAX_PORT 65535

#define ESP_XIAOZHI_CHAT_TASK_NAME "chat_audio"
#define ESP_XIAOZHI_CHAT_MIN_SAMPLE_RATE 8000
#define ESP_XIAOZHI_CHAT_MAX_SAMPLE_RATE 48000
#define ESP_XIAOZHI_CHAT_MIN_CHANNELS 1
#define ESP_XIAOZHI_CHAT_MAX_CHANNELS 2
#define ESP_XIAOZHI_CHAT_MIN_FRAME_DURATION_MS 10
#define ESP_XIAOZHI_CHAT_MAX_FRAME_DURATION_MS 120

/* Internal sync bits: server hello received, receive enabled, shutdown, task exited */
#define ESP_XIAOZHI_CHAT_SERVER_HELLO          (1 << 0) /*!< Server hello received */
#define ESP_XIAOZHI_CHAT_RECV_RECEIVE_ENABLED  (1 << 1) /*!< Receive enabled */
#define ESP_XIAOZHI_CHAT_RECV_SHUTDOWN         (1 << 2) /*!< Shutdown */
#define ESP_XIAOZHI_CHAT_RECV_TASK_EXITED      (1 << 3) /*!< Task exited */

/**
 * @brief  UDP context for MQTT path (audio over UDP). NULL when using WebSocket.
 *        Audio input task uses static stack/TCB memory, is created on first open,
 *        kept alive across close, and exits during deinit.
 */
typedef struct {
    SemaphoreHandle_t channel_mutex;
    EventGroupHandle_t audio_receive_events;   /*!< RECEIVE_ENABLED: may receive; SHUTDOWN: task must exit */
    int udp_socket;
    struct sockaddr_in udp_addr;
    char udp_server[ESP_XIAOZHI_CHAT_MAX_STRING_SIZE];
    int udp_port;
    mbedtls_aes_context aes_ctx;
    uint8_t aes_nonce[ESP_XIAOZHI_CHAT_AES_NONCE_SIZE];
    uint32_t local_sequence;
    uint32_t remote_sequence;
    uint32_t last_incoming_time;
} esp_xiaozhi_chat_udp_ctx_t;

/**
 * @brief  Xiaozhi chat
 */
typedef struct {
    bool use_mqtt;                                      /*!< Use MQTT for this session (prefer MQTT when both supported) */
    bool owns_mcp_engine;                               /*!< Whether this instance owns mcp_engine */
    bool error_occurred;                                /*!< Error occurred */
    bool chat_connected;                                /*!< Chat connected */
    bool audio_channel_opened;                          /*!< Audio channel opened */
    esp_xiaozhi_chat_device_state_t device_state;       /*!< Device state */
    esp_xiaozhi_chat_listening_mode_t listening_mode;   /*!< Listening mode */
    esp_xiaozhi_transport_handle_t mcp_chat_handle;           /*!< Transport handle for MCP chat */
    EventGroupHandle_t event_group_handle;              /*!< Event group for handling events */

    esp_mcp_t *mcp_engine;                              /*!< MCP engine instance for processing MCP protocol */
    esp_mcp_mgr_handle_t mcp_mgr_handle;                /*!< MCP manager handle */

    esp_xiaozhi_chat_udp_ctx_t *udp;                    /*!< UDP context for MQTT path; NULL when WebSocket */

    char session_id[ESP_XIAOZHI_CHAT_MAX_STRING_SIZE];  /*!< Session ID */
    int server_sample_rate;                             /*!< Server sample rate */
    int server_frame_duration;                          /*!< Server frame duration */

    esp_xiaozhi_chat_audio_type_t       audio_type;     /*!< Audio type */
    esp_xiaozhi_chat_audio_callback_t   audio_callback; /*!< Callback function for handling audio data */
    esp_xiaozhi_chat_event_callback_t   event_callback; /*!< Callback function for handling events */
    void                           *audio_callback_ctx; /*!< Context pointer passed to the audio callback */
    void                           *event_callback_ctx; /*!< Context pointer passed to the event callback */
} esp_xiaozhi_chat_t;

/**
 * @brief  Type function for Xiaozhi chat
 */
typedef struct {
    const char *type;
    esp_err_t (*handler)(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root);
} esp_xiaozhi_chat_type_func_t;

ESP_EVENT_DEFINE_BASE(ESP_XIAOZHI_CHAT_EVENTS);

static const char *TAG = "ESP_XIAOZHI_CHAT";

static esp_xiaozhi_chat_t *s_xiaozhi_chat = NULL;
static portMUX_TYPE s_xiaozhi_chat_lock = portMUX_INITIALIZER_UNLOCKED;
#if CONFIG_XIAOZHI_AUDIO_TASK_ALLOC_STATIC
static StaticTask_t s_audio_input_task_tcb;
static StackType_t s_audio_input_task_stack[CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE];
#endif

static void esp_xiaozhi_chat_audio_input(void *pvParameter);

static esp_xiaozhi_chat_t *esp_xiaozhi_chat_get_instance(void)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = NULL;

    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    xiaozhi_chat = s_xiaozhi_chat;
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);

    return xiaozhi_chat;
}

static esp_err_t esp_xiaozhi_chat_register_instance(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid chat instance");

    esp_err_t ret = ESP_OK;
    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    if (s_xiaozhi_chat != NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        s_xiaozhi_chat = xiaozhi_chat;
    }
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);

    return ret;
}

static void esp_xiaozhi_chat_unregister_instance(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    if (s_xiaozhi_chat == xiaozhi_chat) {
        s_xiaozhi_chat = NULL;
    }
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
}

static uint8_t char_to_hex(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return 0;
}

static void esp_xiaozhi_chat_decode_hex_string(const char *hex_string, uint8_t *output, size_t output_size)
{
    if (!hex_string || !output) {
        return;
    }

    size_t hex_len = strlen(hex_string);
    size_t decode_len = hex_len / 2;
    if (decode_len > output_size) {
        decode_len = output_size;
    }

    for (size_t i = 0; i < decode_len; i++) {
        output[i] = (char_to_hex(hex_string[i * 2]) << 4) | char_to_hex(hex_string[i * 2 + 1]);
    }
}

static esp_xiaozhi_chat_t *esp_xiaozhi_chat_get_by_transport(esp_xiaozhi_transport_handle_t handle)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_instance();
    if (xiaozhi_chat && xiaozhi_chat->mcp_chat_handle == handle) {
        return xiaozhi_chat;
    }

    return NULL;
}

static bool esp_xiaozhi_chat_is_audio_ready(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat, false, TAG, "Invalid chat");

    if (!xiaozhi_chat->audio_channel_opened || xiaozhi_chat->error_occurred) {
        return false;
    }
    if (!xiaozhi_chat->use_mqtt) {
        return true;
    }
    return xiaozhi_chat->udp != NULL && xiaozhi_chat->udp->udp_socket != -1;
}

static esp_err_t esp_xiaozhi_chat_set_listening_mode(esp_xiaozhi_chat_t *xiaozhi_chat, esp_xiaozhi_chat_listening_mode_t mode)
{
    const char *const mode_str[] = {
        "unknown",
        "realtime",
        "auto",
        "manual",
        "manual_stop",
        "auto_stop"
    };

    if (xiaozhi_chat->listening_mode == mode) {
        ESP_LOGD(TAG, "Listening mode is already %s", mode_str[mode]);
        return ESP_OK;
    }

    xiaozhi_chat->listening_mode = mode;
    ESP_LOGD(TAG, "Listening mode: %s", mode_str[mode]);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_set_device_state(esp_xiaozhi_chat_t *xiaozhi_chat, esp_xiaozhi_chat_device_state_t state)
{
    if (xiaozhi_chat->device_state == state) {
        ESP_LOGD(TAG, "Device state(%d) is already", state);
        return ESP_OK;
    }
    xiaozhi_chat->device_state = state;
    ESP_LOGD(TAG, "Device state: %d", xiaozhi_chat->device_state);
    /* App derives device state from CHAT_TTS_STATE / connection events; no CHAT_STATE event */
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_hello_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    const cJSON *transport = cJSON_GetObjectItem(root, "transport");
    if (!transport || !cJSON_IsString(transport) ||
            (strcmp(transport->valuestring, "udp") != 0 && strcmp(transport->valuestring, "websocket") != 0)) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport ? transport->valuestring : "null");
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id)) {
        uint16_t session_id_len = strlen(session_id->valuestring);
        if (session_id_len > sizeof(xiaozhi_chat->session_id) - 1) {
            ESP_LOGE(TAG, "Session ID is too long: %u > %u", session_id_len, sizeof(xiaozhi_chat->session_id) - 1);
            return ESP_ERR_INVALID_ARG;
        }
        for (uint16_t i = 0; i < session_id_len; i++) {
            char c = session_id->valuestring[i];
            if (!isalnum((unsigned char)c) && c != '-' && c != '_') {
                ESP_LOGE(TAG, "Invalid session ID character at position %u", i);
                return ESP_ERR_INVALID_ARG;
            }
        }
        strncpy(xiaozhi_chat->session_id, session_id->valuestring, session_id_len);
        xiaozhi_chat->session_id[session_id_len] = '\0';
        ESP_LOGD(TAG, "Session established");
    }

    const cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (cJSON_IsObject(audio_params)) {
        const cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (cJSON_IsNumber(sample_rate)) {
            xiaozhi_chat->server_sample_rate = sample_rate->valueint;
        }

        const cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (cJSON_IsNumber(frame_duration)) {
            xiaozhi_chat->server_frame_duration = frame_duration->valueint;
        }
    }

    if (strcmp(transport->valuestring, "udp") == 0) {
        const cJSON *udp = cJSON_GetObjectItem(root, "udp");
        if (!cJSON_IsObject(udp)) {
            ESP_LOGE(TAG, "UDP is not specified");
            return ESP_ERR_INVALID_ARG;
        }

        const cJSON *server = cJSON_GetObjectItem(udp, "server");
        const cJSON *port = cJSON_GetObjectItem(udp, "port");
        const cJSON *key = cJSON_GetObjectItem(udp, "key");
        const cJSON *nonce = cJSON_GetObjectItem(udp, "nonce");

        if (xiaozhi_chat->udp) {
            if (server && cJSON_IsString(server)) {
                uint16_t server_len = strlen(server->valuestring);
                if (server_len > sizeof(xiaozhi_chat->udp->udp_server) - 1) {
                    ESP_LOGE(TAG, "Server is too long: %u > %u", server_len, sizeof(xiaozhi_chat->udp->udp_server) - 1);
                    return ESP_ERR_INVALID_ARG;
                }
                strncpy(xiaozhi_chat->udp->udp_server, server->valuestring, server_len);
                xiaozhi_chat->udp->udp_server[server_len] = '\0';
            }
            if (port && cJSON_IsNumber(port)) {
                if (port->valueint <= ESP_XIAOZHI_CHAT_UDP_MIN_PORT || port->valueint > ESP_XIAOZHI_CHAT_UDP_MAX_PORT) {
                    ESP_LOGE(TAG, "Invalid port: %d", port->valueint);
                    return ESP_ERR_INVALID_ARG;
                }
                xiaozhi_chat->udp->udp_port = port->valueint;
            }
            if (key && cJSON_IsString(key) && nonce && cJSON_IsString(nonce)) {
                uint8_t aes_key[ESP_XIAOZHI_CHAT_AES_KEY_SIZE] = {0};
                size_t nonce_len = strlen(nonce->valuestring);
                size_t key_len = strlen(key->valuestring);
                if (nonce_len != ESP_XIAOZHI_CHAT_AES_NONCE_SIZE * 2) {
                    ESP_LOGE(TAG, "Invalid nonce length: %zu (expected %zu)", nonce_len, ESP_XIAOZHI_CHAT_AES_NONCE_SIZE * 2);
                    return ESP_ERR_INVALID_ARG;
                }
                if (key_len != ESP_XIAOZHI_CHAT_AES_KEY_SIZE * 2) {
                    ESP_LOGE(TAG, "Invalid key length: %d (expected %d)", key_len, ESP_XIAOZHI_CHAT_AES_KEY_SIZE * 2);
                    return ESP_ERR_INVALID_ARG;
                }
                for (size_t i = 0; i < nonce_len; i++) {
                    if (!isxdigit((unsigned char)nonce->valuestring[i])) {
                        ESP_LOGE(TAG, "Invalid nonce character at position %zu", i);
                        return ESP_ERR_INVALID_ARG;
                    }
                }
                for (size_t i = 0; i < key_len; i++) {
                    if (!isxdigit((unsigned char)key->valuestring[i])) {
                        ESP_LOGE(TAG, "Invalid key character at position %zu", i);
                        return ESP_ERR_INVALID_ARG;
                    }
                }
                esp_xiaozhi_chat_decode_hex_string(nonce->valuestring, xiaozhi_chat->udp->aes_nonce, ESP_XIAOZHI_CHAT_AES_NONCE_SIZE);
                esp_xiaozhi_chat_decode_hex_string(key->valuestring, aes_key, ESP_XIAOZHI_CHAT_AES_KEY_SIZE);

                mbedtls_aes_init(&xiaozhi_chat->udp->aes_ctx);
                mbedtls_aes_setkey_enc(&xiaozhi_chat->udp->aes_ctx, aes_key, 128);
                memset(aes_key, 0, ESP_XIAOZHI_CHAT_AES_KEY_SIZE);

                xiaozhi_chat->udp->local_sequence = 0;
                xiaozhi_chat->udp->remote_sequence = 0;
            }
        }
    }
    xEventGroupSetBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_SERVER_HELLO);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_goodbye_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    if (session_id == NULL || strcmp(xiaozhi_chat->session_id, session_id->valuestring) == 0) {
        esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_SERVER_GOODBYE, NULL, 0, portMAX_DELAY);
    }

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_mcp_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    cJSON *payload_obj = cJSON_GetObjectItem(root, "payload");
    if (!payload_obj || !cJSON_IsObject(payload_obj)) {
        ESP_LOGE(TAG, "Invalid MCP payload");
        return ESP_ERR_INVALID_ARG;
    }

    if (!xiaozhi_chat->mcp_mgr_handle) {
        ESP_LOGW(TAG, "MCP manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    char *payload_mcp = cJSON_PrintUnformatted(payload_obj);
    if (!payload_mcp) {
        ESP_LOGE(TAG, "Failed to print MCP payload");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Received MCP message: %s", payload_mcp);

    esp_err_t ret = esp_xiaozhi_mcp_handle_message(xiaozhi_chat->mcp_mgr_handle,
                                                   "mcp_server",
                                                   payload_mcp,
                                                   strlen(payload_mcp),
                                                   xiaozhi_chat->session_id);

    cJSON_free(payload_mcp);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to process MCP message: %s", esp_err_to_name(ret));
    }

    return ret;
}

static esp_err_t esp_xiaozhi_chat_tts_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (!cJSON_IsString(state)) {
        return ESP_OK;
    }
    ESP_LOGD(TAG, "TTS state: %s", state->valuestring);

    if (xiaozhi_chat->event_callback == NULL) {
        return ESP_OK;
    }

    if (strcmp(state->valuestring, "start") == 0) {
        esp_xiaozhi_chat_tts_state_t tts_state = {
            .state = ESP_XIAOZHI_CHAT_TTS_STATE_START,
            .text = NULL,
        };
        xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE, &tts_state, xiaozhi_chat->event_callback_ctx);
        xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED, NULL, xiaozhi_chat->event_callback_ctx);
    } else if (strcmp(state->valuestring, "stop") == 0) {
        esp_xiaozhi_chat_tts_state_t tts_state = {
            .state = ESP_XIAOZHI_CHAT_TTS_STATE_STOP,
            .text = NULL,
        };
        xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE, &tts_state, xiaozhi_chat->event_callback_ctx);
        xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STOPPED, NULL, xiaozhi_chat->event_callback_ctx);
    } else if (strcmp(state->valuestring, "sentence_start") == 0) {
        cJSON *text = cJSON_GetObjectItem(root, "text");
        const char *text_str = (cJSON_IsString(text)) ? text->valuestring : NULL;
        if (text_str) {
            ESP_LOGD(TAG, "TTS sentence text: %s", text_str);
            esp_xiaozhi_chat_tts_state_t tts_state = {
                .state = ESP_XIAOZHI_CHAT_TTS_STATE_SENTENCE_START,
                .text = text_str,
            };
            xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE, &tts_state, xiaozhi_chat->event_callback_ctx);
            esp_xiaozhi_chat_text_data_t text_data = {
                .role = ESP_XIAOZHI_CHAT_TEXT_ROLE_ASSISTANT,
                .text = text_str,
            };
            xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT, &text_data, xiaozhi_chat->event_callback_ctx);
        }
    }

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_stt_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (cJSON_IsString(text)) {
        ESP_LOGD(TAG, "STT text: %s", text->valuestring);
        esp_xiaozhi_chat_text_data_t text_data = {
            .role = ESP_XIAOZHI_CHAT_TEXT_ROLE_USER,
            .text = text->valuestring,
        };
        if (xiaozhi_chat->event_callback) {
            xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT, &text_data, xiaozhi_chat->event_callback_ctx);
        }
    }

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_llm_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
    if (cJSON_IsString(emotion)) {
        ESP_LOGD(TAG, "LLM emotion: %s", emotion->valuestring);
        if (xiaozhi_chat->event_callback) {
            xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_EMOJI, emotion->valuestring, xiaozhi_chat->event_callback_ctx);
        }
    }

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_system_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (!cJSON_IsString(command)) {
        return ESP_OK;
    }
    ESP_LOGD(TAG, "System command: %s", command->valuestring);
    /* Protocol layer only reports; app decides whether to execute (e.g. esp_restart for "reboot") */
    if (xiaozhi_chat->event_callback) {
        xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_SYSTEM_CMD, command->valuestring, xiaozhi_chat->event_callback_ctx);
    }
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_binary_handler(esp_xiaozhi_transport_handle_t handle, const uint8_t *data, size_t data_len)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    if (!xiaozhi_chat) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xiaozhi_chat->audio_callback) {
        xiaozhi_chat->audio_callback(data, data_len, xiaozhi_chat->audio_callback_ctx);
    }

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_data_handler(esp_xiaozhi_transport_handle_t handle, const char *topic, size_t topic_len, const char *data, size_t data_len)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    if (!xiaozhi_chat) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON message: %.*s", (int)data_len, data);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        ESP_LOGE(TAG, "Message type is invalid");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    esp_xiaozhi_chat_type_func_t esp_xiaozhi_chat_type_handlers[] = {
        { "hello",   esp_xiaozhi_chat_hello_handler },
        { "goodbye", esp_xiaozhi_chat_goodbye_handler },
        { "mcp",     esp_xiaozhi_chat_mcp_handler },
        { "tts",     esp_xiaozhi_chat_tts_handler },
        { "stt",     esp_xiaozhi_chat_stt_handler },
        { "llm",     esp_xiaozhi_chat_llm_handler },
        { "system",  esp_xiaozhi_chat_system_handler },
    };

    for (size_t i = 0; i < sizeof(esp_xiaozhi_chat_type_handlers) / sizeof(esp_xiaozhi_chat_type_handlers[0]); i++) {
        if (strcmp(type->valuestring, esp_xiaozhi_chat_type_handlers[i].type) == 0) {
            ret = esp_xiaozhi_chat_type_handlers[i].handler(xiaozhi_chat, root);
            break;
        }
    }

    cJSON_Delete(root);

    return ret;
}

static esp_err_t esp_xiaozhi_chat_connected_handler(esp_xiaozhi_transport_handle_t handle)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    xiaozhi_chat->chat_connected = true;

    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_disconnected_handler(esp_xiaozhi_transport_handle_t handle)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    xiaozhi_chat->chat_connected = false;

    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_transport_error_handler(esp_xiaozhi_transport_handle_t handle, esp_err_t err)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    if (xiaozhi_chat->event_callback) {
        esp_xiaozhi_chat_error_info_t info = {
            .code = err,
            .source = "transport",
        };
        xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR, &info, xiaozhi_chat->event_callback_ctx);
    }
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_send_json_message(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(root, ESP_ERR_INVALID_ARG, TAG, "Invalid root");

    char *json_str = cJSON_PrintUnformatted(root);
    ESP_RETURN_ON_FALSE(json_str, ESP_ERR_NO_MEM, TAG, "Failed to print JSON");

    esp_err_t ret = esp_xiaozhi_transport_send_text(xiaozhi_chat->mcp_chat_handle, json_str);
    cJSON_free(json_str);

    return ret;
}

static esp_err_t esp_xiaozhi_chat_create_session_message(esp_xiaozhi_chat_t *xiaozhi_chat, const char *type, cJSON **out_root)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(type != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid type");
    ESP_RETURN_ON_FALSE(out_root != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid output");
    ESP_RETURN_ON_FALSE(strlen(xiaozhi_chat->session_id) > 0, ESP_ERR_INVALID_STATE, TAG, "No session, open audio channel first");

    cJSON *root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create JSON object");

    cJSON_AddStringToObject(root, "session_id", xiaozhi_chat->session_id);
    cJSON_AddStringToObject(root, "type", type);
    *out_root = root;
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_validate_audio_params(const esp_xiaozhi_chat_audio_t *audio)
{
    if (audio == NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(audio->format == NULL || audio->format[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "Audio format must not be empty");
    ESP_RETURN_ON_FALSE(audio->sample_rate == 0 ||
                        (audio->sample_rate >= ESP_XIAOZHI_CHAT_MIN_SAMPLE_RATE &&
                         audio->sample_rate <= ESP_XIAOZHI_CHAT_MAX_SAMPLE_RATE),
                        ESP_ERR_INVALID_ARG, TAG, "Audio sample_rate out of range");
    ESP_RETURN_ON_FALSE(audio->channels == 0 ||
                        (audio->channels >= ESP_XIAOZHI_CHAT_MIN_CHANNELS &&
                         audio->channels <= ESP_XIAOZHI_CHAT_MAX_CHANNELS),
                        ESP_ERR_INVALID_ARG, TAG, "Audio channels out of range");
    ESP_RETURN_ON_FALSE(audio->frame_duration == 0 ||
                        (audio->frame_duration >= ESP_XIAOZHI_CHAT_MIN_FRAME_DURATION_MS &&
                         audio->frame_duration <= ESP_XIAOZHI_CHAT_MAX_FRAME_DURATION_MS),
                        ESP_ERR_INVALID_ARG, TAG, "Audio frame_duration out of range");

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_get_hello_message(esp_xiaozhi_chat_t *xiaozhi_chat, const esp_xiaozhi_chat_audio_t *audio,
                                                    char *message, uint16_t message_len)
{
    const char *format = (audio && audio->format && audio->format[0] != '\0') ? audio->format : "opus";
    int sample_rate = (audio && audio->sample_rate > 0) ? audio->sample_rate : 16000;
    int channels = (audio && audio->channels > 0) ? audio->channels : 1;
    int frame_duration = (audio && audio->frame_duration > 0) ? audio->frame_duration : 60;

    cJSON *root = cJSON_CreateObject();
    ESP_RETURN_ON_FALSE(root, ESP_ERR_NO_MEM, TAG, "Failed to create JSON object");

    cJSON_AddStringToObject(root, "type", "hello");
    if (xiaozhi_chat->use_mqtt) {
        cJSON_AddNumberToObject(root, "version", 3);
        cJSON_AddStringToObject(root, "transport", "udp");
    } else {
        cJSON_AddNumberToObject(root, "version", 1);
        cJSON_AddStringToObject(root, "transport", "websocket");
    }

    cJSON *features = cJSON_CreateObject();
    if (!features) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddBoolToObject(features, "mcp", true);
    cJSON_AddItemToObject(root, "features", features);

    cJSON *audio_params = cJSON_CreateObject();
    if (!audio_params) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(audio_params, "format", format);
    cJSON_AddNumberToObject(audio_params, "sample_rate", sample_rate);
    cJSON_AddNumberToObject(audio_params, "channels", channels);
    cJSON_AddNumberToObject(audio_params, "frame_duration", frame_duration);
    cJSON_AddItemToObject(root, "audio_params", audio_params);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(json_str, ESP_ERR_NO_MEM, TAG, "Failed to print JSON");

    uint16_t json_len = (uint16_t)strlen(json_str);
    if (json_len >= message_len) {
        cJSON_free(json_str);
        ESP_LOGE(TAG, "Hello message buffer too small: need %u, have %u", json_len + 1, message_len);
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(message, json_str, json_len + 1);
    cJSON_free(json_str);

    return ESP_OK;
}

static void esp_xiaozhi_chat_audio_input_exit(EventGroupHandle_t evt)
{
    if (evt != NULL) {
        xEventGroupSetBits(evt, ESP_XIAOZHI_CHAT_RECV_TASK_EXITED);
    }
    vTaskDelete(NULL);
}

static esp_err_t esp_xiaozhi_chat_create_audio_input_task(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    BaseType_t core = (CONFIG_XIAOZHI_AUDIO_TASK_CORE < 0) ? tskNO_AFFINITY : CONFIG_XIAOZHI_AUDIO_TASK_CORE;
    TaskHandle_t task_handle = NULL;

#if CONFIG_XIAOZHI_AUDIO_TASK_ALLOC_STATIC
    memset(&s_audio_input_task_tcb, 0, sizeof(s_audio_input_task_tcb));
    task_handle = xTaskCreateStaticPinnedToCore(esp_xiaozhi_chat_audio_input,
                                                ESP_XIAOZHI_CHAT_TASK_NAME,
                                                CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE,
                                                xiaozhi_chat,
                                                CONFIG_XIAOZHI_AUDIO_TASK_PRIORITY,
                                                s_audio_input_task_stack,
                                                &s_audio_input_task_tcb,
                                                core);
    ESP_RETURN_ON_FALSE(task_handle != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create audio input task");
#else
    UBaseType_t stack_caps = CONFIG_XIAOZHI_STACK_IN_PSRAM
                             ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                             : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    BaseType_t task_ret = xTaskCreatePinnedToCoreWithCaps(esp_xiaozhi_chat_audio_input,
                                                          ESP_XIAOZHI_CHAT_TASK_NAME,
                                                          CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE,
                                                          xiaozhi_chat,
                                                          CONFIG_XIAOZHI_AUDIO_TASK_PRIORITY,
                                                          &task_handle,
                                                          core,
                                                          stack_caps);
    if (task_ret != pdPASS && CONFIG_XIAOZHI_STACK_IN_PSRAM) {
        ESP_LOGW(TAG, "Audio task PSRAM allocation failed, retrying with internal memory");
        stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
        task_ret = xTaskCreatePinnedToCoreWithCaps(esp_xiaozhi_chat_audio_input,
                                                   ESP_XIAOZHI_CHAT_TASK_NAME,
                                                   CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE,
                                                   xiaozhi_chat,
                                                   CONFIG_XIAOZHI_AUDIO_TASK_PRIORITY,
                                                   &task_handle,
                                                   core,
                                                   stack_caps);
    }
    ESP_RETURN_ON_FALSE(task_ret == pdPASS && task_handle != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create audio input task");
#endif
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_stop_runtime(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_err_t ret = ESP_OK;
    esp_err_t step_ret = esp_xiaozhi_chat_close_audio_channel((esp_xiaozhi_chat_handle_t)xiaozhi_chat);
    if (step_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to close audio channel during stop: %s", esp_err_to_name(step_ret));
        ret = step_ret;
    }

    if (xiaozhi_chat->mcp_mgr_handle != 0) {
        step_ret = esp_mcp_mgr_stop(xiaozhi_chat->mcp_mgr_handle);
        if (step_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to stop MCP manager during stop: %s", esp_err_to_name(step_ret));
            if (ret == ESP_OK) {
                ret = step_ret;
            }
        }

        step_ret = esp_mcp_mgr_deinit(xiaozhi_chat->mcp_mgr_handle);
        if (step_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to deinit MCP manager during stop: %s", esp_err_to_name(step_ret));
            if (ret == ESP_OK) {
                ret = step_ret;
            }
        } else {
            xiaozhi_chat->mcp_mgr_handle = 0;
        }
    }
    xiaozhi_chat->mcp_chat_handle = 0;

    return ret;
}

static esp_err_t esp_xiaozhi_chat_destroy_owned_resources(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    if (xiaozhi_chat->owns_mcp_engine && xiaozhi_chat->mcp_engine != NULL) {
        esp_err_t ret = esp_mcp_destroy(xiaozhi_chat->mcp_engine);
        xiaozhi_chat->mcp_engine = NULL;
        return ret;
    }

    return ESP_OK;
}

static void esp_xiaozhi_chat_udp_force_close_socket(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    if (xiaozhi_chat == NULL || xiaozhi_chat->udp == NULL) {
        return;
    }

    int sockfd = xiaozhi_chat->udp->udp_socket;
    xiaozhi_chat->udp->udp_socket = -1;
    xiaozhi_chat->audio_channel_opened = false;
    xiaozhi_chat->udp->local_sequence = 0;
    xiaozhi_chat->udp->remote_sequence = 0;
    if (xiaozhi_chat->udp->audio_receive_events != NULL) {
        xEventGroupClearBits(xiaozhi_chat->udp->audio_receive_events, ESP_XIAOZHI_CHAT_RECV_RECEIVE_ENABLED);
    }

    if (sockfd != -1) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
    }
}

static void esp_xiaozhi_chat_audio_input(void *pvParameter)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)pvParameter;
    EventGroupHandle_t evt = NULL;
    if (!xiaozhi_chat || !xiaozhi_chat->udp || !xiaozhi_chat->udp->audio_receive_events) {
        ESP_LOGE(TAG, "Invalid parameter in audio input task");
        vTaskDelete(NULL);
        return;
    }

    evt = xiaozhi_chat->udp->audio_receive_events;
    xEventGroupClearBits(evt, ESP_XIAOZHI_CHAT_RECV_TASK_EXITED);

    char *buffer = calloc(1, CONFIG_XIAOZHI_UDP_RECEIVE_BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate UDP receive buffer");
        esp_xiaozhi_chat_audio_input_exit(evt);
        return;
    }

    uint8_t *decrypted_data = NULL;
    int consecutive_errors = 0;

    for (;;) {
        uint32_t bits = xEventGroupWaitBits(evt,
                                            ESP_XIAOZHI_CHAT_RECV_RECEIVE_ENABLED | ESP_XIAOZHI_CHAT_RECV_SHUTDOWN,
                                            pdFALSE, pdFALSE, portMAX_DELAY);
        if (bits & ESP_XIAOZHI_CHAT_RECV_SHUTDOWN) {
            break;
        }
        if (!(bits & ESP_XIAOZHI_CHAT_RECV_RECEIVE_ENABLED)) {
            continue;
        }

        /* Receive loop: run while socket is valid and not shutdown; reset error count per session */
        consecutive_errors = 0;
        while (xiaozhi_chat->udp && xiaozhi_chat->udp->udp_socket != -1) {
            if (xEventGroupGetBits(evt) & ESP_XIAOZHI_CHAT_RECV_SHUTDOWN) {
                break;
            }
            int received = recv(xiaozhi_chat->udp->udp_socket, buffer, CONFIG_XIAOZHI_UDP_RECEIVE_BUFFER_SIZE, 0);
            if (received <= 0) {
                EventBits_t current_bits = xEventGroupGetBits(evt);
                if ((current_bits & ESP_XIAOZHI_CHAT_RECV_SHUTDOWN) || !xiaozhi_chat->udp || xiaozhi_chat->udp->udp_socket == -1) {
                    break;
                }
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                if (errno == EBADF || errno == ENOTCONN) {
                    ESP_LOGW(TAG, "Audio input socket closed: %s", strerror(errno));
                    break;
                }

                consecutive_errors++;
                ESP_LOGE(TAG, "Audio input error: %s (error count: %d)", strerror(errno), consecutive_errors);

                if (consecutive_errors >= CONFIG_XIAOZHI_MAX_CONSECUTIVE_UDP_ERRORS) {
                    ESP_LOGE(TAG, "Too many consecutive audio input errors, stopping receive");
                    xiaozhi_chat->error_occurred = true;
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            consecutive_errors = 0;
            if (received < ESP_XIAOZHI_CHAT_AES_NONCE_SIZE) {
                ESP_LOGE(TAG, "Invalid audio packet size: %d", received);
                continue;
            }

            if (buffer[0] != 0x01) {
                ESP_LOGE(TAG, "Invalid audio packet type: %x", buffer[0]);
                continue;
            }

            uint32_t timestamp = ntohl(*(uint32_t *)&buffer[8]);
            uint32_t sequence = ntohl(*(uint32_t *)&buffer[12]);

            if (sequence < xiaozhi_chat->udp->remote_sequence) {
                ESP_LOGW(TAG, "Received audio packet with old sequence: %lu, expected: %lu", (unsigned long)sequence, (unsigned long)xiaozhi_chat->udp->remote_sequence);
                continue;
            }

            if (sequence != xiaozhi_chat->udp->remote_sequence + 1) {
                ESP_LOGW(TAG, "Received audio packet with wrong sequence: %lu, expected: %lu", (unsigned long)sequence, (unsigned long)(xiaozhi_chat->udp->remote_sequence + 1));
            }

            size_t decrypted_size = received - ESP_XIAOZHI_CHAT_AES_NONCE_SIZE;
            decrypted_data = calloc(1, decrypted_size);
            if (!decrypted_data) {
                ESP_LOGE(TAG, "Failed to allocate memory for decrypted data");
                continue;
            }

            size_t nc_off = 0;
            uint8_t stream_block[16] = {0};
            const uint8_t *nonce = (const uint8_t *)buffer;
            const uint8_t *encrypted = (const uint8_t *)buffer + ESP_XIAOZHI_CHAT_AES_NONCE_SIZE;

            int ret = mbedtls_aes_crypt_ctr(&xiaozhi_chat->udp->aes_ctx, decrypted_size, &nc_off, (uint8_t *)nonce, stream_block, (uint8_t *)encrypted, decrypted_data);
            if (ret != 0) {
                ESP_LOGE(TAG, "Failed to decrypt audio data, ret: %d", ret);
                free(decrypted_data);
                decrypted_data = NULL;
                continue;
            }

            esp_xiaozhi_chat_audio_t packet;
            packet.sample_rate = xiaozhi_chat->server_sample_rate;
            packet.frame_duration = xiaozhi_chat->server_frame_duration;
            packet.timestamp = timestamp;
            packet.payload = decrypted_data;
            packet.payload_size = decrypted_size;
            esp_xiaozhi_chat_binary_handler(xiaozhi_chat->mcp_chat_handle, packet.payload, packet.payload_size);

            free(decrypted_data);
            decrypted_data = NULL;
            xiaozhi_chat->udp->remote_sequence = sequence;
            xiaozhi_chat->udp->last_incoming_time = xTaskGetTickCount();
        }

        if (decrypted_data != NULL) {
            free(decrypted_data);
            decrypted_data = NULL;
        }
    }

    free(buffer);
    esp_xiaozhi_chat_audio_input_exit(evt);
}

static esp_err_t esp_xiaozhi_chat_audio_open(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    if (!xiaozhi_chat->use_mqtt) {
        xiaozhi_chat->audio_channel_opened = true;
        return ESP_OK;
    }

    if (!xiaozhi_chat->udp) {
        ESP_LOGE(TAG, "UDP context not allocated");
        return ESP_ERR_INVALID_STATE;
    }

    struct addrinfo *res = NULL;
    int sockfd = -1;
    esp_err_t ret = ESP_OK;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
    char port_str[16] = {0};

    snprintf(port_str, sizeof(port_str), "%d", xiaozhi_chat->udp->udp_port);
    if (getaddrinfo(xiaozhi_chat->udp->udp_server, port_str, &hints, &res) != 0) {
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Failed to get address info: %s", strerror(errno));
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "Failed to create socket: %s", strerror(errno));
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        close(sockfd);
        freeaddrinfo(res);
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Failed to connect audio channel: %s", strerror(errno));
    }

    struct timeval recv_timeout = {
        .tv_sec = ESP_XIAOZHI_CHAT_UDP_RECV_TIMEOUT_MS / 1000,
        .tv_usec = (ESP_XIAOZHI_CHAT_UDP_RECV_TIMEOUT_MS % 1000) * 1000,
    };
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
        close(sockfd);
        freeaddrinfo(res);
        ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "Failed to set audio socket recv timeout: %s", strerror(errno));
    }

    if (xSemaphoreTake(xiaozhi_chat->udp->channel_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        xiaozhi_chat->udp->udp_socket = sockfd;
        xiaozhi_chat->audio_channel_opened = true;

        /* Recreate the task after a previous instance exited during deinit. */
        EventBits_t recv_bits = xEventGroupGetBits(xiaozhi_chat->udp->audio_receive_events);
        if (recv_bits & ESP_XIAOZHI_CHAT_RECV_TASK_EXITED) {
            ESP_LOGD(TAG, "Create audio input task (stack=%d)", CONFIG_XIAOZHI_AUDIO_TASK_STACK_SIZE);
            xEventGroupClearBits(xiaozhi_chat->udp->audio_receive_events,
                                 ESP_XIAOZHI_CHAT_RECV_SHUTDOWN | ESP_XIAOZHI_CHAT_RECV_TASK_EXITED);
            esp_err_t task_ret = esp_xiaozhi_chat_create_audio_input_task(xiaozhi_chat);
            if (task_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create audio input task");
                xEventGroupSetBits(xiaozhi_chat->udp->audio_receive_events, ESP_XIAOZHI_CHAT_RECV_TASK_EXITED);
                close(sockfd);
                xiaozhi_chat->udp->udp_socket = -1;
                xiaozhi_chat->audio_channel_opened = false;
                xSemaphoreGive(xiaozhi_chat->udp->channel_mutex);
                freeaddrinfo(res);
                return ESP_ERR_NO_MEM;
            }
        }
        xEventGroupSetBits(xiaozhi_chat->udp->audio_receive_events, ESP_XIAOZHI_CHAT_RECV_RECEIVE_ENABLED);
        xSemaphoreGive(xiaozhi_chat->udp->channel_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take channel mutex");
        close(sockfd);
        ret = ESP_ERR_TIMEOUT;
    }

    freeaddrinfo(res);
    return ret;
}

static esp_err_t esp_xiaozhi_chat_audio_close(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    if (xiaozhi_chat->udp == NULL) {
        xiaozhi_chat->audio_channel_opened = false;
        return ESP_OK;
    }
    if (xiaozhi_chat->udp->channel_mutex == NULL) {
        xiaozhi_chat->audio_channel_opened = false;
        return ESP_OK;
    }

    bool taken = false;
    for (int retry = 0; retry < ESP_XIAOZHI_CHAT_CLOSE_MUTEX_RETRY_CNT && !taken; retry++) {
        taken = (xSemaphoreTake(xiaozhi_chat->udp->channel_mutex, pdMS_TO_TICKS(ESP_XIAOZHI_CHAT_CLOSE_MUTEX_RETRY_MS)) == pdTRUE);
        if (!taken && retry < ESP_XIAOZHI_CHAT_CLOSE_MUTEX_RETRY_CNT - 1) {
            ESP_LOGW(TAG, "Failed to take mutex for closing UDP socket, retry %d/%d", retry + 1, ESP_XIAOZHI_CHAT_CLOSE_MUTEX_RETRY_CNT);
        }
    }
    if (!taken) {
        ESP_LOGW(TAG, "Failed to take mutex for closing UDP socket after %d retries, force closing", ESP_XIAOZHI_CHAT_CLOSE_MUTEX_RETRY_CNT);
        esp_xiaozhi_chat_udp_force_close_socket(xiaozhi_chat);
        return ESP_OK;
    }

    esp_xiaozhi_chat_udp_force_close_socket(xiaozhi_chat);
    xSemaphoreGive(xiaozhi_chat->udp->channel_mutex);
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_audio_send(esp_xiaozhi_chat_t *xiaozhi_chat, const esp_xiaozhi_chat_audio_t *packet)
{
    esp_err_t result = ESP_FAIL;
    if (esp_xiaozhi_chat_is_audio_ready(xiaozhi_chat)) {
        if (!xiaozhi_chat->use_mqtt) {
            return esp_xiaozhi_transport_send_binary(xiaozhi_chat->mcp_chat_handle, packet->payload, packet->payload_size);
        }

        if (xiaozhi_chat->udp == NULL) {
            ESP_LOGE(TAG, "UDP context not allocated");
            return ESP_ERR_INVALID_STATE;
        }
        if (xSemaphoreTake(xiaozhi_chat->udp->channel_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_RETURN_ON_ERROR(ESP_ERR_TIMEOUT, TAG, "Failed to take mutex");
        }
        if (xiaozhi_chat->udp->udp_socket == -1) {
            xSemaphoreGive(xiaozhi_chat->udp->channel_mutex);
            return ESP_ERR_INVALID_STATE;
        }
        uint8_t nonce[ESP_XIAOZHI_CHAT_AES_NONCE_SIZE];
        memcpy(nonce, xiaozhi_chat->udp->aes_nonce, ESP_XIAOZHI_CHAT_AES_NONCE_SIZE);

        *(uint16_t *)&nonce[2] = htons(packet->payload_size);
        *(uint32_t *)&nonce[8] = htonl(packet->timestamp);
        *(uint32_t *)&nonce[12] = htonl(++xiaozhi_chat->udp->local_sequence);

        size_t encrypted_size = ESP_XIAOZHI_CHAT_AES_NONCE_SIZE + packet->payload_size;
        uint8_t *encrypted = malloc(encrypted_size);
        if (encrypted) {
            memcpy(encrypted, nonce, ESP_XIAOZHI_CHAT_AES_NONCE_SIZE);

            size_t nc_off = 0;
            uint8_t stream_block[16] = {0};

            int ret = mbedtls_aes_crypt_ctr(&xiaozhi_chat->udp->aes_ctx, packet->payload_size, &nc_off, nonce, stream_block, packet->payload, &encrypted[ESP_XIAOZHI_CHAT_AES_NONCE_SIZE]);

            if (ret == 0) {
                ssize_t sent = send(xiaozhi_chat->udp->udp_socket, (const char *)encrypted, encrypted_size, 0);
                if (sent == (ssize_t)encrypted_size) {
                    result = ESP_OK;
                } else {
                    result = ESP_FAIL;
                    if (sent < 0) {
                        ESP_LOGW(TAG, "UDP send failed: errno=%d (%s)", errno, strerror(errno));
                    } else {
                        ESP_LOGW(TAG, "UDP send short: sent=%zd, expected=%zu", (size_t)sent, encrypted_size);
                    }
                }
            } else {
                ESP_LOGE(TAG, "Failed to encrypt audio data: %d", ret);
            }

            free(encrypted);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for encrypted audio data");
        }
        xSemaphoreGive(xiaozhi_chat->udp->channel_mutex);
    } else {
        ESP_LOGD(TAG, "Audio channel not opened");
    }

    return result;
}

esp_err_t esp_xiaozhi_chat_init(esp_xiaozhi_chat_config_t *config, esp_xiaozhi_chat_handle_t *chat_hd)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    *chat_hd = 0;

    esp_xiaozhi_chat_t *xiaozhi_chat = calloc(1, sizeof(esp_xiaozhi_chat_t));
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");

    xiaozhi_chat->use_mqtt = config->has_mqtt_config;

    xiaozhi_chat->event_group_handle = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(xiaozhi_chat->event_group_handle, ESP_ERR_NO_MEM, TAG, "Failed to create event group");

    if (xiaozhi_chat->use_mqtt) {
        xiaozhi_chat->udp = calloc(1, sizeof(esp_xiaozhi_chat_udp_ctx_t));
        ESP_RETURN_ON_FALSE(xiaozhi_chat->udp, ESP_ERR_NO_MEM, TAG, "Failed to allocate UDP context");

        xiaozhi_chat->udp->channel_mutex = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(xiaozhi_chat->udp->channel_mutex, ESP_ERR_NO_MEM, TAG, "Failed to create mutex");
        xiaozhi_chat->udp->audio_receive_events = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(xiaozhi_chat->udp->audio_receive_events, ESP_ERR_NO_MEM, TAG, "Failed to create receive event group");
        xEventGroupSetBits(xiaozhi_chat->udp->audio_receive_events, ESP_XIAOZHI_CHAT_RECV_TASK_EXITED);
        xiaozhi_chat->udp->udp_socket = -1;
    }

    xiaozhi_chat->audio_type = config->audio_type;
    xiaozhi_chat->audio_callback = config->audio_callback;
    xiaozhi_chat->event_callback = config->event_callback;
    xiaozhi_chat->audio_callback_ctx = config->audio_callback_ctx;
    xiaozhi_chat->event_callback_ctx = config->event_callback_ctx;
    xiaozhi_chat->mcp_engine = config->mcp_engine;
    xiaozhi_chat->owns_mcp_engine = config->owns_mcp_engine;

    esp_err_t ret = esp_xiaozhi_chat_register_instance(xiaozhi_chat);
    if (ret != ESP_OK) {
        esp_xiaozhi_chat_deinit((esp_xiaozhi_chat_handle_t)xiaozhi_chat);
        ESP_RETURN_ON_ERROR(ret, TAG, "Only one chat instance is supported");
    }

    *chat_hd = (esp_xiaozhi_chat_handle_t)xiaozhi_chat;

    esp_xiaozhi_chat_set_device_state(xiaozhi_chat, ESP_XIAOZHI_CHAT_DEVICE_STATE_IDLE);
    esp_xiaozhi_chat_set_listening_mode(xiaozhi_chat, ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO);

    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_deinit(esp_xiaozhi_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    esp_err_t ret = ESP_OK;

    ret = esp_xiaozhi_chat_stop_runtime(xiaozhi_chat);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop chat runtime during deinit: %s", esp_err_to_name(ret));
    }

    if (xiaozhi_chat->udp != NULL && xiaozhi_chat->udp->audio_receive_events != NULL) {
        xEventGroupSetBits(xiaozhi_chat->udp->audio_receive_events, ESP_XIAOZHI_CHAT_RECV_SHUTDOWN);
        esp_xiaozhi_chat_udp_force_close_socket(xiaozhi_chat);
        EventBits_t bits = xEventGroupWaitBits(xiaozhi_chat->udp->audio_receive_events,
                                               ESP_XIAOZHI_CHAT_RECV_TASK_EXITED,
                                               pdFALSE, pdFALSE, pdMS_TO_TICKS(ESP_XIAOZHI_CHAT_TASK_EXIT_WAIT_MS));
        if (!(bits & ESP_XIAOZHI_CHAT_RECV_TASK_EXITED)) {
            ESP_LOGE(TAG, "Timed out waiting for audio input task to exit; continuing best-effort cleanup");
            if (ret == ESP_OK) {
                ret = ESP_ERR_TIMEOUT;
            }
        }
    }

    if (xiaozhi_chat->udp) {
        mbedtls_aes_free(&xiaozhi_chat->udp->aes_ctx);
        if (xiaozhi_chat->udp->audio_receive_events) {
            vEventGroupDelete(xiaozhi_chat->udp->audio_receive_events);
            xiaozhi_chat->udp->audio_receive_events = NULL;
        }
        if (xiaozhi_chat->udp->channel_mutex) {
            vSemaphoreDelete(xiaozhi_chat->udp->channel_mutex);
        }
        free(xiaozhi_chat->udp);
        xiaozhi_chat->udp = NULL;
    }

    if (xiaozhi_chat->event_group_handle) {
        vEventGroupDelete(xiaozhi_chat->event_group_handle);
    }

    esp_xiaozhi_chat_unregister_instance(xiaozhi_chat);

    esp_err_t destroy_ret = esp_xiaozhi_chat_destroy_owned_resources(xiaozhi_chat);
    if (ret == ESP_OK) {
        ret = destroy_ret;
    }

    free(xiaozhi_chat);

    return ret;
}

esp_err_t esp_xiaozhi_chat_start(esp_xiaozhi_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    ESP_RETURN_ON_FALSE(xiaozhi_chat->mcp_engine, ESP_ERR_INVALID_ARG, TAG, "MCP engine not initialized");

    esp_err_t ret = ESP_OK;
    esp_xiaozhi_mcp_config_t mcp_config = {
        .session_id = xiaozhi_chat->session_id,
        .session_id_len = strlen(xiaozhi_chat->session_id),
        .use_mqtt = xiaozhi_chat->use_mqtt,
        .callbacks = {
            .on_message_callback = esp_xiaozhi_chat_data_handler,
            .on_binary_callback = esp_xiaozhi_chat_binary_handler,
            .on_connected_callback = esp_xiaozhi_chat_connected_handler,
            .on_disconnected_callback = esp_xiaozhi_chat_disconnected_handler,
            .on_error_callback = esp_xiaozhi_chat_transport_error_handler,
        },
        .ctx = xiaozhi_chat,
    };

    esp_mcp_mgr_config_t mcp_mgr_config = {
        .transport = esp_mcp_transport_xiaozhi,
        .config = &mcp_config,
        .instance = xiaozhi_chat->mcp_engine,
    };

    ret = esp_mcp_mgr_init(mcp_mgr_config, &xiaozhi_chat->mcp_mgr_handle);
    ESP_GOTO_ON_ERROR(ret, error, TAG, "Failed to initialize MCP manager: %s", esp_err_to_name(ret));

    ret = esp_mcp_mgr_start(xiaozhi_chat->mcp_mgr_handle);
    ESP_GOTO_ON_ERROR(ret, error, TAG, "Failed to start MCP manager: %s", esp_err_to_name(ret));

    ret = esp_mcp_mgr_register_endpoint(xiaozhi_chat->mcp_mgr_handle, "mcp_server", NULL);
    ESP_GOTO_ON_ERROR(ret, error, TAG, "Failed to register MCP endpoint: %s", esp_err_to_name(ret));

    ret = esp_xiaozhi_mcp_get_handle(xiaozhi_chat->mcp_mgr_handle, &xiaozhi_chat->mcp_chat_handle);
    ESP_GOTO_ON_ERROR(ret, error, TAG, "Failed to get transport handle from MCP: %s", esp_err_to_name(ret));

    ESP_LOGD(TAG, "MCP manager initialized with %s transport", xiaozhi_chat->use_mqtt ? "MQTT" : "WebSocket");

    return ESP_OK;

error:

    esp_mcp_mgr_stop(xiaozhi_chat->mcp_mgr_handle);
    esp_mcp_mgr_deinit(xiaozhi_chat->mcp_mgr_handle);
    xiaozhi_chat->mcp_mgr_handle = 0;

    return ret;
}

esp_err_t esp_xiaozhi_chat_stop(esp_xiaozhi_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;

    esp_err_t ret = esp_xiaozhi_chat_stop_runtime(xiaozhi_chat);
    return ret;
}

esp_err_t esp_xiaozhi_chat_open_audio_channel(esp_xiaozhi_chat_handle_t chat_hd, const esp_xiaozhi_chat_audio_t *audio,
                                              char *message, size_t message_len)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_err_t ret = ESP_OK;
    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    bool message_allocated = false;

    if (message && !message_len) {
        ESP_LOGE(TAG, "Invalid args: message buffer provided but message_len is 0");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(esp_xiaozhi_chat_validate_audio_params(audio), TAG, "Invalid audio parameters");

    if (!message || !message_len) {
        uint16_t alloc_size = (message_len > 0) ? message_len : CONFIG_XIAOZHI_HELLO_MESSAGE_SIZE;
        if (alloc_size > CONFIG_XIAOZHI_HELLO_MESSAGE_SIZE) {
            alloc_size = CONFIG_XIAOZHI_HELLO_MESSAGE_SIZE;
        }
        message = calloc(1, alloc_size);
        ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");
        message_allocated = true;
        ret = esp_xiaozhi_chat_get_hello_message(xiaozhi_chat, audio, message, alloc_size);
        if (ret != ESP_OK) {
            free(message);
            ESP_RETURN_ON_ERROR(ret, TAG, "Failed to get hello message");
        }
    }

    xiaozhi_chat->error_occurred = false;
    memset(xiaozhi_chat->session_id, 0, sizeof(xiaozhi_chat->session_id));
    xEventGroupClearBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_SERVER_HELLO);

    ret = esp_xiaozhi_transport_send_text(xiaozhi_chat->mcp_chat_handle, message);
    if (message_allocated) {
        free(message);
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send hello message");

    EventBits_t bits = xEventGroupWaitBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_SERVER_HELLO, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & ESP_XIAOZHI_CHAT_SERVER_HELLO)) {
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Failed to receive server hello");
    }

    ret = esp_xiaozhi_chat_audio_open(xiaozhi_chat);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to open audio %s", esp_err_to_name(ret));

    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_close_audio_channel(esp_xiaozhi_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    if (strlen(xiaozhi_chat->session_id) > 0) {
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddStringToObject(root, "session_id", xiaozhi_chat->session_id);
            cJSON_AddStringToObject(root, "type", "goodbye");

            esp_xiaozhi_chat_send_json_message(xiaozhi_chat, root);
            cJSON_Delete(root);
        } else {
            ESP_LOGW(TAG, "Failed to create goodbye JSON, skip notifying server (channel will still close)");
        }
    }

    memset(xiaozhi_chat->session_id, 0, sizeof(xiaozhi_chat->session_id));
    esp_xiaozhi_chat_audio_close(xiaozhi_chat);

    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_send_audio_data(esp_xiaozhi_chat_handle_t chat_hd, const char *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data");
    ESP_RETURN_ON_FALSE(data_len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid data length");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    esp_xiaozhi_chat_audio_t packet = {
        .sample_rate = xiaozhi_chat->server_sample_rate,
        .frame_duration = xiaozhi_chat->server_frame_duration,
        .timestamp = xTaskGetTickCount(),
        .payload = (uint8_t *)data,
        .payload_size = data_len,
    };

    return esp_xiaozhi_chat_audio_send(xiaozhi_chat, &packet);
}

esp_err_t esp_xiaozhi_chat_send_wake_word(esp_xiaozhi_chat_handle_t chat_hd, const char *wake_word)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(wake_word && strlen(wake_word) > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid wake word");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    cJSON *root = NULL;
    esp_err_t ret = esp_xiaozhi_chat_create_session_message(xiaozhi_chat, "listen", &root);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create session message");

    cJSON_AddStringToObject(root, "state", "detect");
    cJSON_AddStringToObject(root, "text", wake_word);

    ret = esp_xiaozhi_chat_send_json_message(xiaozhi_chat, root);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_xiaozhi_chat_send_start_listening(esp_xiaozhi_chat_handle_t chat_hd, int mode)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    const char *mode_str = (mode == ESP_XIAOZHI_CHAT_LISTENING_MODE_REALTIME) ? "realtime" :
                           (mode == ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO) ? "auto" :
                           (mode == ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL) ? "manual" : "unknown";

    cJSON *root = NULL;
    esp_err_t ret = esp_xiaozhi_chat_create_session_message(xiaozhi_chat, "listen", &root);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create session message");

    cJSON_AddStringToObject(root, "state", "start");
    cJSON_AddStringToObject(root, "mode", mode_str);

    ret = esp_xiaozhi_chat_send_json_message(xiaozhi_chat, root);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_xiaozhi_chat_send_stop_listening(esp_xiaozhi_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    cJSON *root = NULL;
    esp_err_t ret = esp_xiaozhi_chat_create_session_message(xiaozhi_chat, "listen", &root);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create session message");

    cJSON_AddStringToObject(root, "state", "stop");

    ret = esp_xiaozhi_chat_send_json_message(xiaozhi_chat, root);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_xiaozhi_chat_send_abort_speaking(esp_xiaozhi_chat_handle_t chat_hd, esp_xiaozhi_chat_abort_speaking_reason_t reason)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    const char *reason_str = (reason == ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED) ? "wake_word_detected" : (reason == ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_STOP_LISTENING) ? "stop_listening" : "unknown";

    cJSON *root = NULL;
    esp_err_t ret = esp_xiaozhi_chat_create_session_message(xiaozhi_chat, "abort", &root);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create session message");

    cJSON_AddStringToObject(root, "reason", reason_str);

    ret = esp_xiaozhi_chat_send_json_message(xiaozhi_chat, root);
    cJSON_Delete(root);
    return ret;
}
