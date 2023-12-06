/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sys/queue.h>
#include <string.h>
#include "esp_log.h"
#include "esp_modem_dce_command_lib.h"
#include "esp_modem_internal.h"
#include "esp_modem_dce_common_commands.h"

static const char *TAG = "esp_modem_command_lib";

typedef struct cmd_item_s cmd_item_t;

/**
 * struct for one item in command list
 */
struct cmd_item_s {
    const char *command;            //!< command name
    dce_command_t function;         //!< function pointer
    SLIST_ENTRY(cmd_item_s) next;   //!< next command in the list
};

/**
 * private struct defined for dce internal object
 */
struct esp_modem_dce_cmd_list {
    SLIST_HEAD(cmd_list_, cmd_item_s) command_list;
};

/**
 * @brief List of common AT commands in the library
 *
 */
static const cmd_item_t s_command_list[] = {
    { .command = "sync", .function = esp_modem_dce_sync },
    { .command = "get_imei_number", .function = esp_modem_dce_get_imei_number },
    { .command = "get_imsi_number", .function = esp_modem_dce_get_imsi_number },
    { .command = "get_module_name", .function = esp_modem_dce_get_module_name },
    { .command = "get_operator_name", .function = esp_modem_dce_get_operator_name },
    { .command = "set_echo", .function = esp_modem_dce_set_echo },
    { .command = "store_profile", .function = esp_modem_dce_store_profile },
    { .command = "set_flow_ctrl", .function = esp_modem_dce_set_flow_ctrl },
    { .command = "set_pdp_context", .function = esp_modem_dce_set_pdp_context },
    { .command = "hang_up", .function = esp_modem_dce_hang_up },
    { .command = "get_signal_quality", .function = esp_modem_dce_get_signal_quality },
    { .command = "set_data_mode", .function = esp_modem_dce_set_data_mode },
    { .command = "resume_data_mode", .function = esp_modem_dce_resume_data_mode },
    { .command = "set_command_mode", .function = esp_modem_dce_set_command_mode },
    { .command = "get_battery_status", .function = esp_modem_dce_get_battery_status },
    { .command = "power_down", .function = esp_modem_dce_power_down },
    { .command = "reset", .function = esp_modem_dce_reset },
    { .command = "set_pin", .function = esp_modem_dce_set_pin },
    { .command = "read_pin", .function = esp_modem_dce_read_pin },
    { .command = "set_baud", .function = esp_modem_dce_set_baud_temp },
};

static esp_err_t update_internal_command_refs(esp_modem_dce_t *dce)
{
    ESP_MODEM_ERR_CHECK(dce->set_data_mode = esp_modem_dce_find_command(dce, "set_data_mode"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->resume_data_mode = esp_modem_dce_find_command(dce, "resume_data_mode"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->set_command_mode = esp_modem_dce_find_command(dce, "set_command_mode"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->set_pdp_context = esp_modem_dce_find_command(dce, "set_pdp_context"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->hang_up = esp_modem_dce_find_command(dce, "hang_up"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->set_echo = esp_modem_dce_find_command(dce, "set_echo"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->sync = esp_modem_dce_find_command(dce, "sync"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->set_flow_ctrl = esp_modem_dce_find_command(dce, "set_flow_ctrl"), "cmd not found", err);
    ESP_MODEM_ERR_CHECK(dce->store_profile = esp_modem_dce_find_command(dce, "store_profile"), "cmd not found", err);

    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t esp_modem_dce_init_command_list(esp_modem_dce_t *dce, size_t commands, const cmd_item_t *command_list)
{
    if (commands < 1 || command_list == NULL || dce->dce_cmd_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    SLIST_INIT(&dce->dce_cmd_list->command_list);

    for (int i = 0; i < commands; ++i) {
        cmd_item_t *new_item = calloc(1, sizeof(struct cmd_item_s));
        new_item->command = command_list[i].command;
        new_item->function = command_list[i].function;
        SLIST_INSERT_HEAD(&dce->dce_cmd_list->command_list, new_item, next);
    }
    return ESP_OK;
}

esp_err_t esp_modem_set_default_command_list(esp_modem_dce_t *dce)
{
    esp_err_t err = esp_modem_dce_init_command_list(dce, sizeof(s_command_list) / sizeof(cmd_item_t), s_command_list);
    if (err == ESP_OK) {
        return update_internal_command_refs(dce);
    }
    return err;

}

esp_err_t esp_modem_command_list_run(esp_modem_dce_t *dce, const char * command, void * param, void* result)
{
    if (dce == NULL || dce->dce_cmd_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cmd_item_t *item;
    SLIST_FOREACH(item, &dce->dce_cmd_list->command_list, next) {
        if (strcmp(item->command, command) == 0) {
            return item->function(dce, param, result);
        }
    }
    return ESP_ERR_NOT_FOUND;
}

dce_command_t esp_modem_dce_find_command(esp_modem_dce_t *dce, const char * command)
{
    if (dce == NULL || dce->dce_cmd_list == NULL) {
        return NULL;
    }

    cmd_item_t *item;
    SLIST_FOREACH(item, &dce->dce_cmd_list->command_list, next) {
        if (strcmp(item->command, command) == 0) {
            return item->function;
        }
    }
    return NULL;
}

esp_err_t esp_modem_dce_delete_all_commands(esp_modem_dce_t *dce)
{
    if (dce->dce_cmd_list) {
        while (!SLIST_EMPTY(&dce->dce_cmd_list->command_list)) {
            cmd_item_t *item = SLIST_FIRST(&dce->dce_cmd_list->command_list);
            SLIST_REMOVE_HEAD(&dce->dce_cmd_list->command_list, next);
            free(item);
        }
    }
    return ESP_OK;
}

esp_err_t esp_modem_dce_delete_command(esp_modem_dce_t *dce, const char * command_id)
{
    cmd_item_t *item;
    SLIST_FOREACH(item, &dce->dce_cmd_list->command_list, next) {
        if (strcmp(item->command, command_id) == 0) {
            SLIST_REMOVE(&dce->dce_cmd_list->command_list, item, cmd_item_s, next);
            free(item);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_modem_command_list_set_cmd(esp_modem_dce_t *dce, const char * command_id, dce_command_t command)
{
    if (dce == NULL || dce->dce_cmd_list == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cmd_item_t *item;
    SLIST_FOREACH(item, &dce->dce_cmd_list->command_list, next) {
        if (strcmp(item->command, command_id) == 0) {
            item->function = command;
            return update_internal_command_refs(dce);
        }
    }
    cmd_item_t *new_item = calloc(1, sizeof(struct cmd_item_s));
    new_item->command = command_id;
    new_item->function = command;
    SLIST_INSERT_HEAD(&dce->dce_cmd_list->command_list, new_item, next);
    return update_internal_command_refs(dce);;

}

struct esp_modem_dce_cmd_list* esp_modem_command_list_create(void)
{
    return calloc(1, sizeof(struct esp_modem_dce_cmd_list));
}

esp_err_t esp_modem_command_list_deinit(esp_modem_dce_t *dce)
{
    if (dce->dte) {
        dce->dte->dce = NULL;
    }
    esp_modem_dce_delete_all_commands(dce);
    free(dce->dce_cmd_list);
    return ESP_OK;
}
