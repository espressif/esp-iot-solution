/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bmm150_aux_adapter.h
 * @brief Access BMM150 through BMI270 AUX I2C (`bmi270_aux_read` / `bmi270_aux_write`).
 */

#ifndef BMM150_AUX_ADAPTER_H
#define BMM150_AUX_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "bmm150.h"
#include "bmi270_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BMM150 AUX adapter configuration
 */
typedef struct {
    bmi270_handle_t bmi270_dev; /**< BMI270 handle (AUX already configured in manual mode) */
} bmm150_aux_config_t;

/**
 * @brief BMM150 AUX adapter handle
 */
typedef struct {
    struct bmm150_dev bmm150_dev; /**< BMM150 device structure */
    bmi270_handle_t bmi270_dev;   /**< BMI270 handle used for AUX R/W */
    bool is_initialized;          /**< Initialization status */
} bmm150_aux_handle_t;

/**
 * @brief Initialize BMM150 AUX adapter
 *
 * @param config Configuration structure
 * @param handle Device handle pointer
 * @return int8_t BMM150_OK on success, error code otherwise
 *
 * @note Call `bmi270_aux_set_config()` on the BMI270 handle first (manual mode).
 */
int8_t bmm150_aux_adapter_init(const bmm150_aux_config_t *config, bmm150_aux_handle_t *handle);

/**
 * @brief Deinitialize BMM150 AUX adapter
 *
 * @param handle Device handle
 * @return int8_t BMM150_OK on success, error code otherwise
 */
int8_t bmm150_aux_adapter_deinit(bmm150_aux_handle_t *handle);

/**
 * @brief Read magnetometer data
 *
 * @param handle Device handle
 * @param mag_data Magnetometer data structure
 * @return int8_t BMM150_OK on success, error code otherwise
 */
int8_t bmm150_aux_adapter_read_mag_data(bmm150_aux_handle_t *handle, struct bmm150_mag_data *mag_data);

/**
 * @brief Configure BMM150 data rate and repetition settings
 *
 * @param handle Device handle
 * @param settings BMM150 settings structure
 * @return int8_t BMM150_OK on success, error code otherwise
 */
int8_t bmm150_aux_adapter_configure(bmm150_aux_handle_t *handle, const struct bmm150_settings *settings);

/**
 * @brief Perform BMM150 soft reset
 *
 * @param handle Device handle
 * @return int8_t BMM150_OK on success, error code otherwise
 */
int8_t bmm150_aux_adapter_soft_reset(bmm150_aux_handle_t *handle);

/**
 * @brief Get BMM150 chip ID
 *
 * @param handle Device handle
 * @param chip_id Chip ID pointer
 * @return int8_t BMM150_OK on success, error code otherwise
 */
int8_t bmm150_aux_adapter_get_chip_id(bmm150_aux_handle_t *handle, uint8_t *chip_id);

/**
 * @brief Read BMM150 registers
 *
 * @param handle Device handle
 * @param reg_addr Register address
 * @param data Data buffer
 * @param len Data length
 * @return int8_t BMM150_OK on success, error code otherwise
 */
int8_t bmm150_aux_adapter_read_regs(bmm150_aux_handle_t *handle, uint8_t reg_addr, uint8_t *data, uint32_t len);

/**
 * @brief Write BMM150 registers
 *
 * @param handle Device handle
 * @param reg_addr Register address
 * @param data Data buffer
 * @param len Data length
 * @return int8_t BMM150_OK on success, error code otherwise
 */
int8_t bmm150_aux_adapter_write_regs(bmm150_aux_handle_t *handle, uint8_t reg_addr, const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* BMM150_AUX_ADAPTER_H */
