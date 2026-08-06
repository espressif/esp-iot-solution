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

static bool airmouse_board_preset_can_skip_initial_http_config(void)
{
#if CONFIG_AIRMOUSE_BOARD_PRESET_ESP_SPOT_C5 || \
    CONFIG_AIRMOUSE_BOARD_PRESET_SENSAIR_SHUTTLE
    return true;
#else
    return false;
#endif
}

static bool airmouse_boot_requires_initial_http_config(void)
{
    if (!airmouse_board_preset_can_skip_initial_http_config()) {
        return true;
    }

    bool gyro_bias_stored = false;
    esp_err_t ret = airmouse_gyro_bias_probe(&gyro_bias_stored);
    if (ret != ESP_OK || !gyro_bias_stored) {
        return true;
    }

#if CONFIG_AIRMOUSE_ENABLE_BMM350
    bool mag_calibration_stored = false;
    ret = airmouse_bmm350_calibration_probe(&mag_calibration_stored);
    if (ret != ESP_OK || !mag_calibration_stored) {
        return true;
    }
#endif

    return false;
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

    const bool requires_initial_http_config =
        airmouse_boot_requires_initial_http_config();

    if (requires_initial_http_config) {
        airmouse_http_set_initial_calibration_flow(true);
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
        ret = airmouse_http_server_wait_config_submitted(portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to wait AirMouse runtime config: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "AirMouse runtime config submitted");
    } else {
        ESP_LOGI(TAG,
                 "Skip initial HTTP config because board preset and stored calibration data are ready");
    }

    airmouse_bmi270_handle_t *imu_handle =
        airmouse_bmi270_init(&airmouse_ctx.config.hardware);
    if (imu_handle == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize BMI270 sensor");
        if (requires_initial_http_config) {
            airmouse_http_runtime_notice_set("initial-config",
                                             "error",
                                             "Failed to initialize the BMI270 sensor with the submitted hardware config.",
                                             true);
        }
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    if (requires_initial_http_config) {
        airmouse_http_set_calibration_notice_enabled(true);
        airmouse_http_refresh_calibration_notice_state();
    }

    float startup_gyro_bias_dps[3] = {0.0f, 0.0f, 0.0f};
    bool startup_gyro_bias_valid = false;
    bool startup_gyro_bias_stored = false;
    ret = airmouse_gyro_bias_probe(&startup_gyro_bias_stored);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to probe startup gyro bias state: %s",
                 esp_err_to_name(ret));
        startup_gyro_bias_stored = false;
    }
    if (requires_initial_http_config && !startup_gyro_bias_stored) {
        airmouse_http_prepare_runtime_calibration("gyro", 3);
        airmouse_http_runtime_notice_set("startup-calibration",
                                         "warning",
                                         "Gyroscope calibration required. Open /runtime-state and keep the device still when the 3-second countdown finishes.",
                                         true);
        ret = airmouse_http_server_wait_runtime_calibration_start(portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to wait for runtime calibration start: %s",
                     esp_err_to_name(ret));
            return;
        }
    }
    ret = airmouse_gyro_bias_prepare_or_run(imu_handle,
                                            &airmouse_ctx.config.hardware,
                                            startup_gyro_bias_dps,
                                            &startup_gyro_bias_valid);
    if (requires_initial_http_config) {
        airmouse_http_finish_runtime_calibration();
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "Startup gyro bias calibration unavailable (%s), "
                 "continuing with runtime rest-based learning only",
                 esp_err_to_name(ret));
    }
    if (requires_initial_http_config) {
        airmouse_http_refresh_calibration_notice_state();
    }

#if CONFIG_AIRMOUSE_ENABLE_BMM350
    airmouse_bmm350_handle_t *bmm_handle = airmouse_bmm350_init(
                                               imu_handle->i2c_bus,
                                               &airmouse_ctx.config.hardware);
    if (bmm_handle == nullptr) {
        ESP_LOGW(TAG, "BMM350 magnetometer not available, continuing without it");
        if (requires_initial_http_config) {
            airmouse_http_runtime_notice_set("startup-calibration",
                                             "warning",
                                             "BMM350 magnetometer initialization failed. The device will continue without magnetometer calibration.",
                                             true);
        }
    } else {
        bool mag_calibration_stored = false;
        ret = airmouse_bmm350_calibration_probe(&mag_calibration_stored);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to probe BMM350 calibration state: %s",
                     esp_err_to_name(ret));
            mag_calibration_stored = false;
        }
        if (requires_initial_http_config && !mag_calibration_stored) {
            airmouse_http_prepare_runtime_calibration("mag", 0);
            airmouse_http_runtime_notice_set("startup-calibration",
                                             "warning",
                                             "Magnetometer calibration required. Visit /runtime-state for progress, then rotate the device through different orientations.",
                                             true);
        }
        ret = airmouse_bmm350_calibration_prepare_or_run(bmm_handle, imu_handle);
        if (requires_initial_http_config && !mag_calibration_stored) {
            airmouse_http_finish_runtime_calibration();
        }
        if (ret != ESP_OK) {
            ESP_LOGW(TAG,
                     "BMM350 calibration unavailable (%s), continuing in 6-axis mode",
                     esp_err_to_name(ret));
        }
        if (requires_initial_http_config) {
            airmouse_http_refresh_calibration_notice_state();
        }
    }
#endif

    if (requires_initial_http_config) {
        airmouse_http_set_calibration_notice_enabled(false);
        airmouse_http_runtime_notice_set("", "", "", false);
        airmouse_http_set_initial_calibration_flow(false);
        airmouse_http_signal_config_done();

        ret = airmouse_http_server_stop();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
        }

        ret = airmouse_wifi_deinit_sta();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to deinitialize WiFi: %s", esp_err_to_name(ret));
        }
    }

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
