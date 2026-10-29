/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "at_3gpp_ts_27_007.h"
#include "../../priv_include/modem_at_internal.h"
#include "ym310.h"

static const char *TAG = "4G YM310";

esp_err_t at_cmd_ym310_set_usb(at_handle_t ath, void *param, void *result)
{
    char str[32];
    ym310_usb_mode_t *usbmode = (ym310_usb_mode_t*)param;
    snprintf(str, sizeof(str), "AT+ECPCFG=\"usbNet\",%d", *usbmode);
    return at_send_command_response_ok(ath, str);
}

static bool _handle_power_down(at_handle_t ath, const char *line)
{
    if (strstr(line, "POWER DOWN")) {
        ESP_LOGI(TAG, "Power down successfully: (%s)", line);
        return true;
    }
    if (strstr(line, AT_RESULT_CODE_ERROR)) {
        ESP_LOGE(TAG, "Failed to power down");
        return true;
    }

    return false;
}

esp_err_t at_cmd_ym310_power_down(at_handle_t ath, void *param, void *result)
{
    return modem_at_send_command(ath, "AT+CPOWD", MODEM_COMMAND_TIMEOUT_DEFAULT, _handle_power_down, NULL);
}
