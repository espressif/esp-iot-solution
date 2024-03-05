/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>

#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <esp_log.h>

#include "lightbulb.h"

static const char *TAG = "lightbulb exmaple cmd";

static esp_console_repl_t *s_repl = NULL;
static bool is_active = false;

static struct {
    struct arg_int *hsv;
    struct arg_end *end;
} set_hsv_args;

static struct {
    struct arg_int *rgb;
    struct arg_end *end;
} set_rgb_args;

static struct {
    struct arg_int *cctb;
    struct arg_end *end;
} set_cctb_args;

static struct {
    struct arg_int *fade_ms;
    struct arg_int *enable_fade;
    struct arg_end *end;
} set_config_args;

static int do_set_hsv_cmd(int argc, char **argv)
{
    is_active = true;
    int nerrors = arg_parse(argc, argv, (void **) &set_hsv_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_hsv_args.end, argv[0]);
        return 1;
    }
    if (lightbulb_set_hsv(set_hsv_args.hsv->ival[0], set_hsv_args.hsv->ival[1], set_hsv_args.hsv->ival[2]) != ESP_OK) {
        return 1;
    }

    return 0;
}

static int do_set_rgb_cmd(int argc, char **argv)
{
    is_active = true;
    int nerrors = arg_parse(argc, argv, (void **) &set_rgb_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_rgb_args.end, argv[0]);
        return 1;
    }
    uint16_t hsv[3] = {0};
    if (lightbulb_rgb2hsv(set_rgb_args.rgb->ival[0], set_rgb_args.rgb->ival[1], set_rgb_args.rgb->ival[2], &hsv[0], (uint8_t *)&hsv[1], (uint8_t *)&hsv[2]) != ESP_OK) {
        return 1;
    }

    if (lightbulb_set_hsv(hsv[0], hsv[1], hsv[2]) != ESP_OK) {
        return 1;
    }

    return 0;
}

static int do_set_cctb_cmd(int argc, char **argv)
{
    is_active = true;
    int nerrors = arg_parse(argc, argv, (void **) &set_cctb_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_cctb_args.end, argv[0]);
        return 1;
    }

    if (lightbulb_set_cctb(set_cctb_args.cctb->ival[0], set_cctb_args.cctb->ival[1]) != ESP_OK) {
        return 1;
    }

    return 0;
}

static int do_update_config_cmd(int argc, char **argv)
{
    is_active = true;
    int nerrors = arg_parse(argc, argv, (void **) &set_config_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_cctb_args.end, argv[0]);
        return 1;
    }
    if (set_config_args.fade_ms) {
        lightbulb_set_fade_time(set_config_args.fade_ms->ival[0]);
    }
    if (set_config_args.enable_fade) {
        lightbulb_set_fades_function(set_config_args.enable_fade->ival[0]);
    }

    return 0;
}

static int do_quit_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "ByeBye\r\n");
    s_repl->del(s_repl);
    s_repl = NULL;
    is_active = false;
    return 0;
}

esp_err_t lightbulb_example_console_init(void)
{
    esp_err_t err = ESP_OK;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = ">";
    repl_config.max_cmdline_length = 1024;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    err |= esp_console_new_repl_uart(&uart_config, &repl_config, &s_repl);

    err |= esp_console_register_help_command();

    set_hsv_args.hsv = arg_intn(NULL, NULL, NULL, 3, 3, NULL);
    set_hsv_args.end = arg_end(3);
    const esp_console_cmd_t cmd0 = {
        .command = "sethsv",
        .help = "Use the HSV model to light the lightbulb",
        .hint = NULL,
        .argtable = &set_hsv_args,
        .func = &do_set_hsv_cmd,
    };
    err |= esp_console_cmd_register(&cmd0);

    set_rgb_args.rgb = arg_intn(NULL, NULL, NULL, 3, 3, NULL);
    set_rgb_args.end = arg_end(3);
    const esp_console_cmd_t cmd1 = {
        .command = "setrgb",
        .help = "Use the RGB model to light the lightbulb",
        .hint = NULL,
        .argtable = &set_rgb_args,
        .func = &do_set_rgb_cmd,
    };
    err |= esp_console_cmd_register(&cmd1);

    set_cctb_args.cctb = arg_intn(NULL, NULL, NULL, 2, 2, NULL);
    set_cctb_args.end = arg_end(2);
    const esp_console_cmd_t cmd2 = {
        .command = "setcctb",
        .help = "Use the CCTB model to light the lightbulb",
        .hint = NULL,
        .argtable = &set_cctb_args,
        .func = &do_set_cctb_cmd,
    };
    err |= esp_console_cmd_register(&cmd2);

    set_config_args.enable_fade = arg_int0("e", "enable fade", "<enable/disable>", "Enbale or disable fade？");
    set_config_args.fade_ms = arg_int0("t", "fade time", NULL, "fade time, ms");
    set_config_args.end = arg_end(2);
    const esp_console_cmd_t cmd3 = {
        .command = "updateconfig",
        .help = "Update lightbulb config",
        .hint = NULL,
        .argtable = &set_config_args,
        .func = &do_update_config_cmd,
    };
    err |= esp_console_cmd_register(&cmd3);

    esp_console_cmd_t cmd4 = {
        .command = "quit",
        .help = "Quit REPL environment",
        .func = &do_quit_cmd,
        .argtable = NULL,
        .hint = NULL,
    };
    esp_console_cmd_register(&cmd4);

    printf("\n ==========================================================\n");
    printf(" |       Steps to Use lightbulb console-tools             |\n");
    printf(" |                                                        |\n");
    printf(" |  1. Try 'updateconfig' to configure your lightbub      |\n");
    printf(" |  2. Try 'sethsv' to light the color mode               |\n");
    printf(" |  3. Try 'setrgb' to light the color mode               |\n");
    printf(" |  4. Try 'setcctb' to light the CCT mode                |\n");
    printf(" |  5. Try 'quit' to quit REPL environment                |\n");
    printf(" |                                                        |\n");
    printf(" ==========================================================\n\n");

    // start console REPL
    err |= esp_console_start_repl(s_repl);
    is_active = true;

    return err;
}

esp_err_t lightbulb_example_console_deinit(void)
{
    if (s_repl) {
        esp_err_t err = s_repl->del(s_repl);
        s_repl = NULL;
        return err;
    }

    return ESP_ERR_INVALID_ARG;
}

bool lightbulb_example_get_console_status(void)
{
    return is_active;
}
