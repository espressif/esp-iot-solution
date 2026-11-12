/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "pixel/present_pixel_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logical-to-physical clockwise geometry and its inverse for touch points.
 * width/height are logical dimensions; quarter turns swap physical dimensions.
 *
 * @param width Logical width in pixels.
 * @param height Logical height in pixels.
 * @param rotation Clockwise logical-to-physical orientation.
 * @param area Input rectangle; see the coordinate and ownership contract above.
 * @param out_area Output area.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_display_present_rotate_rect(uint32_t width, uint32_t height, esp_display_present_engine_rotation_t rotation, const esp_display_present_rect_t *area, esp_display_present_rect_t *out_area);

/**
 * @brief Unrotate point.
 *
 * @param width Logical width in pixels.
 * @param height Logical height in pixels.
 * @param rotation Clockwise logical-to-physical orientation.
 * @param physical_x Physical panel X coordinate in pixels.
 * @param physical_y Physical panel Y coordinate in pixels.
 * @param logical_x Receives the logical X coordinate.
 * @param logical_y Receives the logical Y coordinate.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_display_present_unrotate_point(uint32_t width, uint32_t height, esp_display_present_engine_rotation_t rotation, uint32_t physical_x, uint32_t physical_y, uint32_t *logical_x, uint32_t *logical_y);

/**
 * @brief Stateless validated CPU oracle/fallback for platform executors. Caller owns
 * both borrows until return; same validation and overlap rules as submit.
 *
 * @param request Request descriptor, subject to the ownership contract above.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_display_present_pixel_validate_request(const esp_display_present_copy_request_t *request);

/**
 * @brief Pixel execute cpu.
 *
 * @param request Request descriptor, subject to the ownership contract above.
 * @return ESP_OK on success, or an error as specified by the lifecycle contract above.
 */
esp_err_t esp_display_present_pixel_execute_cpu(const esp_display_present_copy_request_t *request);

#ifdef __cplusplus
}
#endif
