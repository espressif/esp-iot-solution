/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "at_3gpp_ts_27_007.h"
#include "../../priv_include/modem_at_internal.h"
#include "ml302.h"

static const char *TAG = "4G ML302";

esp_err_t at_cmd_ml302_power_down(at_handle_t ath, void *param, void *result)
{
    ESP_LOGW(TAG, "Modem ML302 is powering down...");
    return at_send_command_response_ok(ath, "AT+CPOF");
}

esp_err_t at_cmd_ml302_set_usb(at_handle_t ath, void *param, void *result)
{
    char str[32];
    ml302_usb_mode_t *usbmode = (ml302_usb_mode_t*)param;
    snprintf(str, sizeof(str), "AT+SYSNV=1,\"usbmode\",%d", *usbmode);
    return at_send_command_response_ok(ath, str);
}
