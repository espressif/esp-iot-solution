/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "esp_mcp_engine.h"
#include "esp_mcp_mgr.h"
#include "esp_xiaozhi_mcp.h"
#include "esp_xiaozhi_vision.h"
#include "esp_xiaozhi_transport.h"
#include "esp_xiaozhi_chat.h"
#include "esp_xiaozhi_udp.h"
#include "esp_xiaozhi_protocol_priv.h"

#define ESP_XIAOZHI_CHAT_MAX_STRING_SIZE 256
#define ESP_XIAOZHI_CHAT_MIN_SAMPLE_RATE 8000
#define ESP_XIAOZHI_CHAT_MAX_SAMPLE_RATE 48000
#define ESP_XIAOZHI_CHAT_MIN_CHANNELS 1
#define ESP_XIAOZHI_CHAT_MAX_CHANNELS 2
#define ESP_XIAOZHI_CHAT_MIN_FRAME_DURATION_MS 10
#define ESP_XIAOZHI_CHAT_MAX_FRAME_DURATION_MS 120

#define ESP_XIAOZHI_CHAT_SERVER_HELLO BIT0
#define ESP_XIAOZHI_CHAT_TRANSPORT_CONNECTED BIT1
#define ESP_XIAOZHI_CHAT_CANCELLED BIT2
#define ESP_XIAOZHI_CHAT_HELLO_ERROR BIT3

/**
 * @brief  Xiaozhi chat
 */
typedef struct {
    bool use_mqtt;                                      /*!< Use MQTT for this session (prefer MQTT when both supported) */
    bool owns_mcp_engine;                               /*!< Whether this instance owns mcp_engine */
    bool mcp_compatibility_applied;
    bool previous_mcp_compatibility;
    bool error_occurred;                                /*!< Error occurred */
    bool chat_connected;                                /*!< Chat connected */
    bool audio_channel_opened;                          /*!< Audio channel opened */
    esp_xiaozhi_transport_handle_t transport_handle;           /*!< Transport handle for MCP chat */
    EventGroupHandle_t event_group_handle;              /*!< Event group for handling events */

    esp_mcp_t *mcp_engine;                              /*!< MCP engine instance for processing MCP protocol */
    esp_mcp_mgr_handle_t mcp_mgr_handle;                /*!< MCP manager handle */

    esp_xiaozhi_udp_handle_t udp;                    /*!< UDP context for MQTT path; NULL when WebSocket */

    char session_id[ESP_XIAOZHI_CHAT_MAX_STRING_SIZE];  /*!< Session ID */
#if CONFIG_XIAOZHI_VISION_ENABLE
    char vision_connection_id[32];
    esp_xiaozhi_vision_t vision;                        /*!< Vision explain endpoint for this chat */
#endif
    int server_sample_rate;                             /*!< Server sample rate */
    int server_channels;
    char server_format[16];
    bool hello_pending;
    bool hello_probe_requested;
    bool started;
    bool websocket_stopped;
    int64_t last_incoming_time;
    uint32_t connection_generation;
    uint32_t dialogue_generation;
    char connection_id[32];
    esp_xiaozhi_chat_packet_callback_t packet_callback;
    void *packet_callback_ctx;
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

static esp_xiaozhi_chat_t *esp_xiaozhi_chat_get_by_transport(esp_xiaozhi_transport_handle_t handle)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_instance();
    if (xiaozhi_chat && xiaozhi_chat->transport_handle == handle) {
        return xiaozhi_chat;
    }

    return NULL;
}

static bool esp_xiaozhi_chat_is_audio_ready(esp_xiaozhi_chat_t *xiaozhi_chat)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat, false, TAG, "Invalid chat");

    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    bool opened = xiaozhi_chat->audio_channel_opened;
    bool error = xiaozhi_chat->error_occurred;
    bool use_mqtt = xiaozhi_chat->use_mqtt;
    bool connected = xiaozhi_chat->chat_connected;
    int64_t last_incoming = xiaozhi_chat->last_incoming_time;
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);

    if (!opened || error) {
        return false;
    }
    if (!use_mqtt) {
        return connected && esp_timer_get_time() - last_incoming <= 120000000;
    }
    return esp_xiaozhi_udp_is_open(xiaozhi_chat->udp);
}

static esp_err_t esp_xiaozhi_chat_hello_handler(esp_xiaozhi_chat_t *chat, cJSON *root)
{
    if (!chat->hello_pending) {
        return ESP_OK;  /* Late/unsolicited hello cannot mutate the current session. */
    }
    const cJSON *transport = cJSON_GetObjectItemCaseSensitive(root, "transport");
    char session[sizeof(chat->session_id)];
    const char *expected = chat->use_mqtt ? "udp" : "websocket";
    esp_err_t ret = ESP_ERR_INVALID_RESPONSE;
    if (!cJSON_IsString(transport) || strcmp(transport->valuestring, expected) ||
            esp_xiaozhi_protocol_read_session_id(root, session, sizeof(session)) != ESP_OK) {
        goto invalid;
    }
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "audio_params");
    if (params && !cJSON_IsObject(params)) {
        goto invalid;
    }
    const cJSON *rate = cJSON_GetObjectItemCaseSensitive(params, "sample_rate");
    const cJSON *duration = cJSON_GetObjectItemCaseSensitive(params, "frame_duration");
    const cJSON *channels = cJSON_GetObjectItemCaseSensitive(params, "channels");
    const cJSON *format = cJSON_GetObjectItemCaseSensitive(params, "format");
    if ((rate && (!cJSON_IsNumber(rate) || rate->valuedouble != rate->valueint || rate->valueint < 8000 || rate->valueint > 48000)) ||
            (duration && (!cJSON_IsNumber(duration) || duration->valuedouble != duration->valueint || duration->valueint < 10 || duration->valueint > 120)) ||
            (channels && (!cJSON_IsNumber(channels) || channels->valuedouble != channels->valueint || channels->valueint < 1 || channels->valueint > 2)) ||
            (format && (!cJSON_IsString(format) || !format->valuestring[0] || strlen(format->valuestring) >= sizeof(chat->server_format)))) {
        goto invalid;
    }
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features");
    const cJSON *probe = cJSON_GetObjectItemCaseSensitive(features, "udp_probe");
    if ((features && !cJSON_IsObject(features)) || (probe && !cJSON_IsBool(probe))) {
        goto invalid;
    }
    if (chat->use_mqtt) {
        const cJSON *udp = cJSON_GetObjectItemCaseSensitive(root, "udp");
        const cJSON *server = cJSON_GetObjectItemCaseSensitive(udp, "server");
        const cJSON *port = cJSON_GetObjectItemCaseSensitive(udp, "port");
        const cJSON *key = cJSON_GetObjectItemCaseSensitive(udp, "key");
        const cJSON *nonce = cJSON_GetObjectItemCaseSensitive(udp, "nonce");
        if (!cJSON_IsString(server) || !cJSON_IsNumber(port) || port->valuedouble != port->valueint ||
                !cJSON_IsString(key) || !cJSON_IsString(nonce)) {
            goto invalid;
        }
        ret = esp_xiaozhi_udp_configure(chat->udp, server->valuestring, port->valueint,
                                        key->valuestring, nonce->valuestring,
                                        chat->hello_probe_requested && cJSON_IsTrue(probe));
        if (ret != ESP_OK) {
            goto invalid;
        }
    }
    strcpy(chat->session_id, session);
    chat->server_sample_rate = rate ? rate->valueint : 24000;
    chat->server_frame_duration = duration ? duration->valueint : 60;
    chat->server_channels = channels ? channels->valueint : 1;
    strcpy(chat->server_format, format ? format->valuestring : "opus");
    chat->hello_pending = false;
    chat->dialogue_generation++;
    xEventGroupSetBits(chat->event_group_handle, ESP_XIAOZHI_CHAT_SERVER_HELLO);
    return ESP_OK;
invalid:
    chat->hello_pending = false;
    chat->error_occurred = true;
    xEventGroupSetBits(chat->event_group_handle, ESP_XIAOZHI_CHAT_HELLO_ERROR);
    return ret;
}

static esp_err_t esp_xiaozhi_chat_goodbye_handler(esp_xiaozhi_chat_t *chat, cJSON *root)
{
    const cJSON *session = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    if (session && !cJSON_IsString(session)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (chat->audio_channel_opened && session && !strcmp(chat->session_id, session->valuestring)) {
        chat->audio_channel_opened = false;
        (void)esp_xiaozhi_udp_close(chat->udp);
        chat->session_id[0] = '\0';
        esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_SERVER_GOODBYE, NULL, 0, 0);
        esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED, NULL, 0, 0);
    }
    return ESP_OK;
}

#if CONFIG_XIAOZHI_VISION_ENABLE
static void esp_xiaozhi_chat_note_vision(esp_xiaozhi_chat_t *xiaozhi_chat, const cJSON *payload)
{
    const cJSON *method = cJSON_GetObjectItem(payload, "method");
    const cJSON *params = NULL;
    const cJSON *capabilities = NULL;
    const cJSON *vision = NULL;
    const cJSON *url = NULL;
    const cJSON *token = NULL;
    const char *token_str = NULL;

    if (!cJSON_IsString(method) || strcmp(method->valuestring, "initialize") != 0) {
        return;
    }

    params = cJSON_GetObjectItem(payload, "params");
    capabilities = cJSON_IsObject(params) ? cJSON_GetObjectItem(params, "capabilities") : NULL;
    vision = cJSON_IsObject(capabilities) ? cJSON_GetObjectItem(capabilities, "vision") : NULL;
    if (!cJSON_IsObject(vision)) {
        return;
    }

    url = cJSON_GetObjectItem(vision, "url");
    token = cJSON_GetObjectItem(vision, "token");
    if (!cJSON_IsString(url) || url->valuestring[0] == '\0') {
        (void)esp_xiaozhi_vision_set_endpoint(&xiaozhi_chat->vision, NULL, NULL);
        return;
    }

    token_str = cJSON_IsString(token) ? token->valuestring : NULL;
    if (esp_xiaozhi_vision_set_endpoint(&xiaozhi_chat->vision, url->valuestring, token_str) != ESP_OK) {
        /* Helper already logged */
        (void)esp_xiaozhi_vision_set_endpoint(&xiaozhi_chat->vision, NULL, NULL);
        return;
    }
    ESP_LOGD(TAG, "Vision explain URL: %s", xiaozhi_chat->vision.explain_url);
}
#endif

static esp_err_t esp_xiaozhi_chat_mcp_handler(esp_xiaozhi_chat_t *xiaozhi_chat, cJSON *root)
{
    cJSON *payload_obj = cJSON_GetObjectItem(root, "payload");
    if (!payload_obj || !cJSON_IsObject(payload_obj)) {
        ESP_LOGE(TAG, "Invalid MCP payload");
        return ESP_ERR_INVALID_ARG;
    }

    if (!xiaozhi_chat->mcp_mgr_handle) {
        return ESP_OK;  /* External MCP handlers consume CHAT_JSON and use send_mcp(). */
    }

    cJSON *method = cJSON_GetObjectItem(payload_obj, "method");
    if (cJSON_IsString(method)) {
        ESP_LOGI(TAG, "Received MCP method: %s", method->valuestring);
    }

    cJSON *request_id = cJSON_GetObjectItemCaseSensitive(payload_obj, "id");
    if (cJSON_IsNumber(request_id) && isfinite(request_id->valuedouble) &&
            trunc(request_id->valuedouble) == request_id->valuedouble && fabs(request_id->valuedouble) <= 9007199254740991.0) {
        char number[32];
        snprintf(number, sizeof(number), "%.0f", request_id->valuedouble);
        cJSON *raw = cJSON_CreateRaw(number);
        if (!raw) {
            return ESP_ERR_NO_MEM;
        }
        cJSON_ReplaceItemInObjectCaseSensitive(payload_obj, "id", raw);
    }
    char *payload_mcp = cJSON_PrintUnformatted(payload_obj);
    if (!payload_mcp) {
        ESP_LOGE(TAG, "Failed to print MCP payload");
        return ESP_ERR_NO_MEM;
    }

    /* Do not log the payload: initialize carries the vision bearer token. */
    ESP_LOGI(TAG, "Received MCP message: %u bytes", (unsigned)strlen(payload_mcp));

    const char *reply_session = xiaozhi_chat->session_id[0] ? xiaozhi_chat->session_id : NULL;
    esp_err_t ret = esp_xiaozhi_mcp_handle_message(xiaozhi_chat->mcp_mgr_handle,
                                                   "mcp_server",
                                                   payload_mcp,
                                                   strlen(payload_mcp),
                                                   reply_session, xiaozhi_chat->connection_id, xiaozhi_chat->dialogue_generation);

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

static void esp_xiaozhi_chat_deliver_audio(void *ctx, const uint8_t *data, size_t length, uint32_t timestamp)
{
    esp_xiaozhi_chat_t *chat = ctx;
    if (!chat->audio_channel_opened || chat->error_occurred) {
        return;
    }
    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    chat->last_incoming_time = esp_timer_get_time();
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
    esp_xiaozhi_chat_audio_t packet = {
        .format = chat->server_format,
        .sample_rate = chat->server_sample_rate,
        .channels = chat->server_channels,
        .frame_duration = chat->server_frame_duration,
        .timestamp = timestamp,
        .payload = (uint8_t *)data,
        .payload_size = length,
    };
    if (chat->packet_callback) {
        chat->packet_callback(&packet, chat->packet_callback_ctx);
    }
    if (chat->audio_callback) {
        chat->audio_callback(data, length, chat->audio_callback_ctx);
    }
}

static void esp_xiaozhi_chat_udp_error(void *ctx, esp_err_t error, const char *source)
{
    esp_xiaozhi_chat_t *chat = ctx;
    chat->error_occurred = true;
    chat->audio_channel_opened = false;
    if (chat->event_callback) {
        esp_xiaozhi_chat_error_info_t info = {.code = error, .source = source};
        chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR, &info, chat->event_callback_ctx);
    }
    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED, NULL, 0, 0);
}

static esp_err_t esp_xiaozhi_chat_audio_handler(esp_xiaozhi_transport_handle_t handle,
                                                const uint8_t *data, size_t length, uint32_t timestamp)
{
    esp_xiaozhi_chat_t *chat = esp_xiaozhi_chat_get_by_transport(handle);
    if (!chat) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_xiaozhi_chat_deliver_audio(chat, data, length, timestamp);
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_data_handler(esp_xiaozhi_transport_handle_t handle, const char *topic, size_t topic_len, const char *data, size_t data_len)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    if (!xiaozhi_chat) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(data, data_len, &end, false);
    while (end && end < data + data_len && isspace((unsigned char) * end)) {
        end++;
    }
    if (!cJSON_IsObject(root) || end != data + data_len) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Invalid JSON message");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        ESP_LOGE(TAG, "Message type is invalid");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    xiaozhi_chat->last_incoming_time = esp_timer_get_time();
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
    esp_xiaozhi_udp_note_activity(xiaozhi_chat->udp);
    if (xiaozhi_chat->event_callback) {
        if (data_len == SIZE_MAX) {
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        char *json = malloc(data_len + 1);
        if (json == NULL) {
            ESP_LOGE(TAG, "Failed to copy JSON message");
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        memcpy(json, data, data_len);
        json[data_len] = '\0';
        esp_xiaozhi_chat_json_t message = {.type = type->valuestring, .json = json, .length = data_len};
        xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_JSON, &message, xiaozhi_chat->event_callback_ctx);
        free(json);
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

    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    xiaozhi_chat->chat_connected = true;
    xiaozhi_chat->connection_generation++;
    snprintf(xiaozhi_chat->connection_id, sizeof(xiaozhi_chat->connection_id), "xiaozhi-%lu",
             (unsigned long)xiaozhi_chat->connection_generation);
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
    xEventGroupClearBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_CANCELLED);
    xEventGroupSetBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_TRANSPORT_CONNECTED);

    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_disconnected_handler(esp_xiaozhi_transport_handle_t handle)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    xiaozhi_chat->chat_connected = false;
    xiaozhi_chat->hello_pending = false;
    xiaozhi_chat->audio_channel_opened = false;
    xiaozhi_chat->connection_generation++;
    xiaozhi_chat->connection_id[0] = '\0';
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
#if CONFIG_XIAOZHI_VISION_ENABLE
    xiaozhi_chat->vision_connection_id[0] = '\0';
    (void)esp_xiaozhi_vision_set_endpoint(&xiaozhi_chat->vision, NULL, NULL);
#endif
    (void)esp_xiaozhi_udp_close(xiaozhi_chat->udp);
    xEventGroupClearBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_TRANSPORT_CONNECTED);
    xEventGroupSetBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_CANCELLED);

    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED, NULL, 0, 0);
    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED, NULL, 0, 0);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_transport_error_handler(esp_xiaozhi_transport_handle_t handle, esp_err_t err)
{
    esp_xiaozhi_chat_t *xiaozhi_chat = esp_xiaozhi_chat_get_by_transport(handle);
    ESP_RETURN_ON_FALSE(xiaozhi_chat, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    xiaozhi_chat->error_occurred = true;
    if (xiaozhi_chat->hello_pending) {
        xiaozhi_chat->hello_pending = false;
        xEventGroupSetBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_HELLO_ERROR);
    }
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

    esp_err_t ret = esp_xiaozhi_transport_send_text(xiaozhi_chat->transport_handle, json_str);
    cJSON_free(json_str);

    return ret;
}

static esp_err_t esp_xiaozhi_chat_create_session_message(esp_xiaozhi_chat_t *xiaozhi_chat, const char *type, cJSON **out_root)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(type != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid type");
    ESP_RETURN_ON_FALSE(out_root != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid output");
    ESP_RETURN_ON_FALSE(xiaozhi_chat->audio_channel_opened || strlen(xiaozhi_chat->session_id) > 0,
                        ESP_ERR_INVALID_STATE, TAG, "No session, open audio channel first");

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
        cJSON_AddNumberToObject(root, "version", esp_xiaozhi_transport_get_version(xiaozhi_chat->transport_handle));
        cJSON_AddStringToObject(root, "transport", "websocket");
    }

    esp_err_t capability_ret = esp_xiaozhi_protocol_add_capabilities(root, true);
    if (capability_ret != ESP_OK) {
        cJSON_Delete(root);
        return capability_ret;
    }
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

static esp_err_t esp_xiaozhi_chat_validate_hello(esp_xiaozhi_chat_t *chat, const char *message, size_t capacity)
{
    if (!memchr(message, '\0', capacity) || strlen(message) > UINT16_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    cJSON *root = cJSON_ParseWithOpts(message, NULL, true);
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON *transport = cJSON_GetObjectItemCaseSensitive(root, "transport");
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features");
    const cJSON *probe = cJSON_GetObjectItemCaseSensitive(features, "udp_probe");
    bool valid = cJSON_IsString(type) && !strcmp(type->valuestring, "hello") &&
                 cJSON_IsNumber(version) && version->valuedouble == esp_xiaozhi_transport_get_version(chat->transport_handle) &&
                 cJSON_IsString(transport) && !strcmp(transport->valuestring, chat->use_mqtt ? "udp" : "websocket") &&
                 (!features || cJSON_IsObject(features)) && (!probe || cJSON_IsBool(probe));
    chat->hello_probe_requested = valid && esp_xiaozhi_protocol_probe_enabled() && cJSON_IsTrue(probe);
    cJSON_Delete(root);
    return valid ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t esp_xiaozhi_chat_stop_runtime(esp_xiaozhi_chat_t *chat)
{
    chat->started = false;
    chat->hello_pending = false;
    xEventGroupSetBits(chat->event_group_handle, ESP_XIAOZHI_CHAT_CANCELLED);
    esp_err_t ret = esp_xiaozhi_chat_close_audio_channel((esp_xiaozhi_chat_handle_t)chat);
    if (ret != ESP_OK) {
        return ret;
    }
    if (chat->transport_handle) {
        ret = esp_xiaozhi_transport_stop(chat->transport_handle);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    if (chat->mcp_mgr_handle) {
        ret = esp_mcp_mgr_stop(chat->mcp_mgr_handle);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = esp_mcp_mgr_deinit(chat->mcp_mgr_handle);
        if (ret != ESP_OK) {
            return ret;
        }
        chat->mcp_mgr_handle = 0;
    }
    if (chat->mcp_compatibility_applied) {
        ret = esp_mcp_set_xiaozhi_compatibility(chat->mcp_engine, chat->previous_mcp_compatibility);
        if (ret != ESP_OK) {
            return ret;
        }
        chat->mcp_compatibility_applied = false;
    }
    if (chat->transport_handle) {
        ret = esp_xiaozhi_transport_deinit(chat->transport_handle);
        chat->transport_handle = 0;
    }
    chat->chat_connected = false;
    chat->connection_id[0] = '\0';
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

static esp_err_t esp_xiaozhi_chat_audio_open(esp_xiaozhi_chat_t *chat)
{
    chat->audio_channel_opened = true;
    esp_err_t ret = chat->use_mqtt ? esp_xiaozhi_udp_open(chat->udp) : ESP_OK;
    if (ret != ESP_OK) {
        chat->audio_channel_opened = false;
    }
    return ret;
}

static esp_err_t esp_xiaozhi_chat_audio_close(esp_xiaozhi_chat_t *chat)
{
    chat->audio_channel_opened = false;
    return esp_xiaozhi_udp_close(chat->udp);
}

static esp_err_t esp_xiaozhi_chat_audio_send(esp_xiaozhi_chat_t *chat, const esp_xiaozhi_chat_audio_t *packet)
{
    if (!esp_xiaozhi_chat_is_audio_ready(chat)) {
        return ESP_ERR_INVALID_STATE;
    }
    return chat->use_mqtt ? esp_xiaozhi_udp_send(chat->udp, packet->payload, packet->payload_size, packet->timestamp) :
           esp_xiaozhi_transport_send_audio(chat->transport_handle, packet->payload, packet->payload_size, packet->timestamp);
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
    if (!xiaozhi_chat->event_group_handle) {
        free(xiaozhi_chat);
        return ESP_ERR_NO_MEM;
    }

    if (xiaozhi_chat->use_mqtt) {
        esp_err_t ret = esp_xiaozhi_udp_init(&xiaozhi_chat->udp, esp_xiaozhi_chat_deliver_audio,
                                             esp_xiaozhi_chat_udp_error, xiaozhi_chat);
        if (ret != ESP_OK) {
            vEventGroupDelete(xiaozhi_chat->event_group_handle);
            free(xiaozhi_chat);
            return ret;
        }
    }
    xiaozhi_chat->server_sample_rate = 24000;
    xiaozhi_chat->server_channels = 1;
    xiaozhi_chat->server_frame_duration = 60;
    strcpy(xiaozhi_chat->server_format, "opus");
    xiaozhi_chat->packet_callback = config->packet_callback;
    xiaozhi_chat->packet_callback_ctx = config->packet_callback_ctx;
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

    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_deinit(esp_xiaozhi_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    esp_err_t ret = ESP_OK;

    ret = esp_xiaozhi_chat_stop_runtime(xiaozhi_chat);
    if (ret != ESP_OK) {
        return ret;  /* Keep resources alive if a callback/worker has not stopped. */
    }
    ret = esp_xiaozhi_udp_deinit(xiaozhi_chat->udp);
    if (ret != ESP_OK) {
        return ret;
    }
    xiaozhi_chat->udp = NULL;
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

esp_err_t esp_xiaozhi_chat_get_vision_explain(esp_xiaozhi_chat_handle_t chat_hd, char *url, size_t url_size,
                                              char *token, size_t token_size)
{
    if (chat_hd == 0 || url == NULL || url_size == 0 || (token != NULL && token_size == 0)) {
        ESP_LOGE(TAG, "Get vision explain failed: handle, url, or token size is invalid");
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_XIAOZHI_VISION_ENABLE
    esp_xiaozhi_chat_t *xiaozhi_chat = (esp_xiaozhi_chat_t *)chat_hd;
    if (xiaozhi_chat->vision.explain_url[0] == '\0') {
        ESP_LOGW(TAG, "Get vision explain: URL is not set");
        return ESP_ERR_INVALID_STATE;
    }
    if (url_size <= strlen(xiaozhi_chat->vision.explain_url)) {
        url[0] = '\0';
        ESP_LOGE(TAG, "Get vision explain failed: URL buffer is too small");
        return ESP_ERR_INVALID_SIZE;
    }
    if (token != NULL && token_size <= strlen(xiaozhi_chat->vision.explain_token)) {
        token[0] = '\0';
        ESP_LOGE(TAG, "Get vision explain failed: token buffer is too small");
        return ESP_ERR_INVALID_SIZE;
    }

    strlcpy(url, xiaozhi_chat->vision.explain_url, url_size);
    if (token != NULL) {
        strlcpy(token, xiaozhi_chat->vision.explain_token, token_size);
    }
    return ESP_OK;
#else
    ESP_LOGW(TAG, "Get vision explain: support is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static bool esp_xiaozhi_chat_connection_matches(esp_xiaozhi_chat_t *chat, const char *routing_session)
{
    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    bool matches = chat->started && chat->chat_connected &&
                   (!routing_session || !strcmp(routing_session, chat->connection_id));
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
    return matches;
}

static esp_err_t esp_xiaozhi_chat_prepare_mcp_request(void *ctx, const char *routing_session, const char *payload)
{
    esp_xiaozhi_chat_t *chat = ctx;
    if (!esp_xiaozhi_chat_connection_matches(chat, routing_session)) {
        return ESP_ERR_INVALID_STATE;
    }
#if CONFIG_XIAOZHI_VISION_ENABLE
    if (strcmp(chat->vision_connection_id, routing_session)) {
        (void)esp_xiaozhi_vision_set_endpoint(&chat->vision, NULL, NULL);
        strlcpy(chat->vision_connection_id, routing_session, sizeof(chat->vision_connection_id));
    }
    cJSON *request = cJSON_ParseWithOpts(payload, NULL, true);
    if (esp_xiaozhi_protocol_is_legacy_initialize(request)) {
        esp_xiaozhi_chat_note_vision(chat, request);
    }
    cJSON_Delete(request);
#endif
    return ESP_OK;
}

static esp_err_t esp_xiaozhi_chat_send_mcp_reply(void *ctx, const char *routing_session,
                                                 const char *reply_session, const char *payload)
{
    esp_xiaozhi_chat_t *chat = ctx;
    if (!esp_xiaozhi_chat_connection_matches(chat, routing_session)) {
        return ESP_ERR_INVALID_STATE;
    }
    cJSON *session = reply_session && reply_session[0] ? cJSON_CreateString(reply_session) : NULL;
    char *encoded_session = session ? cJSON_PrintUnformatted(session) : NULL;
    cJSON_Delete(session);
    if (reply_session && reply_session[0] && !encoded_session) {
        return ESP_ERR_NO_MEM;
    }
    size_t capacity = strlen(payload) + (encoded_session ? strlen(encoded_session) : 0) + 64;
    char *message = malloc(capacity);
    if (!message) {
        cJSON_free(encoded_session);
        return ESP_ERR_NO_MEM;
    }
    if (encoded_session) {
        snprintf(message, capacity, "{\"type\":\"mcp\",\"session_id\":%s,\"payload\":%s}", encoded_session, payload);
    } else {
        snprintf(message, capacity, "{\"type\":\"mcp\",\"payload\":%s}", payload);
    }
    cJSON_free(encoded_session);
    esp_err_t ret = esp_xiaozhi_transport_send_text(chat->transport_handle, message);
    free(message);
    return ret;
}

static esp_err_t esp_xiaozhi_chat_start_transport(esp_xiaozhi_chat_t *chat)
{
    esp_xiaozhi_transport_callbacks_t callbacks = {
        .on_message_callback = esp_xiaozhi_chat_data_handler,
        .on_audio_callback = esp_xiaozhi_chat_audio_handler,
        .on_connected_callback = esp_xiaozhi_chat_connected_handler,
        .on_disconnected_callback = esp_xiaozhi_chat_disconnected_handler,
        .on_error_callback = esp_xiaozhi_chat_transport_error_handler,
    };
    xEventGroupClearBits(chat->event_group_handle, ESP_XIAOZHI_CHAT_CANCELLED);
    esp_err_t ret = esp_xiaozhi_transport_start(chat->transport_handle, chat->use_mqtt, &callbacks);
    if (ret == ESP_OK) {
        chat->websocket_stopped = false;
    }
    return ret;
}

esp_err_t esp_xiaozhi_chat_start(esp_xiaozhi_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    esp_xiaozhi_chat_t *chat = (esp_xiaozhi_chat_t *)chat_hd;
    if (chat->started || chat->transport_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_xiaozhi_transport_init(&chat->transport_handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "Transport initialization failed");
    if (chat->mcp_engine) {
        esp_xiaozhi_mcp_config_t adapter = {
            .ctx = chat, .transport_handle = chat->transport_handle,
            .send_message = esp_xiaozhi_chat_send_mcp_reply,
            .prepare_request = esp_xiaozhi_chat_prepare_mcp_request,
        };
        esp_mcp_mgr_config_t manager = {
            .transport = esp_mcp_transport_xiaozhi, .config = &adapter, .instance = chat->mcp_engine,
        };
        chat->previous_mcp_compatibility = esp_mcp_is_xiaozhi_compatibility_enabled(chat->mcp_engine);
        ret = esp_mcp_set_xiaozhi_compatibility(chat->mcp_engine, true);
        if (ret != ESP_OK) {
            goto error;
        }
        chat->mcp_compatibility_applied = true;
        ret = esp_mcp_mgr_init(manager, &chat->mcp_mgr_handle);
        if (ret != ESP_OK) {
            goto error;
        }
        ret = esp_mcp_mgr_register_endpoint(chat->mcp_mgr_handle, "mcp_server", NULL);
        if (ret != ESP_OK) {
            goto error;
        }
        ret = esp_mcp_mgr_start(chat->mcp_mgr_handle);
        if (ret != ESP_OK) {
            goto error;
        }
    }
    chat->started = true;
    ret = esp_xiaozhi_chat_start_transport(chat);
    if (ret == ESP_OK) {
        return ESP_OK;
    }
error:
    (void)esp_xiaozhi_chat_stop_runtime(chat);
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

    if (!xiaozhi_chat->started || xiaozhi_chat->audio_channel_opened || xiaozhi_chat->hello_pending) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xiaozhi_chat->websocket_stopped) {
        ret = esp_xiaozhi_chat_start_transport(xiaozhi_chat);
        if (ret != ESP_OK) {
            return ret;
        }
        xiaozhi_chat->websocket_stopped = false;
    }
    if (!message || !message_len) {
        size_t alloc_size = message_len && message_len < CONFIG_XIAOZHI_HELLO_MESSAGE_SIZE ?
                            message_len : CONFIG_XIAOZHI_HELLO_MESSAGE_SIZE;
        message = calloc(1, alloc_size);
        ESP_RETURN_ON_FALSE(message, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");
        message_allocated = true;
        message_len = alloc_size;
        ret = esp_xiaozhi_chat_get_hello_message(xiaozhi_chat, audio, message, alloc_size);
        if (ret != ESP_OK) {
            free(message);
            ESP_RETURN_ON_ERROR(ret, TAG, "Failed to get hello message");
        }
    }

    if (!xiaozhi_chat->started || xiaozhi_chat->audio_channel_opened || xiaozhi_chat->hello_pending) {
        if (message_allocated) {
            free(message);
        }
        return ESP_ERR_INVALID_STATE;
    }
    ret = esp_xiaozhi_chat_validate_hello(xiaozhi_chat, message, message_len);
    if (ret != ESP_OK) {
        if (message_allocated) {
            free(message);
        }
        return ret;
    }
    EventBits_t connected = xEventGroupWaitBits(xiaozhi_chat->event_group_handle,
                                                ESP_XIAOZHI_CHAT_TRANSPORT_CONNECTED | ESP_XIAOZHI_CHAT_CANCELLED,
                                                pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(connected & ESP_XIAOZHI_CHAT_TRANSPORT_CONNECTED) || (connected & ESP_XIAOZHI_CHAT_CANCELLED)) {
        if (message_allocated) {
            free(message);
        }
        return ESP_ERR_TIMEOUT;
    }
    xiaozhi_chat->hello_pending = true;
    xiaozhi_chat->error_occurred = false;
    memset(xiaozhi_chat->session_id, 0, sizeof(xiaozhi_chat->session_id));
    xEventGroupClearBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_SERVER_HELLO | ESP_XIAOZHI_CHAT_HELLO_ERROR);

    ret = esp_xiaozhi_transport_send_text(xiaozhi_chat->transport_handle, message);
    if (message_allocated) {
        free(message);
    }
    if (ret != ESP_OK) {
        xiaozhi_chat->hello_pending = false;
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(xiaozhi_chat->event_group_handle,
                                           ESP_XIAOZHI_CHAT_SERVER_HELLO | ESP_XIAOZHI_CHAT_HELLO_ERROR | ESP_XIAOZHI_CHAT_CANCELLED,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    xiaozhi_chat->hello_pending = false;
    if (!(bits & ESP_XIAOZHI_CHAT_SERVER_HELLO) ||
            (bits & (ESP_XIAOZHI_CHAT_HELLO_ERROR | ESP_XIAOZHI_CHAT_CANCELLED)) || !xiaozhi_chat->chat_connected) {
        xiaozhi_chat->error_occurred = true;
        esp_xiaozhi_chat_error_info_t error = {
            .code = bits & ESP_XIAOZHI_CHAT_CANCELLED ? ESP_ERR_INVALID_STATE :
            bits & ESP_XIAOZHI_CHAT_HELLO_ERROR ? ESP_ERR_INVALID_RESPONSE : ESP_ERR_TIMEOUT,
            .source = "hello",
        };
        if (xiaozhi_chat->event_callback) {
            xiaozhi_chat->event_callback(ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR, &error, xiaozhi_chat->event_callback_ctx);
        }
        return error.code;
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
    if (xiaozhi_chat->use_mqtt && xiaozhi_chat->chat_connected && strlen(xiaozhi_chat->session_id) > 0) {
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
    bool was_open = xiaozhi_chat->audio_channel_opened;
    esp_err_t close_ret = esp_xiaozhi_chat_audio_close(xiaozhi_chat);
    if (!xiaozhi_chat->use_mqtt &&
            (was_open || !xiaozhi_chat->chat_connected || xiaozhi_chat->error_occurred) &&
            !xiaozhi_chat->websocket_stopped && xiaozhi_chat->transport_handle) {
        close_ret = esp_xiaozhi_transport_stop(xiaozhi_chat->transport_handle);
        if (close_ret == ESP_OK) {
            xiaozhi_chat->websocket_stopped = true;
            xiaozhi_chat->chat_connected = false;
            xEventGroupClearBits(xiaozhi_chat->event_group_handle, ESP_XIAOZHI_CHAT_TRANSPORT_CONNECTED);
        }
    }

    esp_event_post(ESP_XIAOZHI_CHAT_EVENTS, ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED, NULL, 0, portMAX_DELAY);

    return close_ret;
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
        .timestamp = esp_timer_get_time() / 1000,
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
    if (mode < ESP_XIAOZHI_CHAT_LISTENING_MODE_REALTIME || mode > ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL_STOP) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *mode_str = (mode == ESP_XIAOZHI_CHAT_LISTENING_MODE_REALTIME) ? "realtime" :
                           (mode == ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO || mode == ESP_XIAOZHI_CHAT_LISTENING_MODE_AUTO_STOP) ? "auto" :
                           "manual";

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
    if (reason < ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED || reason > ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_UNKNOWN) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = NULL;
    esp_err_t ret = esp_xiaozhi_chat_create_session_message(xiaozhi_chat, "abort", &root);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create session message");

    if (reason == ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED) {
        cJSON_AddStringToObject(root, "reason", "wake_word_detected");
    }

    ret = esp_xiaozhi_chat_send_json_message(xiaozhi_chat, root);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_xiaozhi_chat_send_json(esp_xiaozhi_chat_handle_t handle, const char *json)
{
    if (!handle || !json || strlen(json) > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *object = cJSON_ParseWithOpts(json, NULL, true);
    bool valid = cJSON_IsObject(object) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(object, "type"));
    cJSON_Delete(object);
    if (!valid) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xiaozhi_chat_t *chat = (esp_xiaozhi_chat_t *)handle;
    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    bool ready = chat->started && chat->chat_connected;
    esp_xiaozhi_transport_handle_t transport = chat->transport_handle;
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
    return ready && transport ? esp_xiaozhi_transport_send_text(transport, json) : ESP_ERR_INVALID_STATE;
}

esp_err_t esp_xiaozhi_chat_send_mcp(esp_xiaozhi_chat_handle_t handle, const char *payload)
{
    if (!handle || !payload || strlen(payload) > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *object = cJSON_ParseWithOpts(payload, NULL, true);
    bool valid = cJSON_IsObject(object);
    cJSON_Delete(object);
    if (!valid) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xiaozhi_chat_t *chat = (esp_xiaozhi_chat_t *)handle;
    char connection_id[sizeof(chat->connection_id)];
    char session_id[sizeof(chat->session_id)];
    portENTER_CRITICAL(&s_xiaozhi_chat_lock);
    memcpy(connection_id, chat->connection_id, sizeof(connection_id));
    memcpy(session_id, chat->session_id, sizeof(session_id));
    portEXIT_CRITICAL(&s_xiaozhi_chat_lock);
    return esp_xiaozhi_chat_send_mcp_reply(chat, connection_id, session_id, payload);
}

esp_err_t esp_xiaozhi_chat_send_audio_packet(esp_xiaozhi_chat_handle_t handle, const esp_xiaozhi_chat_audio_t *packet)
{
    if (!handle || !packet || !packet->payload || !packet->payload_size) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_xiaozhi_chat_audio_send((esp_xiaozhi_chat_t *)handle, packet);
}

esp_err_t esp_xiaozhi_chat_get_audio_params(esp_xiaozhi_chat_handle_t handle, esp_xiaozhi_chat_audio_t *params)
{
    if (!handle || !params) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xiaozhi_chat_t *chat = (esp_xiaozhi_chat_t *)handle;
    *params = (esp_xiaozhi_chat_audio_t) {
        .format = chat->server_format, .sample_rate = chat->server_sample_rate,
        .channels = chat->server_channels, .frame_duration = chat->server_frame_duration,
    };
    return ESP_OK;
}

esp_err_t esp_xiaozhi_chat_get_request_vision(esp_xiaozhi_chat_handle_t handle,
                                              const esp_mcp_request_context_t *request,
                                              char *url, size_t url_size, char *token, size_t token_size)
{
    if (!handle || !url || !url_size || (token && !token_size)) {
        return ESP_ERR_INVALID_ARG;
    }
    url[0] = '\0';
    if (token) {
        token[0] = '\0';
    }
    if (!request || !request->protocol_version || strcmp(request->protocol_version, "2026-07-28")) {
        return esp_xiaozhi_chat_get_vision_explain(handle, url, url_size, token, token_size);
    }
    cJSON *meta = request->meta_json ? cJSON_ParseWithOpts(request->meta_json, NULL, true) : NULL;
    const cJSON *caps = cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/clientCapabilities");
    const cJSON *extensions = cJSON_GetObjectItemCaseSensitive(caps, "extensions");
    const cJSON *vision = cJSON_GetObjectItemCaseSensitive(extensions, "me.xiaozhi/vision");
    const cJSON *target = cJSON_GetObjectItemCaseSensitive(vision, "url");
    const cJSON *bearer = cJSON_GetObjectItemCaseSensitive(vision, "token");
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (cJSON_IsObject(vision) && cJSON_IsString(target) && (!bearer || cJSON_IsString(bearer))) {
        const char *value = target->valuestring;
        size_t prefix = !strncmp(value, "https://", 8) ? 8 : !strncmp(value, "http://", 7) ? 7 : 0;
        bool valid = prefix && value[prefix] && !strchr("/?#", value[prefix]);
        for (const unsigned char *c = (const unsigned char *)value; valid && *c; c++) {
            valid = !isspace(*c) && !iscntrl(*c);
        }
        const char *secret = bearer ? bearer->valuestring : "";
        valid = valid && !strpbrk(secret, "\r\n");
        if (valid) {
            ret = strlen(value) < url_size && (!token || strlen(secret) < token_size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
            if (ret == ESP_OK) {
                strcpy(url, value);
                if (token) {
                    strcpy(token, secret);
                }
            }
        }
    }
    cJSON_Delete(meta);
    return ret;
}

bool esp_xiaozhi_chat_is_audio_channel_opened(esp_xiaozhi_chat_handle_t handle)
{
    return handle && esp_xiaozhi_chat_is_audio_ready((esp_xiaozhi_chat_t *)handle);
}

esp_err_t esp_xiaozhi_chat_get_udp_health(esp_xiaozhi_chat_handle_t handle, esp_xiaozhi_udp_health_info_t *info)
{
    if (!handle || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xiaozhi_chat_t *chat = (esp_xiaozhi_chat_t *)handle;
    return chat->use_mqtt ? esp_xiaozhi_udp_get_health(chat->udp, info) : ESP_ERR_NOT_SUPPORTED;
}
