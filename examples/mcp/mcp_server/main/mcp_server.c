/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_check.h"
#include "cJSON.h"

#include "protocol_examples_common.h"
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

    // Initialize MCP engine
    esp_mcp_t *mcp = NULL;
    esp_mcp_property_t *property = NULL;
    ESP_ERROR_CHECK(esp_mcp_create(&mcp));

    // Add get_device_status tool
    esp_mcp_tool_t *tool = esp_mcp_tool_create("self.get_device_status", "Get device status including audio, screen, battery, and network information", get_device_status_callback);
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
    esp_mcp_add_tool(mcp, tool);

    esp_mcp_mgr_handle_t mcp_mgr_handle = 0;

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    esp_mcp_mgr_config_t mcp_mgr_config = {
        .transport = esp_mcp_transport_http,
        .config = &http_config,
        .instance = mcp,
    };
    ESP_ERROR_CHECK(esp_mcp_mgr_init(mcp_mgr_config, &mcp_mgr_handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_start(mcp_mgr_handle));
    ESP_ERROR_CHECK(esp_mcp_mgr_register_endpoint(mcp_mgr_handle, "mcp_server", NULL));
}
