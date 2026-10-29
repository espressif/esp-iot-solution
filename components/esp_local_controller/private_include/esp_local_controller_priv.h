/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>
#include <protocomm_ext.h>

#include "esp_local_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

struct esp_local_controller {
    protocomm_ext_t *pc;
    bool session_started;
    char *version_json;
    esp_local_controller_version_t version;
    bool version_valid;
};

/**
 * @brief Send a raw request on a named endpoint (session must be started)
 *
 * Used internally and by host unit tests.
 */
esp_err_t esp_local_controller_send_ep(esp_local_controller_t *ctrl, const char *ep_name,
                                       const uint8_t *req, size_t req_len,
                                       uint8_t **resp, size_t *resp_len);

#ifdef __cplusplus
}
#endif
