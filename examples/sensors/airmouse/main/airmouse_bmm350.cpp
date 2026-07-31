/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_bmm350.h"

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

#include "bmm350_api.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "nvs.h"

static const char *TAG = "BMM350";
static constexpr uint32_t AIRMOUSE_BMM350_POST_INTERFACE_DELAY_US = 100000;
static constexpr uint32_t AIRMOUSE_BMM350_POST_NORMAL_MODE_DELAY_US = 30000;
static constexpr int64_t AIRMOUSE_BMM350_LOG_THROTTLE_US = 1000000;
static constexpr enum bmm350_data_rates AIRMOUSE_BMM350_ODR = BMM350_DATA_RATE_200HZ;
static constexpr enum bmm350_performance_parameters AIRMOUSE_BMM350_PERFORMANCE =
    BMM350_REGULARPOWER;
static constexpr int32_t AIRMOUSE_BMM350_CALIBRATION_SAMPLE_PERIOD_MS = 10;
static constexpr int32_t AIRMOUSE_BMM350_CALIBRATION_SAMPLE_TARGET = 500;
static constexpr int32_t AIRMOUSE_BMM350_CALIBRATION_MIN_SAMPLE_COUNT = 300;
static constexpr int32_t AIRMOUSE_BMM350_CALIBRATION_INITIAL_SAMPLES = 100;
static constexpr int32_t AIRMOUSE_BMM350_CALIBRATION_TIMEOUT_MS = 120000;
static constexpr float AIRMOUSE_BMM350_COMPLEMENTARY_FILTER_ALPHA = 0.9f;
static constexpr uint32_t AIRMOUSE_BMM350_NVS_VERSION = 1;
static constexpr const char *AIRMOUSE_BMM350_NVS_NAMESPACE = "compass_cal";
static constexpr const char *AIRMOUSE_BMM350_NVS_KEY_HARD_IRON = "hard_iron";
static constexpr const char *AIRMOUSE_BMM350_NVS_KEY_SOFT_IRON = "soft_iron";
static constexpr const char *AIRMOUSE_BMM350_NVS_KEY_CALIBRATED = "calibrated";
static constexpr const char *AIRMOUSE_BMM350_NVS_KEY_VERSION = "version";

static constexpr uint8_t AIRMOUSE_BMM350_I2C_ADDR_CANDIDATES[] = {
    BMM350_I2C_ADSEL_SET_LOW,
    BMM350_I2C_ADSEL_SET_HIGH,
};
static constexpr uint32_t AIRMOUSE_BMM350_I2C_CLK_HZ = 100 * 1000;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float pitch;
    float roll;
    float yaw;
} airmouse_bmm350_euler_angles_t;

typedef struct {
    airmouse_bmm350_euler_angles_t euler;
    airmouse_bmm350_euler_angles_t euler_gyro;
    airmouse_bmm350_euler_angles_t euler_accel;
    float dt;
    bool initialized;
} airmouse_bmm350_complementary_filter_t;

static void airmouse_bmm350_reset_calibration_defaults(airmouse_bmm350_handle_t *handle)
{
    if (handle == nullptr) {
        return;
    }

    handle->mag_cal = {};
    handle->mag_cal.soft_iron[0][0] = 1.0f;
    handle->mag_cal.soft_iron[1][1] = 1.0f;
    handle->mag_cal.soft_iron[2][2] = 1.0f;
    handle->calibration_available = false;
}

static bool airmouse_bmm350_calibration_blob_is_valid(
    const airmouse_bmm350_mag_calibration_t *cal)
{
    if (cal == nullptr || !cal->calibrated) {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis) {
        if (!isfinite(cal->hard_iron[axis])) {
            return false;
        }
        for (int col = 0; col < 3; ++col) {
            if (!isfinite(cal->soft_iron[axis][col])) {
                return false;
            }
        }
    }

    if (fabsf(cal->soft_iron[0][0]) < 1e-6f ||
            fabsf(cal->soft_iron[1][1]) < 1e-6f ||
            fabsf(cal->soft_iron[2][2]) < 1e-6f) {
        return false;
    }

    return true;
}

static airmouse_bmm350_euler_angles_t airmouse_bmm350_calculate_euler_from_accel(
    float acc_x,
    float acc_y,
    float acc_z)
{
    airmouse_bmm350_euler_angles_t angles = {};

    angles.pitch = atan2f(-acc_x, sqrtf((acc_y * acc_y) + (acc_z * acc_z))) *
                   180.0f / (float)M_PI;
    angles.roll = atan2f(acc_y, acc_z) * 180.0f / (float)M_PI;

    return angles;
}

static void airmouse_bmm350_update_complementary_filter(
    airmouse_bmm350_complementary_filter_t *comp_filter,
    float acc_x,
    float acc_y,
    float acc_z,
    float gyro_x,
    float gyro_y,
    float gyro_z,
    float dt)
{
    if (comp_filter == nullptr) {
        return;
    }

    comp_filter->euler_accel =
        airmouse_bmm350_calculate_euler_from_accel(acc_x, acc_y, acc_z);

    if (!comp_filter->initialized) {
        comp_filter->euler = comp_filter->euler_accel;
        comp_filter->euler_gyro = comp_filter->euler_accel;
        comp_filter->initialized = true;
        return;
    }

    comp_filter->euler_gyro.pitch += gyro_y * dt;
    comp_filter->euler_gyro.roll += gyro_x * dt;
    comp_filter->euler_gyro.yaw += gyro_z * dt;

    comp_filter->euler.pitch =
        (AIRMOUSE_BMM350_COMPLEMENTARY_FILTER_ALPHA *
         comp_filter->euler_gyro.pitch) +
        ((1.0f - AIRMOUSE_BMM350_COMPLEMENTARY_FILTER_ALPHA) *
         comp_filter->euler_accel.pitch);
    comp_filter->euler.roll =
        (AIRMOUSE_BMM350_COMPLEMENTARY_FILTER_ALPHA *
         comp_filter->euler_gyro.roll) +
        ((1.0f - AIRMOUSE_BMM350_COMPLEMENTARY_FILTER_ALPHA) *
         comp_filter->euler_accel.roll);
    comp_filter->euler.yaw = comp_filter->euler_gyro.yaw;

    comp_filter->euler_gyro.pitch = comp_filter->euler.pitch;
    comp_filter->euler_gyro.roll = comp_filter->euler.roll;
}

static bool airmouse_bmm350_is_calibration_sufficient(
    float mag_min[3],
    float mag_max[3],
    float pitch_min,
    float pitch_max,
    float roll_min,
    float roll_max,
    uint8_t octant_coverage,
    int sample_count)
{
    if (sample_count < AIRMOUSE_BMM350_CALIBRATION_MIN_SAMPLE_COUNT) {
        return false;
    }

    float pitch_range = pitch_max - pitch_min;
    float roll_range = roll_max - roll_min;
    bool attitude_ok = (pitch_range > 90.0f) && (roll_range > 120.0f);
    if (!attitude_ok) {
        return false;
    }

    float mag_range[3] = {};
    for (int axis = 0; axis < 3; ++axis) {
        mag_range[axis] = mag_max[axis] - mag_min[axis];
    }

    float avg_range = (mag_range[0] + mag_range[1] + mag_range[2]) / 3.0f;
    float min_range = fminf(fminf(mag_range[0], mag_range[1]), mag_range[2]);
    bool mag_ok = (min_range > (avg_range * 0.5f)) && (avg_range > 50.0f);
    if (!mag_ok) {
        ESP_LOGW(TAG,
                 "Magnetic range insufficient: X=%.1f Y=%.1f Z=%.1f avg=%.1f",
                 mag_range[0], mag_range[1], mag_range[2], avg_range);
        return false;
    }

    int covered_octants = __builtin_popcount(octant_coverage);
    bool octant_ok = (covered_octants >= 4);
    if (!octant_ok) {
        ESP_LOGW(TAG, "Octant coverage low: %d/8 (minimum 4 required)",
                 covered_octants);
    }

    float max_range = fmaxf(fmaxf(mag_range[0], mag_range[1]), mag_range[2]);
    float ellipsoid_ratio = min_range / max_range;
    if (ellipsoid_ratio < 0.25f) {
        ESP_LOGW(TAG, "Ellipsoid too flat: ratio=%.2f (need >0.25)",
                 ellipsoid_ratio);
        return false;
    }

    if (!octant_ok && ellipsoid_ratio < 0.6f) {
        ESP_LOGW(TAG, "Both octant coverage and ellipsoid quality are insufficient");
        return false;
    }

    ESP_LOGI(TAG, "Calibration sufficient: samples=%d pitch=%.1f roll=%.1f",
             sample_count, pitch_range, roll_range);
    ESP_LOGI(TAG,
             "Coverage: octants=%d/8 ellipsoid=%.2f mag_range=[%.1f %.1f %.1f]",
             covered_octants, ellipsoid_ratio,
             mag_range[0], mag_range[1], mag_range[2]);
    return true;
}

void airmouse_bmm350_deinit(airmouse_bmm350_handle_t *handle)
{
    if (handle == nullptr) {
        return;
    }

    bmm350_interface_deinit();
    if (handle->owns_i2c_bus && handle->i2c_bus != nullptr) {
        i2c_bus_delete(&handle->i2c_bus);
    }
    free(handle);
}

airmouse_bmm350_handle_t *airmouse_bmm350_init(
    i2c_bus_handle_t imu_i2c_bus,
    const airmouse_hardware_config_t *hardware_config)
{
    if (imu_i2c_bus == nullptr || hardware_config == nullptr) {
        ESP_LOGE(TAG, "invalid BMM350 init arguments");
        return nullptr;
    }

    airmouse_bmm350_handle_t *handle =
        (airmouse_bmm350_handle_t *)calloc(1, sizeof(airmouse_bmm350_handle_t));
    if (handle == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for airmouse_bmm350_handle_t");
        return nullptr;
    }

    const bool reuse_imu_bus =
        (hardware_config->mag_i2c_scl == hardware_config->i2c_scl) &&
        (hardware_config->mag_i2c_sda == hardware_config->i2c_sda);
    if (reuse_imu_bus) {
        handle->i2c_bus = imu_i2c_bus;
        handle->owns_i2c_bus = false;
        ESP_LOGI(TAG, "BMM350 reusing IMU I2C bus: SDA=GPIO%d, SCL=GPIO%d",
                 hardware_config->mag_i2c_sda,
                 hardware_config->mag_i2c_scl);
    } else {
        ESP_LOGI(TAG, "BMM350 dedicated I2C bus: SDA=GPIO%d, SCL=GPIO%d",
                 hardware_config->mag_i2c_sda,
                 hardware_config->mag_i2c_scl);
        i2c_config_t i2c_bus_conf = {};
        i2c_bus_conf.mode = I2C_MODE_MASTER;
        i2c_bus_conf.sda_io_num = hardware_config->mag_i2c_sda;
        i2c_bus_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_bus_conf.scl_io_num = hardware_config->mag_i2c_scl;
        i2c_bus_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_bus_conf.master.clk_speed = AIRMOUSE_BMM350_I2C_CLK_HZ;
        handle->i2c_bus = i2c_bus_create(I2C_NUM_1, &i2c_bus_conf);
        if (handle->i2c_bus == nullptr) {
            ESP_LOGE(TAG, "Failed to create dedicated BMM350 I2C bus");
            airmouse_bmm350_deinit(handle);
            return nullptr;
        }
        handle->owns_i2c_bus = true;
    }

    airmouse_bmm350_reset_calibration_defaults(handle);

    int8_t init_rslt = BMM350_E_DEV_NOT_FOUND;
    size_t addr_candidate_count =
        sizeof(AIRMOUSE_BMM350_I2C_ADDR_CANDIDATES) / sizeof(AIRMOUSE_BMM350_I2C_ADDR_CANDIDATES[0]);

    for (size_t i = 0; i < addr_candidate_count; ++i) {
        handle->i2c_addr = AIRMOUSE_BMM350_I2C_ADDR_CANDIDATES[i];
        ESP_LOGI(TAG, "BMM350 I2C address selected: 0x%02X", handle->i2c_addr);

        int8_t rslt = bmm350_interface_init(&handle->bmm_dev, handle->i2c_bus, &handle->i2c_addr);
        if (rslt != BMM350_OK) {
            bmm350_error_codes_print_result("bmm350_interface_init", rslt);
            continue;
        }

        bmm350_delay_us(AIRMOUSE_BMM350_POST_INTERFACE_DELAY_US, &handle->bmm_dev);

        init_rslt = bmm350_init(&handle->bmm_dev);
        ESP_LOGI(TAG, "BMM350 chip id: 0x%02X (expect 0x%02X)", handle->bmm_dev.chip_id, BMM350_CHIP_ID);
        ESP_LOGI(TAG, "BMM350 init result: %d", init_rslt);

        if (handle->bmm_dev.chip_id == BMM350_CHIP_ID) {
            break;
        }
    }

    if (handle->bmm_dev.chip_id != BMM350_CHIP_ID) {
        ESP_LOGE(TAG, "BMM350 not found at 0x%02X or 0x%02X",
                 BMM350_I2C_ADSEL_SET_LOW, BMM350_I2C_ADSEL_SET_HIGH);
        airmouse_bmm350_deinit(handle);
        return nullptr;
    }

    if (init_rslt != BMM350_OK) {
        bmm350_error_codes_print_result("bmm350_init", init_rslt);
        ESP_LOGE(TAG,
                 "BMM350 init failed before minimal bring-up completed; skip ODR/axes/normal-mode "
                 "configuration. If interface flow matches Bosch examples and this persists, "
                 "check BMM350 VDD/VDDIO level, decoupling, and I2C pull-ups.");
        airmouse_bmm350_deinit(handle);
        return nullptr;
    }

    int8_t rslt = bmm350_configure_interrupt(BMM350_PULSED,
                                             BMM350_ACTIVE_HIGH,
                                             BMM350_INTR_PUSH_PULL,
                                             BMM350_UNMAP_FROM_PIN,
                                             &handle->bmm_dev);
    bmm350_error_codes_print_result("bmm350_configure_interrupt", rslt);
    if (rslt != BMM350_OK) {
        airmouse_bmm350_deinit(handle);
        return nullptr;
    }

    rslt = bmm350_enable_interrupt(BMM350_ENABLE_INTERRUPT, &handle->bmm_dev);
    bmm350_error_codes_print_result("bmm350_enable_interrupt", rslt);
    if (rslt != BMM350_OK) {
        airmouse_bmm350_deinit(handle);
        return nullptr;
    }

    rslt = bmm350_set_odr_performance(AIRMOUSE_BMM350_ODR,
                                      AIRMOUSE_BMM350_PERFORMANCE,
                                      &handle->bmm_dev);
    bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);
    if (rslt != BMM350_OK) {
        airmouse_bmm350_deinit(handle);
        return nullptr;
    }
    ESP_LOGI(TAG,
             "BMM350 ODR / performance configured: odr=%d performance=%d",
             (int)AIRMOUSE_BMM350_ODR,
             (int)AIRMOUSE_BMM350_PERFORMANCE);

    rslt = bmm350_enable_axes(BMM350_X_EN, BMM350_Y_EN, BMM350_Z_EN, &handle->bmm_dev);
    bmm350_error_codes_print_result("bmm350_enable_axes", rslt);
    if (rslt != BMM350_OK) {
        airmouse_bmm350_deinit(handle);
        return nullptr;
    }
    ESP_LOGI(TAG, "BMM350 axes enabled");

    rslt = bmm350_set_powermode(BMM350_NORMAL_MODE, &handle->bmm_dev);
    bmm350_error_codes_print_result("bmm350_set_powermode", rslt);
    if (rslt != BMM350_OK) {
        airmouse_bmm350_deinit(handle);
        return nullptr;
    }
    ESP_LOGI(TAG, "BMM350 normal mode enabled");

    bmm350_delay_us(AIRMOUSE_BMM350_POST_NORMAL_MODE_DELAY_US, &handle->bmm_dev);

    return handle;
}

static esp_err_t airmouse_bmm350_read_sample_once(
    airmouse_bmm350_handle_t *handle,
    float mag_data[3])
{
    if (handle == nullptr || mag_data == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t drdy_status = 0;
    int8_t rslt = bmm350_get_interrupt_status(&drdy_status, &handle->bmm_dev);
    if (rslt != BMM350_OK) {
        int64_t now_us = esp_timer_get_time();
        if ((now_us - handle->last_error_log_us) >= AIRMOUSE_BMM350_LOG_THROTTLE_US) {
            ESP_LOGW(TAG, "BMM350 data-ready status read failed: %d", rslt);
            handle->last_error_log_us = now_us;
        }
        return ESP_ERR_INVALID_STATE;
    }

    if (drdy_status == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    struct bmm350_mag_temp_data mag_temp_data;
    rslt = bmm350_get_compensated_mag_xyz_temp_data(&mag_temp_data, &handle->bmm_dev);
    if (rslt != BMM350_OK) {
        int64_t now_us = esp_timer_get_time();
        if ((now_us - handle->last_error_log_us) >= AIRMOUSE_BMM350_LOG_THROTTLE_US) {
            ESP_LOGW(TAG, "BMM350 compensated read failed: %d", rslt);
            handle->last_error_log_us = now_us;
        }
        return ESP_ERR_INVALID_STATE;
    }

    /* Board-level axis convention for the airmouse body frame. */
    mag_data[0] =  mag_temp_data.x;
    mag_data[1] = -mag_temp_data.y;
    mag_data[2] = -mag_temp_data.z;

    return ESP_OK;
}

esp_err_t airmouse_bmm350_calibration_load(airmouse_bmm350_handle_t *handle)
{
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    airmouse_bmm350_reset_calibration_defaults(handle);

    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(AIRMOUSE_BMM350_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No stored Compass calibration found in NVS: %s",
                 esp_err_to_name(err));
        return err;
    }

    uint32_t version = 0;
    err = nvs_get_u32(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_VERSION, &version);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read calibration version: %s",
                 esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    size_t hard_iron_size = sizeof(handle->mag_cal.hard_iron);
    err = nvs_get_blob(nvs_handle,
                       AIRMOUSE_BMM350_NVS_KEY_HARD_IRON,
                       handle->mag_cal.hard_iron,
                       &hard_iron_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load hard-iron calibration: %s",
                 esp_err_to_name(err));
        nvs_close(nvs_handle);
        airmouse_bmm350_reset_calibration_defaults(handle);
        return err;
    }

    size_t soft_iron_size = sizeof(handle->mag_cal.soft_iron);
    err = nvs_get_blob(nvs_handle,
                       AIRMOUSE_BMM350_NVS_KEY_SOFT_IRON,
                       handle->mag_cal.soft_iron,
                       &soft_iron_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load soft-iron calibration: %s",
                 esp_err_to_name(err));
        nvs_close(nvs_handle);
        airmouse_bmm350_reset_calibration_defaults(handle);
        return err;
    }

    uint8_t calibrated = 0;
    err = nvs_get_u8(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_CALIBRATED, &calibrated);
    nvs_close(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load calibration flag: %s",
                 esp_err_to_name(err));
        airmouse_bmm350_reset_calibration_defaults(handle);
        return err;
    }

    handle->mag_cal.calibrated = (calibrated != 0);
    handle->calibration_available =
        airmouse_bmm350_calibration_blob_is_valid(&handle->mag_cal);

    if (!handle->calibration_available) {
        ESP_LOGW(TAG, "Stored calibration is invalid, ignore it");
        airmouse_bmm350_reset_calibration_defaults(handle);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG,
             "Loaded Compass calibration from NVS (version=%" PRIu32 "): "
             "hard_iron=[%.2f %.2f %.2f] soft_diag=[%.3f %.3f %.3f]",
             version,
             handle->mag_cal.hard_iron[0],
             handle->mag_cal.hard_iron[1],
             handle->mag_cal.hard_iron[2],
             handle->mag_cal.soft_iron[0][0],
             handle->mag_cal.soft_iron[1][1],
             handle->mag_cal.soft_iron[2][2]);

    return ESP_OK;
}

esp_err_t airmouse_bmm350_calibration_save(airmouse_bmm350_handle_t *handle)
{
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!airmouse_bmm350_calibration_blob_is_valid(&handle->mag_cal)) {
        ESP_LOGW(TAG, "Refuse to save invalid Compass calibration");
        return ESP_ERR_INVALID_STATE;
    }

    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(AIRMOUSE_BMM350_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open(write) failed for Compass calibration: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_VERSION,
                      AIRMOUSE_BMM350_NVS_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_HARD_IRON,
                           handle->mag_cal.hard_iron,
                           sizeof(handle->mag_cal.hard_iron));
    }
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_SOFT_IRON,
                           handle->mag_cal.soft_iron,
                           sizeof(handle->mag_cal.soft_iron));
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_CALIBRATED, 1);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save Compass calibration: %s",
                 esp_err_to_name(err));
        return err;
    }

    handle->calibration_available = true;
    ESP_LOGI(TAG, "Compass calibration saved to NVS");
    return ESP_OK;
}

esp_err_t airmouse_bmm350_calibration_erase(void)
{
    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(AIRMOUSE_BMM350_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open(erase) failed for Compass calibration: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_key(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_VERSION);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_HARD_IRON);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_SOFT_IRON);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(nvs_handle, AIRMOUSE_BMM350_NVS_KEY_CALIBRATED);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Compass calibration erased from NVS");
    }

    return err;
}

esp_err_t airmouse_bmm350_apply_calibration(
    airmouse_bmm350_handle_t *handle,
    const float raw_mag[3],
    float calibrated_mag[3])
{
    if (handle == nullptr || raw_mag == nullptr || calibrated_mag == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!airmouse_bmm350_calibration_blob_is_valid(&handle->mag_cal)) {
        return ESP_ERR_INVALID_STATE;
    }

    float mag_hi_corrected[3] = {};
    for (int axis = 0; axis < 3; ++axis) {
        mag_hi_corrected[axis] = raw_mag[axis] - handle->mag_cal.hard_iron[axis];
    }

    for (int row = 0; row < 3; ++row) {
        calibrated_mag[row] =
            (handle->mag_cal.soft_iron[row][0] * mag_hi_corrected[0]) +
            (handle->mag_cal.soft_iron[row][1] * mag_hi_corrected[1]) +
            (handle->mag_cal.soft_iron[row][2] * mag_hi_corrected[2]);
    }

    return ESP_OK;
}

bool airmouse_bmm350_is_calibration_valid(const airmouse_bmm350_handle_t *handle)
{
    if (handle == nullptr) {
        return false;
    }

    return handle->calibration_available &&
           airmouse_bmm350_calibration_blob_is_valid(&handle->mag_cal);
}

esp_err_t airmouse_bmm350_calibration_prepare_or_run(
    airmouse_bmm350_handle_t *handle,
    airmouse_bmi270_handle_t *bmi270_handle)
{
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG,
             "AirMouse BMM350 calibration uses the SensairShuttle factory_demo "
             "Compass algorithm: hard-iron offsets + diagonal soft-iron scale "
             "with pitch/roll/octant coverage checks.");

    esp_err_t err = airmouse_bmm350_calibration_load(handle);
    if (err == ESP_OK && airmouse_bmm350_is_calibration_valid(handle)) {
        ESP_LOGI(TAG, "Valid Compass calibration already present, skip recalibration");
        return ESP_OK;
    }

    if (bmi270_handle == nullptr) {
        ESP_LOGW(TAG,
                 "Cannot run Compass calibration because BMI270 handle is not bound");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGW(TAG,
             "No valid Compass calibration in NVS, starting BMM350 calibration. "
             "Rotate the device slowly to cover as many orientations as possible.");

    float mag_min[3] = { 10000.0f, 10000.0f, 10000.0f };
    float mag_max[3] = { -10000.0f, -10000.0f, -10000.0f };
    float pitch_min = 10000.0f;
    float pitch_max = -10000.0f;
    float roll_min = 10000.0f;
    float roll_max = -10000.0f;
    float mag_center[3] = {};
    bool center_initialized = false;
    uint8_t octant_coverage = 0;
    int sample_count = 0;
    int64_t start_us = esp_timer_get_time();
    int64_t last_progress_log_us = 0;
    airmouse_bmm350_complementary_filter_t comp_filter = {};
    comp_filter.dt =
        (float)AIRMOUSE_BMM350_CALIBRATION_SAMPLE_PERIOD_MS / 1000.0f;

    airmouse_bmm350_reset_calibration_defaults(handle);

    while (((esp_timer_get_time() - start_us) / 1000LL) <
            AIRMOUSE_BMM350_CALIBRATION_TIMEOUT_MS) {
        float mag_raw[3] = {};
        err = airmouse_bmm350_read_sample_once(handle, mag_raw);
        if (err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(AIRMOUSE_BMM350_CALIBRATION_SAMPLE_PERIOD_MS));
            continue;
        }

        for (int axis = 0; axis < 3; ++axis) {
            if (mag_raw[axis] < mag_min[axis]) {
                mag_min[axis] = mag_raw[axis];
            }
            if (mag_raw[axis] > mag_max[axis]) {
                mag_max[axis] = mag_raw[axis];
            }
        }
        ++sample_count;

        if (sample_count >= AIRMOUSE_BMM350_CALIBRATION_INITIAL_SAMPLES) {
            for (int axis = 0; axis < 3; ++axis) {
                mag_center[axis] = (mag_max[axis] + mag_min[axis]) * 0.5f;
            }
            if (!center_initialized) {
                center_initialized = true;
                ESP_LOGI(TAG, "Estimated magnetic center: [%.1f %.1f %.1f]",
                         mag_center[0], mag_center[1], mag_center[2]);
            }

            uint8_t octant = 0;
            if (mag_raw[0] > mag_center[0]) {
                octant |= (1U << 0);
            }
            if (mag_raw[1] > mag_center[1]) {
                octant |= (1U << 1);
            }
            if (mag_raw[2] > mag_center[2]) {
                octant |= (1U << 2);
            }
            octant_coverage |= (1U << octant);
        }

        float accel[3] = {};
        float gyro[3] = {};
        err = airmouse_bmi270_read_imu_once(bmi270_handle, accel, gyro);
        if (err == ESP_OK) {
            airmouse_bmm350_update_complementary_filter(&comp_filter,
                                                        accel[0], accel[1], accel[2],
                                                        gyro[0], gyro[1], gyro[2],
                                                        comp_filter.dt);
            if (comp_filter.euler.pitch < pitch_min) {
                pitch_min = comp_filter.euler.pitch;
            }
            if (comp_filter.euler.pitch > pitch_max) {
                pitch_max = comp_filter.euler.pitch;
            }
            if (comp_filter.euler.roll < roll_min) {
                roll_min = comp_filter.euler.roll;
            }
            if (comp_filter.euler.roll > roll_max) {
                roll_max = comp_filter.euler.roll;
            }
        }

        int64_t now_us = esp_timer_get_time();
        if ((now_us - last_progress_log_us) >= AIRMOUSE_BMM350_LOG_THROTTLE_US) {
            int covered = __builtin_popcount(octant_coverage);
            float mag_range[3] = {
                mag_max[0] - mag_min[0],
                mag_max[1] - mag_min[1],
                mag_max[2] - mag_min[2],
            };
            ESP_LOGI(TAG,
                     "Calibration progress: samples=%d octants=%d/8 pitch=%.1f roll=%.1f "
                     "mag_range=[%.1f %.1f %.1f]",
                     sample_count, covered,
                     pitch_max - pitch_min,
                     roll_max - roll_min,
                     mag_range[0], mag_range[1], mag_range[2]);
            last_progress_log_us = now_us;
        }

        if (sample_count > AIRMOUSE_BMM350_CALIBRATION_SAMPLE_TARGET &&
                airmouse_bmm350_is_calibration_sufficient(
                    mag_min,
                    mag_max,
                    pitch_min,
                    pitch_max,
                    roll_min,
                    roll_max,
                    octant_coverage,
                    sample_count)) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(AIRMOUSE_BMM350_CALIBRATION_SAMPLE_PERIOD_MS));
    }

    if (sample_count <= AIRMOUSE_BMM350_CALIBRATION_SAMPLE_TARGET ||
            !airmouse_bmm350_is_calibration_sufficient(
                mag_min,
                mag_max,
                pitch_min,
                pitch_max,
                roll_min,
                roll_max,
                octant_coverage,
                sample_count)) {
        ESP_LOGW(TAG,
                 "Compass calibration did not complete successfully within %d ms; "
                 "magnetometer will stay disabled and the system continues in 6-axis mode",
                 AIRMOUSE_BMM350_CALIBRATION_TIMEOUT_MS);
        airmouse_bmm350_reset_calibration_defaults(handle);
        return ESP_ERR_TIMEOUT;
    }

    for (int axis = 0; axis < 3; ++axis) {
        handle->mag_cal.hard_iron[axis] = (mag_max[axis] + mag_min[axis]) * 0.5f;
    }

    float avg_delta[3] = {};
    for (int axis = 0; axis < 3; ++axis) {
        avg_delta[axis] = (mag_max[axis] - mag_min[axis]) * 0.5f;
    }
    float avg_radius = (avg_delta[0] + avg_delta[1] + avg_delta[2]) / 3.0f;
    if (avg_radius <= 0.0f ||
            avg_delta[0] <= 0.0f ||
            avg_delta[1] <= 0.0f ||
            avg_delta[2] <= 0.0f) {
        ESP_LOGW(TAG, "Compass calibration produced invalid radius values");
        airmouse_bmm350_reset_calibration_defaults(handle);
        return ESP_ERR_INVALID_STATE;
    }

    handle->mag_cal.soft_iron[0][0] = avg_radius / avg_delta[0];
    handle->mag_cal.soft_iron[1][1] = avg_radius / avg_delta[1];
    handle->mag_cal.soft_iron[2][2] = avg_radius / avg_delta[2];
    handle->mag_cal.calibrated = true;
    handle->calibration_available = true;

    ESP_LOGI(TAG,
             "Compass calibration complete: hard_iron=[%.2f %.2f %.2f] "
             "soft_diag=[%.3f %.3f %.3f]",
             handle->mag_cal.hard_iron[0],
             handle->mag_cal.hard_iron[1],
             handle->mag_cal.hard_iron[2],
             handle->mag_cal.soft_iron[0][0],
             handle->mag_cal.soft_iron[1][1],
             handle->mag_cal.soft_iron[2][2]);

    err = airmouse_bmm350_calibration_save(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Compass calibration completed but failed to save to NVS: %s",
                 esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}
