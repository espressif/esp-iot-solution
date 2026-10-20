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

typedef struct tps55288_dev_t *tps55288_handle_t;

/**
 * @brief Internal feedback ratio
 *
 * Zero selects the chip reset value, 0.0564.
 * VOUT = 0.8 V + REF * step.
 */
typedef enum {
    TPS55288_FB_RATIO_DEFAULT = 0, /*!< Ratio 0.0564. The step is 20 mV and the range is 800–21260 mV. */
    TPS55288_FB_RATIO_0_0564,      /*!< Ratio 0.0564. The step is 20 mV and the range is 800–21260 mV. */
    TPS55288_FB_RATIO_0_0752,      /*!< Ratio 0.0752. The step is 15 mV and the range is 800–16145 mV. */
    TPS55288_FB_RATIO_0_1128,      /*!< Ratio 0.1128. The step is 10 mV and the range is 800–11030 mV. */
    TPS55288_FB_RATIO_0_2256,      /*!< Ratio 0.2256. The step is 5 mV and the range is 800–5915 mV. */
} tps55288_fb_ratio_t;

/**
 * @brief TPS55288 configuration
 */
typedef struct {
    i2c_master_bus_handle_t bus;   /*!< I2C master bus handle. */
    uint32_t scl_speed_hz;         /*!< SCL frequency, in hertz. */
    uint16_t i2c_addr;             /*!< 7-bit I2C address, 0x74 or 0x75. */
    uint32_t sense_resistor_mohm;  /*!< Resistance of the resistor soldered between ISP and ISN, in milliohms. Use that part's value. Zero is invalid, because the board chooses the resistor. */
    tps55288_fb_ratio_t fb_ratio;  /*!< Internal feedback ratio. Zero selects the default ratio 0.0564. */
} tps55288_config_t;

/**
 * @brief Create a TPS55288 device
 *
 * The 7-bit address is 0x74 or 0x75, selected by the MODE pin.
 * EN/UVLO must be high before the device answers.
 * This call selects internal feedback and programs @p config->fb_ratio.
 *
 * @param config Configuration
 * @param[out] ret_handle Created device handle
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p config or @p ret_handle is NULL, @p sense_resistor_mohm is 0, or @p fb_ratio is not a supported value
 *      - ESP_ERR_NO_MEM: Out of memory
 *      - Others: I2C communication failed
 */
esp_err_t tps55288_create(const tps55288_config_t *config, tps55288_handle_t *ret_handle);

/**
 * @brief Delete a TPS55288 device
 *
 * @param handle Device handle from tps55288_create()
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p handle is NULL
 *      - Others: Failed to remove the I2C device
 */
esp_err_t tps55288_delete(tps55288_handle_t handle);

/**
 * @brief Set the output voltage
 *
 * The range and step come from the feedback ratio passed to tps55288_create().
 * A value that is not a multiple of the step is truncated toward 800 mV.
 * Writing the target does not enable the converter.
 * Call tps55288_enable_output() to turn the output on.
 *
 * @param handle Device handle from tps55288_create()
 * @param vout_mv Target output voltage, in millivolts
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p handle is NULL, or @p vout_mv is outside the range of the selected ratio
 *      - Others: I2C communication failed
 */
esp_err_t tps55288_set_vout_mv(tps55288_handle_t handle, uint32_t vout_mv);

/**
 * @brief Set and enable the average output current limit
 *
 * The requested current is rounded to the nearest IOUT_LIMIT code.
 * I(mA) = code * 500 / R(mohm), where R is @p sense_resistor_mohm.
 * The maximum is 127 * 500 / R(mohm).
 *
 * @param handle Device handle from tps55288_create()
 * @param limit_ma Output current limit, in milliamps
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p handle is NULL, or @p limit_ma is above the maximum for this sense resistor
 *      - Others: I2C communication failed
 */
esp_err_t tps55288_set_iout_limit_ma(tps55288_handle_t handle, uint32_t limit_ma);

/**
 * @brief Enable or disable the converter output
 *
 * Sets the MODE register OE bit.
 * The other MODE bits are left unchanged.
 *
 * @param handle Device handle from tps55288_create()
 * @param enable true to enable the output, false to disable it
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p handle is NULL
 *      - Others: I2C communication failed
 */
esp_err_t tps55288_enable_output(tps55288_handle_t handle, bool enable);

/**
 * @brief Read the STATUS register
 *
 * Reading STATUS clears the latched fault flags.
 *
 * @param handle Device handle from tps55288_create()
 * @param[out] status STATUS register value
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p handle or @p status is NULL
 *      - Others: I2C communication failed
 */
esp_err_t tps55288_read_status(tps55288_handle_t handle, uint8_t *status);

/**
 * @brief Read one TPS55288 register
 *
 * @param handle Device handle from tps55288_create()
 * @param reg Register address, 0x00–0x07
 * @param[out] data Register value
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p handle or @p data is NULL
 *      - Others: I2C communication failed
 */
esp_err_t tps55288_read_reg(tps55288_handle_t handle, uint8_t reg, uint8_t *data);

/**
 * @brief Write one TPS55288 register
 *
 * @param handle Device handle from tps55288_create()
 * @param reg Register address, 0x00–0x07
 * @param data Value to write
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: @p handle is NULL
 *      - Others: I2C communication failed
 */
esp_err_t tps55288_write_reg(tps55288_handle_t handle, uint8_t reg, uint8_t data);

#ifdef __cplusplus
}
#endif
