/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_tag_events_handle(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);

#ifdef __cplusplus
}
#endif
