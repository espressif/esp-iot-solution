/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gmp_session_teardown.h"

esp_err_t gmp_session_teardown(gatt_session_t *session, gmp_session_pre_destroy_fn_t pre_destroy)
{
    if (!session) {
        return ESP_ERR_INVALID_ARG;
    }

    if (pre_destroy) {
        pre_destroy(session);
    }

    return gatt_session_schedule_destroy(session);
}
