/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_check.h>
#include <esp_log.h>
#include <esp_websocket_client.h>
#include <esp_crt_bundle.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "esp_xiaozhi_keystore.h"
#include "esp_xiaozhi_board.h"
#include "esp_xiaozhi_websocket.h"
#include "esp_xiaozhi_payload.h"
#include "esp_xiaozhi_protocol_priv.h"

typedef struct esp_xiaozhi_websocket_s {
    char url[ESP_XIAOZHI_WEBSOCKET_MAX_STRING_SIZE];
    char token[ESP_XIAOZHI_WEBSOCKET_MAX_STRING_SIZE];
    char protocol_version[16];
    char headers[ESP_XIAOZHI_WEBSOCKET_MAX_HEADER_SIZE];
    bool websocket_connected;

    esp_websocket_client_handle_t websocket_client;
    esp_xiaozhi_transport_handle_t transport_handle; /*!< Passed to callbacks so chat can resolve by transport handle */
    esp_xiaozhi_transport_callbacks_t callbacks;

    esp_xiaozhi_payload_t payload;
    size_t frame_received;
    uint8_t payload_opcode;  /* 1=TEXT, 2=BINARY; 0=idle */
} esp_xiaozhi_websocket_t;

static const char *TAG = "ESP_XIAOZHI_WEBSOCKET";

static esp_err_t esp_xiaozhi_websocket_map_missing_config_error(esp_err_t err, const char *field_name)
{
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Required WebSocket config '%s' is missing", field_name);
        return ESP_ERR_NOT_FOUND;
    }
    return err;
}

static void esp_xiaozhi_websocket_build_headers(esp_xiaozhi_websocket_t *websocket)
{
    esp_xiaozhi_chat_board_info_t board_info = {0};
    esp_err_t ret = esp_xiaozhi_chat_get_board_info(&board_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get board info: %s", esp_err_to_name(ret));
    }

    websocket->headers[0] = '\0';
    if (strlen(websocket->token) > 0) {
        if (strchr(websocket->token, ' ') == NULL) {
            snprintf(websocket->headers + strlen(websocket->headers),
                     sizeof(websocket->headers) - strlen(websocket->headers),
                     "Authorization: Bearer %s\r\n", websocket->token);
        } else {
            snprintf(websocket->headers + strlen(websocket->headers),
                     sizeof(websocket->headers) - strlen(websocket->headers),
                     "Authorization: %s\r\n", websocket->token);
        }
    }

    snprintf(websocket->headers + strlen(websocket->headers),
             sizeof(websocket->headers) - strlen(websocket->headers),
             "Protocol-Version: %s\r\n", websocket->protocol_version);

    if (board_info.initialized) {
        snprintf(websocket->headers + strlen(websocket->headers),
                 sizeof(websocket->headers) - strlen(websocket->headers),
                 "Device-Id: %s\r\nClient-Id: %s\r\n",
                 board_info.mac_address, board_info.uuid);
    }
}

static esp_err_t esp_xiaozhi_websocket_dispatch(esp_xiaozhi_websocket_t *websocket, unsigned opcode,
                                                const uint8_t *data, size_t length)
{
    if (opcode == WS_OPCODE_TEXT) {
        return websocket->callbacks.on_message_callback ? websocket->callbacks.on_message_callback(
                   websocket->transport_handle, NULL, 0, (const char *)data, length) : ESP_OK;
    }
    const uint8_t *payload;
    size_t payload_length;
    uint32_t timestamp;
    unsigned type;
    esp_err_t ret = esp_xiaozhi_protocol_decode_frame(esp_xiaozhi_websocket_get_version(
                                                          (esp_xiaozhi_websocket_handle_t)websocket), data, length, &payload, &payload_length, &timestamp, &type);
    if (ret != ESP_OK) {
        return ret;
    }
    if (type == 1 && websocket->callbacks.on_message_callback) {
        return websocket->callbacks.on_message_callback(websocket->transport_handle, NULL, 0,
                                                        (const char *)payload, payload_length);
    }
    if (type == 0 && websocket->callbacks.on_audio_callback) {
        return websocket->callbacks.on_audio_callback(websocket->transport_handle, payload, payload_length, timestamp);
    }
    return type == 0 && websocket->callbacks.on_binary_callback ?
           websocket->callbacks.on_binary_callback(websocket->transport_handle, payload, payload_length) : ESP_OK;
}

esp_err_t esp_xiaozhi_websocket_feed_frame(esp_xiaozhi_websocket_handle_t handle,
                                           unsigned opcode, bool fin, const char *data,
                                           int data_length, int frame_offset, int frame_length)
{
    if (!handle || (data_length > 0 && !data)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    if (opcode != WS_OPCODE_TEXT && opcode != WS_OPCODE_BINARY && opcode != WS_OPCODE_CONTINUATION) {
        return ESP_OK;  /* Control frames never alter message assembly. */
    }
    if ((opcode == WS_OPCODE_TEXT || opcode == WS_OPCODE_BINARY) && fin &&
            frame_offset == 0 && data_length == frame_length && data_length > 0 &&
            websocket->payload_opcode == 0 && websocket->payload.buf == NULL) {
        return esp_xiaozhi_websocket_dispatch(websocket, opcode, (const uint8_t *)data, (size_t)data_length);
    }
    esp_err_t ret = ESP_OK;
    if (data_length < 0 || frame_length < 0 || frame_offset < 0 ||
            frame_offset > frame_length ||
            data_length > frame_length - frame_offset) {
        ret = ESP_ERR_INVALID_SIZE;
    } else {
        if (frame_offset == 0) {
            websocket->frame_received = 0;
            if (opcode == WS_OPCODE_TEXT || opcode == WS_OPCODE_BINARY) {
                if (websocket->payload_opcode) {
                    ret = ESP_ERR_INVALID_STATE;
                } else {
                    websocket->payload_opcode = opcode;
                }
            } else if (!websocket->payload_opcode) {
                ret = ESP_ERR_INVALID_STATE;
            }
        }
        if ((size_t)frame_offset != websocket->frame_received) {
            ret = ESP_ERR_INVALID_STATE;
        }
        if (ret == ESP_OK && data_length > 0) {
            ret = esp_xiaozhi_payload_append(&websocket->payload, data, data_length,
                                             websocket->payload.len, 0, ESP_XIAOZHI_PAYLOAD_MAX_DEFAULT);
            websocket->frame_received += data_length;
        }
        if (ret == ESP_OK && fin && websocket->frame_received == (size_t)frame_length) {
            ret = esp_xiaozhi_websocket_dispatch(websocket, websocket->payload_opcode,
                                                 websocket->payload.buf, websocket->payload.len);
            esp_xiaozhi_payload_clear(&websocket->payload);
            websocket->payload_opcode = 0;
            websocket->frame_received = 0;
        }
    }
    if (ret != ESP_OK) {
        esp_xiaozhi_payload_clear(&websocket->payload);
        websocket->payload_opcode = 0;
        websocket->frame_received = 0;
    }
    return ret;
}

static void esp_xiaozhi_websocket_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)base;
    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handler_args;
    esp_websocket_event_data_t *event = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_CONNECTED");
        websocket->websocket_connected = true;
        if (websocket->callbacks.on_connected_callback) {
            websocket->callbacks.on_connected_callback(websocket->transport_handle);
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        websocket->websocket_connected = false;
        esp_xiaozhi_payload_clear(&websocket->payload);
        websocket->payload_opcode = 0;
        websocket->frame_received = 0;
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback(websocket->transport_handle);
        }
        break;

    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_CLOSED");
        websocket->websocket_connected = false;
        esp_xiaozhi_payload_clear(&websocket->payload);
        websocket->payload_opcode = 0;
        websocket->frame_received = 0;
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback(websocket->transport_handle);
        }
        break;

    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_FINISH");
        websocket->websocket_connected = false;
        esp_xiaozhi_payload_clear(&websocket->payload);
        websocket->payload_opcode = 0;
        websocket->frame_received = 0;
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback(websocket->transport_handle);
        }
        break;

    case WEBSOCKET_EVENT_DATA: {
        esp_err_t ret = esp_xiaozhi_websocket_feed_frame((esp_xiaozhi_websocket_handle_t)websocket,
                                                         event->op_code, event->fin, event->data_ptr,
                                                         event->data_len, event->payload_offset, event->payload_len);
        if (ret != ESP_OK && websocket->callbacks.on_error_callback) {
            websocket->callbacks.on_error_callback(websocket->transport_handle, ret);
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
        if (websocket->callbacks.on_error_callback) {
            websocket->callbacks.on_error_callback(websocket->transport_handle, ESP_FAIL);
        }
        break;

    default:
        ESP_LOGD(TAG, "Other websocket event id: %" PRId32, event_id);
        break;
    }
}

esp_err_t esp_xiaozhi_websocket_init(esp_xiaozhi_websocket_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_websocket_t *websocket = calloc(1, sizeof(esp_xiaozhi_websocket_t));
    ESP_RETURN_ON_FALSE(websocket, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for websocket");

    *handle = (esp_xiaozhi_websocket_handle_t)websocket;

    return ESP_OK;
}

esp_err_t esp_xiaozhi_websocket_deinit(esp_xiaozhi_websocket_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    if (websocket->websocket_client) {
        ESP_RETURN_ON_ERROR(esp_websocket_client_destroy(websocket->websocket_client), TAG,
                            "Failed to destroy WebSocket client");
        websocket->websocket_client = NULL;
    }
    esp_xiaozhi_payload_clear(&websocket->payload);
    free(websocket);
    return ESP_OK;
}

esp_err_t esp_xiaozhi_websocket_start(esp_xiaozhi_websocket_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_chat_keystore_t xiaozhi_chat_keystore;
    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    esp_err_t ret = esp_xiaozhi_chat_keystore_init(&xiaozhi_chat_keystore, "websocket", false);
    ret = esp_xiaozhi_websocket_map_missing_config_error(ret, "namespace");
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to initialize keystore: %s", esp_err_to_name(ret));

    int protocol_version = 1;
    ret = esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "url", "", websocket->url, sizeof(websocket->url));
    if (ret == ESP_OK) {
        ret = esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "token", "", websocket->token, sizeof(websocket->token));
    }
    if (ret == ESP_OK) {
        ret = esp_xiaozhi_chat_keystore_get_int(&xiaozhi_chat_keystore, "version", 1, (int32_t *)&protocol_version);
    }
    esp_err_t close_ret = esp_xiaozhi_chat_keystore_deinit(&xiaozhi_chat_keystore);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read WebSocket configuration");
    ESP_RETURN_ON_ERROR(close_ret, TAG, "Failed to close WebSocket configuration");

    ESP_RETURN_ON_FALSE(strlen(websocket->url) > 0, ESP_ERR_NOT_FOUND, TAG, "Websocket url is not specified");

    ESP_RETURN_ON_FALSE(protocol_version >= 1 && protocol_version <= 3, ESP_ERR_NOT_SUPPORTED,
                        TAG, "Unsupported WebSocket protocol version");
    snprintf(websocket->protocol_version, sizeof(websocket->protocol_version), "%d", protocol_version);
    esp_xiaozhi_websocket_build_headers(websocket);

    esp_websocket_client_config_t websocket_cfg = {
        .uri = websocket->url,
        .headers = websocket->headers,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_reconnect = false,
        .enable_close_reconnect = true,
        .reconnect_timeout_ms = 5000,
    };

    if (websocket->websocket_client) {
        esp_websocket_client_destroy(websocket->websocket_client);
        websocket->websocket_client = NULL;
    }

    websocket->websocket_client = esp_websocket_client_init(&websocket_cfg);
    ESP_RETURN_ON_FALSE(websocket->websocket_client, ESP_ERR_NO_MEM, TAG, "Failed to initialize websocket client");

    ret = esp_websocket_register_events(websocket->websocket_client, WEBSOCKET_EVENT_ANY, esp_xiaozhi_websocket_event, websocket);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to register websocket events: %s", esp_err_to_name(ret));

    ret = esp_websocket_client_start(websocket->websocket_client);
    if (ret) {
        ESP_LOGE(TAG, "Failed to start websocket client: %s", esp_err_to_name(ret));
        esp_websocket_client_destroy(websocket->websocket_client);
        websocket->websocket_client = NULL;
        return ret;
    }

    return ESP_OK;
}

esp_err_t esp_xiaozhi_websocket_stop(esp_xiaozhi_websocket_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    if (!websocket->websocket_client) {
        return ESP_OK;
    }

    esp_err_t ret = esp_websocket_client_stop(websocket->websocket_client);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to stop websocket client: %s", esp_err_to_name(ret));
    websocket->websocket_connected = false;
    esp_xiaozhi_payload_clear(&websocket->payload);
    websocket->payload_opcode = 0;
    websocket->frame_received = 0;

    return ESP_OK;
}

esp_err_t esp_xiaozhi_websocket_send_text(esp_xiaozhi_websocket_handle_t handle, const char *text)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(text, ESP_ERR_INVALID_ARG, TAG, "Invalid text");

    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    ESP_RETURN_ON_FALSE(websocket->websocket_client, ESP_ERR_INVALID_STATE, TAG, "Websocket client not initialized");
    ESP_RETURN_ON_FALSE(websocket->websocket_connected, ESP_ERR_INVALID_STATE, TAG, "Websocket is not connected");

    int sent = esp_websocket_client_send_text(websocket->websocket_client, text, strlen(text), portMAX_DELAY);
    if (sent != (int)strlen(text)) {
        websocket->websocket_connected = false;
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback((esp_xiaozhi_transport_handle_t)websocket->transport_handle);
        }
        ESP_LOGE(TAG, "Failed to send text frame");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_xiaozhi_websocket_send_binary(esp_xiaozhi_websocket_handle_t handle, const uint8_t *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data");
    ESP_RETURN_ON_FALSE(data_len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid data_len");

    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    ESP_RETURN_ON_FALSE(websocket->websocket_client, ESP_ERR_INVALID_STATE, TAG, "Websocket client not initialized");
    ESP_RETURN_ON_FALSE(websocket->websocket_connected, ESP_ERR_INVALID_STATE, TAG, "Websocket is not connected");

    int sent = esp_websocket_client_send_bin(websocket->websocket_client, (const char *)data, data_len, portMAX_DELAY);
    if (sent != (int)data_len) {
        websocket->websocket_connected = false;
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback((esp_xiaozhi_transport_handle_t)websocket->transport_handle);
        }
        ESP_LOGE(TAG, "Failed to send binary frame");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_xiaozhi_websocket_set_callbacks(esp_xiaozhi_websocket_handle_t handle, esp_xiaozhi_transport_handle_t transport_handle, const esp_xiaozhi_transport_callbacks_t *callbacks)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(callbacks, ESP_ERR_INVALID_ARG, TAG, "Invalid callbacks");

    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    websocket->transport_handle = transport_handle;
    websocket->callbacks = *callbacks;

    return ESP_OK;
}

int esp_xiaozhi_websocket_get_version(esp_xiaozhi_websocket_handle_t handle)
{
    if (!handle) {
        return 1;
    }
    esp_xiaozhi_websocket_t *websocket = (esp_xiaozhi_websocket_t *)handle;
    return websocket->protocol_version[0] ? atoi(websocket->protocol_version) : 1;
}

esp_err_t esp_xiaozhi_websocket_send_audio(esp_xiaozhi_websocket_handle_t handle, const uint8_t *data,
                                           size_t length, uint32_t timestamp)
{
    if (!handle || !data || !length || length > INT_MAX - 16) {
        return ESP_ERR_INVALID_ARG;
    }
    int version = esp_xiaozhi_websocket_get_version(handle);
    if (version == 1) {
        return esp_xiaozhi_websocket_send_binary(handle, data, length);
    }
    uint8_t stack_buf[512];
    uint8_t *packet = (length + 16 <= sizeof(stack_buf)) ? stack_buf : malloc(length + 16);
    if (!packet) {
        return ESP_ERR_NO_MEM;
    }
    size_t written = 0;
    esp_err_t ret = esp_xiaozhi_protocol_encode_audio(version, data, length, timestamp,
                                                      packet, length + 16, &written);
    if (ret == ESP_OK) {
        ret = esp_xiaozhi_websocket_send_binary(handle, packet, written);
    }
    if (packet != stack_buf) {
        free(packet);
    }
    return ret;
}
