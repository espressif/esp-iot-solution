/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"

#include "audio_processor.h"
#include "esp_gmf_afe.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_oal_mem.h"
#include "esp_codec_dev.h"
#include "esp_board_manager_adapter.h"
#include "esp_board_manager_includes.h"

#include "esp_mcp_engine.h"
#include "esp_mcp_tool.h"
#include "esp_mcp_property.h"
#include "esp_mcp_data.h"
#include "esp_xiaozhi_camera.h"
#include "esp_xiaozhi_info.h"
#include "esp_xiaozhi_chat_app.h"
#include "esp_xiaozhi_chat_display.h"

#if CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
#include "esp_camera.h"
#endif

static char *TAG = "ESP_XIAOZHI_CHAT_APP";

#define ESP_XIAOZHI_CHAT_APP_AUDIO_READ_ERROR_BACKOFF_MS 20
#define ESP_XIAOZHI_CHAT_APP_AUDIO_READ_ERROR_LOG_INTERVAL 50
#define ESP_XIAOZHI_CHAT_APP_AUDIO_READ_MAX_ERRORS 200

static int current_volume = 50;
static int current_brightness = 80;
static char current_theme[32] = "light";
static int current_hue = 0;          // 0..360
static int current_saturation = 0;   // 0..100
static int current_value = 0;        // 0..100
static int current_red = 0;          // 0..255
static int current_green = 0;        // 0..255
static int current_blue = 0;         // 0..255
static esp_xiaozhi_camera_handle_t *s_camera_explain = NULL;

static esp_err_t init_camera_explain_if_needed(void)
{
    if (s_camera_explain != NULL) {
        return ESP_OK;
    }

    esp_xiaozhi_camera_config_t camera_config = {
        .explain_url = NULL,
        .explain_token = NULL,
    };
    return esp_xiaozhi_camera_create(&camera_config, &s_camera_explain);
}

static esp_mcp_value_t camera_take_photo_callback(const esp_mcp_property_list_t *properties)
{
    const char *question = esp_mcp_property_list_get_property_string(properties, "question");
    const char *url = esp_mcp_property_list_get_property_string(properties, "url");
    const char *token = esp_mcp_property_list_get_property_string(properties, "token");
    if (question == NULL || url == NULL) {
        ESP_LOGE(TAG, "Missing question or vision url");
        return esp_mcp_value_create_bool(false);
    }

    esp_err_t ret = init_camera_explain_if_needed();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create camera explain client: %s", esp_err_to_name(ret));
        return esp_mcp_value_create_bool(false);
    }

    ret = esp_xiaozhi_camera_set_explain_url(s_camera_explain, url, token);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure explain url: %s", esp_err_to_name(ret));
        return esp_mcp_value_create_bool(false);
    }
#if CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    camera_fb_t *frame_buffer = NULL;
    for (int i = 0; i < 2; i++) {
        if (frame_buffer != NULL) {
            esp_camera_fb_return(frame_buffer);
            frame_buffer = NULL;
        }
        frame_buffer = esp_camera_fb_get();
        if (frame_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to capture photo");
            return esp_mcp_value_create_bool(false);
        }
    }

    esp_xiaozhi_camera_frame_t frame = {
        .data = frame_buffer->buf,
        .len = frame_buffer->len,
    };

    if (frame_buffer->format != PIXFORMAT_JPEG) {
        ESP_LOGE(TAG, "Camera frame must already be JPEG, got format=%d", frame_buffer->format);
        esp_camera_fb_return(frame_buffer);
        return esp_mcp_value_create_bool(false);
    }

    char *response = heap_caps_calloc(1, 4096, MALLOC_CAP_8BIT);
    if (response == NULL) {
        esp_camera_fb_return(frame_buffer);
        ESP_LOGE(TAG, "Failed to allocate explain response buffer");
        return esp_mcp_value_create_bool(false);
    }

    uint16_t response_len = 0;
    ret = esp_xiaozhi_camera_explain(s_camera_explain, &frame, question, response, 4096, &response_len);
    esp_camera_fb_return(frame_buffer);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to explain photo: %s", esp_err_to_name(ret));
        free(response);
        return esp_mcp_value_create_bool(false);
    }

    esp_mcp_value_t result = esp_mcp_value_create_string(response);
    free(response);
    if (result.type == ESP_MCP_VALUE_TYPE_INVALID) {
        ESP_LOGE(TAG, "Failed to create explain result value");
        return esp_mcp_value_create_bool(false);
    }

    return result;
#else
    return esp_mcp_value_create_bool(false);
#endif
}

static esp_mcp_value_t get_device_status_callback(const esp_mcp_property_list_t* properties)
{
    ESP_LOGI(TAG, "get_device_status_callback called");

    cJSON *status = cJSON_CreateObject();
    if (!status) {
        ESP_LOGE(TAG, "Failed to create status object");
        return esp_mcp_value_create_bool(false);
    }

    cJSON *audio = cJSON_CreateObject();
    if (!audio) {
        ESP_LOGE(TAG, "Failed to create audio object");
        cJSON_Delete(status);
        return esp_mcp_value_create_bool(false);
    }

    cJSON *screen = cJSON_CreateObject();
    if (!screen) {
        ESP_LOGE(TAG, "Failed to create screen object");
        cJSON_Delete(audio);
        cJSON_Delete(status);
        return esp_mcp_value_create_bool(false);
    }

    cJSON *volume_item = cJSON_CreateNumber(current_volume);
    if (!volume_item) {
        ESP_LOGE(TAG, "Failed to create volume item");
        cJSON_Delete(screen);
        cJSON_Delete(audio);
        cJSON_Delete(status);
        return esp_mcp_value_create_bool(false);
    }
    cJSON_AddItemToObject(audio, "volume", volume_item);
    cJSON_AddItemToObject(status, "audio", audio);

    cJSON *brightness_item = cJSON_CreateNumber(current_brightness);
    if (!brightness_item) {
        ESP_LOGE(TAG, "Failed to create brightness item");
        cJSON_Delete(screen);
        cJSON_Delete(status);
        return esp_mcp_value_create_bool(false);
    }
    cJSON_AddItemToObject(screen, "brightness", brightness_item);

    cJSON *theme_item = cJSON_CreateString(current_theme);
    if (!theme_item) {
        ESP_LOGE(TAG, "Failed to create theme item");
        cJSON_Delete(screen);
        cJSON_Delete(status);
        return esp_mcp_value_create_bool(false);
    }
    cJSON_AddItemToObject(screen, "theme", theme_item);
    cJSON_AddItemToObject(status, "screen", screen);

    char *json_string = cJSON_Print(status);
    cJSON_Delete(status);

    if (!json_string) {
        ESP_LOGE(TAG, "Failed to print JSON string");
        return esp_mcp_value_create_bool(false);
    }

    esp_mcp_value_t result = esp_mcp_value_create_string(json_string);
    free(json_string);

    if (result.type == ESP_MCP_VALUE_TYPE_INVALID) {
        ESP_LOGE(TAG, "Failed to create string value for device status");
        return esp_mcp_value_create_bool(false);
    }

    return result;
}

static esp_mcp_value_t set_volume_callback(const esp_mcp_property_list_t* properties)
{
    int volume = esp_mcp_property_list_get_property_int(properties, "volume");
    if (volume < 0 || volume > 100) {
        ESP_LOGE(TAG, "Invalid volume value: %d", volume);
        return esp_mcp_value_create_bool(false);
    }

    current_volume = volume;
    ESP_LOGI(TAG, "Volume set to: %d", current_volume);
    esp_xiaozhi_chat_display_set_volume(current_volume);

    return esp_mcp_value_create_bool(true);
}

static esp_mcp_value_t set_brightness_callback(const esp_mcp_property_list_t* properties)
{
    int brightness = esp_mcp_property_list_get_property_int(properties, "brightness");
    if (brightness < 0 || brightness > 100) {
        ESP_LOGE(TAG, "Invalid brightness value: %d", brightness);
        return esp_mcp_value_create_bool(false);
    }

    current_brightness = brightness;
    ESP_LOGI(TAG, "Brightness set to: %d", current_brightness);
    esp_xiaozhi_chat_display_set_brightness(current_brightness);

    return esp_mcp_value_create_bool(true);
}

static esp_mcp_value_t set_theme_callback(const esp_mcp_property_list_t* properties)
{
    const char *theme = esp_mcp_property_list_get_property_string(properties, "theme");
    if (!theme) {
        ESP_LOGE(TAG, "Failed to get theme");
        return esp_mcp_value_create_bool(false);
    }

    ESP_LOGI(TAG, "Theme set to: %s", theme);
    strncpy(current_theme, theme, sizeof(current_theme) - 1);
    current_theme[sizeof(current_theme) - 1] = '\0';

    esp_xiaozhi_chat_display_set_theme(theme);

    return esp_mcp_value_create_bool(true);
}

static esp_mcp_value_t set_hsv_callback(const esp_mcp_property_list_t* properties)
{
    const char *hsv = esp_mcp_property_list_get_property_array(properties, "HSV");
    if (!hsv) {
        ESP_LOGE(TAG, "Failed to get HSV");
        return esp_mcp_value_create_bool(false);
    }

    ESP_LOGI(TAG, "HSV set to: %s", hsv);
    cJSON *hsv_json = cJSON_Parse(hsv);
    if (!hsv_json) {
        ESP_LOGE(TAG, "Invalid HSV value: %s", hsv);
        return esp_mcp_value_create_bool(false);
    }

    if (!cJSON_IsArray(hsv_json)) {
        ESP_LOGE(TAG, "Invalid HSV value (expect JSON array): %s", hsv);
        cJSON_Delete(hsv_json);
        return esp_mcp_value_create_bool(false);
    }

    int size = cJSON_GetArraySize(hsv_json);
    if (size <= 0 || size > 3) {
        ESP_LOGE(TAG, "Invalid HSV array size=%d (expect 1..3): %s", size, hsv);
        cJSON_Delete(hsv_json);
        return esp_mcp_value_create_bool(false);
    }

    int hue = current_hue;
    int saturation = current_saturation;
    int value = current_value;

    cJSON *h_item = cJSON_GetArrayItem(hsv_json, 0);
    if (h_item && !cJSON_IsNull(h_item)) {
        if (!cJSON_IsNumber(h_item)) {
            ESP_LOGE(TAG, "Invalid HSV[0] type (expect number or null): %s", hsv);
            cJSON_Delete(hsv_json);
            return esp_mcp_value_create_bool(false);
        }
        hue = h_item->valueint;
    }

    cJSON *s_item = (size >= 2) ? cJSON_GetArrayItem(hsv_json, 1) : NULL;
    if (s_item && !cJSON_IsNull(s_item)) {
        if (!cJSON_IsNumber(s_item)) {
            ESP_LOGE(TAG, "Invalid HSV[1] type (expect number or null): %s", hsv);
            cJSON_Delete(hsv_json);
            return esp_mcp_value_create_bool(false);
        }
        saturation = s_item->valueint;
    }

    cJSON *v_item = (size >= 3) ? cJSON_GetArrayItem(hsv_json, 2) : NULL;
    if (v_item && !cJSON_IsNull(v_item)) {
        if (!cJSON_IsNumber(v_item)) {
            ESP_LOGE(TAG, "Invalid HSV[2] type (expect number or null): %s", hsv);
            cJSON_Delete(hsv_json);
            return esp_mcp_value_create_bool(false);
        }
        value = v_item->valueint;
    }
    cJSON_Delete(hsv_json);

    if (hue < 0 || hue > 360 || saturation < 0 || saturation > 100 || value < 0 || value > 100) {
        ESP_LOGE(TAG, "HSV out of range: hue=%d saturation=%d value=%d (expect hue 0-360, sat/val 0-100)", hue, saturation, value);
        return esp_mcp_value_create_bool(false);
    }

    current_hue = hue;
    current_saturation = saturation;
    current_value = value;

    ESP_LOGI(TAG, "HSV set to: hue: %d, saturation: %d, value: %d", hue, saturation, value);
    return esp_mcp_value_create_bool(true);
}

static esp_mcp_value_t set_rgb_callback(const esp_mcp_property_list_t* properties)
{
    const char *rgb = esp_mcp_property_list_get_property_object(properties, "RGB");
    if (!rgb) {
        ESP_LOGE(TAG, "Failed to get RGB");
        return esp_mcp_value_create_bool(false);
    }

    ESP_LOGI(TAG, "RGB set to: %s", rgb);
    cJSON *rgb_json = cJSON_Parse(rgb);
    if (!rgb_json) {
        ESP_LOGE(TAG, "Invalid RGB value: %s", rgb);
        return esp_mcp_value_create_bool(false);
    }

    if (!cJSON_IsObject(rgb_json)) {
        ESP_LOGE(TAG, "Invalid RGB value (expect JSON object): %s", rgb);
        cJSON_Delete(rgb_json);
        return esp_mcp_value_create_bool(false);
    }

    int red = current_red;
    int green = current_green;
    int blue = current_blue;
    bool updated = false;

    cJSON *r_item = cJSON_GetObjectItem(rgb_json, "red");
    if (r_item && !cJSON_IsNull(r_item)) {
        if (!cJSON_IsNumber(r_item)) {
            ESP_LOGE(TAG, "Invalid RGB.red type (expect number or null): %s", rgb);
            cJSON_Delete(rgb_json);
            return esp_mcp_value_create_bool(false);
        }
        red = r_item->valueint;
        updated = true;
    }

    cJSON *g_item = cJSON_GetObjectItem(rgb_json, "green");
    if (g_item && !cJSON_IsNull(g_item)) {
        if (!cJSON_IsNumber(g_item)) {
            ESP_LOGE(TAG, "Invalid RGB.green type (expect number or null): %s", rgb);
            cJSON_Delete(rgb_json);
            return esp_mcp_value_create_bool(false);
        }
        green = g_item->valueint;
        updated = true;
    }

    cJSON *b_item = cJSON_GetObjectItem(rgb_json, "blue");
    if (b_item && !cJSON_IsNull(b_item)) {
        if (!cJSON_IsNumber(b_item)) {
            ESP_LOGE(TAG, "Invalid RGB.blue type (expect number or null): %s", rgb);
            cJSON_Delete(rgb_json);
            return esp_mcp_value_create_bool(false);
        }
        blue = b_item->valueint;
        updated = true;
    }
    cJSON_Delete(rgb_json);

    if (!updated) {
        ESP_LOGE(TAG, "No RGB fields provided (expect at least one of red/green/blue): %s", rgb);
        return esp_mcp_value_create_bool(false);
    }

    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
        ESP_LOGE(TAG, "RGB out of range: red=%d green=%d blue=%d (expect 0-255)", red, green, blue);
        return esp_mcp_value_create_bool(false);
    }

    current_red = red;
    current_green = green;
    current_blue = blue;

    ESP_LOGI(TAG, "RGB set to: red: %d, green: %d, blue: %d", red, green, blue);
    return esp_mcp_value_create_bool(true);
}

static void esp_xiaozhi_chat_app_audio_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return;
    }
    /* Transient failures (mutex timeout, channel already closed, UDP backpressure): log only, no UI error */
    if (error == ESP_ERR_TIMEOUT || error == ESP_FAIL) {
        ESP_LOGW(TAG, "Send audio transient: %s", esp_err_to_name(error));
        return;
    }
    ESP_LOGE(TAG, "Failed to send audio data: %s", esp_err_to_name(error));
    esp_xiaozhi_chat_display_set_status("Error");
    esp_xiaozhi_chat_display_set_notification("Error", 2000);
    esp_xiaozhi_chat_display_set_emotion("sad");
}

static void esp_xiaozhi_chat_app_audio_event(esp_xiaozhi_chat_event_t event, void *event_data, void *ctx)
{
    switch (event) {
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED:
        ESP_LOGI(TAG, "chat start");
        esp_xiaozhi_chat_display_set_status("Speaking...");
        esp_xiaozhi_chat_display_set_emotion("thinking");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STOPPED:
        ESP_LOGI(TAG, "chat stop");
        esp_xiaozhi_chat_display_set_status("Ready");
        esp_xiaozhi_chat_display_set_emotion("neutral");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_TTS_STATE: {
        esp_xiaozhi_chat_tts_state_t *tts = (esp_xiaozhi_chat_tts_state_t *)event_data;
        if (tts) {
            if (tts->state == ESP_XIAOZHI_CHAT_TTS_STATE_START) {
                esp_xiaozhi_chat_display_set_status("Speaking...");
                esp_xiaozhi_chat_display_set_emotion("thinking");
            } else if (tts->state == ESP_XIAOZHI_CHAT_TTS_STATE_STOP) {
                esp_xiaozhi_chat_display_set_status("Ready");
                esp_xiaozhi_chat_display_set_emotion("neutral");
            }
            /* SENTENCE_START: CHAT_TEXT is also emitted, UI updated there */
        }
        break;
    }
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SYSTEM_CMD:
        if (event_data && strcmp((const char *)event_data, "reboot") == 0) {
            ESP_LOGI(TAG, "System command reboot, restarting");
            esp_restart();
        }
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT: {
        esp_xiaozhi_chat_text_data_t *text_data = (esp_xiaozhi_chat_text_data_t *)event_data;
        if (text_data && text_data->text) {
            const char *role_str = (text_data->role == ESP_XIAOZHI_CHAT_TEXT_ROLE_USER) ? "user" : "assistant";
            esp_xiaozhi_chat_display_set_chat_message(role_str, text_data->text);
        }
        break;
    }
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_EMOJI:
        esp_xiaozhi_chat_display_set_emotion((char *)event_data);
        break;
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_ERROR: {
        esp_xiaozhi_chat_error_info_t *info = (esp_xiaozhi_chat_error_info_t *)event_data;
        if (info) {
            ESP_LOGE(TAG, "chat error: %s (source: %s)", esp_err_to_name(info->code), info->source ? info->source : "");
        } else {
            ESP_LOGE(TAG, "chat error: unknown");
        }
        esp_xiaozhi_chat_display_set_status("Error");
        esp_xiaozhi_chat_display_set_notification("Error", 2000);
        esp_xiaozhi_chat_display_set_emotion("sad");
        break;
    }
    default:
        break;
    }
}

static void esp_xiaozhi_chat_app_audio_data(const uint8_t *data, int len, void *ctx)
{
    audio_feeder_feed_data((uint8_t *)data, len);
}

static void esp_xiaozhi_chat_app_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_xiaozhi_chat_app_t *xiaozhi_chat_app = (esp_xiaozhi_chat_app_t *)arg;
    (void)event_base;
    switch (event_id) {
    case ESP_XIAOZHI_CHAT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        esp_xiaozhi_chat_display_set_status("Connected");
        esp_xiaozhi_chat_display_set_notification("Connected", 2000);
        esp_xiaozhi_chat_display_set_emotion("happy");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "disconnected");
        esp_xiaozhi_chat_display_set_status("Disconnected");
        esp_xiaozhi_chat_display_set_notification("Disconnected", 2000);
        esp_xiaozhi_chat_display_set_emotion("sad");
        if (xiaozhi_chat_app->data_evt_group) {
            xEventGroupSetBits(xiaozhi_chat_app->data_evt_group, ESP_XIAOZHI_CHAT_APP_OFFLINE);
        }
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED:
        ESP_LOGI(TAG, "audio channel opened");
        xiaozhi_chat_app->wakeuped = true;
        esp_xiaozhi_chat_display_set_status("Listening...");
        esp_xiaozhi_chat_display_set_emotion("thinking");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED:
        ESP_LOGI(TAG, "audio channel closed");
        xiaozhi_chat_app->wakeuped = false;
        esp_xiaozhi_chat_display_set_status("Ready");
        esp_xiaozhi_chat_display_set_emotion("neutral");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_SERVER_GOODBYE:
        ESP_LOGI(TAG, "server goodbye");
        esp_xiaozhi_chat_display_set_status("Goodbye");
        esp_xiaozhi_chat_display_set_notification("Goodbye", 2000);
        esp_xiaozhi_chat_display_set_emotion("neutral");
        if (xiaozhi_chat_app->data_evt_group) {
            xEventGroupSetBits(xiaozhi_chat_app->data_evt_group, ESP_XIAOZHI_CHAT_APP_OFFLINE);
        }
        break;
    default:
        break;
    }
}

static esp_err_t esp_xiaozhi_chat_app_init(esp_xiaozhi_chat_app_t *xiaozhi_chat_app)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_app != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid app");

    esp_err_t ret = ESP_OK;
    esp_xiaozhi_chat_info_t info = {0};
    esp_mcp_t *mcp = NULL;
    esp_mcp_property_t *property = NULL;
    esp_mcp_tool_t *tool = NULL;
    xiaozhi_chat_app->chat = 0;

    do {
        ret = esp_xiaozhi_chat_get_info(&info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get chat info: %s", esp_err_to_name(ret));
            break;
        }

        ret = esp_mcp_create(&mcp);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create MCP engine: %s", esp_err_to_name(ret));
            break;
        }

        tool = esp_mcp_tool_create("self.get_device_status", "Get device status including audio, screen, battery, and network information", get_device_status_callback);
        esp_mcp_add_tool(mcp, tool);

        tool = esp_mcp_tool_create("self.audio_speaker.set_volume", "Set audio speaker volume (0-100)", set_volume_callback);
        property = esp_mcp_property_create_with_range("volume", 0, 100);
        esp_mcp_tool_add_property(tool, property);
        esp_mcp_add_tool(mcp, tool);

        tool = esp_mcp_tool_create("self.screen.set_brightness", "Set screen brightness (0-100)", set_brightness_callback);
        property = esp_mcp_property_create_with_range("brightness", 0, 100);
        esp_mcp_tool_add_property(tool, property);
        esp_mcp_add_tool(mcp, tool);

        tool = esp_mcp_tool_create("self.screen.set_theme", "Set screen theme (light/dark)", set_theme_callback);
        property = esp_mcp_property_create_with_string("theme", "light");
        esp_mcp_tool_add_property(tool, property);
        esp_mcp_add_tool(mcp, tool);

        tool = esp_mcp_tool_create("self.screen.set_hsv", "Set screen HSV, first value is hue which range is (0, 360), \
                                    second value is saturation which range is (0, 100), \
                                    third value is value which range is (0, 100)", set_hsv_callback);
        property = esp_mcp_property_create_with_array("HSV", "[120, 80, 50]");
        esp_mcp_tool_add_property(tool, property);
        esp_mcp_add_tool(mcp, tool);

        tool = esp_mcp_tool_create("self.screen.set_rgb", "Set screen RGB, red value range is (0, 255), \
                                    green value range is (0, 255), \
                                    blue value range is (0, 255)", set_rgb_callback);
        property = esp_mcp_property_create_with_object("RGB", "{\"red\": 0, \"green\": 120, \"blue\": 240}");
        esp_mcp_tool_add_property(tool, property);
        esp_mcp_add_tool(mcp, tool);

        tool = esp_mcp_tool_create("self.camera.take_photo",
                                   "Take a photo with an application-managed camera and send it to the vision explain service.",
                                   camera_take_photo_callback);
        property = esp_mcp_property_create_with_string("question", "What is in this image?");
        esp_mcp_tool_add_property(tool, property);
        property = esp_mcp_property_create_with_string("url", "http://api.xiaozhi.me/vision/explain");
        esp_mcp_tool_add_property(tool, property);
        property = esp_mcp_property_create_with_string("token", "test-token");
        esp_mcp_tool_add_property(tool, property);
        esp_mcp_add_tool(mcp, tool);

        esp_xiaozhi_chat_config_t chat_config = {0};
        chat_config.audio_type = ESP_XIAOZHI_CHAT_AUDIO_TYPE_OPUS;
        chat_config.audio_callback = esp_xiaozhi_chat_app_audio_data;
        chat_config.event_callback = esp_xiaozhi_chat_app_audio_event;
        chat_config.mcp_engine = mcp;
        chat_config.owns_mcp_engine = true;
        chat_config.has_mqtt_config = info.has_mqtt_config;
        chat_config.has_websocket_config = info.has_websocket_config;
#if defined(CONFIG_XIAOZHI_CHAT_APP_TRANSPORT_WEBSOCKET)
        chat_config.has_mqtt_config = false;
#endif
        ret = esp_xiaozhi_chat_init(&chat_config, &xiaozhi_chat_app->chat);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init chat: %s", esp_err_to_name(ret));
            break;
        }
        mcp = NULL;

        ret = esp_xiaozhi_chat_start(xiaozhi_chat_app->chat);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start chat: %s", esp_err_to_name(ret));
            break;
        }
    } while (0);

    if (ret != ESP_OK && xiaozhi_chat_app->chat != 0) {
        esp_xiaozhi_chat_deinit(xiaozhi_chat_app->chat);
        xiaozhi_chat_app->chat = 0;
    }
    if (mcp != NULL) {
        esp_mcp_destroy(mcp);
    }
    esp_xiaozhi_chat_free_info(&info);

    return ret;
}

static void esp_xiaozhi_chat_app_audio_recorder(void *event, void *ctx)
{
    esp_gmf_afe_evt_t *afe_evt = (esp_gmf_afe_evt_t *)event;
    esp_xiaozhi_chat_app_t *xiaozhi_chat_app = (esp_xiaozhi_chat_app_t *)ctx;

    switch (afe_evt->type) {
    case ESP_GMF_AFE_EVT_WAKEUP_START:
        ESP_LOGI(TAG, "wakeup start");
        if (xiaozhi_chat_app->chat != 0 && xiaozhi_chat_app->data_evt_group) {
            xEventGroupSetBits(xiaozhi_chat_app->data_evt_group, ESP_XIAOZHI_CHAT_APP_ONLINE);
        }
        break;
    case ESP_GMF_AFE_EVT_WAKEUP_END:
        ESP_LOGI(TAG, "wakeup end");
        break;
    case ESP_GMF_AFE_EVT_VAD_START:
        ESP_LOGI(TAG, "vad start");
        break;
    case ESP_GMF_AFE_EVT_VAD_END:
        ESP_LOGI(TAG, "vad end");
        break;
    case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
        ESP_LOGI(TAG, "vcmd detect timeout");
        break;
    default:
        break;
    }
}

static void esp_xiaozhi_chat_app_audio_channel(void *pv)
{
    esp_xiaozhi_chat_app_t *xiaozhi_chat_app = (esp_xiaozhi_chat_app_t *)pv;
    const EventBits_t wait_bits = ESP_XIAOZHI_CHAT_APP_ONLINE | ESP_XIAOZHI_CHAT_APP_OFFLINE;
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(xiaozhi_chat_app->data_evt_group, wait_bits, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & ESP_XIAOZHI_CHAT_APP_ONLINE) {
            if (xiaozhi_chat_app->chat == 0) {
                ESP_LOGW(TAG, "Ignore online event before chat is initialized");
                continue;
            }
            bool audio_opened = false;
            esp_err_t ret = esp_xiaozhi_chat_open_audio_channel(xiaozhi_chat_app->chat, &xiaozhi_chat_app->audio, NULL, 0);
            esp_xiaozhi_chat_app_audio_error(ret);
            audio_opened = (ret == ESP_OK);
            if (ret == ESP_OK) {
                ret = esp_xiaozhi_chat_send_wake_word(xiaozhi_chat_app->chat, "你好小智");
                esp_xiaozhi_chat_app_audio_error(ret);
            }
            if (ret == ESP_OK) {
                ret = esp_xiaozhi_chat_send_start_listening(xiaozhi_chat_app->chat, 0);
                esp_xiaozhi_chat_app_audio_error(ret);
            }
            if (ret != ESP_OK && audio_opened) {
                esp_xiaozhi_chat_app_audio_error(esp_xiaozhi_chat_close_audio_channel(xiaozhi_chat_app->chat));
            }
        }

        if (bits & ESP_XIAOZHI_CHAT_APP_OFFLINE) {
            if (xiaozhi_chat_app->chat == 0) {
                ESP_LOGW(TAG, "Ignore offline event before chat is initialized");
                continue;
            }
            esp_xiaozhi_chat_app_audio_error(esp_xiaozhi_chat_close_audio_channel(xiaozhi_chat_app->chat));
        }

        xEventGroupClearBits(xiaozhi_chat_app->data_evt_group, wait_bits);
    }
}

static void esp_xiaozhi_chat_app_audio_read(void *pv)
{
    int ret = 0;
    int consecutive_errors = 0;
    esp_xiaozhi_chat_app_t *xiaozhi_chat_app = (esp_xiaozhi_chat_app_t *)pv;
    uint8_t *data = esp_gmf_oal_calloc(1, ESP_XIAOZHI_CHAT_REC_READ_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio data");
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        ret = audio_recorder_read_data(data, ESP_XIAOZHI_CHAT_REC_READ_SIZE);
        if (ret > 0) {
            consecutive_errors = 0;
        }

        if (ret > 0 && xiaozhi_chat_app->wakeuped && xiaozhi_chat_app->chat != 0) {
            esp_xiaozhi_chat_app_audio_error(esp_xiaozhi_chat_send_audio_data(xiaozhi_chat_app->chat, (char *)data, ret));
            continue;
        }

        if (ret <= 0) {
            consecutive_errors++;
            if (consecutive_errors == 1 ||
                    (consecutive_errors % ESP_XIAOZHI_CHAT_APP_AUDIO_READ_ERROR_LOG_INTERVAL) == 0) {
                ESP_LOGW(TAG, "audio_recorder_read_data failed: %d (consecutive=%d)", ret, consecutive_errors);
            }
            if (consecutive_errors >= ESP_XIAOZHI_CHAT_APP_AUDIO_READ_MAX_ERRORS) {
                ESP_LOGE(TAG, "audio recorder read failed too many times, stopping audio read thread");
                xiaozhi_chat_app->wakeuped = false;
                if (xiaozhi_chat_app->data_evt_group != NULL) {
                    xEventGroupSetBits(xiaozhi_chat_app->data_evt_group, ESP_XIAOZHI_CHAT_APP_OFFLINE);
                }
                esp_xiaozhi_chat_display_set_status("Recorder Error");
                esp_xiaozhi_chat_display_set_notification("Recorder Error", 2000);
                esp_xiaozhi_chat_display_set_emotion("sad");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(ESP_XIAOZHI_CHAT_APP_AUDIO_READ_ERROR_BACKOFF_MS));
        }
    }

    esp_gmf_oal_free(data);
    xiaozhi_chat_app->read_thread = NULL;
    vTaskDelete(NULL);
}

static esp_err_t esp_xiaozhi_chat_app_audio(esp_xiaozhi_chat_app_t *xiaozhi_chat_app, esp_board_manager_adapter_info_t bsp_info)
{
    ESP_RETURN_ON_FALSE(xiaozhi_chat_app != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid app");
    ESP_RETURN_ON_FALSE(bsp_info.play_dev != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid playback device");
    ESP_RETURN_ON_FALSE(bsp_info.rec_dev != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid record device");

    av_processor_afe_config_t afe_config = DEFAULT_AV_PROCESSOR_AFE_CONFIG();
    afe_config.ai_mode_wakeup = true;

    audio_manager_config_t config = DEFAULT_AUDIO_MANAGER_CONFIG();
    esp_err_t ret = ESP_OK;
    bool audio_manager_inited = false;
    bool play_dev_opened = false;
    bool rec_dev_opened = false;
    bool playback_opened = false;
    bool recorder_opened = false;
    bool feeder_opened = false;
    bool read_thread_created = false;
    bool audio_channel_created = false;

    config.play_dev = bsp_info.play_dev;
    config.rec_dev = bsp_info.rec_dev;
    strcpy(config.mic_layout, bsp_info.mic_layout);
    config.board_sample_rate = bsp_info.sample_rate;
    config.board_bits = bsp_info.sample_bits;
    config.board_channels = bsp_info.channels;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = config.board_sample_rate,
        .channel = config.board_channels,
        .bits_per_sample = config.board_bits,
    };
    audio_playback_config_t playback_config = DEFAULT_AUDIO_PLAYBACK_CONFIG();
    av_processor_encoder_config_t recorder_cfg = {0};
    audio_recorder_config_t recorder_config = DEFAULT_AUDIO_RECORDER_CONFIG();
    av_processor_decoder_config_t feeder_cfg = {0};
    audio_feeder_config_t feeder_config = DEFAULT_AUDIO_FEEDER_CONFIG();

    do {
        ret = audio_manager_init(&config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init audio manager: %s", esp_err_to_name(ret));
            break;
        }
        audio_manager_inited = true;

        if (esp_codec_dev_set_out_vol(config.play_dev, 60) != ESP_CODEC_DEV_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to set output volume");
            break;
        }
        if (esp_codec_dev_set_in_gain(config.rec_dev, 26.0) != ESP_CODEC_DEV_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to set input gain");
            break;
        }

        if (esp_codec_dev_open(config.play_dev, &fs) != ESP_CODEC_DEV_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to open playback codec device");
            break;
        }
        play_dev_opened = true;
        if (esp_codec_dev_open(config.rec_dev, &fs) != ESP_CODEC_DEV_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to open record codec device");
            break;
        }
        rec_dev_opened = true;

        ret = audio_playback_open(&playback_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open audio playback: %s", esp_err_to_name(ret));
            break;
        }
        playback_opened = true;

        recorder_cfg.format = AV_PROCESSOR_FORMAT_ID_OPUS;
        recorder_cfg.params.opus.audio_info.sample_rate = xiaozhi_chat_app->audio.sample_rate;
        recorder_cfg.params.opus.audio_info.sample_bits = 16;
        recorder_cfg.params.opus.audio_info.channels = xiaozhi_chat_app->audio.channels;
        recorder_cfg.params.opus.audio_info.frame_duration = xiaozhi_chat_app->audio.frame_duration;
        recorder_cfg.params.opus.enable_vbr = false;
        recorder_cfg.params.opus.bitrate = 24000;

        recorder_config.encoder_cfg = recorder_cfg;
        recorder_config.afe_config = afe_config;
        recorder_config.recorder_event_cb = esp_xiaozhi_chat_app_audio_recorder;
        recorder_config.recorder_ctx = (void *)xiaozhi_chat_app;
        recorder_config.recorder_task_config.task_core = 1;
        ret = audio_recorder_open(&recorder_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open audio recorder: %s", esp_err_to_name(ret));
            break;
        }
        recorder_opened = true;

        feeder_cfg.format = AV_PROCESSOR_FORMAT_ID_OPUS;
        feeder_cfg.params.opus.audio_info.sample_rate = xiaozhi_chat_app->audio.sample_rate;
        feeder_cfg.params.opus.audio_info.sample_bits = 16;
        feeder_cfg.params.opus.audio_info.channels = xiaozhi_chat_app->audio.channels;
        feeder_cfg.params.opus.audio_info.frame_duration = xiaozhi_chat_app->audio.frame_duration;

        feeder_config.feeder_task_config.task_core = 1;
        feeder_config.decoder_cfg = feeder_cfg;
        ret = audio_feeder_open(&feeder_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open audio feeder: %s", esp_err_to_name(ret));
            break;
        }
        feeder_opened = true;

        ret = audio_feeder_run();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to run audio feeder: %s", esp_err_to_name(ret));
            break;
        }

        if (esp_gmf_oal_thread_create(&xiaozhi_chat_app->read_thread, "audio_read",
                                      esp_xiaozhi_chat_app_audio_read, (void *)xiaozhi_chat_app,
                                      3096, 12, true, 1) != ESP_GMF_ERR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to create audio read thread");
            break;
        }
        read_thread_created = true;

        if (esp_gmf_oal_thread_create(&xiaozhi_chat_app->audio_channel, "audio_channel",
                                      esp_xiaozhi_chat_app_audio_channel, (void *)xiaozhi_chat_app,
                                      3096, 12, true, 1) != ESP_GMF_ERR_OK) {
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to create audio channel thread");
            break;
        }
        audio_channel_created = true;
    } while (0);

    if (ret != ESP_OK) {
        if (audio_channel_created) {
            esp_gmf_oal_thread_delete(xiaozhi_chat_app->audio_channel);
            xiaozhi_chat_app->audio_channel = NULL;
        }
        if (read_thread_created) {
            esp_gmf_oal_thread_delete(xiaozhi_chat_app->read_thread);
            xiaozhi_chat_app->read_thread = NULL;
        }
        if (feeder_opened) {
            audio_feeder_close();
        }
        if (recorder_opened) {
            audio_recorder_close();
        }
        if (playback_opened) {
            audio_playback_close();
        }
        if (rec_dev_opened) {
            esp_codec_dev_close(config.rec_dev);
        }
        if (play_dev_opened) {
            esp_codec_dev_close(config.play_dev);
        }
        if (audio_manager_inited) {
            audio_manager_deinit();
        }
    }

    return ret;
}

esp_err_t esp_xiaozhi_chat_app(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_xiaozhi_chat_app_t *xiaozhi_chat_app = (esp_xiaozhi_chat_app_t *)calloc(1, sizeof(esp_xiaozhi_chat_app_t));
    ESP_RETURN_ON_FALSE(xiaozhi_chat_app, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for Xiaozhi chat app");
    esp_err_t ret = ESP_OK;
    bool evt_group_created = false;
    bool event_registered = false;
    bool board_inited = false;
    esp_board_manager_adapter_info_t bsp_info = {0};

    do {
        xiaozhi_chat_app->data_evt_group = xEventGroupCreate();
        if (xiaozhi_chat_app->data_evt_group == NULL) {
            ret = ESP_ERR_NO_MEM;
            ESP_LOGE(TAG, "Failed to create event group");
            break;
        }
        evt_group_created = true;

        ret = esp_event_handler_register(ESP_XIAOZHI_CHAT_EVENTS, ESP_EVENT_ANY_ID, esp_xiaozhi_chat_app_event, xiaozhi_chat_app);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register chat app event handler: %s", esp_err_to_name(ret));
            break;
        }
        event_registered = true;

        xiaozhi_chat_app->audio = (esp_xiaozhi_chat_audio_t) {
            .format = "opus",
            .sample_rate = 16000,
            .channels = 1,
            .frame_duration = 60,
        };

        esp_board_manager_adapter_config_t bsp_config = ESP_BOARD_MANAGER_ADAPTER_CONFIG_DEFAULT();
        bsp_config.enable_lcd = true;
        bsp_config.enable_lcd_backlight = true;
        bsp_config.enable_lvgl = false;
        ret = esp_board_manager_adapter_init(&bsp_config, &bsp_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init board manager adapter: %s", esp_err_to_name(ret));
            break;
        }
        board_inited = true;

        ret = esp_xiaozhi_chat_display_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init display: %s", esp_err_to_name(ret));
            break;
        }

        ret = esp_xiaozhi_chat_app_init(xiaozhi_chat_app);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init chat app: %s", esp_err_to_name(ret));
            break;
        }

        ret = esp_xiaozhi_chat_app_audio(xiaozhi_chat_app, bsp_info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init chat app audio: %s", esp_err_to_name(ret));
            break;
        }
    } while (0);

    if (ret == ESP_OK) {
        return ESP_OK;
    }

    if (xiaozhi_chat_app->chat != 0) {
        esp_xiaozhi_chat_deinit(xiaozhi_chat_app->chat);
        xiaozhi_chat_app->chat = 0;
    }
    if (event_registered) {
        esp_event_handler_unregister(ESP_XIAOZHI_CHAT_EVENTS, ESP_EVENT_ANY_ID, esp_xiaozhi_chat_app_event);
    }
    if (board_inited) {
        esp_board_manager_adapter_deinit();
    }
    if (evt_group_created) {
        vEventGroupDelete(xiaozhi_chat_app->data_evt_group);
        xiaozhi_chat_app->data_evt_group = NULL;
    }
    free(xiaozhi_chat_app);
    return ret;
}
