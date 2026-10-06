/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

/** Pipeline frame commit / barrier stage. */
typedef enum {
    ESP_DISPLAY_PRESENT_FRAME_COMMIT_WAIT_INFLIGHT = 0,
    ESP_DISPLAY_PRESENT_FRAME_COMMIT_TE_PUSH,
    ESP_DISPLAY_PRESENT_FRAME_COMMIT_SWITCH,
} esp_display_present_frame_commit_policy_t;

/** Runtime endpoint selection and draw-buffer allocation for the selected mode. */
typedef struct {
    esp_display_present_frame_commit_policy_t commit;
    uint8_t slot_count;
    uint16_t slot_lines;
} esp_display_present_pipeline_policy_t;
