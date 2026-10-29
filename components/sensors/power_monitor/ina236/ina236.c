/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "ina236.h"
#include "ina_236_reg.h"

static const char *TAG = "INA236";
#define INA236_DEVICE_ID 0xA080
#define ESP_INTR_FLAG_DEFAULT 0

typedef struct {
    bool alert_en;
    uint8_t alert_pin;
    uint8_t i2c_addr;
    i2c_bus_device_handle_t i2c_dev;
    ina236_reg_t reg;
    int236_alert_cb_t cb;
} ina236_t;

static esp_err_t ina236_read_reg(ina236_t *ina236, uint8_t reg, uint16_t *data)
{
    uint8_t ina236_data[2] = { 0 };
    esp_err_t ret = i2c_bus_read_bytes(ina236->i2c_dev, reg, 2, &ina236_data[0]);
    *data = ((ina236_data[0] << 8 | ina236_data[1]));
    return ret;
}

static esp_err_t ina236_write_reg(ina236_t *ina236, uint8_t reg, uint16_t *data)
{
    esp_err_t ret = i2c_bus_write_bytes(ina236->i2c_dev, reg, 2, (uint8_t *)&data);
    return ret;
}

static esp_err_t ina236_reg_init(ina236_t *ina236)
{
    uint16_t calibration = 10;
    uint16_t buffer_id = 0;
    ina236_read_reg(ina236, INA236_REG_DEVID, &buffer_id);
    if (buffer_id == INA236_DEVICE_ID) {
        ESP_LOGI(TAG, "Check ina236 id OK");
    }
    ina236_write_reg(ina236, INA236_REG_CALIBRATION, &calibration);
    ina236_reg_cfg_t cfg = {
        .bit.adcrange = 0,
        .bit.avg = 0,
        .bit.mode = 0x07,
        .bit.rst = 0,
        .bit.vbusct = 0x02,
        .bit.vshct = 0x02,
    };
    ina236_write_reg(ina236, INA236_REG_CFG, (uint16_t *)&cfg);
    ina236_reg_mask_t mask = { 0 };
    mask.bit.cnvr = 1;
    ina236_write_reg(ina236, INA236_REG_MASK, (uint16_t *)&mask);
    return ESP_OK;
}
static void ina236_read_all_reg(ina236_t *ina236)
{
    ina236_read_reg(ina236, INA236_REG_CFG, (uint16_t *)&ina236->reg.cfg);
    ina236_read_reg(ina236, INA236_REG_VSHUNT, (uint16_t *)&ina236->reg.vshunt);
    ina236_read_reg(ina236, INA236_REG_VBUS, (uint16_t *)&ina236->reg.vbus);
    ina236_read_reg(ina236, INA236_REG_POWER, (uint16_t *)&ina236->reg.power);
    ina236_read_reg(ina236, INA236_REG_CURRENT, (uint16_t *)&ina236->reg.current);
    ina236_read_reg(ina236, INA236_REG_CALIBRATION, (uint16_t *)&ina236->reg.calibration);
    ina236_read_reg(ina236, INA236_REG_MASK, (uint16_t *)&ina236->reg.mask);
    ina236_read_reg(ina236, INA236_REG_ALERT_LIM, (uint16_t *)&ina236->reg.lim);
    ina236_read_reg(ina236, INA236_REG_MAF_ID, (uint16_t *)&ina236->reg.maf_id);
    ina236_read_reg(ina236, INA236_REG_DEVID, (uint16_t *)&ina236->reg.devid_id);
}

esp_err_t ina236_create(ina236_handle_t *handle, ina236_config_t *config)
{
    esp_err_t err = ESP_OK;
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ina236_t *ina236 = (ina236_t *)calloc(1, sizeof(ina236_t));
    ESP_RETURN_ON_FALSE(ina236, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for ina236");

    ina236->i2c_addr = config->dev_addr;
    ina236->alert_pin = config->alert_pin;
    ina236->alert_en = config->alert_en;
    ina236->cb = config->alert_cb;
    ina236->i2c_dev = i2c_bus_device_create(config->bus, ina236->i2c_addr, i2c_bus_get_current_clk_speed(config->bus));
    if (ina236->i2c_dev == NULL) {
        free(ina236);
        return ESP_FAIL;
    }
    ina236_reg_init(ina236);
    ina236_read_all_reg(ina236);
    if (ina236->alert_en) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << ina236->alert_pin);
        io_conf.pull_up_en = 1;
        err = gpio_config(&io_conf);
        gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
        gpio_isr_handler_add(ina236->alert_pin, ina236->cb, NULL);
    }
    *handle = (ina236_handle_t)ina236;
    return err;
}

esp_err_t ina236_delete(ina236_handle_t handle)
{
    ina236_t *ina236 = (ina236_t *)handle;
    i2c_bus_device_delete(&ina236->i2c_dev);
    if (ina236->alert_en) {
        gpio_isr_handler_remove(ina236->alert_pin);
    }
    free(ina236);
    return ESP_OK;
}

esp_err_t ina236_get_voltage(ina236_handle_t handle, float *volt)
{
    ina236_t *ina236 = (ina236_t *)handle;
    uint16_t buffer = 0;
    ina236_read_reg(ina236, INA236_REG_VBUS, &buffer);
    *volt = buffer * 0.0016f;
    return ESP_OK;
}

esp_err_t ina236_get_current(ina236_handle_t handle, float *curr)
{
    uint16_t buffer = 0;
    ina236_t *ina236 = (ina236_t *)handle;
    ina236_read_reg(ina236, INA236_REG_VSHUNT, &buffer);
    *curr = (int16_t)buffer / 3970.0f;
    return ESP_OK;
}

esp_err_t ina236_clear_mask(ina236_handle_t handle)
{
    ina236_t *ina236 = (ina236_t *)handle;
    ina236_reg_mask_t mask = { 0 };
    ina236_read_reg(ina236, INA236_REG_MASK, (uint16_t *)&mask);
    return ESP_OK;
}
