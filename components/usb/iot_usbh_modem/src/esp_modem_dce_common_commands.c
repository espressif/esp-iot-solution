/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "esp_modem_dce.h"
#include "esp_modem_dce_common_commands.h"

typedef struct common_string_s {
    const char * command;
    char * string;
    size_t len;
} common_string_t;

static inline esp_err_t generic_command_default_handle(esp_modem_dce_t *dce, const char * command)
{
    return esp_modem_dce_generic_command(dce, command, MODEM_COMMAND_TIMEOUT_DEFAULT, esp_modem_dce_handle_response_default, NULL);
}

static esp_err_t common_handle_string(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
        return err;
    }

    if (strstr(line, "+")) {
        common_string_t *result_str = dce->handle_line_ctx;
        assert(result_str->command != NULL && result_str->string != NULL && result_str->len != 0);
        char *result = strstr(line, "+");
        if (result) {
            int len = snprintf(result_str->string, result_str->len, "%s", result);
            if (len > 2) {
                /* Strip "\r\n" */
                strip_cr_lf_tail(result_str->string, len);
            }
        }
        err = ESP_OK;
    }

    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}

/**
 * @brief Handle response from AT+CBC
 */
static esp_err_t esp_modem_dce_common_handle_cbc(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
        return err;
    }

    if (strstr(line, "+CBC")) {
        esp_modem_dce_cbc_ctx_t *cbc = dce->handle_line_ctx;
        /* +CBC: <bcs>,<bcl>,<voltage> */
        sscanf(strstr(line, "+CBC"), "%*s%d,%d,%d", &cbc->bcs, &cbc->bcl, &cbc->battery_status);
        err = ESP_OK;
    }

    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}
/**
 * @brief Handle response from AT+CSQ
 */
static esp_err_t esp_modem_dce_common_handle_csq(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
        return err;
    }

    if (strstr(line, "+CSQ")) {
        /* store value of rssi and ber */
        esp_modem_dce_csq_ctx_t *csq = dce->handle_line_ctx;
        /* +CSQ: <rssi>,<ber> */
        sscanf(strstr(line, "+CSQ"), "%*s%d,%d", &csq->rssi, &csq->ber);
        err = ESP_OK;
    }

    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}

/**
 * @brief Handle response from AT+QPOWD=1
 */
static esp_err_t esp_modem_dce_handle_power_down(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = ESP_OK;
    }

    if (strstr(line, "POWERED DOWN")) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}

/**
 * @brief Handle response from exiting the PPP mode
 */
static esp_err_t esp_modem_dce_handle_exit_data_mode(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_NO_CARRIER)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
    }
    return err;
}

/**
 * @brief Handle response from entry of the PPP mode
 */
static esp_err_t esp_modem_dce_handle_atd_ppp(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_CONNECT)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
    }
    return err;
}

static esp_err_t esp_modem_dce_handle_read_pin(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
        return err;
    }

    if (strstr(line, "READY")) {
        int *ready = (int*)dce->handle_line_ctx;
        *ready = true;
        err = ESP_OK;
    } else if (strstr(line, "PIN") || strstr(line, "PUK")) {
        int *ready = (int*)dce->handle_line_ctx;
        *ready = false;
        err = ESP_OK;
    }

    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}

static esp_err_t esp_modem_dce_handle_reset(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_OK;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = ESP_OK;
    } else if (strstr(line, "PB DONE")) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
    }
    return err;
}

esp_err_t esp_modem_dce_sync(esp_modem_dce_t *dce, void *param, void *result)
{
    return generic_command_default_handle(dce, "AT\r");
}

esp_err_t esp_modem_dce_set_echo(esp_modem_dce_t *dce, void *param, void *result)
{
    bool echo_on = (bool)param;
    if (echo_on) {
        return generic_command_default_handle(dce, "ATE1\r");
    } else {
        return generic_command_default_handle(dce, "ATE0\r");
    }
}

static esp_err_t common_get_operator_after_mode_format(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_FAIL);
        return err;
    }

    if (strstr(line, "+COPS")) {
        common_string_t *result_str = dce->handle_line_ctx;
        assert(result_str->string != NULL && result_str->len != 0);
        /* there might be some random spaces in operator's name, we can not use sscanf to parse the result */
        /* strtok will break the string, we need to create a copy */
        char *line_copy = strdup(strstr(line, "+COPS"));
        /* +COPS: <mode>[, <format>[, <oper>]] */
        char *str_ptr = NULL;
        char *p[6] = {0};
        uint8_t i = 0;
        /* strtok will broke string by replacing delimiter with '\0' */
        p[i] = strtok_r(line_copy, ",", &str_ptr);
        while (p[i] && i < 5) {
            p[++i] = strtok_r(NULL, ",", &str_ptr);
        }
        if (i >= 3) {
            int len = snprintf(result_str->string, result_str->len, "%s", p[2]);
            if (len > 2) {
                /* Strip "\r\n" */
                strip_cr_lf_tail(result_str->string, len);
                err = ESP_OK;
            }
        }
        free(line_copy);
        err = ESP_OK;
    }

    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}

static esp_err_t common_get_common_string(esp_modem_dce_t *dce, void *ctx)
{
    common_string_t * param_str = ctx;
    return esp_modem_dce_generic_command(dce, param_str->command, MODEM_COMMAND_TIMEOUT_DEFAULT, common_handle_string, ctx);
}

esp_err_t esp_modem_dce_get_imei_number(esp_modem_dce_t *dce, void *param, void *result)
{
    common_string_t common_str = { .command = "AT+CGSN?\r", .string = result, .len = (size_t)param };
    return common_get_common_string(dce, &common_str);
}

esp_err_t esp_modem_dce_get_imsi_number(esp_modem_dce_t *dce, void *param, void *result)
{
    common_string_t common_str = { .command = "AT+CIMI?\r", .string = result, .len = (size_t)param };
    return common_get_common_string(dce, &common_str);
}

esp_err_t esp_modem_dce_get_module_name(esp_modem_dce_t *dce, void *param, void *result)
{
    common_string_t common_str = { .command = "AT+CGMM?\r", .string = result, .len = (size_t)param };
    return common_get_common_string(dce, &common_str);
}

esp_err_t esp_modem_dce_get_operator_name(esp_modem_dce_t *dce, void *param, void *result)
{
    common_string_t common_str = { .command = "AT+COPS?\r", .string = result, .len = (size_t)param };
    return esp_modem_dce_generic_command(dce, common_str.command, MODEM_COMMAND_TIMEOUT_OPERATOR,
                                         common_get_operator_after_mode_format, &common_str);
}

esp_err_t esp_modem_dce_reset(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce,  "AT+CRESET\r", MODEM_COMMAND_TIMEOUT_RESET, esp_modem_dce_handle_reset, NULL);
}

esp_err_t esp_modem_dce_set_pin(esp_modem_dce_t *dce, void *param, void *result)
{
    char command[] = "AT+CPIN=0000\r";
    memcpy(command + 8, param, 4); // copy 4 bytes to the "0000" placeholder
    esp_err_t err = esp_modem_dce_generic_command(dce,  command, MODEM_COMMAND_TIMEOUT_DEFAULT, esp_modem_dce_handle_response_default, NULL);
    return err;
}

esp_err_t esp_modem_dce_read_pin(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce,  "AT+CPIN?\r", MODEM_COMMAND_TIMEOUT_DEFAULT, esp_modem_dce_handle_read_pin, result);
}

esp_err_t esp_modem_dce_store_profile(esp_modem_dce_t *dce, void *param, void *result)
{
    return generic_command_default_handle(dce, "AT&W\r");
}

esp_err_t esp_modem_dce_set_flow_ctrl(esp_modem_dce_t *dce, void *param, void *result)
{
    esp_modem_dte_t *dte = dce->dte;
    esp_modem_flow_ctrl_t flow_ctrl = (esp_modem_flow_ctrl_t)param;
    char *command;
    int len = asprintf(&command, "AT+IFC=%d,%d\r", dte->flow_ctrl, flow_ctrl);
    if (len <= 0) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = generic_command_default_handle(dce, command);
    free(command);
    return err;
}

esp_err_t esp_modem_dce_set_pdp_context(esp_modem_dce_t *dce, void *param, void *result)
{
    esp_modem_dce_pdp_ctx_t *pdp = param;
    char *command;
    int len = asprintf(&command, "AT+CGDCONT=%d,\"%s\",\"%s\"\r", pdp->cid, pdp->type, pdp->apn);
    if (len <= 0) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = generic_command_default_handle(dce, command);
    free(command);
    return err;
}

#define MODEM_COMMAND_TIMEOUT_HANG_UP (90000)    /*!< Timeout value for hang up */

esp_err_t esp_modem_dce_hang_up(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "ATH\r", MODEM_COMMAND_TIMEOUT_HANG_UP,
                                         esp_modem_dce_handle_response_default, NULL);
}

esp_err_t esp_modem_dce_get_signal_quality(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "AT+CSQ\r", MODEM_COMMAND_TIMEOUT_DEFAULT,
                                         esp_modem_dce_common_handle_csq, result);
}

esp_err_t esp_modem_dce_get_battery_status(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "AT+CBC\r", MODEM_COMMAND_TIMEOUT_DEFAULT,
                                         esp_modem_dce_common_handle_cbc, result);
}

esp_err_t esp_modem_dce_set_data_mode(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "ATD*99***1#\r", MODEM_COMMAND_TIMEOUT_MODE_CHANGE,
                                         esp_modem_dce_handle_atd_ppp, NULL);
}

esp_err_t esp_modem_dce_resume_data_mode(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "ATO\r", MODEM_COMMAND_TIMEOUT_MODE_CHANGE,
                                         esp_modem_dce_handle_atd_ppp, NULL);
}

esp_err_t esp_modem_dce_set_command_mode(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "+++", MODEM_COMMAND_TIMEOUT_MODE_CHANGE,
                                         esp_modem_dce_handle_exit_data_mode, NULL);
}

esp_err_t esp_modem_dce_power_down(esp_modem_dce_t *dce, void *param, void *result)
{
    return esp_modem_dce_generic_command(dce, "AT+QPOWD=1\r", MODEM_COMMAND_TIMEOUT_POWEROFF,
                                         esp_modem_dce_handle_power_down, NULL);
}

esp_err_t esp_modem_dce_set_baud_temp(esp_modem_dce_t *dce, void *param, void *result)
{
    char command[] = "AT+IPR=3686400\r"; // reserve space with max baud placeholder
    size_t cmd_placeholder_len = strlen(command);
    strncpy(command + 7, param, cmd_placeholder_len - 7); // copy param string to the param
    size_t cmd_len = strlen(command);
    if (cmd_len + 1 >= cmd_placeholder_len) {
        return ESP_FAIL;
    }
    command[cmd_len] = '\r';
    command[cmd_len + 1] = '\0';
    return esp_modem_dce_generic_command(dce, command, MODEM_COMMAND_TIMEOUT_DEFAULT,
                                         esp_modem_dce_handle_response_default, NULL);
}
