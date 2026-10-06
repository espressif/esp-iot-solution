/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_esl.h"
#include "esp_eslp_tag.h"

static const char *TAG = "ble_esl_tag";

/* Mesh Device Property ID: Present Ambient Temperature (short Sensor Information). */
#define EXAMPLE_SENSOR_TYPE_TEMPERATURE     0x004F

static const char *eslp_state_str(esp_ble_eslp_state_t state)
{
    switch (state) {
    case BLE_ESLP_STATE_UNASSOCIATED: return "UNASSOCIATED";
    case BLE_ESLP_STATE_CONFIGURING:  return "CONFIGURING";
    case BLE_ESLP_STATE_UPDATING:     return "UPDATING";
    case BLE_ESLP_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
    case BLE_ESLP_STATE_UNSYNCHRONIZED: return "UNSYNCHRONIZED";
    default:                          return "UNKNOWN";
    }
}

static void app_eslp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;

    if (base != BLE_ESLP_EVENTS) {
        return;
    }

    switch (id) {
    case BLE_ESLP_EVENT_STATE_CHANGED: {
        const esp_ble_eslp_state_changed_t *ev = event_data;
        ESP_LOGI(TAG, "State: %s -> %s",
                 eslp_state_str(ev->prev_state), eslp_state_str(ev->new_state));
        break;
    }
    case BLE_ESLP_EVENT_ADDRESS_WRITTEN: {
        const esp_ble_esl_address_t *addr = event_data;
        ESP_LOGI(TAG, "Address written: esl_id=0x%02x group_id=%u",
                 addr->esl_id, (unsigned)BLE_ESL_ADDR_GROUP_ID(*addr));
        break;
    }
    case BLE_ESLP_EVENT_AP_SYNC_KEY_WRITTEN:
        ESP_LOGI(TAG, "AP Sync Key Material written");
        break;
    case BLE_ESLP_EVENT_RESP_KEY_WRITTEN:
        ESP_LOGI(TAG, "Response Key Material written");
        break;
    case BLE_ESLP_EVENT_ABS_TIME_WRITTEN: {
        const esp_ble_esl_abs_time_t *abs_time = event_data;
        ESP_LOGI(TAG, "Absolute Time written: %lu", (unsigned long)(*abs_time));
        break;
    }
    case BLE_ESLP_EVENT_ECP_COMMAND: {
        const esp_ble_eslp_ecp_command_t *cmd = event_data;
        ESP_LOGI(TAG, "ECP command: opcode=0x%02x params_len=%u",
                 cmd->opcode, cmd->params_len);
        break;
    }
    case BLE_ESLP_EVENT_ASSOCIATED:
        ESP_LOGI(TAG, "Associated");
        break;
    case BLE_ESLP_EVENT_UNASSOCIATED:
        ESP_LOGI(TAG, "Unassociated");
        break;
    case BLE_ESLP_EVENT_SYNCHRONIZED:
        ESP_LOGI(TAG, "Synchronized");
        break;
    case BLE_ESLP_EVENT_SYNC_LOST:
        ESP_LOGI(TAG, "Sync lost");
        break;
    case BLE_ESLP_EVENT_PAWR_COMMAND:
        ESP_LOGI(TAG, "PAwR command received");
        break;
    case BLE_ESLP_EVENT_IMAGE_WRITTEN: {
        const esp_ble_eslp_image_written_t *ev = event_data;
        ESP_LOGI(TAG, "Image written: index=%u size=%lu status=%s",
                 ev->image_index, (unsigned long)ev->size, esp_err_to_name(ev->status));
        break;
    }
    case BLE_ESLP_EVENT_DISPLAY_IMAGE: {
        const esp_ble_eslp_display_image_t *ev = event_data;
        ESP_LOGI(TAG, "Display image: display=%u image=%u",
                 ev->display_index, ev->image_index);
#if defined(CONFIG_BLE_ESL_OTS_SUPPORT)
        const uint8_t *img = NULL;
        uint32_t img_len = 0;
        if (esp_ble_eslp_get_image(ev->image_index, &img, &img_len) == ESP_OK) {
            ESP_LOGI(TAG, "Image buffer ready: %lu bytes (demo: no panel)",
                     (unsigned long)img_len);
        }
#endif
        break;
    }
    case BLE_ESLP_EVENT_REFRESH_DISPLAY: {
        const esp_ble_eslp_refresh_display_t *ev = event_data;
        ESP_LOGI(TAG, "Refresh display: display=%u", ev->display_index);
        break;
    }
    case BLE_ESLP_EVENT_LED_CONTROL: {
        const esp_ble_eslp_led_control_t *ev = event_data;
        ESP_LOGI(TAG, "LED control: led=%u RGB=(%u,%u,%u) bright=%u off=%d",
                 ev->led_index, ev->color_red, ev->color_green, ev->color_blue,
                 ev->brightness, (int)ev->is_off);
        break;
    }
    case BLE_ESLP_EVENT_SENSOR_READ: {
        const esp_ble_eslp_sensor_read_t *ev = event_data;
        /* Demo ambient temperature payload (2 octets, Mesh Present Ambient Temperature). */
        const uint8_t demo_temp[2] = { 0x32, 0x00 }; /* 25.0 °C in 0.5 °C units */
        esp_err_t ret = esp_ble_eslp_report_sensor_data(ev->sensor_index, 0,
                                                        demo_temp, sizeof(demo_temp));
        ESP_LOGI(TAG, "Sensor read: index=%u report=%s",
                 ev->sensor_index, esp_err_to_name(ret));
        break;
    }
    default:
        break;
    }
}

static void app_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)event_data;

    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected");
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        break;
    default:
        break;
    }
}

static void app_esl_set_demo_capabilities(void)
{
    esp_ble_esl_display_info_t display = {
        .width = CONFIG_EXAMPLE_ESL_DISPLAY_WIDTH,
        .height = CONFIG_EXAMPLE_ESL_DISPLAY_HEIGHT,
        .display_type = 0x01, /* Black/White */
    };
    esp_err_t ret = esp_ble_esl_set_display_info(&display, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_display_info: %s", esp_err_to_name(ret));
    }

    esp_ble_esl_image_info_t image = {
        .max_image_index = CONFIG_EXAMPLE_ESL_MAX_IMAGE_INDEX,
    };
    ret = esp_ble_esl_set_image_info(&image);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_image_info: %s", esp_err_to_name(ret));
    }

#if defined(CONFIG_BLE_ESL_LED_INFO)
    /* sRGB LED, full R/G/B capability. */
    uint8_t led_info = 0x3F;
    ret = esp_ble_esl_set_led_info(&led_info, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_led_info: %s", esp_err_to_name(ret));
    }
#endif

#if defined(CONFIG_BLE_ESL_SENSOR_INFO)
    uint8_t sensor_info[3] = {
        0x00, /* short format */
        (uint8_t)(EXAMPLE_SENSOR_TYPE_TEMPERATURE & 0xFF),
        (uint8_t)((EXAMPLE_SENSOR_TYPE_TEMPERATURE >> 8) & 0xFF),
    };
    ret = esp_ble_esl_set_sensor_info(sensor_info, sizeof(sensor_info));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "set_sensor_info: %s", esp_err_to_name(ret));
    }
#endif
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_ESLP_EVENTS, ESP_EVENT_ANY_ID,
                                               app_eslp_event_handler, NULL));

    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV,
        .include_service_uuid = 1,
        .adv_uuid_type = BLE_CONN_UUID_TYPE_16,
        .adv_uuid16 = BLE_ESL_UUID16,
    };

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    /* Set Display/Image and optional LED/Sensor before ESL service init. */
    app_esl_set_demo_capabilities();
    ESP_ERROR_CHECK(esp_ble_eslp_init(NULL));

    ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_conn_start failed: %s", esp_err_to_name(ret));
        esp_ble_conn_stop();
        esp_ble_eslp_deinit();
        esp_ble_conn_deinit();
        return;
    }

    ESP_LOGI(TAG, "ESL Tag started. Advertising as %s",
             CONFIG_EXAMPLE_BLE_ADV_NAME);
}
