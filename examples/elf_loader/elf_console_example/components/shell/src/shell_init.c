/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shell_cmd.h"

void shell_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = CONFIG_SHELL_PROMPT;

#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif

    ESP_ERROR_CHECK(setenv("$PWD", "", 1));

#ifdef CONFIG_SHELL_CMD_LS
    shell_regitser_cmd_ls();
#endif

#ifdef CONFIG_SHELL_CMD_FREE
    shell_regitser_cmd_free();
#endif

#ifdef CONFIG_SHELL_CMD_EXEC
    shell_regitser_cmd_exec();
#endif

#ifdef CONFIG_SHELL_CMD_LIST
    shell_regitser_cmd_list();
#endif

#ifdef CONFIG_SHELL_CMD_INSMOD
    shell_regitser_cmd_insmod();
#endif

#ifdef CONFIG_SHELL_CMD_RMMOD
    shell_regitser_cmd_rmmod();
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
