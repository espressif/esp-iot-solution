/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_check.h>
#include <esp_log.h>
#include <mqtt_client.h>
#include <esp_crt_bundle.h>

#include "esp_xiaozhi_keystore.h"
#include "esp_xiaozhi_mqtt.h"
#include "esp_xiaozhi_payload.h"

typedef struct esp_xiaozhi_mqtt_s {
    char publish_topic[ESP_XIAOZHI_MQTT_MAX_STRING_SIZE];
    char subscribe_topic[ESP_XIAOZHI_MQTT_MAX_STRING_SIZE];
    bool mqtt_connected;

    esp_mqtt_client_handle_t mqtt_client;
    esp_xiaozhi_transport_handle_t transport_handle; /*!< Passed to callbacks so chat can resolve by transport handle */
    esp_xiaozhi_transport_callbacks_t callbacks;

    /* Protocol-layer reassembly for fragmented MQTT payload (same as WebSocket). */
    esp_xiaozhi_payload_t payload;
    char payload_topic[ESP_XIAOZHI_MQTT_MAX_STRING_SIZE];
    int payload_topic_len;
} esp_xiaozhi_mqtt_t;

static const char *TAG = "ESP_XIAOZHI_MQTT";

static esp_err_t esp_xiaozhi_mqtt_map_missing_config_error(esp_err_t err, const char *field_name)
{
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Required MQTT config '%s' is missing", field_name);
        return ESP_ERR_NOT_FOUND;
    }
    return err;
}

static void esp_xiaozhi_mqtt_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_xiaozhi_mqtt_t *mqtt = (esp_xiaozhi_mqtt_t *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
        mqtt->mqtt_connected = true;

        if (strlen(mqtt->subscribe_topic) > 0) {
            int msg_id = esp_mqtt_client_subscribe(mqtt->mqtt_client, mqtt->subscribe_topic, 0);
            ESP_LOGD(TAG, "Subscribed to topic %s, msg_id=%d", mqtt->subscribe_topic, msg_id);
        }

        if (mqtt->callbacks.on_connected_callback) {
            mqtt->callbacks.on_connected_callback(mqtt->transport_handle);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt->mqtt_connected = false;
        esp_xiaozhi_payload_clear(&mqtt->payload);
        if (mqtt->callbacks.on_disconnected_callback) {
            mqtt->callbacks.on_disconnected_callback(mqtt->transport_handle);
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: {
        int data_len = event->data_len;
        int total_len = event->total_data_len;
        int offset = event->current_data_offset;

        if (!event->data || data_len <= 0) {
            break;
        }
        /* Single-chunk message: no append payload needed */
        if (total_len <= 0 || (offset == 0 && data_len == total_len)) {
            ESP_LOGD(TAG, "MQTT_EVENT_DATA, topic: %.*s, data: %.*s", (int)event->topic_len, event->topic, (int)data_len, event->data);
            if (mqtt->callbacks.on_message_callback) {
                mqtt->callbacks.on_message_callback(mqtt->transport_handle, event->topic, event->topic_len, event->data, (size_t)data_len);
            }
            break;
        }
        /* Multi-chunk message: append payload until complete delivery to upper layer */
        if (offset == 0) {
            mqtt->payload_topic_len = event->topic_len;
            if (mqtt->payload_topic_len >= (int)sizeof(mqtt->payload_topic)) {
                ESP_LOGW(TAG, "MQTT payload topic truncated from %d to %d", mqtt->payload_topic_len, (int)sizeof(mqtt->payload_topic) - 1);
                mqtt->payload_topic_len = (int)sizeof(mqtt->payload_topic) - 1;
            }
            if (mqtt->payload_topic_len > 0 && event->topic) {
                memcpy(mqtt->payload_topic, event->topic, (size_t)mqtt->payload_topic_len);
                mqtt->payload_topic[mqtt->payload_topic_len] = '\0';
            } else {
                mqtt->payload_topic[0] = '\0';
                mqtt->payload_topic_len = 0;
            }
        }
        esp_err_t err = esp_xiaozhi_payload_append(&mqtt->payload, event->data, data_len, offset, total_len, ESP_XIAOZHI_PAYLOAD_MAX_DEFAULT);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "MQTT payload append failed: %s, offset=%d total=%d chunk=%d", esp_err_to_name(err), offset, total_len, data_len);
            esp_xiaozhi_payload_clear(&mqtt->payload);
            mqtt->payload_topic_len = 0;
            if (mqtt->callbacks.on_error_callback) {
                mqtt->callbacks.on_error_callback(mqtt->transport_handle, err);
            }
            break;
        }
        if (offset + data_len == total_len) {
            if (mqtt->callbacks.on_message_callback) {
                mqtt->callbacks.on_message_callback(mqtt->transport_handle, mqtt->payload_topic, (size_t)mqtt->payload_topic_len, (const char *)mqtt->payload.buf, mqtt->payload.len);
            }
            esp_xiaozhi_payload_clear(&mqtt->payload);
            mqtt->payload_topic_len = 0;
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        ESP_LOGD(TAG, "Other MQTT event id: %d", event->event_id);
        break;
    }
}

esp_err_t esp_xiaozhi_mqtt_init(esp_xiaozhi_mqtt_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_mqtt_t *mqtt = calloc(1, sizeof(esp_xiaozhi_mqtt_t));
    ESP_RETURN_ON_FALSE(mqtt, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MQTT");

    *handle = (esp_xiaozhi_mqtt_handle_t)mqtt;

    return ESP_OK;
}

esp_err_t esp_xiaozhi_mqtt_deinit(esp_xiaozhi_mqtt_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_mqtt_t *mqtt = (esp_xiaozhi_mqtt_t *)handle;
    esp_xiaozhi_payload_clear(&mqtt->payload);
    if (mqtt->mqtt_client) {
        esp_err_t ret = esp_mqtt_client_destroy(mqtt->mqtt_client);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to destroy MQTT client: %s", esp_err_to_name(ret));
        }
        mqtt->mqtt_client = NULL;
    }

    free(mqtt);

    return ESP_OK;
}

esp_err_t esp_xiaozhi_mqtt_start(esp_xiaozhi_mqtt_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    char endpoint[ESP_XIAOZHI_MQTT_MAX_STRING_SIZE];
    char client_id[ESP_XIAOZHI_MQTT_MAX_STRING_SIZE];
    char username[ESP_XIAOZHI_MQTT_MAX_STRING_SIZE];
    char password[ESP_XIAOZHI_MQTT_MAX_STRING_SIZE];
    esp_xiaozhi_chat_keystore_t xiaozhi_chat_keystore;
    esp_err_t ret = ESP_OK;
    esp_xiaozhi_mqtt_t *mqtt = (esp_xiaozhi_mqtt_t *)handle;
    ret = esp_xiaozhi_chat_keystore_init(&xiaozhi_chat_keystore, "mqtt", false);
    ret = esp_xiaozhi_mqtt_map_missing_config_error(ret, "namespace");
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to initialize keystore %s", esp_err_to_name(ret));

    memset(endpoint, 0, sizeof(endpoint));
    memset(client_id, 0, sizeof(client_id));
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    memset(mqtt->publish_topic, 0, sizeof(mqtt->publish_topic));
    memset(mqtt->subscribe_topic, 0, sizeof(mqtt->subscribe_topic));
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "endpoint", "", endpoint, sizeof(endpoint));
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "client_id", "", client_id, sizeof(client_id));
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "username", "", username, sizeof(username));
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "password", "", password, sizeof(password));
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "publish_topic", "", mqtt->publish_topic, sizeof(mqtt->publish_topic) - 1);
    esp_xiaozhi_chat_keystore_get_string(&xiaozhi_chat_keystore, "subscribe_topic", "", mqtt->subscribe_topic, sizeof(mqtt->subscribe_topic) - 1);
    esp_xiaozhi_chat_keystore_deinit(&xiaozhi_chat_keystore);
    ESP_RETURN_ON_FALSE(strlen(endpoint) > 0, ESP_ERR_NOT_FOUND, TAG, "MQTT endpoint is not specified");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .hostname = endpoint,
                .port = ESP_XIAOZHI_MQTT_DEFAULT_PORT,
                .transport = MQTT_TRANSPORT_OVER_SSL,
            },
            .verification = {
                .crt_bundle_attach = esp_crt_bundle_attach,
            },
        },
        .credentials = {
            .client_id = client_id,
            .username = username,
            .authentication = {
                .password = password,
            },
        },
        .session = {
            .keepalive = 90,
        },
    };

    if (mqtt->mqtt_client) {
        ESP_LOGW(TAG, "MQTT client already started");
        esp_mqtt_client_destroy(mqtt->mqtt_client);
        mqtt->mqtt_client = NULL;
    }

    mqtt->mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_RETURN_ON_FALSE(mqtt->mqtt_client, ESP_ERR_INVALID_ARG, TAG, "Failed to initialize MQTT client");

    esp_mqtt_client_register_event(mqtt->mqtt_client, MQTT_EVENT_ANY, esp_xiaozhi_mqtt_event, mqtt);
    ret = esp_mqtt_client_start(mqtt->mqtt_client);
    if (ret) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt->mqtt_client);
        mqtt->mqtt_client = NULL;
        return ret;
    }

    ESP_LOGD(TAG, "MQTT client started successfully");
    return ESP_OK;
}

esp_err_t esp_xiaozhi_mqtt_stop(esp_xiaozhi_mqtt_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_mqtt_t *mqtt = (esp_xiaozhi_mqtt_t *)handle;
    if (!mqtt->mqtt_client) {
        return ESP_OK;
    }

    esp_err_t ret = esp_mqtt_client_stop(mqtt->mqtt_client);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
    mqtt->mqtt_connected = false;
    mqtt->payload_topic_len = 0;
    esp_xiaozhi_payload_clear(&mqtt->payload);

    return ESP_OK;
}

esp_err_t esp_xiaozhi_mqtt_send_text(esp_xiaozhi_mqtt_handle_t handle, const char *text)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(text, ESP_ERR_INVALID_ARG, TAG, "Invalid text");

    esp_xiaozhi_mqtt_t *mqtt = (esp_xiaozhi_mqtt_t *)handle;
    ESP_RETURN_ON_FALSE(strlen(mqtt->publish_topic) > 0, ESP_ERR_INVALID_ARG, TAG, "Publish topic is not specified");

    ESP_LOGD(TAG, "Sending text to topic %s: %s", mqtt->publish_topic, text);
    int msg_id = esp_mqtt_client_publish(mqtt->mqtt_client, mqtt->publish_topic, text, 0, 0, 0);
    ESP_RETURN_ON_FALSE(msg_id != -1, ESP_ERR_INVALID_ARG, TAG, "Failed to publish message: %s", text);

    return ESP_OK;
}

esp_err_t esp_xiaozhi_mqtt_set_callbacks(esp_xiaozhi_mqtt_handle_t handle, esp_xiaozhi_transport_handle_t transport_handle, const esp_xiaozhi_transport_callbacks_t *callbacks)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(callbacks, ESP_ERR_INVALID_ARG, TAG, "Invalid callbacks");

    esp_xiaozhi_mqtt_t *mqtt = (esp_xiaozhi_mqtt_t *)handle;
    mqtt->transport_handle = transport_handle;
    mqtt->callbacks = *callbacks;

    return ESP_OK;
}
