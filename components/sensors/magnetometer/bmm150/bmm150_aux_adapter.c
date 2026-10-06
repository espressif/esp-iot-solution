/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bmm150_aux_adapter.c
 * @brief BMM150 magnetometer AUX adapter for BMI270
 */

#include <string.h>
#include "bmm150_aux_adapter.h"
#include "bmm150_common.h"
#include "esp_log.h"

static const char *TAG = "BMM150_AUX_ADAPTER";

static int8_t bmm150_aux_read(unsigned char reg_addr, unsigned char *data, uint32_t len, void *intf_ptr)
{
    bmi270_handle_t bmi = (bmi270_handle_t)intf_ptr;
    return bmi270_aux_read(bmi, reg_addr, data, (uint16_t)len);
}

static int8_t bmm150_aux_write(unsigned char reg_addr, const unsigned char *data, uint32_t len, void *intf_ptr)
{
    bmi270_handle_t bmi = (bmi270_handle_t)intf_ptr;
    return bmi270_aux_write(bmi, reg_addr, data, (uint16_t)len);
}

static int8_t bmm150_aux_check_result(int8_t rslt, const struct bmm150_dev *dev, const char *operation)
{
    if (rslt != BMM150_OK) {
        return rslt;
    }
    if (dev->intf_rslt != BMM150_INTF_RET_SUCCESS) {
        ESP_LOGE(TAG, "%s communication failed: %d", operation, dev->intf_rslt);
        return BMM150_E_COM_FAIL;
    }
    return BMM150_OK;
}

int8_t bmm150_aux_adapter_init(const bmm150_aux_config_t *config, bmm150_aux_handle_t *handle)
{
    if (!config || !handle || !config->bmi270_dev) {
        ESP_LOGE(TAG, "Invalid parameters");
        return BMM150_E_NULL_PTR;
    }

    memset(handle, 0, sizeof(bmm150_aux_handle_t));
    handle->bmi270_dev = config->bmi270_dev;

    handle->bmm150_dev.intf = BMM150_I2C_INTF;
    handle->bmm150_dev.read = bmm150_aux_read;
    handle->bmm150_dev.write = bmm150_aux_write;
    handle->bmm150_dev.delay_us = bmm150_delay;
    handle->bmm150_dev.intf_ptr = config->bmi270_dev;

    int8_t rslt = bmm150_init(&handle->bmm150_dev);
    rslt = bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 init");
    if (rslt != BMM150_OK) {
        ESP_LOGE(TAG, "BMM150 init failed: %d", rslt);
        return rslt;
    }

    struct bmm150_settings settings = {0};
    settings.pwr_mode = BMM150_POWERMODE_NORMAL;
    rslt = bmm150_set_op_mode(&settings, &handle->bmm150_dev);
    rslt = bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 set op mode");
    if (rslt != BMM150_OK) {
        ESP_LOGE(TAG, "BMM150 set op mode failed: %d", rslt);
        return rslt;
    }
    handle->bmm150_dev.delay_us(5000, handle->bmm150_dev.intf_ptr);

    handle->is_initialized = true;
    ESP_LOGI(TAG, "BMM150 AUX adapter initialized, chip ID=0x%02X", handle->bmm150_dev.chip_id);
    return BMM150_OK;
}

int8_t bmm150_aux_adapter_deinit(bmm150_aux_handle_t *handle)
{
    if (!handle) {
        return BMM150_E_NULL_PTR;
    }

    handle->is_initialized = false;
    return BMM150_OK;
}

int8_t bmm150_aux_adapter_read_mag_data(bmm150_aux_handle_t *handle, struct bmm150_mag_data *mag_data)
{
    if (!handle || !mag_data || !handle->is_initialized) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmm150_read_mag_data(mag_data, &handle->bmm150_dev);
    return bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 data read");
}

int8_t bmm150_aux_adapter_configure(bmm150_aux_handle_t *handle, const struct bmm150_settings *settings)
{
    if (!handle || !settings || !handle->is_initialized) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmm150_set_sensor_settings(
                      BMM150_SEL_DATA_RATE | BMM150_SEL_XY_REP | BMM150_SEL_Z_REP | BMM150_SEL_CONTROL_MEASURE,
                      settings,
                      &handle->bmm150_dev
                  );
    return bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 configure");
}

int8_t bmm150_aux_adapter_soft_reset(bmm150_aux_handle_t *handle)
{
    if (!handle || !handle->is_initialized) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmm150_soft_reset(&handle->bmm150_dev);
    return bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 soft reset");
}

int8_t bmm150_aux_adapter_get_chip_id(bmm150_aux_handle_t *handle, uint8_t *chip_id)
{
    if (!handle || !chip_id || !handle->is_initialized) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmm150_get_regs(BMM150_REG_CHIP_ID, chip_id, 1, &handle->bmm150_dev);
    return bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 Chip ID read");
}

int8_t bmm150_aux_adapter_read_regs(bmm150_aux_handle_t *handle, uint8_t reg_addr, uint8_t *data, uint32_t len)
{
    if (!handle || !data || !handle->is_initialized) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmm150_get_regs(reg_addr, data, len, &handle->bmm150_dev);
    return bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 register read");
}

int8_t bmm150_aux_adapter_write_regs(bmm150_aux_handle_t *handle, uint8_t reg_addr, const uint8_t *data, uint32_t len)
{
    if (!handle || !data || !handle->is_initialized) {
        return BMM150_E_NULL_PTR;
    }

    int8_t rslt = bmm150_set_regs(reg_addr, data, len, &handle->bmm150_dev);
    return bmm150_aux_check_result(rslt, &handle->bmm150_dev, "BMM150 register write");
}
