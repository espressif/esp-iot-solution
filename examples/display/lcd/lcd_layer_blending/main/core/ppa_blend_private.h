/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "ppa_blend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PPA_BLEND_UPDATE_BG = 0,
    PPA_BLEND_UPDATE_FG,
} ppa_blend_update_type_t;

typedef struct {
    void *buffer;
    size_t buffer_size;
    uint16_t w;
    uint16_t h;
    ppa_blend_color_mode_t color_mode;
} ppa_blend_out_layer_t;

typedef void (*user_ppa_blend_notify_done_cb_t)(void);

/**
 * @brief Set the output layer for PPA blend operation.
 *
 * This function configures the output layer for the PPA blend operation by updating
 * the global blend configuration with the provided output layer parameters.
 *
 * @param out_layer Pointer to the output layer configuration structure.
 *
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if the input parameter is NULL.
 */
esp_err_t ppa_blend_set_out_layer(ppa_blend_out_layer_t *out_layer);

/**
 * @brief Register a callback function for PPA blend operation completion.
 *
 * This function registers the provided callback function to be called when the PPA blend operation is complete.
 *
 * @param callback Pointer to the callback function.
 *
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if the input parameter is NULL.
 */
void ppa_blend_check_done(uint32_t timeout);

/**
 * @brief Enable or disable the PPA blend operation.
 *
 * This function enables or disables the PPA blend operation based on the provided flag.
 * It also updates the global blend configuration with the new blend operation status.
 *
 * @param flag Boolean value indicating whether to enable (true) or disable (false) the PPA blend operation.
 */
void ppa_blend_enable(bool flag);

/**
 * @brief Enables or disables the user data PPA blend feature.
 *
 * This function sets a flag to control whether the PPA (Post-Processing Accelerator)
 * blend operation uses user-defined data. When enabled, the blend operation will
 * prioritize user-defined blending logic, bypassing the default pipeline.
 *
 * @param flag A boolean value indicating whether to enable or disable the user data PPA blend.
 *             - `true`: Enables user-defined PPA blending.
 *             - `false`: Disables user-defined PPA blending.
 *
 * This flag influences the behavior of the `ppa_blend_run` function.
 */
void ppa_blend_user_data_enable(bool flag);

/**
 * @brief Checks whether the user data PPA blend feature is enabled.
 *
 * This function returns the status of the user-defined PPA (Post-Processing Accelerator)
 * blend feature. It indicates whether the blend operation is currently configured
 * to use user-defined data.
 *
 * @return `true` if user-defined PPA blending is enabled, `false` otherwise.
 */
bool ppa_blend_user_data_is_enabled(void);

/**
 * @brief Checks whether the PPA blend feature is globally enabled.
 *
 * This function returns the status of the global PPA (Post-Processing Accelerator)
 * blend feature. It indicates whether blending operations are allowed to proceed.
 *
 * @return `true` if PPA blending is enabled, `false` otherwise.
 */
bool ppa_blend_is_enabled(void);

#ifdef __cplusplus
}
#endif
