/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_HTTP_OUTPUT_BUFFER      2048

esp_err_t http_rest_with_url(char *district_id, char *text, char *wind_class,
                             char *wind_dir, char *uptime, char *week, int *high, int *low);
#ifdef __cplusplus
}
#endif
