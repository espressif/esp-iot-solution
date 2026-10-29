/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct as5600_dev_t *as5600_handle_t;

/**
 * @brief AS5600 I2C configuration
 *
 */
typedef struct {
    uint32_t scl_speed_hz;          /*!< I2C clock speed, use default if set to 0 */
    uint32_t timeout_ms;            /*!< I2C transaction timeout, use default if set to 0 */
} as5600_i2c_config_t;

/** Magnet status from STATUS register */
typedef enum {
    AS5600_MAGNET_OK = 0,           /**< Magnet detected and in range */
    AS5600_MAGNET_TOO_WEAK = 1,     /**< Magnet too far or too weak */
    AS5600_MAGNET_TOO_STRONG = 2,   /**< Magnet too close or too strong */
    AS5600_MAGNET_NOT_DETECTED = 3, /**< Magnet not detected */
} as5600_magnet_status_t;

/** CONF[1:0] PM – Power Mode */
typedef enum {
    AS5600_POWER_MODE_NOM  = 0, /**< Normal mode */
    AS5600_POWER_MODE_LPM1 = 1, /**< Low-power mode 1 */
    AS5600_POWER_MODE_LPM2 = 2, /**< Low-power mode 2 */
    AS5600_POWER_MODE_LPM3 = 3, /**< Low-power mode 3 */
} as5600_power_mode_t;

/** CONF[3:2] HYST – Hysteresis */
typedef enum {
    AS5600_HYSTERESIS_OFF    = 0, /**< No hysteresis */
    AS5600_HYSTERESIS_1_LSB  = 1, /**< 1 LSB */
    AS5600_HYSTERESIS_2_LSB  = 2, /**< 2 LSBs */
    AS5600_HYSTERESIS_3_LSB  = 3, /**< 3 LSBs */
} as5600_hysteresis_t;

/** CONF[5:4] OUTS – Output Stage */
typedef enum {
    AS5600_OUTPUT_STAGE_ANALOG_FULL    = 0, /**< Analog 0–100% VDD */
    AS5600_OUTPUT_STAGE_ANALOG_REDUCED = 1, /**< Analog 10–90% VDD */
    AS5600_OUTPUT_STAGE_DIGITAL_PWM    = 2, /**< Digital PWM */
} as5600_output_stage_t;

/** CONF[7:6] PWMF – PWM Frequency (valid when OUTS = DIGITAL_PWM) */
typedef enum {
    AS5600_PWM_FREQ_115HZ = 0, /**< 115 Hz */
    AS5600_PWM_FREQ_230HZ = 1, /**< 230 Hz */
    AS5600_PWM_FREQ_460HZ = 2, /**< 460 Hz */
    AS5600_PWM_FREQ_920HZ = 3, /**< 920 Hz */
} as5600_pwm_freq_t;

/** CONF[9:8] SF – Slow Filter */
typedef enum {
    AS5600_SLOW_FILTER_16X = 0, /**< 16× (slowest, lowest noise) */
    AS5600_SLOW_FILTER_8X  = 1, /**< 8×  */
    AS5600_SLOW_FILTER_4X  = 2, /**< 4×  */
    AS5600_SLOW_FILTER_2X  = 3, /**< 2×  (fastest) */
} as5600_slow_filter_t;

/** CONF[12:10] FTH – Fast Filter Threshold */
typedef enum {
    AS5600_FAST_FILTER_SLOW_ONLY = 0, /**< Slow filter only */
    AS5600_FAST_FILTER_6_LSB     = 1, /**< 6 LSBs */
    AS5600_FAST_FILTER_7_LSB     = 2, /**< 7 LSBs */
    AS5600_FAST_FILTER_9_LSB     = 3, /**< 9 LSBs */
    AS5600_FAST_FILTER_18_LSB    = 4, /**< 18 LSBs */
    AS5600_FAST_FILTER_21_LSB    = 5, /**< 21 LSBs */
    AS5600_FAST_FILTER_24_LSB    = 6, /**< 24 LSBs */
    AS5600_FAST_FILTER_10_LSB    = 7, /**< 10 LSBs */
} as5600_fast_filter_t;

/**
 * @brief AS5600 CONF register configuration
 */
typedef struct {
    as5600_power_mode_t   power_mode;   /*!< PM  [1:0] Power mode */
    as5600_hysteresis_t   hysteresis;   /*!< HYST[3:2] Output hysteresis */
    as5600_output_stage_t output_stage; /*!< OUTS[5:4] Output stage type */
    as5600_pwm_freq_t     pwm_freq;     /*!< PWMF[7:6] PWM frequency (when OUTS=DIGITAL_PWM) */
    as5600_slow_filter_t  slow_filter;  /*!< SF  [9:8] Slow filter coefficient */
    as5600_fast_filter_t  fast_filter;  /*!< FTH [12:10] Fast filter threshold */
    bool                  watchdog;     /*!< WD  [13] Watchdog enable */
} as5600_conf_t;

/**
 * @brief Create a new AS5600 sensor handle
 *
 * @param[in]  bus I2C master bus handle (from `i2c_new_master_bus`)
 * @param[in]  i2c_conf I2C device configuration used by the AS5600
 * @param[out] handle_out New AS5600 device handle
 * @return
 *          - ESP_OK                  Device handle creation success
 *          - ESP_ERR_INVALID_ARG     Invalid argument
 *          - ESP_ERR_NO_MEM          Memory allocation failed
 *          - Other                   I2C device creation failed
 */
esp_err_t as5600_new_sensor(i2c_master_bus_handle_t bus, const as5600_i2c_config_t *i2c_conf, as5600_handle_t *handle_out);

/**
 * @brief Delete an AS5600 sensor handle
 *
 * @param[in] sensor AS5600 sensor handle
 * @return
 *          - ESP_OK                  Device handle deletion success
 *          - ESP_ERR_INVALID_ARG     Invalid argument
 *          - Other                   I2C device deletion failed
 */
esp_err_t as5600_del_sensor(as5600_handle_t sensor);

/**
 * @brief Get angle as a 12-bit raw value (0 - 4095, offset by zero position)
 *
 * Reads the ANGLE register (0x0E/0x0F), which is the filtered angle after
 * on-chip ZPOS subtraction. Equivalent to @ref as5600_get_angle_degrees but
 * returns the unscaled 12-bit integer instead of degrees.
 *
 * @param sensor Sensor handle
 * @param raw_angle Output: raw register value (0-4095)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t as5600_get_angle_raw(as5600_handle_t sensor, uint16_t *raw_angle);

/**
 * @brief Get angle in degrees (0.0 - 360.0, offset by zero position)
 *
 * @param sensor Sensor handle
 * @param degrees Output: angle in degrees
 * @return esp_err_t ESP_OK on success
 */
esp_err_t as5600_get_angle_degrees(as5600_handle_t sensor, float *degrees);

/**
 * @brief Get angle in radians (0.0 - 2π, offset by zero position)
 *
 * @param sensor Sensor handle
 * @param radians Output: angle in radians
 * @return esp_err_t ESP_OK on success
 */
esp_err_t as5600_get_angle_radians(as5600_handle_t sensor, float *radians);

/**
 * @brief Get magnet status (detection / too weak / too strong)
 *
 * @param sensor Sensor handle
 * @param status Output: magnet status
 * @return esp_err_t ESP_OK on success
 */
esp_err_t as5600_get_magnet_status(as5600_handle_t sensor, as5600_magnet_status_t *status);

/**
 * @brief Write the CONF register from an @ref as5600_conf_t structure
 *
 * @param sensor Sensor handle
 * @param conf   Pointer to configuration to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t as5600_set_conf(as5600_handle_t sensor, const as5600_conf_t *conf);

/**
 * @brief Read the CONF register into an @ref as5600_conf_t structure
 *
 * @param sensor Sensor handle
 * @param conf   Pointer to configuration to fill
 * @return esp_err_t ESP_OK on success
 */
esp_err_t as5600_get_conf(as5600_handle_t sensor, as5600_conf_t *conf);

/**
 * @brief Set zero position (ZPOS): set current position as zero angle position
 *
 * Call when the magnet is at the desired mechanical zero.
 * Reads RAW_ANGLE (0x0C/0x0D) and writes it to the ZPOS registers.
 *
 * @param sensor Sensor handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t as5600_set_zero_position(as5600_handle_t sensor);

#ifdef __cplusplus
}
#endif
