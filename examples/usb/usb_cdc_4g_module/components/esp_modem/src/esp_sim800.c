// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
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

static const char *TAG = "esp_sim800";

/**
 * @brief @brief Response to the SIM800 specific power-down command
 */
static esp_err_t sim800_handle_power_down(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, "POWER DOWN")) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}

/**
 * @brief Response to the SIM800 specific data mode command
 *
 */
static esp_err_t sim800_handle_atd_ppp(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_CONNECT)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
    }
    return err;
}

/**
 * @brief Set data mode specific to SIM800
 *
 */
static esp_err_t sim800_set_data_mode(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "ATD*99##\r", MODEM_COMMAND_TIMEOUT_MODE_CHANGE,
                                         sim800_handle_atd_ppp, NULL);

}

/**
 * @brief Specific power down command to SMI800
 *
 */
static esp_err_t sim800_power_down(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "AT+CPOWD=1\r", MODEM_COMMAND_TIMEOUT_POWEROFF,
                                         sim800_handle_power_down, NULL);
}


static esp_err_t sim800_start_up(esp_modem_dce_t* dce)
{
    if (esp_modem_dce_default_start_up(dce) != ESP_OK) {
        esp_modem_wait_ms(30000); // SIM800 wakes-up 30s after sending a command
    }
    return esp_modem_dce_default_start_up(dce);
}


esp_err_t esp_modem_sim800_specific_init(esp_modem_dce_t *dce)
{
    ESP_MODEM_ERR_CHECK(dce, "failed to specific init with zero dce", err_params);
    /* Update some commands which differ from the defaults */
    if (dce->config.populate_command_list) {
        ESP_MODEM_ERR_CHECK(esp_modem_command_list_set_cmd(dce, "set_data_mode", sim800_set_data_mode) == ESP_OK, "esp_modem_dce_set_command failed", err);
        ESP_MODEM_ERR_CHECK(esp_modem_command_list_set_cmd(dce, "power_down", sim800_power_down) == ESP_OK, "esp_modem_dce_set_command failed", err);
    } else {
        dce->set_data_mode = sim800_set_data_mode;
    }
    dce->start_up = sim800_start_up;
    return ESP_OK;
err:
    return ESP_FAIL;
err_params:
    return ESP_ERR_INVALID_ARG;
}
