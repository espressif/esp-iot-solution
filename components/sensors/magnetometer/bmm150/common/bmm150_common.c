/**
 * Copyright (C) 2023 Bosch Sensortec GmbH.
 * Copyright (C) 2025-2026 Espressif Systems (Shanghai) CO LTD.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "bmm150_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bmm150.h"
#include "bmm150_defs.h"

/******************************************************************************/
/*!                 Macro definitions                                         */

#define TAG "BMM150"
#define BMM150_I2C_TIMEOUT_MS 200
#define BMM150_I2C_WRITE_MAX 32

/******************************************************************************/
/*!                Static variable definition                                 */

/*! Per-sensor I2C state, reached through bmm150_dev::intf_ptr */
typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    i2c_master_dev_handle_t i2c_master_dev;
    bool pending_removal;
} bmm150_i2c_ctx_t;

/*! Bus and address used by the file-level bmm150_interface_init() */
static uint8_t dev_addr = BMM150_DEFAULT_I2C_ADDRESS;
static i2c_bus_handle_t i2c_bus = NULL;
/*! Kept beside the caller's struct so an address probe can zero that struct. */
static bmm150_i2c_ctx_t *legacy_ctx = NULL;
static struct bmm150_dev *legacy_dev = NULL;

static esp_err_t bmm150_remove_i2c_device(bmm150_i2c_ctx_t *ctx)
{
    esp_err_t ret = ESP_OK;
    if (ctx->i2c_dev != NULL) {
        ret = i2c_bus_device_delete(&ctx->i2c_dev);
    }
#if !CONFIG_I2C_BUS_BACKWARD_CONFIG
    if (ret == ESP_OK && ctx->i2c_master_dev != NULL) {
        ret = i2c_master_bus_rm_device(ctx->i2c_master_dev);
        if (ret == ESP_OK) {
            ctx->i2c_master_dev = NULL;
        }
    }
#endif
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device removal failed: %s", esp_err_to_name(ret));
        ctx->pending_removal = true;
    } else {
        ctx->pending_removal = false;
    }
    return ret;
}

static bmm150_i2c_ctx_t *bmm150_usable_ctx(void *intf_ptr)
{
    bmm150_i2c_ctx_t *ctx = intf_ptr;
    if (ctx == NULL || ctx->pending_removal || (ctx->i2c_dev == NULL && ctx->i2c_master_dev == NULL)) {
        return NULL;
    }
    return ctx;
}

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to ESP32 platform
 */
BMM150_INTF_RET_TYPE bmm150_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    bmm150_i2c_ctx_t *ctx = bmm150_usable_ctx(intf_ptr);
    if (ctx == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return BMM150_E_COM_FAIL;
    }

    ESP_LOGD(TAG, "I2C read reg 0x%02" PRIx8 " len %" PRIu32, reg_addr, length);
    esp_err_t ret;
    if (ctx->i2c_master_dev != NULL) {
#if CONFIG_I2C_BUS_BACKWARD_CONFIG
        return BMM150_E_COM_FAIL;
#else
        ret = i2c_master_transmit_receive(ctx->i2c_master_dev, &reg_addr, 1, reg_data, length,
                                          BMM150_I2C_TIMEOUT_MS);
#endif
    } else {
        ret = i2c_bus_read_bytes(ctx->i2c_dev, reg_addr, (uint16_t)length, reg_data);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
        return BMM150_E_COM_FAIL;
    }

    return BMM150_INTF_RET_SUCCESS;
}

/*!
 * I2C write function map to ESP32 platform
 */
BMM150_INTF_RET_TYPE bmm150_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    bmm150_i2c_ctx_t *ctx = bmm150_usable_ctx(intf_ptr);
    if (ctx == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return BMM150_E_COM_FAIL;
    }

    ESP_LOGD(TAG, "I2C write reg 0x%02" PRIx8 " len %" PRIu32, reg_addr, length);
    esp_err_t ret;
    if (ctx->i2c_master_dev != NULL) {
#if CONFIG_I2C_BUS_BACKWARD_CONFIG
        return BMM150_E_COM_FAIL;
#else
        if (reg_data == NULL || length == 0 || length > BMM150_I2C_WRITE_MAX) {
            return BMM150_E_COM_FAIL;
        }
        uint8_t buffer[BMM150_I2C_WRITE_MAX + 1];
        buffer[0] = reg_addr;
        memcpy(buffer + 1, reg_data, length);
        ret = i2c_master_transmit(ctx->i2c_master_dev, buffer, length + 1, BMM150_I2C_TIMEOUT_MS);
#endif
    } else {
        ret = i2c_bus_write_bytes(ctx->i2c_dev, reg_addr, (uint16_t)length, reg_data);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(ret));
        return BMM150_E_COM_FAIL;
    }

    return BMM150_INTF_RET_SUCCESS;
}

void bmm150_delay(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;

    if (period == 0) {
        return;
    }

    int64_t start_us = esp_timer_get_time();
    uint64_t full_ticks = ((uint64_t)period * configTICK_RATE_HZ) / 1000000ULL;

    /*
     * vTaskDelay() wakes on tick boundaries and can therefore block for up
     * to one tick less than expected. Measure the actual elapsed time below
     * and busy-wait only for the remaining microseconds.
     */
    if (full_ticks > 0) {
        vTaskDelay((TickType_t)full_ticks);
    }

    int64_t elapsed_us = esp_timer_get_time() - start_us;
    if (elapsed_us < period) {
        esp_rom_delay_us(period - (uint32_t)elapsed_us);
    }
}

static esp_err_t bmm150_interface_init_internal(struct bmm150_dev *dev, void *bus, bool native_master,
                                                uint8_t i2c_addr, uint32_t scl_speed_hz)
{
    if (dev == NULL || bus == NULL || (native_master && scl_speed_hz == 0)) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_I2C_BUS_BACKWARD_CONFIG
    if (native_master) {
        ESP_LOGE(TAG, "Native I2C requires CONFIG_I2C_BUS_BACKWARD_CONFIG disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

    bmm150_i2c_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "I2C Interface, dev_addr=0x%02" PRIx8, i2c_addr);

    if (native_master) {
#if !CONFIG_I2C_BUS_BACKWARD_CONFIG
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = i2c_addr,
            .scl_speed_hz = scl_speed_hz,
        };
        esp_err_t ret = i2c_master_bus_add_device(bus, &config, &ctx->i2c_master_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(ret));
            free(ctx);
            return ret;
        }
#endif
    } else {
        ctx->i2c_dev = i2c_bus_device_create(bus, i2c_addr, 0);
        if (ctx->i2c_dev == NULL) {
            ESP_LOGE(TAG, "i2c_bus_device_create failed");
            free(ctx);
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "I2C device created at address 0x%02" PRIx8, i2c_addr);

    dev->intf = BMM150_I2C_INTF;
    dev->intf_ptr = ctx;
    dev->read = bmm150_i2c_read;
    dev->write = bmm150_i2c_write;
    dev->delay_us = bmm150_delay;

    return ESP_OK;
}

esp_err_t bmm150_interface_init_from_master_bus(struct bmm150_dev *dev, i2c_master_bus_handle_t bus,
                                                uint8_t i2c_addr, uint32_t scl_speed_hz)
{
    return bmm150_interface_init_internal(dev, bus, true, i2c_addr, scl_speed_hz);
}

esp_err_t bmm150_interface_init_from_i2c_bus(struct bmm150_dev *dev, i2c_bus_handle_t bus, uint8_t i2c_addr)
{
    return bmm150_interface_init_internal(dev, bus, false, i2c_addr, 0);
}

esp_err_t bmm150_interface_deinit_device(struct bmm150_dev *dev)
{
    if (dev == NULL || dev->intf_ptr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bmm150_i2c_ctx_t *ctx = dev->intf_ptr;
    esp_err_t ret = bmm150_remove_i2c_device(ctx);
    if (ret != ESP_OK) {
        /* Keep the context so the caller can retry the removal. */
        return ret;
    }

    if (ctx == legacy_ctx) {
        legacy_ctx = NULL;
        legacy_dev = NULL;
    }
    free(ctx);
    dev->intf_ptr = NULL;
    dev->read = NULL;
    dev->write = NULL;
    dev->delay_us = NULL;

    return ESP_OK;
}

static esp_err_t bmm150_release_legacy_device(void)
{
    if (legacy_ctx == NULL) {
        legacy_dev = NULL;
        return ESP_OK;
    }

    /* Address probes zero the struct. The context is still in legacy_ctx. */
    if (legacy_dev != NULL && legacy_dev->intf_ptr == legacy_ctx) {
        return bmm150_interface_deinit_device(legacy_dev);
    }

    esp_err_t ret = bmm150_remove_i2c_device(legacy_ctx);
    if (ret != ESP_OK) {
        return ret;
    }
    free(legacy_ctx);
    legacy_ctx = NULL;
    legacy_dev = NULL;
    return ESP_OK;
}

int8_t bmm150_interface_init(struct bmm150_dev *dev)
{
    if (dev == NULL) {
        return BMM150_E_NULL_PTR;
    }

    /* Retry before the bus check so a failed deinit can still be cleaned up. */
    if (bmm150_release_legacy_device() != ESP_OK) {
        return BMM150_E_COM_FAIL;
    }

    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL, please call bmm150_set_i2c_bus_handle first");
        return BMM150_E_COM_FAIL;
    }

    if (bmm150_interface_init_from_i2c_bus(dev, i2c_bus, dev_addr) != ESP_OK) {
        return BMM150_E_COM_FAIL;
    }
    legacy_ctx = dev->intf_ptr;
    legacy_dev = dev;

    return BMM150_OK;
}

void bmm150_error_codes_print_result(const char api_name[], int8_t rslt)
{
    switch (rslt) {
    case BMM150_OK:
        break;
    case BMM150_E_NULL_PTR:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Null pointer", api_name, rslt);
        break;
    case BMM150_E_COM_FAIL:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Communication fail", api_name, rslt);
        break;
    case BMM150_E_DEV_NOT_FOUND:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Device not found", api_name, rslt);
        break;
    case BMM150_E_INVALID_CONFIG:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Invalid configuration", api_name, rslt);
        break;
    case BMM150_W_NORMAL_SELF_TEST_YZ_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Normal self-test YZ fail", api_name, rslt);
        break;
    case BMM150_W_NORMAL_SELF_TEST_XZ_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Normal self-test XZ fail", api_name, rslt);
        break;
    case BMM150_W_NORMAL_SELF_TEST_Z_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Normal self-test Z fail", api_name, rslt);
        break;
    case BMM150_W_NORMAL_SELF_TEST_XY_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Normal self-test XY fail", api_name, rslt);
        break;
    case BMM150_W_NORMAL_SELF_TEST_Y_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Normal self-test Y fail", api_name, rslt);
        break;
    case BMM150_W_NORMAL_SELF_TEST_X_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Normal self-test X fail", api_name, rslt);
        break;
    case BMM150_W_NORMAL_SELF_TEST_XYZ_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Normal self-test XYZ fail", api_name, rslt);
        break;
    case BMM150_W_ADV_SELF_TEST_FAIL:
        ESP_LOGW(TAG, "API [%s] Warning [%" PRIi8 "] : Advanced self-test fail", api_name, rslt);
        break;
    default:
        ESP_LOGE(TAG, "API [%s] Error [%" PRIi8 "] : Unknown error code", api_name, rslt);
        break;
    }
}

void bmm150_set_i2c_bus_handle(i2c_bus_handle_t bus_handle)
{
    i2c_bus = bus_handle;
    ESP_LOGI(TAG, "I2C bus handle set");
}

void bmm150_set_i2c_address(uint8_t i2c_addr)
{
    dev_addr = i2c_addr;
    ESP_LOGI(TAG, "I2C address set to 0x%02" PRIx8, i2c_addr);
}

void bmm150_interface_deinit(void)
{
    ESP_LOGI(TAG, "BMM150 ESP32 deinit");

    (void)bmm150_release_legacy_device();
    /* Clear the bus handle, but do not delete the I2C bus */
    i2c_bus = NULL;
    dev_addr = BMM150_DEFAULT_I2C_ADDRESS;
}
