/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_netif_t *ap_netif;
    uint32_t redirect_ip;
    bool configure_dhcp_dns;
} captive_dns_config_t;

esp_err_t captive_dns_start(const captive_dns_config_t *config);
void captive_dns_stop(void);

#ifdef __cplusplus
}
#endif
