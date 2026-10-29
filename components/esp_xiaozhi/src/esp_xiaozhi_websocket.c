/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_check.h>
#include <esp_log.h>
#include <esp_websocket_client.h>
#include <esp_crt_bundle.h>

#include "esp_xiaozhi_keystore.h"
#include "esp_xiaozhi_board.h"
#include "esp_xiaozhi_websocket.h"
#include "esp_xiaozhi_payload.h"

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
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback(websocket->transport_handle);
        }
        break;

    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_CLOSED");
        websocket->websocket_connected = false;
        esp_xiaozhi_payload_clear(&websocket->payload);
        websocket->payload_opcode = 0;
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback(websocket->transport_handle);
        }
        break;

    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_FINISH");
        websocket->websocket_connected = false;
        esp_xiaozhi_payload_clear(&websocket->payload);
        websocket->payload_opcode = 0;
        if (websocket->callbacks.on_disconnected_callback) {
            websocket->callbacks.on_disconnected_callback(websocket->transport_handle);
        }
        break;

    case WEBSOCKET_EVENT_DATA: {
        /* Only handle WebSocket protocol-layer fragmentation here: reassemble frames (fin + payload_offset
         * /payload_len) into one complete WS message, then pass to upper layer. Application/business-layer
         * message completeness is the responsibility of the application layer. */
        uint8_t op = (uint8_t)event->op_code;
        bool fin = event->fin;
        const char *data = event->data_ptr;
        int data_len = event->data_len;
        int payload_offset = event->payload_offset;
        int payload_len = event->payload_len;

        if (op != WS_OPCODE_TEXT && op != WS_OPCODE_BINARY && op != WS_OPCODE_CONTINUATION) {
            ESP_LOGD(TAG, "Ignore websocket opcode=%d len=%d", op, data_len);
            break;
        }
        if (!data || data_len <= 0) {
            break;
        }

        /* Single-frame message: no append payload needed */
        if ((op == WS_OPCODE_TEXT || op == WS_OPCODE_BINARY) && fin && websocket->payload.buf == NULL) {
            if (op == WS_OPCODE_TEXT && websocket->callbacks.on_message_callback) {
                websocket->callbacks.on_message_callback(websocket->transport_handle, NULL, 0, data, (size_t)data_len);
            } else if (op == WS_OPCODE_BINARY && websocket->callbacks.on_binary_callback) {
                websocket->callbacks.on_binary_callback(websocket->transport_handle, (const uint8_t *)data, (size_t)data_len);
            }
            break;
        }

        /* Multi-frame: append payload until fin */
        if (op == WS_OPCODE_CONTINUATION && websocket->payload_opcode == 0) {
            ESP_LOGW(TAG, "Unexpected continuation frame without prior TEXT/BINARY");
            break;
        }
        if (op == WS_OPCODE_TEXT || op == WS_OPCODE_BINARY) {
            websocket->payload_opcode = op;
        }
        esp_err_t err = esp_xiaozhi_payload_append(&websocket->payload, data, data_len, payload_offset, payload_len, ESP_XIAOZHI_PAYLOAD_MAX_DEFAULT);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "WebSocket payload append failed: %s, offset=%d total=%d chunk=%d opcode=%d",
                     esp_err_to_name(err), payload_offset, payload_len, data_len, op);
            esp_xiaozhi_payload_clear(&websocket->payload);
            websocket->payload_opcode = 0;
            if (websocket->callbacks.on_error_callback) {
                websocket->callbacks.on_error_callback(websocket->transport_handle, err);
            }
            break;
        }
        if (fin) {
            esp_xiaozhi_payload_t *r = &websocket->payload;
            uint8_t o = websocket->payload_opcode;
            if (o == WS_OPCODE_TEXT && websocket->callbacks.on_message_callback) {
                websocket->callbacks.on_message_callback(websocket->transport_handle, NULL, 0, (const char *)r->buf, r->len);
            } else if (o == WS_OPCODE_BINARY && websocket->callbacks.on_binary_callback) {
                websocket->callbacks.on_binary_callback(websocket->transport_handle, r->buf, r->len);
            }
            esp_xiaozhi_payload_clear(&websocket->payload);
            websocket->payload_opcode = 0;
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
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
    esp_xiaozhi_payload_clear(&websocket->payload);
    if (websocket->websocket_client) {
        esp_websocket_client_destroy(websocket->websocket_client);
        websocket->websocket_client = NULL;
    }

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
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "url", "", websocket->url, sizeof(websocket->url));
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "token", "", websocket->token, sizeof(websocket->token));
    esp_xiaozhi_chat_keystore_get_int(&xiaozhi_chat_keystore, "version", 1, (int32_t *)&protocol_version);
    esp_xiaozhi_chat_keystore_deinit(&xiaozhi_chat_keystore);

    ESP_RETURN_ON_FALSE(strlen(websocket->url) > 0, ESP_ERR_NOT_FOUND, TAG, "Websocket url is not specified");

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
    if (sent < 0) {
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
    if (sent < 0) {
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
