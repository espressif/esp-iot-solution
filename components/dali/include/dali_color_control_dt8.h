/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_color_control_dt8.h
 * @brief DALI Part 209 — Color Control (DT8) helpers.
 *
 * Provides DTR pre-load + SET_COLOR + ACTIVATE sequences for CCT,
 * RGB, and RGBWAF color modes.  Depends on dali_system_components.h (transaction
 * primitives) and dali_command.h (Part 209 command codes).
 */

#pragma once

#include "dali_system_components.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Color mode and value types
 * ========================================================================= */

/**
 * @brief Color mode selector passed to dali_master_set_color().
 */
typedef enum {
    DALI_COLOR_CCT    = 0, /*!< Correlated color temperature (Mirek) */
    DALI_COLOR_RGB    = 1, /*!< RGB dimlevels (r, g, b each 0–254)    */
    DALI_COLOR_RGBWAF = 2, /*!< Full 6-channel RGBWAF (0xFF = no change per
                             *   IEC 62386-209 §8.6.19-20)             */
} dali_color_mode_t;

/**
 * @brief Color value union for dali_master_set_color().
 */
typedef union {
    struct {
        uint16_t mirek; /*!< CCT in Mirek (153 ≈ 6500 K … 370 ≈ 2700 K) */
    } cct;
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } rgb;
    struct {
        uint8_t r; /*!< Red channel (0–254, 0xFF = no change) */
        uint8_t g; /*!< Green channel (0–254, 0xFF = no change) */
        uint8_t b; /*!< Blue channel (0–254, 0xFF = no change) */
        uint8_t w; /*!< White channel (0–254, 0xFF = no change) */
        uint8_t a; /*!< Amber channel (0–254, 0xFF = no change) */
        uint8_t f; /*!< Free channel (0–254, 0xFF = no change) */
    } rgbwaf; /*!< RGBWAF 6-channel color (IEC 62386-209) */
} dali_color_val_t;

/* =========================================================================
 * API
 * ========================================================================= */

/**
 * @brief Send ENABLE_DEVICE_TYPE to unlock the next application-extended command.
 *
 * Must be called immediately before any Part 209 command.  DTR pre-loads must
 * be sent *before* this function so the timing window is not consumed.
 *
 * @param[in] handle         Handle from dali_new_master_rmt().
 * @param[in] device_type    Device-type number (8 for DT8).
 * @param[in] tx_timeout_ms  TX timeout (ms).
 */
esp_err_t dali_enable_device_type(dali_master_handle_t handle, uint8_t device_type, int tx_timeout_ms);

/**
 * @brief Set the color of a DALI Part 209 color-control gear.
 *
 * Executes the full DTR pre-load → SET_COLOR → ACTIVATE sequence.
 *
 * @param[in] handle        Handle from dali_new_master_rmt().
 * @param[in] addr_type     Address type for color/activate commands.
 * @param[in] addr          Device address.
 * @param[in] mode          Color mode.
 * @param[in] val           Color value (mirek / r,g,b / r,g,b,w,a,f).
 * @param[in] tx_timeout_ms TX timeout per frame (ms).
 */
esp_err_t dali_master_set_color(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                dali_color_mode_t mode, dali_color_val_t val, int tx_timeout_ms);

#ifdef __cplusplus
}
#endif
