/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_err.h"
#include "esp_gmp_ft.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "file_transfer_example_common.h"
#include "file_transfer_example_internal.h"

static const char *TAG = "ft_console";
static esp_console_repl_t *s_repl;

static const char *state_name(esp_file_transfer_state_t state)
{
    switch (state) {
    case ESP_FILE_TRANSFER_STATE_IDLE:
        return "IDLE";
    case ESP_FILE_TRANSFER_STATE_PREPARING:
        return "PREPARING";
    case ESP_FILE_TRANSFER_STATE_WAIT_META_RSP:
        return "WAIT_META_RSP";
    case ESP_FILE_TRANSFER_STATE_WAIT_DATA_BLOCK:
        return "WAIT_DATA_BLOCK";
    case ESP_FILE_TRANSFER_STATE_SENDING_DATA:
        return "SENDING_DATA";
    case ESP_FILE_TRANSFER_STATE_WAIT_FINAL_CONFIRM:
        return "WAIT_FINAL_CONFIRM";
    case ESP_FILE_TRANSFER_STATE_FINALIZING:
        return "FINALIZING";
    case ESP_FILE_TRANSFER_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

static const char *role_name(esp_file_transfer_role_t role)
{
    switch (role) {
    case ESP_FILE_TRANSFER_ROLE_SENDER:
        return "sender";
    case ESP_FILE_TRANSFER_ROLE_RECEIVER:
        return "receiver";
    default:
        return "none";
    }
}

static void print_error(const char *operation, esp_err_t err)
{
    ESP_LOGE(TAG, "%s failed: err=0x%x (%s)", operation, (unsigned)err,
             esp_err_to_name(err));
}

static int command_send(const char *path)
{
    const esp_file_transfer_send_param_t param = {
        .src_path = path,
        .remote_name = NULL,
    };
    esp_err_t err = esp_file_transfer_send(&param);
    if (err != ESP_OK) {
        print_error("Send", err);
        return 1;
    }
    ESP_LOGI(TAG, "Send accepted: %s", path);
    return 0;
}

static int command_status(void)
{
    esp_file_transfer_status_t status;
    esp_err_t err = esp_file_transfer_get_status(&status);
    if (err != ESP_OK) {
        print_error("Status", err);
        return 1;
    }
    if (!status.in_progress) {
        ESP_LOGI(TAG, "Status: idle");
        return 0;
    }

    ESP_LOGI(TAG,
             "Status: %s, %s, %u%% (%" PRIu64 "/%" PRIu64 "), state=%s",
             role_name(status.role), status.file_name, status.percent,
             status.bytes_transferred, status.total_bytes,
             state_name(status.state));
    return 0;
}

static int command_cancel(void)
{
    esp_err_t err = esp_file_transfer_abort();
    if (err != ESP_OK) {
        print_error("Cancel", err);
        return 1;
    }
    ESP_LOGI(TAG, "Cancel requested");
    return 0;
}

static int ft_command(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: ft send <path> | ft status | ft cancel | ft config\n");
        return 1;
    }

    if (strcmp(argv[1], "config") == 0 && argc == 2) {
        ESP_LOGI(TAG,
                 "Config: recv_dir=%s, max_file_size=%u, block_size=auto",
                 FILE_TRANSFER_EXAMPLE_RECV_DIR,
                 (unsigned)FILE_TRANSFER_EXAMPLE_MAX_FILE_SIZE);
        ESP_LOGI(TAG, "Link: %s",
                 file_transfer_example_link_ready() ? "ready" : "not ready");
        return 0;
    }

    if (!file_transfer_example_runtime_lock()) {
        print_error("File transfer", ESP_ERR_INVALID_STATE);
        return 1;
    }
    if (!file_transfer_example_runtime_ready_locked()) {
        file_transfer_example_runtime_unlock();
        ESP_LOGE(TAG, "File transfer link is not ready");
        return 1;
    }

    int result = 1;
    if (strcmp(argv[1], "send") == 0 && argc == 3) {
        result = command_send(argv[2]);
    } else if (strcmp(argv[1], "status") == 0 && argc == 2) {
        result = command_status();
    } else if (strcmp(argv[1], "cancel") == 0 && argc == 2) {
        result = command_cancel();
    } else {
        printf("Usage: ft send <path> | ft status | ft cancel | ft config\n");
    }
    file_transfer_example_runtime_unlock();
    return result;
}

esp_err_t file_transfer_example_console_init(void)
{
    esp_err_t err = esp_console_register_help_command();
    if (err != ESP_OK) {
        return err;
    }

    const esp_console_cmd_t command = {
        .command = "ft",
        .help = "File transfer: send <path> | status | cancel | config",
        .hint = NULL,
        .func = ft_command,
        .argtable = NULL,
    };
    err = esp_console_cmd_register(&command);
    if (err != ESP_OK) {
        return err;
    }

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "file-transfer>";
    repl_config.max_cmdline_length = 256;

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t device_config =
        ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    err = esp_console_new_repl_uart(&device_config, &repl_config, &s_repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t device_config =
        ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_cdc(&device_config, &repl_config, &s_repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t device_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_serial_jtag(&device_config, &repl_config,
                                               &s_repl);
#else
#error "No supported console backend configured"
#endif
    if (err != ESP_OK) {
        return err;
    }

    err = esp_console_start_repl(s_repl);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Console ready; type 'help' for commands");
    }
    return err;
}
