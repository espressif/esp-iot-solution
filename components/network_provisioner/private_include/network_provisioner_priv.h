/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <esp_err.h>
#include <protocomm_ext.h>

#include "network_provisioner.h"

#ifdef __cplusplus
extern "C" {
#endif

struct network_provisioner {
    protocomm_ext_t *pc;
    bool owns_pc;
    bool session_started;
    char *version_json;
    network_provisioner_capabilities_t caps;
    bool caps_valid;
};

esp_err_t network_provisioner_send_ep(network_provisioner_t *np, const char *ep_name,
                                      const uint8_t *req, size_t req_len,
                                      uint8_t **resp, size_t *resp_len);

esp_err_t network_provisioner_parse_capabilities(const char *json,
                                                 network_provisioner_capabilities_t *caps);

/** Clear sensitive bytes then free (passphrase / dataset pack buffers). */
static inline void network_provisioner_wipe_free(void *ptr, size_t len)
{
    if (!ptr) {
        return;
    }
    if (len) {
        volatile uint8_t *p = (volatile uint8_t *)ptr;
        while (len--) {
            *p++ = 0;
        }
    }
    free(ptr);
}

#ifdef __cplusplus
}
#endif
