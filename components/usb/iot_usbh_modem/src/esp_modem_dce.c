/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_modem_dce.h"
#include "esp_modem_dce_command_lib.h"
#include "esp_modem_dce_common_commands.h"
#include "esp_modem_internal.h"
#include "esp_log.h"

static const char *TAG = "esp_modem_dce";

esp_err_t esp_modem_dce_generic_command(esp_modem_dce_t *dce, const char * command, uint32_t timeout, esp_modem_dce_handle_line_t handle_line, void *ctx)
{
    esp_modem_dte_t *dte = dce->dte;
    ESP_LOGD(TAG, "%s(%d): Sending command:%s\n", __func__, __LINE__, command);
    xSemaphoreTake(dte->send_cmd_lock, portMAX_DELAY);
    dce->handle_line = handle_line;
    dce->handle_line_ctx = ctx;
    if (dte->send_cmd(dte, command, timeout) != ESP_OK) {
        xSemaphoreGive(dte->send_cmd_lock);
        ESP_LOGW(TAG, "%s(%d): Command:%s response timeout", __func__, __LINE__, command);
        return ESP_ERR_TIMEOUT;
    }
    if (dce->state == ESP_MODEM_STATE_FAIL) {
        xSemaphoreGive(dte->send_cmd_lock);
        ESP_LOGW(TAG, "%s(%d): Command:%s\n...failed", __func__, __LINE__, command);
        return ESP_FAIL;
    }
    xSemaphoreGive(dte->send_cmd_lock);
    ESP_LOGD(TAG, "%s(%d): Command:%s\n succeeded", __func__, __LINE__, command);
    return ESP_OK;
}

esp_err_t esp_modem_dce_set_params(esp_modem_dce_t *dce, esp_modem_dce_config_t* config)
{
    // save the config
    memcpy(&dce->config, config, sizeof(esp_modem_dce_config_t));
    return ESP_OK;
}

esp_err_t esp_modem_dce_set_apn(esp_modem_dce_t *dce, const char *new_apn)
{
    static char s_apn[64] = "";
    if (strcmp(new_apn, dce->config.pdp_context.apn) == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    strncpy(s_apn, new_apn, sizeof(s_apn) - 1);
    dce->config.pdp_context.apn = s_apn;
    ESP_LOGI(TAG, "New APN = %s, require a restart to effect", dce->config.pdp_context.apn);
    return ESP_OK;
}

esp_err_t esp_modem_dce_default_init(esp_modem_dce_t *dce, esp_modem_dce_config_t* config)
{
    // Check parameters
    ESP_MODEM_ERR_CHECK(dce && config, "dce object or configuration is NULL", err);

    // Set default commands needed for the DCE to operate
    // note: command links will be overwritten if cmd-list enabled
    dce->set_data_mode = esp_modem_dce_set_data_mode;
    dce->resume_data_mode = esp_modem_dce_resume_data_mode;
    dce->set_command_mode = esp_modem_dce_set_command_mode;
    dce->set_pdp_context = esp_modem_dce_set_pdp_context;
    dce->hang_up = esp_modem_dce_hang_up;
    dce->set_echo = esp_modem_dce_set_echo;
    dce->sync = esp_modem_dce_sync;
    dce->set_flow_ctrl = esp_modem_dce_set_flow_ctrl;
    dce->store_profile = esp_modem_dce_store_profile;

    ESP_MODEM_ERR_CHECK(esp_modem_dce_set_params(dce, config) == ESP_OK, "Failed to configure dce object", err);

    // set DCE basic API
    dce->start_up = esp_modem_dce_default_start_up;
    dce->deinit = esp_modem_dce_default_destroy;
    dce->set_working_mode = esp_modem_dce_set_working_mode;

    // initialize the list if enabled
    if (config->populate_command_list) {
        dce->dce_cmd_list = esp_modem_command_list_create();
        ESP_MODEM_ERR_CHECK(dce->dce_cmd_list, "Allocation of dce internal object has failed", err);
    }
    return ESP_OK;
err:
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_modem_dce_default_destroy(esp_modem_dce_t *dce)
{
    ESP_MODEM_ERR_CHECK(esp_modem_command_list_deinit(dce) == ESP_OK, "failed", err);
    free(dce);
    return ESP_OK;
err:
    return ESP_FAIL;
}

esp_err_t esp_modem_dce_handle_response_default(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
    }
    return err;
}

esp_err_t esp_modem_process_command_done(esp_modem_dce_t *dce, esp_modem_state_t state)
{
    dce->state = state;
    return dce->dte->process_cmd_done(dce->dte);
}

static esp_err_t esp_modem_switch_to_command_mode(esp_modem_dce_t *dce)
{
    esp_modem_wait_ms(1000);   // 1s delay for the device to recognize the data escape sequence
    if (dce->set_command_mode(dce, NULL, NULL) != ESP_OK) {
        // exiting data mode could fail if the modem is already in command mode via PPP netif closed
        ESP_MODEM_ERR_CHECK(dce->sync(dce, NULL, NULL) == ESP_OK, "sync after PPP exit failed", err);
    } else {
        // hang-up if exit PPP succeeded
        dce->mode = ESP_MODEM_COMMAND_MODE;
        ESP_MODEM_ERR_CHECK(dce->hang_up(dce, NULL, NULL) == ESP_OK, "hang-up after PPP exit failed", err);
    }
    dce->mode = ESP_MODEM_COMMAND_MODE;
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t esp_modem_switch_to_data_mode(esp_modem_dce_t *dce)
{
    // before going to data mode, set the PDP data context
    ESP_MODEM_ERR_CHECK(dce->set_pdp_context(dce, &dce->config.pdp_context, NULL) == ESP_OK, "setting pdp context failed", err);
    // now set the data mode
    if (dce->set_data_mode(dce, NULL, NULL) != ESP_OK) {
        // Initiate PPP mode could fail, if we've already "dialed" the data call before.
        // in that case we retry to just resume the data mode
        ESP_LOGD(TAG, "set_data_mode, retry with resume_data_mode");
        ESP_MODEM_ERR_CHECK(dce->resume_data_mode(dce, NULL, NULL) == ESP_OK, "Resume data mode failed", err);
    }
    dce->mode = ESP_MODEM_PPP_MODE;
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Set Working Mode
 *
 * @param dce Modem DCE object
 * @param mode working mode
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_dce_set_working_mode(esp_modem_dce_t *dce, esp_modem_mode_t mode)
{
    switch (mode) {
    case ESP_MODEM_COMMAND_MODE:
        ESP_MODEM_ERR_CHECK(esp_modem_switch_to_command_mode(dce) == ESP_OK, "Setting command mode failed", err);
        break;
    case ESP_MODEM_PPP_MODE:
        ESP_MODEM_ERR_CHECK(esp_modem_switch_to_data_mode(dce) == ESP_OK, "Setting data mode failed", err);
        break;
    default:
        ESP_LOGW(TAG, "unsupported working mode: %d", mode);
        goto err;
    }
    return ESP_OK;
err:
    return ESP_FAIL;
}

esp_err_t esp_modem_dce_default_start_up(esp_modem_dce_t *dce)
{
    ESP_MODEM_ERR_CHECK(dce->sync(dce, NULL, NULL) == ESP_OK, "sending sync failed", err);
    ESP_MODEM_ERR_CHECK(dce->set_echo(dce, (void*)false, NULL) == ESP_OK, "set_echo failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}
