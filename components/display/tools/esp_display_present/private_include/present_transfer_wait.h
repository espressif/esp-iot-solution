/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_display_present_frame_tracker.h"

/** Wait until every submitted transport ticket has completed. */
esp_err_t present_transfer_wait_idle(
    esp_display_present_frame_tracker_t *tracker,
    uint32_t timeout_ms);
