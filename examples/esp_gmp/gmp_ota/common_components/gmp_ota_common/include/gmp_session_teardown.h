/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "flux_gatt_session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gmp_session_pre_destroy_fn_t)(gatt_session_t *session);

/**
 * Run optional pre-destroy hook and schedule gatt_session destroy on NimBLE host task.
 */
esp_err_t gmp_session_teardown(gatt_session_t *session, gmp_session_pre_destroy_fn_t pre_destroy);

#ifdef __cplusplus
}
#endif
