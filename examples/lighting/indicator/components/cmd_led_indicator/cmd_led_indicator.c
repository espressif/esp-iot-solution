/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "cmd_led_indicator.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_check.h"

static struct {
    struct arg_int *start;
    struct arg_int *stop;
    struct arg_int *preempt_start;
    struct arg_int *preempt_stop;
    struct arg_end *end;
} cmd_indicator_args;

typedef struct {
    led_indicator_cmd_cb cmd_cb;
} cmd_led_indicator_cmd_t;

static const char *TAG = "cmd_led_indicator";
static cmd_led_indicator_cmd_t cmd_led_indicator_cmd = {0};

static int cmd_br_fdb_remove(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &cmd_indicator_args); \
    if (nerrors != 0) {
        arg_print_errors(stderr, cmd_indicator_args.end, argv[0]);
        return 1;
    }

    if (cmd_indicator_args.start->count > 0) {
        cmd_led_indicator_cmd.cmd_cb(START, cmd_indicator_args.start->ival[0]);
    } else if (cmd_indicator_args.stop->count > 0) {
        cmd_led_indicator_cmd.cmd_cb(STOP, cmd_indicator_args.stop->ival[0]);
    } else if (cmd_indicator_args.preempt_start->count > 0) {
        cmd_led_indicator_cmd.cmd_cb(PREEMPT_START, cmd_indicator_args.preempt_start->ival[0]);
    } else if (cmd_indicator_args.preempt_stop->count > 0) {
        cmd_led_indicator_cmd.cmd_cb(PREEMPT_STOP, cmd_indicator_args.preempt_stop->ival[0]);
    } else {
        ESP_LOGE(TAG, "no valid arguments");
        return 1;
    }

    return 0;
}

esp_err_t cmd_led_indicator_init(cmd_led_indicator_t *cmd_led_indicator)
{
    ESP_RETURN_ON_FALSE(cmd_led_indicator != NULL, ESP_ERR_INVALID_ARG, TAG, "cmd_led_indicator is NULL");
    ESP_RETURN_ON_FALSE(cmd_led_indicator->cmd_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "cmd_led_indicator->cmd_cb is NULL");

    cmd_led_indicator_cmd.cmd_cb = cmd_led_indicator->cmd_cb;
    uint32_t max_mode = cmd_led_indicator->mode_count - 1;

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    /* Register commands */
    esp_console_register_help_command();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&hw_config, &repl_config, &repl), TAG, "Failed to initialize UART REPL");

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl), TAG, "Failed to initialize USB CDC REPL");

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl), TAG, "Failed to initialize USB serial REPL");

#else
#error Unsupported console type
#endif

    cmd_indicator_args.start = arg_intn("s", "start", "<start>", 0, max_mode, "Start blinking the mode with given index");
    cmd_indicator_args.stop = arg_intn("e", "stop", "<stop>", 0, max_mode, "Stop blinking the mode with given index");
    cmd_indicator_args.preempt_start = arg_intn("p", "preempt_start", "<preempt_start>", 0, max_mode, "Preemptively start blinking the mode with given index");
    cmd_indicator_args.preempt_stop = arg_intn("x", "preempt_stop", "<preempt_stop>", 0, max_mode, "Preemptively stop blinking the mode with given index");
    cmd_indicator_args.end = arg_end(4);

    const esp_console_cmd_t cmd = {
        .command = "led",
        .help = "LED indicator commands",
        .hint = NULL,
        .func = &cmd_br_fdb_remove,
        .argtable = &cmd_indicator_args
    };

    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&cmd), TAG, "Failed to register command");
    ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "Failed to start REPL");

    return ESP_OK;
}
