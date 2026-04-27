/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_check.h"
#include "cJSON.h"

#include "protocol_examples_common.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "mcp_server.h"

static const char *TAG = "mcp_server";

// Device state simulation
static int current_volume = 50;
static int current_brightness = 80;
static char current_theme[32] = "light";
static int current_hue = 0;          // 0..360
static int current_saturation = 0;   // 0..100
static int current_value = 0;        // 0..100
static int current_red = 0;          // 0..255
static int current_green = 0;        // 0..255
static int current_blue = 0;         // 0..255
static esp_mcp_t *g_mcp = NULL;
static esp_mcp_resource_t *g_status_resource = NULL;

// MCP Resource callback (resources/read)
static esp_err_t read_device_info_resource(const char *uri,
                                           char **out_mime,
                                           char **out_text,
                                           char **out_blob,
                                           void *ctx)
{
    (void)uri;
    (void)ctx;
    ESP_RETURN_ON_FALSE(out_mime && out_text && out_blob, ESP_ERR_INVALID_ARG, TAG, "Invalid resource output");

    *out_mime = strdup("application/json");
    *out_blob = NULL;
    if (!*out_mime) {
        return ESP_ERR_NO_MEM;
    }

    char payload[256] = {0};
    snprintf(payload, sizeof(payload),
             "{\"volume\":%d,\"brightness\":%d,\"theme\":\"%s\",\"rgb\":{\"red\":%d,\"green\":%d,\"blue\":%d}}",
             current_volume, current_brightness, current_theme, current_red, current_green, current_blue);
    *out_text = strdup(payload);
    if (!*out_text) {
        free(*out_mime);
        *out_mime = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t read_sensor_template_resource(const char *uri,
                                               char **out_mime,
                                               char **out_text,
                                               char **out_blob,
                                               void *ctx)
{
    (void)ctx;
    ESP_RETURN_ON_FALSE(uri && out_mime && out_text && out_blob, ESP_ERR_INVALID_ARG, TAG, "Invalid resource output");
    *out_mime = strdup("application/json");
    *out_blob = NULL;
    if (!*out_mime) {
        return ESP_ERR_NO_MEM;
    }
    char payload[192] = {0};
    snprintf(payload, sizeof(payload), "{\"uri\":\"%s\",\"value\":42,\"unit\":\"C\"}", uri);
    *out_text = strdup(payload);
    if (!*out_text) {
        free(*out_mime);
        *out_mime = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

// MCP Prompt callback (prompts/get)
static esp_err_t render_status_prompt(const char *arguments_json,
                                      char **out_description,
                                      char **out_messages_json,
                                      void *ctx)
{
    (void)ctx;
    ESP_RETURN_ON_FALSE(out_description && out_messages_json, ESP_ERR_INVALID_ARG, TAG, "Invalid prompt output");

    const char *target = "all";
    char target_buf[64] = "all";
    if (arguments_json) {
        cJSON *args = cJSON_Parse(arguments_json);
        if (args) {
            cJSON *device = cJSON_GetObjectItem(args, "device");
            if (device && cJSON_IsString(device) && device->valuestring && device->valuestring[0]) {
                snprintf(target_buf, sizeof(target_buf), "%s", device->valuestring);
                target = target_buf;
            }
            cJSON_Delete(args);
        }
    }

    *out_description = strdup("Generate a concise device status summary.");
    if (!*out_description) {
        return ESP_ERR_NO_MEM;
    }

    char messages_buf[320] = {0};
    snprintf(messages_buf, sizeof(messages_buf),
             "[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"Summarize %s device status.\"}}]",
             target);
    *out_messages_json = strdup(messages_buf);
    if (!*out_messages_json) {
        free(*out_description);
        *out_description = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

// MCP Completion callback (completion/complete)
static esp_err_t completion_provider_cb(const char *ref_type,
                                        const char *name_or_uri,
                                        const char *argument_name,
                                        const char *argument_value,
                                        const char *context_args_json,
                                        cJSON **out_result_obj,
                                        void *ctx)
{
    (void)ref_type;
    (void)name_or_uri;
    (void)argument_value;
    (void)context_args_json;
    (void)ctx;
    ESP_RETURN_ON_FALSE(argument_name && out_result_obj, ESP_ERR_INVALID_ARG, TAG, "Invalid completion args");

    cJSON *result = cJSON_CreateObject();
    cJSON *completion = cJSON_CreateObject();
    cJSON *values = cJSON_CreateArray();
    if (!result || !completion || !values) {
        cJSON_Delete(result);
        cJSON_Delete(completion);
        cJSON_Delete(values);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(result, "completion", completion);
    cJSON_AddItemToObject(completion, "values", values);

    if (strcmp(argument_name, "theme") == 0) {
        cJSON_AddItemToArray(values, cJSON_CreateString("light"));
        cJSON_AddItemToArray(values, cJSON_CreateString("dark"));
    } else if (strcmp(argument_name, "device") == 0) {
        cJSON_AddItemToArray(values, cJSON_CreateString("screen"));
        cJSON_AddItemToArray(values, cJSON_CreateString("audio"));
        cJSON_AddItemToArray(values, cJSON_CreateString("all"));
    } else {
        cJSON_AddItemToArray(values, cJSON_CreateString("default"));
    }
    cJSON_AddBoolToObject(completion, "hasMore", false);

    *out_result_obj = result;
    return ESP_OK;
}

// MCP Tool callbacks
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

    // Default to current state; update only provided entries.
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

    // Support partial updates: {"red": 10}, {"green": 20}, {"blue": 30}, etc.
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

// Trigger one notifications/resources/updated event for E2E SSE tests.
static esp_mcp_value_t touch_resource_callback(const esp_mcp_property_list_t *properties)
{
    (void)properties;
    if (!g_mcp || !g_status_resource) {
        ESP_LOGE(TAG, "Resource touch unavailable");
        return esp_mcp_value_create_bool(false);
    }
    if (esp_mcp_remove_resource(g_mcp, g_status_resource) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove resource");
        return esp_mcp_value_create_bool(false);
    }
    if (esp_mcp_add_resource(g_mcp, g_status_resource) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to re-add resource");
        return esp_mcp_value_create_bool(false);
    }
    return esp_mcp_value_create_bool(true);
}

static esp_mcp_value_t request_roots_list_callback(const esp_mcp_property_list_t *properties)
{
    (void)properties;
    if (!g_mcp) {
        return esp_mcp_value_create_bool(false);
    }
    const char *session_id = esp_mcp_get_request_session_id(g_mcp);
    if (!session_id) {
        ESP_LOGE(TAG, "No request session for roots/list");
        return esp_mcp_value_create_bool(false);
    }
    char req_id[40] = {0};
    esp_err_t ret = esp_mcp_request_roots_list(g_mcp, session_id, NULL, NULL, 30000, req_id, sizeof(req_id));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "roots/list request emit failed: %s", esp_err_to_name(ret));
        return esp_mcp_value_create_bool(false);
    }
    return esp_mcp_value_create_string(req_id);
}

static esp_mcp_value_t request_sampling_create_callback(const esp_mcp_property_list_t *properties)
{
    (void)properties;
    if (!g_mcp) {
        return esp_mcp_value_create_bool(false);
    }
    const char *session_id = esp_mcp_get_request_session_id(g_mcp);
    if (!session_id) {
        ESP_LOGE(TAG, "No request session for sampling/create");
        return esp_mcp_value_create_bool(false);
    }
    const char *params_json =
        "{\"messages\":[{\"role\":\"user\",\"content\":{\"type\":\"text\",\"text\":\"echo: hello\"}}],"
        "\"maxTokens\":32}";
    char req_id[40] = {0};
    esp_err_t ret = esp_mcp_request_sampling_create(g_mcp, session_id, params_json, NULL, NULL, 30000, req_id, sizeof(req_id));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sampling/create request emit failed: %s", esp_err_to_name(ret));
        return esp_mcp_value_create_bool(false);
    }
    return esp_mcp_value_create_string(req_id);
}

static esp_mcp_value_t request_elicitation_callback(const esp_mcp_property_list_t *properties)
{
    (void)properties;
    if (!g_mcp) {
        return esp_mcp_value_create_bool(false);
    }
    const char *session_id = esp_mcp_get_request_session_id(g_mcp);
    if (!session_id) {
        ESP_LOGE(TAG, "No request session for elicitation/request");
        return esp_mcp_value_create_bool(false);
    }
    const char *params_json =
        "{\"prompt\":\"Please confirm operation\","
        "\"schema\":{\"type\":\"object\",\"properties\":{\"confirm\":{\"type\":\"boolean\"}},\"required\":[\"confirm\"]}}";
    char req_id[40] = {0};
    esp_err_t ret = esp_mcp_request_elicitation(g_mcp, session_id, params_json, NULL, NULL, 30000, req_id, sizeof(req_id));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "elicitation/request emit failed: %s", esp_err_to_name(ret));
        return esp_mcp_value_create_bool(false);
    }
    return esp_mcp_value_create_string(req_id);
}

static void obtain_time(void)
{
    ESP_LOGI(TAG, "Initializing and starting SNTP");
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                                                                      ESP_SNTP_SERVER_LIST(CONFIG_SNTP_TIME_SERVER, "pool.ntp.org"));
#else
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
#endif
    esp_netif_sntp_init(&config);

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }

    char strftime_buf[64];
    setenv("TZ", "CST-8", 1);
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    esp_netif_sntp_deinit();
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize networking
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Connect to WiFi
    ESP_ERROR_CHECK(example_connect());

    obtain_time();

    // Initialize MCP engine
    esp_mcp_t *mcp = NULL;
    esp_mcp_property_t *property = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));
    g_mcp = mcp;

    // Add get_device_status tool
    esp_mcp_tool_t *tool = esp_mcp_tool_create("self.get_device_status", "Get device status including audio, screen, battery, and network information", get_device_status_callback);
    (void)esp_mcp_tool_set_task_support(tool, "optional");
    esp_mcp_add_tool(mcp, tool);

    // Add testing helper tool: trigger resources/updated SSE notification by remove+add
    tool = esp_mcp_tool_create("self.resource.touch_status", "Trigger notifications/resources/updated for SSE testing", touch_resource_callback);
    esp_mcp_add_tool(mcp, tool);

    tool = esp_mcp_tool_create("self.server.roots_list", "Emit server-initiated roots/list to current session", request_roots_list_callback);
    esp_mcp_add_tool(mcp, tool);

    tool = esp_mcp_tool_create("self.server.sampling_create", "Emit server-initiated sampling/create to current session", request_sampling_create_callback);
    esp_mcp_add_tool(mcp, tool);

    tool = esp_mcp_tool_create("self.server.elicitation_request", "Emit server-initiated elicitation/request to current session", request_elicitation_callback);
    esp_mcp_add_tool(mcp, tool);

    // Add set_volume tool
    tool = esp_mcp_tool_create("self.audio_speaker.set_volume", "Set audio speaker volume (0-100)", set_volume_callback);
    property = esp_mcp_property_create_with_range("volume", 0, 100);
    esp_mcp_tool_add_property(tool, property);
    esp_mcp_add_tool(mcp, tool);

    // Add set_brightness tool
    tool = esp_mcp_tool_create("self.screen.set_brightness", "Set screen brightness (0-100)", set_brightness_callback);
    property = esp_mcp_property_create_with_range("brightness", 0, 100);
    esp_mcp_tool_add_property(tool, property);
    esp_mcp_add_tool(mcp, tool);

    // Add set_theme tool
    tool = esp_mcp_tool_create("self.screen.set_theme", "Set screen theme (light/dark)", set_theme_callback);
    (void)esp_mcp_tool_set_task_support(tool, "optional");
    property = esp_mcp_property_create_with_string("theme", "light");
    esp_mcp_tool_add_property(tool, property);
    esp_mcp_add_tool(mcp, tool);

    // Add set_hsv tool
    // HSV can be partially provided, e.g. [120], [120, 80], [null, 80], [null, null, 50]
    tool = esp_mcp_tool_create("self.screen.set_hsv", "Set screen HSV, first value is hue which range is (0, 360), \
                                second value is saturation which range is (0, 100), \
                                third value is value which range is (0, 100)", set_hsv_callback);
    property = esp_mcp_property_create_with_array("HSV", "[120, 80, 50]");
    esp_mcp_tool_add_property(tool, property);
    esp_mcp_add_tool(mcp, tool);

    // Add set_rgb tool
    tool = esp_mcp_tool_create("self.screen.set_rgb", "Set screen RGB, red value range is (0, 255), \
                                green value range is (0, 255), \
                                blue value range is (0, 255)", set_rgb_callback);
    property = esp_mcp_property_create_with_object("RGB", "{\"red\": 0, \"green\": 120, \"blue\": 240}");
    esp_mcp_tool_add_property(tool, property);
    ESP_ERROR_CHECK(esp_mcp_tool_set_annotations_json(
                        tool,
                        "{\"audience\":[\"assistant\"],\"priority\":0.6,\"lastModified\":\"2026-02-12T00:00:00Z\"}"));
    esp_mcp_add_tool(mcp, tool);

    // Register MCP resources example: resources/list + resources/read
    esp_mcp_resource_t *resource = esp_mcp_resource_create(
                                       "device://status",
                                       "device.status",
                                       "Device Status",
                                       "Current simulated device status",
                                       "application/json",
                                       read_device_info_resource,
                                       NULL);
    ESP_ERROR_CHECK(resource ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_mcp_resource_set_annotations(
                        resource,
                        "[\"assistant\",\"user\"]",
                        0.8,
                        "2026-02-12T00:00:00Z"));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, resource));
    g_status_resource = resource;

    // Register MCP resource template example: resources/templates/list
    esp_mcp_resource_t *resource_template = esp_mcp_resource_create(
                                                "device://sensors/{id}",
                                                "device.sensor.template",
                                                "Sensor Template",
                                                "Template URI for per-sensor data",
                                                "application/json",
                                                read_sensor_template_resource,
                                                NULL);
    ESP_ERROR_CHECK(resource_template ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_mcp_resource_set_annotations(
                        resource_template,
                        "[\"assistant\"]",
                        0.5,
                        "2026-02-12T00:00:00Z"));
    ESP_ERROR_CHECK(esp_mcp_add_resource(mcp, resource_template));

    // Register MCP prompt example: prompts/list + prompts/get
    esp_mcp_prompt_t *prompt = esp_mcp_prompt_create(
                                   "status.summary",
                                   "Status Summary",
                                   "Generate device status summary prompt",
                                   NULL,
                                   render_status_prompt,
                                   NULL);
    ESP_ERROR_CHECK(prompt ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_mcp_prompt_set_annotations(
                        prompt,
                        "[\"assistant\",\"user\"]",
                        0.7,
                        "2026-02-12T00:00:00Z"));
    ESP_ERROR_CHECK(esp_mcp_add_prompt(mcp, prompt));

    // Register MCP completion provider example: completion/complete
    esp_mcp_completion_provider_t *completion_provider = esp_mcp_completion_provider_create(completion_provider_cb, NULL);
    ESP_ERROR_CHECK(completion_provider ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_mcp_set_completion_provider(mcp, completion_provider));

    esp_mcp_mgr_handle_t mcp_mgr_handle = 0;

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.stack_size = 8192;  // Increase stack for SSE support
    esp_mcp_mgr_config_t mcp_mgr_config = {
        .transport = esp_mcp_transport_http_server,
        .config = &http_config,
        .instance = mcp,
    };
    ESP_ERROR_CHECK(esp_mcp_mgr_init(mcp_mgr_config, &mcp_mgr_handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(mcp_mgr_handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(mcp_mgr_handle, "mcp_server", NULL));
}
