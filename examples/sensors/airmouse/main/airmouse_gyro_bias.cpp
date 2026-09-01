/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_gyro_bias.h"

#include <inttypes.h>
#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

static const char *TAG = "AIRMOUSE_GYRO";

static constexpr const char *AIRMOUSE_GYRO_BIAS_NVS_NAMESPACE = "gyro_bias";
static constexpr const char *AIRMOUSE_GYRO_BIAS_NVS_KEY_VERSION = "version";
static constexpr const char *AIRMOUSE_GYRO_BIAS_NVS_KEY_BIAS = "gyro_dps";
static constexpr const char *AIRMOUSE_GYRO_BIAS_NVS_KEY_VALID = "valid";
static constexpr uint32_t AIRMOUSE_GYRO_BIAS_NVS_VERSION = 1;

static constexpr int64_t AIRMOUSE_GYRO_BIAS_CALIBRATION_TIME_US = 3000000;
static constexpr TickType_t AIRMOUSE_GYRO_BIAS_SAMPLE_DELAY_TICKS =
    pdMS_TO_TICKS(10);
static constexpr float AIRMOUSE_GYRO_BIAS_MAX_STILL_NORM_DPS = 5.0f;
static constexpr float AIRMOUSE_GYRO_BIAS_MIN_STILL_ACC_G = 0.90f;
static constexpr float AIRMOUSE_GYRO_BIAS_MAX_STILL_ACC_G = 1.10f;
static constexpr uint32_t AIRMOUSE_GYRO_BIAS_MIN_VALID_SAMPLES = 120;
static constexpr int64_t AIRMOUSE_GYRO_BIAS_PROGRESS_LOG_US = 1000000;

static float airmouse_vec_norm3(const float v[3])
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

esp_err_t airmouse_gyro_bias_probe(bool *stored)
{
    if (stored == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *stored = false;

    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(AIRMOUSE_GYRO_BIAS_NVS_NAMESPACE,
                             NVS_READONLY,
                             &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    uint32_t version = 0;
    uint8_t valid = 0;
    float gyro_bias_dps[3] = {0.0f, 0.0f, 0.0f};
    size_t blob_size = sizeof(gyro_bias_dps);

    err = nvs_get_u32(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_VERSION, &version);
    if (err == ESP_OK) {
        err = nvs_get_u8(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_VALID, &valid);
    }
    if (err == ESP_OK) {
        err = nvs_get_blob(nvs_handle,
                           AIRMOUSE_GYRO_BIAS_NVS_KEY_BIAS,
                           gyro_bias_dps,
                           &blob_size);
    }
    nvs_close(nvs_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    *stored = (version == AIRMOUSE_GYRO_BIAS_NVS_VERSION) &&
              (valid != 0) &&
              (blob_size == sizeof(gyro_bias_dps));
    return ESP_OK;
}

static esp_err_t airmouse_gyro_bias_calibrate(
    const airmouse_hardware_config_t *hardware_config,
    airmouse_bmi270_handle_t *imu_handle,
    float gyro_bias_dps[3])
{
    if (hardware_config == nullptr || imu_handle == nullptr ||
            gyro_bias_dps == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)hardware_config;

    ESP_LOGW(TAG,
             "No valid startup gyro bias in NVS, starting calibration sampling immediately.");

    esp_err_t ret = ESP_OK;

    ESP_LOGW(TAG, "Startup gyro calibration sampling started; keep still for 3 seconds");

    const int64_t start_us = esp_timer_get_time();
    int64_t last_progress_log_us = start_us;
    uint32_t valid_samples = 0;
    float gyro_sum[3] = {0.0f, 0.0f, 0.0f};

    while ((esp_timer_get_time() - start_us) < AIRMOUSE_GYRO_BIAS_CALIBRATION_TIME_US) {
        float accel[3] = {0.0f, 0.0f, 0.0f};
        float gyro[3] = {0.0f, 0.0f, 0.0f};

        ret = airmouse_bmi270_read_imu_once(imu_handle, accel, gyro);
        if (ret == ESP_OK) {
            const float gyro_norm_dps = airmouse_vec_norm3(gyro);
            const float accel_norm_g = airmouse_vec_norm3(accel);

            if (gyro_norm_dps <= AIRMOUSE_GYRO_BIAS_MAX_STILL_NORM_DPS &&
                    accel_norm_g >= AIRMOUSE_GYRO_BIAS_MIN_STILL_ACC_G &&
                    accel_norm_g <= AIRMOUSE_GYRO_BIAS_MAX_STILL_ACC_G) {
                gyro_sum[0] += gyro[0];
                gyro_sum[1] += gyro[1];
                gyro_sum[2] += gyro[2];
                valid_samples++;
            }
        }

        const int64_t now_us = esp_timer_get_time();
        if ((now_us - last_progress_log_us) >= AIRMOUSE_GYRO_BIAS_PROGRESS_LOG_US) {
            ESP_LOGI(TAG,
                     "Gyro startup calibration progress: valid_samples=%" PRIu32,
                     valid_samples);
            last_progress_log_us = now_us;
        }

        vTaskDelay(AIRMOUSE_GYRO_BIAS_SAMPLE_DELAY_TICKS);
    }

    if (valid_samples < AIRMOUSE_GYRO_BIAS_MIN_VALID_SAMPLES) {
        ESP_LOGW(TAG,
                 "Gyro startup calibration failed: not enough still samples (%" PRIu32
                 "/%" PRIu32 ")",
                 valid_samples,
                 AIRMOUSE_GYRO_BIAS_MIN_VALID_SAMPLES);
        return ESP_ERR_INVALID_STATE;
    }

    gyro_bias_dps[0] = gyro_sum[0] / (float)valid_samples;
    gyro_bias_dps[1] = gyro_sum[1] / (float)valid_samples;
    gyro_bias_dps[2] = gyro_sum[2] / (float)valid_samples;

    ESP_LOGI(TAG,
             "Gyro startup calibration complete: bias_dps=[%.5f %.5f %.5f], "
             "valid_samples=%" PRIu32,
             gyro_bias_dps[0],
             gyro_bias_dps[1],
             gyro_bias_dps[2],
             valid_samples);
    return ESP_OK;
}

esp_err_t airmouse_gyro_bias_load(float gyro_bias_dps[3], bool *loaded)
{
    if (gyro_bias_dps == nullptr || loaded == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *loaded = false;
    gyro_bias_dps[0] = 0.0f;
    gyro_bias_dps[1] = 0.0f;
    gyro_bias_dps[2] = 0.0f;

    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(AIRMOUSE_GYRO_BIAS_NVS_NAMESPACE,
                             NVS_READONLY,
                             &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No stored startup gyro bias found in NVS");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open(read) failed for gyro bias: %s",
                 esp_err_to_name(err));
        return err;
    }

    uint32_t version = 0;
    uint8_t valid = 0;
    size_t blob_size = sizeof(float) * 3;

    err = nvs_get_u32(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_VERSION, &version);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No startup gyro bias version key found in NVS");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "nvs_get_u32(version) failed for gyro bias: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = nvs_get_u8(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_VALID, &valid);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No valid flag found for startup gyro bias in NVS");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "nvs_get_u8(valid) failed for gyro bias: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = nvs_get_blob(nvs_handle,
                       AIRMOUSE_GYRO_BIAS_NVS_KEY_BIAS,
                       gyro_bias_dps,
                       &blob_size);
    nvs_close(nvs_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No startup gyro bias blob found in NVS");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "nvs_get_blob(bias) failed for gyro bias: %s",
                 esp_err_to_name(err));
        return err;
    }

    if (version != AIRMOUSE_GYRO_BIAS_NVS_VERSION || valid == 0 ||
            blob_size != (sizeof(float) * 3)) {
        ESP_LOGW(TAG,
                 "Stored startup gyro bias is invalid: version=%" PRIu32
                 " valid=%u size=%u",
                 version,
                 (unsigned int)valid,
                 (unsigned int)blob_size);
        gyro_bias_dps[0] = 0.0f;
        gyro_bias_dps[1] = 0.0f;
        gyro_bias_dps[2] = 0.0f;
        return ESP_OK;
    }

    *loaded = true;
    ESP_LOGI(TAG,
             "Loaded startup gyro bias from NVS: [%.5f %.5f %.5f] dps",
             gyro_bias_dps[0],
             gyro_bias_dps[1],
             gyro_bias_dps[2]);
    return ESP_OK;
}

esp_err_t airmouse_gyro_bias_save(const float gyro_bias_dps[3])
{
    if (gyro_bias_dps == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(AIRMOUSE_GYRO_BIAS_NVS_NAMESPACE,
                             NVS_READWRITE,
                             &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open(write) failed for gyro bias: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(nvs_handle,
                      AIRMOUSE_GYRO_BIAS_NVS_KEY_VERSION,
                      AIRMOUSE_GYRO_BIAS_NVS_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs_handle,
                           AIRMOUSE_GYRO_BIAS_NVS_KEY_BIAS,
                           gyro_bias_dps,
                           sizeof(float) * 3);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_VALID, 1);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save startup gyro bias to NVS: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG,
             "Startup gyro bias saved to NVS: [%.5f %.5f %.5f] dps",
             gyro_bias_dps[0],
             gyro_bias_dps[1],
             gyro_bias_dps[2]);
    return ESP_OK;
}

esp_err_t airmouse_gyro_bias_erase(void)
{
    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(AIRMOUSE_GYRO_BIAS_NVS_NAMESPACE,
                             NVS_READWRITE,
                             &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open(erase) failed for gyro bias: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_key(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_VERSION);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_BIAS);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(nvs_handle, AIRMOUSE_GYRO_BIAS_NVS_KEY_VALID);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Startup gyro bias erased from NVS");
    } else {
        ESP_LOGW(TAG, "Failed to erase startup gyro bias from NVS: %s",
                 esp_err_to_name(err));
    }
    return err;
}

esp_err_t airmouse_gyro_bias_prepare_or_run(
    airmouse_bmi270_handle_t *imu_handle,
    const airmouse_hardware_config_t *hardware_config,
    float gyro_bias_dps[3],
    bool *valid)
{
    if (imu_handle == nullptr || hardware_config == nullptr ||
            gyro_bias_dps == nullptr || valid == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *valid = false;
    gyro_bias_dps[0] = 0.0f;
    gyro_bias_dps[1] = 0.0f;
    gyro_bias_dps[2] = 0.0f;

    bool loaded = false;
    esp_err_t err = airmouse_gyro_bias_load(gyro_bias_dps, &loaded);
    if (err != ESP_OK) {
        return err;
    }
    if (loaded) {
        *valid = true;
        return ESP_OK;
    }

    err = airmouse_gyro_bias_calibrate(hardware_config, imu_handle, gyro_bias_dps);
    if (err != ESP_OK) {
        return err;
    }

    *valid = true;
    err = airmouse_gyro_bias_save(gyro_bias_dps);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}
