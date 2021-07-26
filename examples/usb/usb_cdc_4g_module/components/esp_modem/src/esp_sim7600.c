// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_modem_dce_common_commands.h"
#include "esp_modem_internal.h"

/**
 * @brief This module supports SIM7600 module, which has a very similar interface
 * to the BG96, so it just references most of the handlers from BG96 and implements
 * only those that differ.
 */
static const char *TAG = "sim7600";

/**
 * @brief Handle response from AT+CBC
 */
static esp_err_t sim7600_handle_cbc(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
    } else if (!strncmp(line, "+CBC", strlen("+CBC"))) {
        esp_modem_dce_cbc_ctx_t *cbc = dce->handle_line_ctx;
        int32_t volts = 0, fraction = 0;
        /* +CBC: <voltage in Volts> V*/
        sscanf(line, "+CBC: %d.%dV", &volts, &fraction);
        /* Since the "read_battery_status()" API (besides voltage) returns also values for BCS, BCL (charge status),
         * which are not applicable to this modem, we return -1 to indicate invalid value
         */
        cbc->bcs = -1; // BCS
        cbc->bcl = -1; // BCL
        cbc->battery_status = volts*1000 + fraction;
        err = ESP_OK;
    }
    return err;
}

/**
 * @brief Get battery status
 *
 * @param dce Modem DCE object
 * @param bcs Battery charge status
 * @param bcl Battery connection level
 * @param voltage Battery voltage
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t sim7600_get_battery_status(esp_modem_dce_t *dce, void *p, void *r)
{
    return esp_modem_dce_generic_command(dce, "AT+CBC\r", 20000,
                                         sim7600_handle_cbc, r);
}

static esp_err_t sim7600_handle_power_down(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_OK;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_NO_CARRIER)) {
        err = ESP_OK;
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
    }
    return err;
}

static esp_err_t sim7600_power_down(esp_modem_dce_t *dce, void *p, void *r)
{
    return esp_modem_dce_generic_command(dce, " AT+CPOF\r", MODEM_COMMAND_TIMEOUT_POWEROFF,
                                         sim7600_handle_power_down, NULL);
}

esp_err_t esp_modem_sim7600_specific_init(esp_modem_dce_t *dce)
{
    ESP_MODEM_ERR_CHECK(dce, "failed to specific init with zero dce", err_params);
    if (dce->config.populate_command_list) {
        ESP_MODEM_ERR_CHECK(esp_modem_set_default_command_list(dce) == ESP_OK, "esp_modem_dce_set_default_commands failed", err);

        /* Update some commands which differ from the defaults */
        ESP_MODEM_ERR_CHECK(esp_modem_command_list_set_cmd(dce, "power_down", sim7600_power_down) == ESP_OK, "esp_modem_dce_set_command failed", err);
        ESP_MODEM_ERR_CHECK(
                esp_modem_command_list_set_cmd(dce, "get_battery_status", sim7600_get_battery_status) == ESP_OK, "esp_modem_dce_set_command failed", err);
    }
    return ESP_OK;
err:
    return ESP_FAIL;
err_params:
    return ESP_ERR_INVALID_ARG;
}
