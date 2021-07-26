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
#include "esp_log.h"

#include "esp_modem.h"
#include "esp_modem_dte.h"
#include "esp_modem_dce.h"
#include "esp_modem_compat.h"
#include "esp_modem_dce_common_commands.h"

/**
 * @brief Error check macro
 *
 */
#define ESP_MODEM_COMPAT_CHECK(a, str, goto_tag, ...)                                 \
    do                                                                                \
    {                                                                                 \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);     \
            goto goto_tag;                                                            \
        }                                                                             \
    } while (0)

static const char *TAG = "esp-modem-compat";

/**
 * @note Below are the backward compatible functions defined using esp_modem_dce framework
 *
 */
static esp_err_t compat_hang_up(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_dce->hang_up(esp_dce, NULL, NULL);
}

static esp_err_t compat_echo(modem_dce_t *dce, bool on)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_dce->set_echo(esp_dce, (void*)on, NULL);
}

static esp_err_t compat_define_pdp_context(modem_dce_t *dce, uint32_t cid, const char *type, const char *apn)
{
    esp_modem_dce_pdp_ctx_t pdp = { .type = type, .cid = cid, .apn = apn };
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_dce->set_pdp_context(esp_dce, &pdp, NULL);
}

static esp_err_t compat_sync(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_dce->sync(esp_dce, NULL, NULL);
}

static esp_err_t compat_set_flow_ctrl(modem_dce_t *dce, esp_modem_flow_ctrl_t flow_ctrl)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_dce->set_flow_ctrl(esp_dce, (void*)flow_ctrl, NULL);
}

static esp_err_t compat_store_profile(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_dce->store_profile(esp_dce, NULL, NULL);
}

static esp_err_t compat_get_signal_quality(modem_dce_t *dce, uint32_t *rssi, uint32_t *ber)
{
    esp_modem_dce_csq_ctx_t result;
    esp_modem_dce_t* esp_dce = &dce->parent;
    esp_err_t err = esp_modem_command_list_run(esp_dce, "get_signal_quality", NULL, &result);
    if (err == ESP_OK) {
        *rssi = result.rssi;
        *ber = result.ber;
    }
    return err;
}

static esp_err_t compat_get_battery_status(modem_dce_t *dce, uint32_t *bcs, uint32_t *bcl, uint32_t *voltage)
{
    esp_modem_dce_cbc_ctx_t result;
    esp_modem_dce_t* esp_dce = &dce->parent;
    esp_err_t err = esp_modem_command_list_run(esp_dce, "get_battery_status", NULL, &result);
    if (err == ESP_OK) {
        *bcs = result.bcs;
        *bcl = result.bcl;
        *voltage = result.battery_status;
    }
    return err;
}

static esp_err_t compat_set_working_mode(modem_dce_t *dce, esp_modem_mode_t mode)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_modem_dce_set_working_mode(esp_dce, mode);
}

static esp_err_t compat_get_module_name(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_modem_command_list_run(esp_dce, "get_module_name", (void *) sizeof(dce->name), dce->name);
}

static esp_err_t compat_get_imei_number(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_modem_command_list_run(esp_dce, "get_imei_number", (void *) sizeof(dce->imei), dce->imei);
}

static esp_err_t compat_get_operator_name(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_modem_command_list_run(esp_dce, "get_operator_name", (void *) sizeof(dce->oper), dce->oper);
}

static esp_err_t compat_get_imsi_number(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_modem_command_list_run(esp_dce, "get_imsi_number", (void *) sizeof(dce->imsi), dce->imsi);
}

static esp_err_t compat_power_down(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    return esp_modem_command_list_run(esp_dce, "power_down", (void *) sizeof(dce->imsi), dce->imsi);
}

/**
 * @brief Compatibility deinitialize
 *
 * @param dce Modem DCE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on fail
 */
static esp_err_t compat_deinit(modem_dce_t *dce)
{
    esp_modem_dce_t* esp_dce = &dce->parent;
    esp_err_t err = esp_modem_command_list_deinit(esp_dce);
    if (err == ESP_OK) {
        free(dce);
    }
    return err;
}


/**
 * @brief Compatibility init
 *
 */
static modem_dce_t *esp_modem_compat_init(modem_dte_t *dte, esp_modem_dce_device_t device)
{
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_MODEM_PPP_APN);
    dce_config.device = device;
    dce_config.populate_command_list = true;
    modem_dce_t *dce = calloc(1, sizeof(modem_dce_t));
    if (dce == NULL) {
        return NULL;
    }

    if (esp_modem_dce_init(&dce->parent, &dce_config) != ESP_OK) {
        free(dce);
        return NULL;
    }

    dce->sync = compat_sync;
    dce->echo_mode = compat_echo;
    dce->store_profile = compat_store_profile;
    dce->set_flow_ctrl = compat_set_flow_ctrl;
    dce->define_pdp_context = compat_define_pdp_context;
    dce->hang_up = compat_hang_up;
    dce->get_signal_quality = compat_get_signal_quality;
    dce->get_battery_status = compat_get_battery_status;
    dce->set_working_mode = compat_set_working_mode;
    dce->power_down = compat_power_down;
    dce->deinit = compat_deinit;
    esp_modem_dce_t* esp_dce = &dce->parent;
    /* Bind DTE with DCE */
    esp_dce->dte = dte;
    dte->dce = esp_dce;
    /* All units initialized, notify the modem before sending commands */
    ESP_MODEM_COMPAT_CHECK(esp_modem_notify_initialized(dte) == ESP_OK, "modem start notification failed", err);

    ESP_MODEM_COMPAT_CHECK(compat_sync(dce) == ESP_OK, "sync failed", err);
    /* Close echo */
    ESP_MODEM_COMPAT_CHECK(compat_echo(dce, false) == ESP_OK, "close echo mode failed", err);


    bool ready;
    ESP_ERROR_CHECK(esp_modem_command_list_run(esp_dce, "read_pin", NULL, &ready));
    if (!ready) {
        ESP_LOGE(TAG, "PIN not ready man");
        ESP_ERROR_CHECK(esp_modem_command_list_run(esp_dce, "set_pin", "1234", NULL));
    }

    /* Get Module name */
    ESP_MODEM_COMPAT_CHECK(compat_get_module_name(dce) == ESP_OK, "get module name failed", err);
    /* Get IMEI number */
    ESP_MODEM_COMPAT_CHECK(compat_get_imei_number(dce) == ESP_OK, "get imei number failed", err);
    /* Get IMSI number */
    ESP_MODEM_COMPAT_CHECK(compat_get_imsi_number(dce) == ESP_OK, "get imsi number failed", err);
    /* Get operator name */
    ESP_MODEM_COMPAT_CHECK(compat_get_operator_name(dce) == ESP_OK, "get operator name failed", err);
    return dce;
err:
    return NULL;
}

/**
 * @brief Legacy init of SIM800 module
 *
 */
modem_dce_t *sim800_init(modem_dte_t *dte)
{
    return esp_modem_compat_init(dte, ESP_MODEM_DEVICE_SIM800);
}

/**
 * @brief Legacy init of BG96 module
 *
 */
modem_dce_t *bg96_init(modem_dte_t *dte)
{
    return esp_modem_compat_init(dte, ESP_MODEM_DEVICE_BG96);
}

/**
 * @brief Legacy init of SIM7600 module
 *
 */
modem_dce_t *sim7600_init(modem_dte_t *dte)
{
    return esp_modem_compat_init(dte, ESP_MODEM_DEVICE_SIM7600);
}
