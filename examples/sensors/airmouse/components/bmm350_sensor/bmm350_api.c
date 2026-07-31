/**
 * Copyright (c) 2024 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file  bmm350_api.c
 * @brief ESP-IDF I2C platform glue for the Bosch BMM350 SensorAPI.
 *
 * Adapted from the BMM350_SensorAPI "examples/common" HAL and from
 * Espressif's esp-dev-kits factory_demo Compass app, to run on top of
 * this repository's `i2c_bus` component instead of the COINES platform.
 */

#include "bmm350_api.h"

#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BMM350_ESP32";

/*! Shared I2C bus handle set by bmm350_interface_init(); not owned/deleted here */
static i2c_bus_handle_t s_i2c_bus = NULL;

/*! Lazily (re)created per resolved I2C address, cached across calls */
static i2c_bus_device_handle_t s_i2c_dev = NULL;
static uint8_t s_i2c_dev_addr;

static int8_t bmm350_get_device_addr(void *intf_ptr, uint8_t *device_addr)
{
    if (intf_ptr == NULL || device_addr == NULL) {
        return BMM350_E_NULL_PTR;
    }

    *device_addr = *(uint8_t *)intf_ptr;
    return BMM350_OK;
}

static i2c_bus_device_handle_t bmm350_get_i2c_dev(uint8_t device_addr)
{
    if (s_i2c_dev != NULL && device_addr == s_i2c_dev_addr) {
        return s_i2c_dev;
    }

    if (s_i2c_dev != NULL) {
        i2c_bus_device_delete(&s_i2c_dev);
        s_i2c_dev = NULL;
    }

    s_i2c_dev = i2c_bus_device_create(s_i2c_bus, device_addr, 0);
    if (s_i2c_dev == NULL) {
        ESP_LOGE(TAG, "i2c_bus_device_create failed for 0x%02X", device_addr);
        return NULL;
    }
    s_i2c_dev_addr = device_addr;
    return s_i2c_dev;
}

BMM350_INTF_RET_TYPE bmm350_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    uint8_t device_addr = 0;
    int8_t addr_rslt = bmm350_get_device_addr(intf_ptr, &device_addr);
    if (addr_rslt != BMM350_OK) {
        return addr_rslt;
    }

    i2c_bus_device_handle_t dev = bmm350_get_i2c_dev(device_addr);
    if (dev == NULL) {
        return BMM350_E_COM_FAIL;
    }

    esp_err_t ret = i2c_bus_read_bytes(dev, reg_addr, (size_t)length, reg_data);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "I2C read failed: addr=0x%02X reg=0x%02X len=%lu err=%s",
                 device_addr,
                 reg_addr,
                 (unsigned long)length,
                 esp_err_to_name(ret));
    }

    return (ret == ESP_OK) ? BMM350_INTF_RET_SUCCESS : BMM350_E_COM_FAIL;
}

BMM350_INTF_RET_TYPE bmm350_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    uint8_t device_addr = 0;
    int8_t addr_rslt = bmm350_get_device_addr(intf_ptr, &device_addr);
    if (addr_rslt != BMM350_OK) {
        return addr_rslt;
    }

    i2c_bus_device_handle_t dev = bmm350_get_i2c_dev(device_addr);
    if (dev == NULL) {
        return BMM350_E_COM_FAIL;
    }

    esp_err_t ret = i2c_bus_write_bytes(dev, reg_addr, (size_t)length, reg_data);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "I2C write failed: addr=0x%02X reg=0x%02X len=%lu err=%s",
                 device_addr,
                 reg_addr,
                 (unsigned long)length,
                 esp_err_to_name(ret));
    }

    return (ret == ESP_OK) ? BMM350_INTF_RET_SUCCESS : BMM350_E_COM_FAIL;
}

void bmm350_delay(uint32_t period_us, void *intf_ptr)
{
    (void)intf_ptr;

    if (period_us < 1000) {
        esp_rom_delay_us(period_us);
        return;
    }

    const int64_t start_us = esp_timer_get_time();
    const int64_t target_us = start_us + (int64_t)period_us;
    const TickType_t delay_ticks = pdMS_TO_TICKS(period_us / 1000);

    if (delay_ticks > 0) {
        vTaskDelay(delay_ticks);
    }

    int64_t now_us = esp_timer_get_time();
    if (now_us < target_us) {
        esp_rom_delay_us((uint32_t)(target_us - now_us));
    }
}

int8_t bmm350_interface_init(struct bmm350_dev *dev, i2c_bus_handle_t i2c_bus, uint8_t *dev_addr_ref)
{
    if (dev == NULL || dev_addr_ref == NULL) {
        return BMM350_E_NULL_PTR;
    }
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "i2c_bus is NULL");
        return BMM350_E_COM_FAIL;
    }

    if (s_i2c_dev != NULL) {
        i2c_bus_device_delete(&s_i2c_dev);
        s_i2c_dev = NULL;
    }

    s_i2c_bus = i2c_bus;
    s_i2c_dev_addr = *dev_addr_ref;

    dev->chip_id = 0;
    dev->intf_ptr = dev_addr_ref;
    dev->read = bmm350_i2c_read;
    dev->write = bmm350_i2c_write;
    dev->delay_us = bmm350_delay;
    dev->intf_rslt = BMM350_INTF_RET_SUCCESS;
    dev->axis_en = 0;
    dev->boot_done_status = BMM350_BOOT_NOT_DONE;
    dev->enable_auto_br = BMM350_ENABLE;
    dev->mraw_override = NULL;

    return BMM350_OK;
}

void bmm350_interface_deinit(void)
{
    if (s_i2c_dev != NULL) {
        i2c_bus_device_delete(&s_i2c_dev);
        s_i2c_dev = NULL;
    }
    s_i2c_bus = NULL;
}

void bmm350_error_codes_print_result(const char *api_name, int8_t rslt)
{
    switch (rslt) {
    case BMM350_OK:
        break;
    case BMM350_E_NULL_PTR:
        ESP_LOGE(TAG, "%s Error [%d] : Null pointer", api_name, rslt);
        break;
    case BMM350_E_COM_FAIL:
        ESP_LOGE(TAG, "%s Error [%d] : Communication fail", api_name, rslt);
        break;
    case BMM350_E_DEV_NOT_FOUND:
        ESP_LOGE(TAG, "%s Error [%d] : Device not found", api_name, rslt);
        break;
    case BMM350_E_INVALID_CONFIG:
        ESP_LOGE(TAG, "%s Error [%d] : Invalid configuration", api_name, rslt);
        break;
    case BMM350_E_BAD_PAD_DRIVE:
        ESP_LOGE(TAG, "%s Error [%d] : Bad pad drive", api_name, rslt);
        break;
    case BMM350_E_RESET_UNFINISHED:
        ESP_LOGE(TAG, "%s Error [%d] : Reset unfinished", api_name, rslt);
        break;
    case BMM350_E_INVALID_INPUT:
        ESP_LOGE(TAG, "%s Error [%d] : Invalid input", api_name, rslt);
        break;
    case BMM350_E_SELF_TEST_INVALID_AXIS:
        ESP_LOGE(TAG, "%s Error [%d] : Self-test invalid axis selection", api_name, rslt);
        break;
    case BMM350_E_OTP_BOOT:
        ESP_LOGE(TAG, "%s Error [%d] : OTP boot", api_name, rslt);
        break;
    case BMM350_E_OTP_PAGE_RD:
        ESP_LOGE(TAG, "%s Error [%d] : OTP page read", api_name, rslt);
        break;
    case BMM350_E_OTP_PAGE_PRG:
        ESP_LOGE(TAG, "%s Error [%d] : OTP page prog", api_name, rslt);
        break;
    case BMM350_E_OTP_SIGN:
        ESP_LOGE(TAG, "%s Error [%d] : OTP sign", api_name, rslt);
        break;
    case BMM350_E_OTP_INV_CMD:
        ESP_LOGE(TAG, "%s Error [%d] : OTP invalid command", api_name, rslt);
        break;
    case BMM350_E_OTP_UNDEFINED:
        ESP_LOGE(TAG, "%s Error [%d] : OTP undefined", api_name, rslt);
        break;
    case BMM350_E_ALL_AXIS_DISABLED:
        ESP_LOGE(TAG, "%s Error [%d] : All axes are disabled", api_name, rslt);
        break;
    case BMM350_E_PMU_CMD_VALUE:
        ESP_LOGW(TAG, "%s Warning [%d] : Unexpected PMU CMD value", api_name, rslt);
        break;
    default:
        ESP_LOGE(TAG, "%s Error [%d] : Unknown error code", api_name, rslt);
        break;
    }
}
