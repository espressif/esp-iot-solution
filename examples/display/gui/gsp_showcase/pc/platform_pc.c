/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "gsp_sim_bridge.h"
#include "showcase_ui.h"

static uint8_t *s_badge;

esp_gsp_err_t gsp_bridge_app_init(esp_gsp_handle_t ui)
{
    FILE *file = fopen(SHOWCASE_BADGE_PATH, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return ESP_GSP_ERR_INVALID_ARG;
    }
    long length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ESP_GSP_ERR_INVALID_ARG;
    }
    s_badge = malloc((size_t)length);
    if (s_badge == NULL) {
        fclose(file);
        return ESP_GSP_ERR_NO_MEM;
    }
    size_t read_size = fread(s_badge, 1, (size_t)length, file);
    fclose(file);
    if (read_size != (size_t)length) {
        free(s_badge);
        s_badge = NULL;
        return ESP_GSP_ERR_INVALID_ARG;
    }
    return showcase_ui_init(ui, s_badge, read_size);
}

void gsp_bridge_app_deinit(esp_gsp_handle_t ui)
{
    showcase_ui_deinit(ui);
    free(s_badge);
    s_badge = NULL;
}
