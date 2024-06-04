/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_check.h"
#include "lamp_array_matrix.h"
#include "pixel.h"

static const char *TAG = "lamp_array_matrix";

#define LAMPARRAY_KIND 1

typedef struct {
    uint16_t current_lamp_Id;
    lamp_array_matrix_cfg_t cfg;
} lamp_array_matrix_t;

static lamp_array_matrix_t *lamp_array_matrix;

esp_err_t lamp_array_matrix_init(lamp_array_matrix_cfg_t cfg)
{
    ESP_RETURN_ON_FALSE(lamp_array_matrix == NULL, ESP_ERR_INVALID_STATE, TAG, "lamp_array_matrix is not NULL");
    lamp_array_matrix = (lamp_array_matrix_t *)calloc(1, sizeof(lamp_array_matrix_t));
    ESP_RETURN_ON_FALSE(lamp_array_matrix != NULL, ESP_ERR_NO_MEM, TAG, "lamp_array_matrix is NULL");
    lamp_array_matrix->cfg = cfg;

    NeopixelInit(cfg.handle, cfg.pixel_cnt);
    return ESP_OK;
}

uint16_t GetLampArrayAttributesReport(uint8_t* buffer)
{
    LampArrayAttributesReport report = {
        lamp_array_matrix->cfg.pixel_cnt,
        lamp_array_matrix->cfg.lamp_array_width,
        lamp_array_matrix->cfg.lamp_array_height,
        lamp_array_matrix->cfg.lamp_array_depth,
        LAMPARRAY_KIND,
        lamp_array_matrix->cfg.update_interval,
    };

    memcpy(buffer, &report, sizeof(LampArrayAttributesReport));

    return sizeof(LampArrayAttributesReport);
}

uint16_t GetLampAttributesReport(uint8_t* buffer)
{
    LampAttributesResponseReport report = {
        lamp_array_matrix->current_lamp_Id,                                          // LampId
        lamp_array_matrix->cfg.lamp_array_rotation[lamp_array_matrix->current_lamp_Id], // Lamp rotation
        lamp_array_matrix->cfg.update_interval,      // Lamp update interval
        LAMP_PURPOSE_CONTROL,                                   // Lamp purpose
        255,                                                    // Red level count
        255,                                                    // Blue level count
        255,                                                    // Green level count
        1,                                                      // Intensity
        1,                                                      // Is Programmable
        lamp_array_matrix->cfg.bind_key,                        // InputBinding
    };

    memcpy(buffer, &report, sizeof(LampAttributesResponseReport));
    lamp_array_matrix->current_lamp_Id = lamp_array_matrix->current_lamp_Id + 1 >= lamp_array_matrix->cfg.pixel_cnt ?  lamp_array_matrix->current_lamp_Id : lamp_array_matrix->current_lamp_Id + 1;

    return sizeof(LampAttributesResponseReport);
}

void SetLampAttributesId(const uint8_t* buffer)
{
    LampAttributesRequestReport* report = (LampAttributesRequestReport*) buffer;
    lamp_array_matrix->current_lamp_Id = report->lampId;
}

void SetMultipleLamps(const uint8_t* buffer)
{
    LampMultiUpdateReport* report = (LampMultiUpdateReport*) buffer;

    for (int i = 0; i < report->lampCount; i++) {
        NeopixelSetColor(report->lampIds[i], report->colors[i]);
    }
}

void SetLampRange(const uint8_t* buffer)
{
    LampRangeUpdateReport* report = (LampRangeUpdateReport*) buffer;
    NeopixelSetColorRange(report->lampIdStart, report->lampIdEnd, report->color);
}

void SetAutonomousMode(const uint8_t* buffer)
{
    LampArrayControlReport* report = (LampArrayControlReport*) buffer;
    NeopixelSetEffect(report->autonomousMode ? AUTONOMOUS_LIGHTING_EFFECT : HID, AUTONOMOUS_LIGHTING_COLOR);
}
