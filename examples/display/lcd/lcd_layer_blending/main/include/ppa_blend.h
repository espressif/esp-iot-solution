/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/ppa.h"
#include "esp_lcd_panel_ops.h"
#include "ppa_blend_private.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *buffer;
    uint16_t w;
    uint16_t h;
    ppa_blend_color_mode_t color_mode;
    ppa_alpha_update_mode_t alpha_update_mode;
    uint8_t alpha_value;
} ppa_blend_in_layer_t;

typedef struct {
    ppa_blend_in_layer_t bg_layer;
    ppa_blend_in_layer_t fg_layer;
    ppa_blend_out_layer_t out_layer;
    int task_priority;
    esp_lcd_panel_handle_t display_panel;
} ppa_blend_config_t;

typedef struct {
    uint16_t bg_offset_x;
    uint16_t bg_offset_y;

    uint16_t fg_offset_x;
    uint16_t fg_offset_y;

    uint16_t out_offset_x;
    uint16_t out_offset_y;

    uint16_t w;
    uint16_t h;

    ppa_blend_update_type_t update_type;
} ppa_blend_block_t;

typedef void (*ppa_blend_done_cb_t)(ppa_blend_block_t *block);

/**
 * @brief Initialize a new PPA blend operation.
 *
 * This function initializes and configures a new PPA (Pixel Processing Accelerator) blend operation
 * based on the provided configuration. It checks for valid input, retrieves the data cache line size,
 * registers the PPA client, creates a blend queue, and sets up the blend operation configuration.
 *
 * @param config Pointer to the PPA blend configuration structure.
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 *
 */
esp_err_t ppa_blend_new(ppa_blend_config_t *config);

/**
 * @brief Run a PPA blend operation.
 *
 * This function submits a blend block to the PPA blend queue for processing. It handles the timeout
 * for the queue operation and checks if the PPA blend is enabled before attempting to send the block.
 *
 * @param block Pointer to the blend block to be processed.
 * @param timeout_ms Timeout in milliseconds for the queue send operation. A negative value indicates no timeout.
 *
 * @return esp_err_t ESP_OK on successful submission, ESP_ERR_TIMEOUT if the queue send operation times out,
 *                   or ESP_FAIL if the PPA blend is not enabled.
 */
esp_err_t ppa_blend_run(ppa_blend_block_t *block, int timeout_ms);

/**
 * @brief Run a PPA blend operation in the ISR (Interrupt Service Routine).
 *
 * This function submits a blend block to the PPA blend queue for processing in the ISR. It handles the
 * need to yield from the ISR and checks if the PPA blend is enabled before attempting to send the block.
 *
 * @param block Pointer to the blend block to be processed.
 *
 * @return bool true if the ISR needs to yield, false otherwise.
 */
bool ppa_blend_run_isr(ppa_blend_block_t *block);

/**
 * @brief Set the background layer for PPA blend operation.
 *
 * This function configures the background layer for the PPA blend operation by updating
 * the global blend configuration with the provided background layer parameters.
 *
 * @param bg_layer Pointer to the background layer configuration structure.
 *
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if the input parameter is NULL.
 */
esp_err_t ppa_blend_set_bg_layer(ppa_blend_in_layer_t *bg_layer);

/**
 * @brief Set the foreground layer for PPA blend operation.
 *
 * This function configures the foreground layer for the PPA blend operation by updating
 * the global blend configuration with the provided foreground layer parameters.
 *
 * @param fg_layer Pointer to the foreground layer configuration structure.
 *
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if the input parameter is NULL.
 */
esp_err_t ppa_blend_set_fg_layer(ppa_blend_in_layer_t *fg_layer);

/**
 * @brief Register a callback for PPA blend completion notification.
 *
 * This function registers a user-defined callback function that will be called when the PPA blend operation is done.
 *
 * @param callback The callback function to be registered.
 *
 * @return esp_err_t ESP_OK on successful registration.
 */
esp_err_t ppa_blend_register_notify_done_callback(user_ppa_blend_notify_done_cb_t callback);

#ifdef __cplusplus
}
#endif
