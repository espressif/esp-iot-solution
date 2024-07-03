/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_rainmaker.h"
#include "esp_rmaker_standard_types.h"
#include "esp_rmaker_standard_params.h"
#include "esp_rmaker_standard_devices.h"
#include "esp_rmaker_schedule.h"
#include "esp_rmaker_scenes.h"
#include "esp_rmaker_console.h"
#include "esp_rmaker_ota.h"
#include "esp_rmaker_common_events.h"
#include "app_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "app_variable.h"
#include "hal_bldc.h"
#include "hal_stepper_motor.h"
#include "bldc_fan_config.h"

const static char *TAG = "app_rainmaker";
static esp_rmaker_device_t *fan_device;
static esp_rmaker_param_t *mode_param;
static esp_rmaker_param_t *speed_param;
static esp_rmaker_param_t *power_param;
static esp_rmaker_param_t *shake_param;

static esp_err_t app_rainmaker_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param, esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }

    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        //*!< get power status */
        ESP_LOGI(TAG, "Power received value = %s for %s - %s",
                 val.val.b ? "true" : "false", esp_rmaker_device_get_name(device),
                 esp_rmaker_param_get_name(param));
        motor_parameter.is_start = val.val.b ? 1 : 0;
        if (motor_parameter.is_start == 0) {
            motor_parameter.start_count = 0;
            motor_parameter.target_speed = BLDC_MIN_SPEED;
        }
        hal_bldc_start_stop(motor_parameter.is_start);
    } else if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_SPEED_NAME) == 0) {
        //*!< get target speed value */
        ESP_LOGI(TAG, "Received Speed:%d", val.val.i);
        motor_parameter.target_speed = val.val.i;
        hal_bldc_set_speed(motor_parameter.target_speed);
    } else if (strcmp(esp_rmaker_param_get_name(param), "Shake") == 0) {
        //*!< get shake status */
        ESP_LOGI(TAG, "Shake received value = %s for %s - %s",
                 val.val.b ? "true" : "false", esp_rmaker_device_get_name(device),
                 esp_rmaker_param_get_name(param));
        stepper_motor.is_start = val.val.b ? 1 : 0;
    } else if (strcmp(esp_rmaker_param_get_name(param), "Natural") == 0) {
        //*!< get shake status */
        ESP_LOGI(TAG, "Natural received value = %s for %s - %s",
                 val.val.b ? "true" : "false", esp_rmaker_device_get_name(device),
                 esp_rmaker_param_get_name(param));
        motor_parameter.is_natural = val.val.b ? 1 : 0;
    }

    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static void app_rainmaker_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == RMAKER_EVENT) {
        switch (event_id) {
        case RMAKER_EVENT_INIT_DONE:
            ESP_LOGI(TAG, "RainMaker Initialised.");
            break;
        case RMAKER_EVENT_CLAIM_STARTED:
            ESP_LOGI(TAG, "RainMaker Claim Started.");
            break;
        case RMAKER_EVENT_CLAIM_SUCCESSFUL:
            ESP_LOGI(TAG, "RainMaker Claim Successful.");
            break;
        case RMAKER_EVENT_CLAIM_FAILED:
            ESP_LOGI(TAG, "RainMaker Claim Failed.");
            break;
        default:
            ESP_LOGW(TAG, "Unhandled RainMaker Event: %" PRIi32, event_id);
        }
    } else if (event_base == RMAKER_COMMON_EVENT) {
        switch (event_id) {
        case RMAKER_EVENT_REBOOT:
            ESP_LOGI(TAG, "Rebooting in %d seconds.", *((uint8_t *)event_data));
            break;
        case RMAKER_EVENT_WIFI_RESET:
            ESP_LOGI(TAG, "Wi-Fi credentials reset.");
            break;
        case RMAKER_EVENT_FACTORY_RESET:
            ESP_LOGI(TAG, "Node reset to factory defaults.");
            break;
        case RMAKER_MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected.");
            break;
        case RMAKER_MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected.");
            break;
        case RMAKER_MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT Published. Msg id: %d.", *((int *)event_data));
            break;
        default:
            ESP_LOGW(TAG, "Unhandled RainMaker Common Event: %" PRIi32, event_id);
        }
    } else if (event_base == APP_WIFI_EVENT) {
        switch (event_id) {
        case APP_WIFI_EVENT_QR_DISPLAY:
            ESP_LOGI(TAG, "Provisioning QR : %s", (char *)event_data);
            break;
        case APP_WIFI_EVENT_PROV_TIMEOUT:
            ESP_LOGI(TAG, "Provisioning Timed Out. Please reboot.");
            break;
        case APP_WIFI_EVENT_PROV_RESTART:
            ESP_LOGI(TAG, "Provisioning has restarted due to failures.");
            break;
        default:
            ESP_LOGW(TAG, "Unhandled App Wi-Fi Event: %" PRIi32, event_id);
            break;
        }
    } else if (event_base == RMAKER_OTA_EVENT) {
        switch (event_id) {
        case RMAKER_OTA_EVENT_STARTING:
            ESP_LOGI(TAG, "Starting OTA.");
            break;
        case RMAKER_OTA_EVENT_IN_PROGRESS:
            ESP_LOGI(TAG, "OTA is in progress.");
            break;
        case RMAKER_OTA_EVENT_SUCCESSFUL:
            ESP_LOGI(TAG, "OTA successful.");
            break;
        case RMAKER_OTA_EVENT_FAILED:
            ESP_LOGI(TAG, "OTA Failed.");
            break;
        case RMAKER_OTA_EVENT_REJECTED:
            ESP_LOGI(TAG, "OTA Rejected.");
            break;
        case RMAKER_OTA_EVENT_DELAYED:
            ESP_LOGI(TAG, "OTA Delayed.");
            break;
        case RMAKER_OTA_EVENT_REQ_FOR_REBOOT:
            ESP_LOGI(TAG, "Firmware image downloaded. Please reboot your device to apply the upgrade.");
            break;
        default:
            ESP_LOGW(TAG, "Unhandled OTA Event: %" PRIi32, event_id);
            break;
        }
    } else {
        ESP_LOGW(TAG, "Invalid event received!");
    }
}

static void rainmaker_task(void *arg)
{
    esp_rmaker_console_init();                                                                                                   //*!< init rainmaker console */
    app_wifi_init();                                                                                                             //*!< init wifi, choose wifi or ble */

    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &app_rainmaker_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &app_rainmaker_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_WIFI_EVENT, ESP_EVENT_ANY_ID, &app_rainmaker_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_OTA_EVENT, ESP_EVENT_ANY_ID, &app_rainmaker_event_handler, NULL));

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Switch");                            //*!< create rainmaker node */
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    fan_device = esp_rmaker_device_create("Fan", ESP_RMAKER_DEVICE_FAN, NULL);                                                   //*!< create device */
    esp_rmaker_device_add_cb(fan_device, app_rainmaker_write_cb, NULL);

    power_param = esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, false);                                               //*!< create power param */
    esp_rmaker_device_add_param(fan_device, power_param);
    esp_rmaker_device_assign_primary_param(fan_device, power_param);                                                             //*!< set powr param display on home page */
    esp_rmaker_node_add_device(node, fan_device);

    speed_param = esp_rmaker_param_create(ESP_RMAKER_DEF_SPEED_NAME, ESP_RMAKER_PARAM_SPEED,
                                          esp_rmaker_int(300), PROP_FLAG_READ | PROP_FLAG_WRITE);                                //*!< create slide param */
    if (speed_param) {
        esp_rmaker_param_add_ui_type(speed_param, ESP_RMAKER_UI_SLIDER);
        esp_rmaker_param_add_bounds(speed_param, esp_rmaker_int(300), esp_rmaker_int(800), esp_rmaker_int(1));                   //*!< set slider range */
    }
    esp_rmaker_device_add_param(fan_device, speed_param);

    mode_param = esp_rmaker_power_param_create("Natural", false);                                                                //*!< create mode param */
    esp_rmaker_device_add_param(fan_device, mode_param);

    shake_param = esp_rmaker_power_param_create("Shake", false);                                                                 //*!< create shake param */
    esp_rmaker_device_add_param(fan_device, shake_param);

    esp_rmaker_ota_enable_default();                                                                                             //*!< enable ota */

    esp_rmaker_system_serv_config_t system_serv_config = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reboot_seconds = 2,
        .reset_seconds = 2,
        .reset_reboot_seconds = 2,
    };
    esp_rmaker_system_service_enable(&system_serv_config);                                                                       //*!< enable system service */

    esp_rmaker_schedule_enable();                                                                                                //*!< enable schedule

    esp_rmaker_start();                                                                                                          //*!< start rainmaker */
    esp_err_t err = app_wifi_start(POP_TYPE_NONE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    vTaskDelete(NULL);
}

esp_err_t app_rainmaker_init()
{
    esp_err_t err = nvs_flash_init();                                                                                            //*!< init nvs */
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    xTaskCreate(rainmaker_task, "rainmaker_task", 4096, NULL, 5, NULL);
    return ESP_OK;                                              //*!< create stepper motor task */
}

esp_rmaker_param_t *app_rainmaker_get_param(const char *name)
{
    if (!strcmp(name, "Power")) {
        return power_param;
    } else if (!strcmp(name, "Speed")) {
        return speed_param;
    } else if (!strcmp(name, "Natural")) {
        return mode_param;
    } else if (!strcmp(name, "Shake")) {
        return shake_param;
    }
    return NULL;
}
