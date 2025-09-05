/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "ina236.h"
#include "ina236_reg.h"

#define INA236_DEVICE_ID 0xA080
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "INA236";

typedef struct {
    bool alert_en;
    uint8_t alert_pin;
    uint8_t i2c_addr;
    i2c_bus_device_handle_t i2c_dev;
    ina236_reg_t reg;
    int236_alert_cb_t cb;
} ina236_dev_t;

static esp_err_t ina236_read_reg(ina236_dev_t *ina236, uint8_t reg, uint16_t *data)
{
    uint8_t ina236_data[2] = { 0 };
    esp_err_t ret = i2c_bus_read_bytes(ina236->i2c_dev, reg, 2, &ina236_data[0]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(ret));
        return ret;
    }
    *data = ((ina236_data[0] << 8 | ina236_data[1]));
    return ESP_OK;
}

static esp_err_t ina236_write_reg(ina236_dev_t *ina236, uint8_t reg, uint16_t *data)
{
    esp_err_t ret = i2c_bus_write_bytes(ina236->i2c_dev, reg, 2, (uint8_t *)data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02X: %s", reg, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static esp_err_t ina236_reg_init(ina236_dev_t *ina236)
{
    uint16_t calibration = 10;
    uint16_t buffer_id = 0;
    esp_err_t ret;

    ret = ina236_read_reg(ina236, INA236_REG_DEVID, &buffer_id);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read device ID");

    if (buffer_id == INA236_DEVICE_ID) {
        ESP_LOGI(TAG, "Check ina236 id OK");
    } else {
        ESP_LOGW(TAG, "Unexpected device ID: 0x%04X (expected: 0x%04X)", buffer_id, INA236_DEVICE_ID);
    }

    // Set calibration
    ret = ina236_write_reg(ina236, INA236_REG_CALIBRATION, &calibration);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set calibration");

    // Configure device
    ina236_reg_cfg_t cfg = {
        .bit.adcrange = 0,
        .bit.avg = 0,
        .bit.mode = 0x07,
        .bit.rst = 0,
        .bit.vbusct = 0x02,
        .bit.vshct = 0x02,
    };
    ret = ina236_write_reg(ina236, INA236_REG_CFG, (uint16_t *)&cfg);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to configure device");

    // Set mask
    ina236_reg_mask_t mask = { 0 };
    mask.bit.cnvr = 1;
    ret = ina236_write_reg(ina236, INA236_REG_MASK, (uint16_t *)&mask);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set mask");

    return ESP_OK;
}

static esp_err_t ina236_read_all_reg(ina236_dev_t *ina236)
{
    esp_err_t ret;

    ret = ina236_read_reg(ina236, INA236_REG_CFG, (uint16_t *)&ina236->reg.cfg);
    ESP_RETURN_ON_ERROR(ret, TAG, "read cfg failed");

    ret = ina236_read_reg(ina236, INA236_REG_VSHUNT, (uint16_t *)&ina236->reg.vshunt);
    ESP_RETURN_ON_ERROR(ret, TAG, "read vshunt failed");

    ret = ina236_read_reg(ina236, INA236_REG_VBUS, (uint16_t *)&ina236->reg.vbus);
    ESP_RETURN_ON_ERROR(ret, TAG, "read vbus failed");

    ret = ina236_read_reg(ina236, INA236_REG_POWER, (uint16_t *)&ina236->reg.power);
    ESP_RETURN_ON_ERROR(ret, TAG, "read power failed");

    ret = ina236_read_reg(ina236, INA236_REG_CURRENT, (uint16_t *)&ina236->reg.current);
    ESP_RETURN_ON_ERROR(ret, TAG, "read current failed");

    ret = ina236_read_reg(ina236, INA236_REG_CALIBRATION, (uint16_t *)&ina236->reg.calibration);
    ESP_RETURN_ON_ERROR(ret, TAG, "read calibration failed");

    ret = ina236_read_reg(ina236, INA236_REG_MASK, (uint16_t *)&ina236->reg.mask);
    ESP_RETURN_ON_ERROR(ret, TAG, "read mask failed");

    ret = ina236_read_reg(ina236, INA236_REG_ALERT_LIM, (uint16_t *)&ina236->reg.lim);
    ESP_RETURN_ON_ERROR(ret, TAG, "read alert lim failed");

    ret = ina236_read_reg(ina236, INA236_REG_MAF_ID, (uint16_t *)&ina236->reg.maf_id);
    ESP_RETURN_ON_ERROR(ret, TAG, "read maf id failed");

    ret = ina236_read_reg(ina236, INA236_REG_DEVID, (uint16_t *)&ina236->reg.devid_id);
    ESP_RETURN_ON_ERROR(ret, TAG, "read devid id failed");

    return ESP_OK;
}

esp_err_t ina236_create(ina236_handle_t *handle, driver_ina236_config_t *config)
{
    esp_err_t err = ESP_OK;
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ina236_dev_t *ina236 = (ina236_dev_t *)calloc(1, sizeof(ina236_dev_t));
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
    err = ina236_reg_init(ina236);
    if (err != ESP_OK) {
        free(ina236);
        return err;
    }

    err = ina236_read_all_reg(ina236);
    if (err != ESP_OK) {
        free(ina236);
        return err;
    }
    if (ina236->alert_en) {
        gpio_config_t io_conf = {0};
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << ina236->alert_pin);
        io_conf.pull_up_en = 1;
        err = gpio_config(&io_conf);
        if (err == ESP_OK) {
            // Install ISR service only if not already installed
            static bool isr_service_installed = false;
            if (!isr_service_installed) {
                esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
                if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
                    isr_service_installed = true;
                } else {
                    err = ret;
                }
            }
            if (err == ESP_OK) {
                gpio_isr_handler_add(ina236->alert_pin, ina236->cb, NULL);
            }
        }
    }
    *handle = (ina236_handle_t)ina236;
    return err;
}

esp_err_t ina236_delete(ina236_handle_t handle)
{
    ina236_dev_t *ina236 = (ina236_dev_t *)handle;
    i2c_bus_device_delete(&ina236->i2c_dev);
    if (ina236->alert_en) {
        gpio_isr_handler_remove(ina236->alert_pin);
    }
    free(ina236);
    return ESP_OK;
}

esp_err_t ina236_get_voltage(ina236_handle_t handle, float *volt)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(volt, ESP_ERR_INVALID_ARG, TAG, "volt is NULL");

    ina236_dev_t *ina236 = (ina236_dev_t *)handle;
    uint16_t buffer = 0;
    esp_err_t ret = ina236_read_reg(ina236, INA236_REG_VBUS, &buffer);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read voltage");
    *volt = buffer * 0.0016f;
    return ESP_OK;
}

esp_err_t ina236_get_current(ina236_handle_t handle, float *curr)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(curr, ESP_ERR_INVALID_ARG, TAG, "curr is NULL");

    ina236_dev_t *ina236 = (ina236_dev_t *)handle;
    uint16_t buffer = 0;
    esp_err_t ret = ina236_read_reg(ina236, INA236_REG_VSHUNT, &buffer);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read current");
    *curr = (int16_t)buffer / 3970.0f;
    return ESP_OK;
}

esp_err_t ina236_clear_mask(ina236_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    ina236_dev_t *ina236 = (ina236_dev_t *)handle;
    ina236_reg_mask_t mask = { 0 };
    esp_err_t ret = ina236_read_reg(ina236, INA236_REG_MASK, (uint16_t *)&mask);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read mask register");
    return ESP_OK;
}
