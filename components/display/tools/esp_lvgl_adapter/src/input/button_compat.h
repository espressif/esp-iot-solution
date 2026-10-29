/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file button_compat.h
 * @brief Thin helpers for the button component (v4+).
 */

#pragma once

#include "iot_button.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef button_handle_t button_config_or_handle_t;

static inline bool button_compat_is_valid(button_config_or_handle_t handle)
{
    return handle != NULL;
}

static inline esp_err_t button_compat_create(button_config_or_handle_t cfg_or_handle,
                                             button_handle_t *created_handle)
{
    if (!cfg_or_handle || !created_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    *created_handle = cfg_or_handle;
    return ESP_OK;
}

static inline bool button_compat_should_delete(void)
{
    return false;
}

static inline esp_err_t button_compat_register_cb(button_handle_t handle,
                                                  button_event_t event,
                                                  button_cb_t cb,
                                                  void *user_data)
{
    return iot_button_register_cb(handle, event, NULL, cb, user_data);
}

static inline void button_compat_delete(button_handle_t handle)
{
    (void)handle;
}

#ifdef __cplusplus
}
#endif
