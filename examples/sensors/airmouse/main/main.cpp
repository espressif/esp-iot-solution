/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_function.h"
#include "airmouse_gyro_bias.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#if CONFIG_AIRMOUSE_ENABLE_BMM350
#include "airmouse_bmm350.h"
#endif

static const char *TAG = "AIRMOUSE";

static bool airmouse_boot_requires_initial_http_config(void)
{
#if CONFIG_AIRMOUSE_BOARD_PRESET_ESP_SPOT_C5 || \
    CONFIG_AIRMOUSE_BOARD_PRESET_SENSAIR_SHUTTLE
    return false;
#else
    return true;
#endif
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting AirMouse");

    esp_err_t ret;
    static airmouse_function_handle_t airmouse_ctx;

    airmouse_config_set_default(&airmouse_ctx.config);

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        return;
    }

    if (airmouse_boot_requires_initial_http_config()) {
        ret = airmouse_wifi_init_sta();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
            return;
        }

        ret = airmouse_http_server_start(&airmouse_ctx.config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
            return;
        }

        ESP_LOGI(TAG, "Waiting for AirMouse runtime config");
        ret = airmouse_http_server_wait_config_done(portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to wait AirMouse runtime config: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "AirMouse runtime config done");

        ret = airmouse_http_server_stop();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
        }

        ret = airmouse_wifi_deinit_sta();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to deinitialize WiFi: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG,
                 "Skip initial HTTP config because board preset already provides hardware defaults");
    }

    airmouse_bmi270_handle_t *imu_handle =
        airmouse_bmi270_init(&airmouse_ctx.config.hardware);
    if (imu_handle == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize BMI270 sensor");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    float startup_gyro_bias_dps[3] = {0.0f, 0.0f, 0.0f};
    bool startup_gyro_bias_valid = false;
    ret = airmouse_gyro_bias_prepare_or_run(imu_handle,
                                            &airmouse_ctx.config.hardware,
                                            startup_gyro_bias_dps,
                                            &startup_gyro_bias_valid);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "Startup gyro bias calibration unavailable (%s), "
                 "continuing with runtime rest-based learning only",
                 esp_err_to_name(ret));
    }

#if CONFIG_AIRMOUSE_ENABLE_BMM350
    airmouse_bmm350_handle_t *bmm_handle = airmouse_bmm350_init(
                                               imu_handle->i2c_bus,
                                               &airmouse_ctx.config.hardware);
    if (bmm_handle == nullptr) {
        ESP_LOGW(TAG, "BMM350 magnetometer not available, continuing without it");
    } else {
        ret = airmouse_bmm350_calibration_prepare_or_run(bmm_handle, imu_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG,
                     "BMM350 calibration unavailable (%s), continuing in 6-axis mode",
                     esp_err_to_name(ret));
        }
    }
#endif

    // Initialize BLE
    ret = airmouse_ble_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE: %s", esp_err_to_name(ret));
        airmouse_bmi270_deinit(imu_handle);
        return;
    }

    airmouse_ble_handle_t *ble_handle = airmouse_ble_hid_get_ctx();
    if (ble_handle == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize BLE HID");
        airmouse_bmi270_deinit(imu_handle);
        return;
    }

    static airmouse_active_pose_handle_t pose_ctx;

    ret = airmouse_function_prepare_resources(&airmouse_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare airmouse resources");
        airmouse_bmi270_deinit(imu_handle);
        return;
    }

    airmouse_btn_handle_t *btn_handle =
        airmouse_btn_init(airmouse_get_mouse_event_queue(),
                          &airmouse_ctx.config.hardware,
                          airmouse_get_runtime_gesture_task_handle_ref(),
                          airmouse_get_inference_task_handle_ref());
    if (btn_handle == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize reset button");
        airmouse_bmi270_deinit(imu_handle);
        return;
    }

    airmouse_ctx.imu_handle = imu_handle;
#if CONFIG_AIRMOUSE_ENABLE_BMM350
    airmouse_ctx.bmm_handle = bmm_handle;
#endif
    airmouse_ctx.ble_handle = ble_handle;
    airmouse_ctx.btn_handle = btn_handle;
    airmouse_ctx.pose_handle = &pose_ctx;

    if (startup_gyro_bias_valid) {
        ret = airmouse_pose_mapping_set_startup_gyro_bias(
                  airmouse_ctx.pose_handle,
                  startup_gyro_bias_dps);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG,
                     "Failed to stage startup gyro bias for pose solver: %s",
                     esp_err_to_name(ret));
        }
    }

    ret = airmouse_function_start(&airmouse_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start airmouse");
        airmouse_bmi270_deinit(imu_handle);
        return;
    }
}
