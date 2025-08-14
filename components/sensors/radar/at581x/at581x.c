/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "at581x.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "at581x_reg.h"

const static char *TAG = "AT581X";

typedef struct {
    i2c_bus_device_handle_t bus_device_inst;
    at581x_i2c_config_t config;
} at581x_dev_t;

static inline esp_err_t at581x_write_reg(at581x_dev_handle_t dev, uint8_t reg_addr, uint8_t data)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev handle can't be NULL");
    at581x_dev_t *sens = (at581x_dev_t *)dev;
    return i2c_bus_write_byte(sens->bus_device_inst, reg_addr, data);
}

static inline esp_err_t at581x_read_reg(at581x_dev_handle_t dev, uint8_t reg_addr, uint8_t *reg_value)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "dev handle can't be NULL");
    ESP_RETURN_ON_FALSE(reg_value, ESP_ERR_INVALID_ARG, TAG, "reg_value pointer can't be NULL");
    at581x_dev_t *sens = (at581x_dev_t *)dev;
    return i2c_bus_read_byte(sens->bus_device_inst, reg_addr, reg_value);
}

esp_err_t at581x_soft_reset(at581x_dev_handle_t handle)
{
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RSTN_SW, AT581X_RSTN_SW_SOFT_RESET), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RSTN_SW, AT581X_RSTN_SW_RELEASE), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_freq_point(at581x_dev_handle_t handle, uint8_t freq_0x5f, uint8_t freq_0x60)
{
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_VCO_CAP_DR, 0xC0 | (0x10 << AT581X_VCO_CAP_DR_BIT)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_VCO_1, freq_0x5f), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_VCO_2, freq_0x60), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_self_check_time(at581x_dev_handle_t handle, uint32_t self_check_time)
{
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_SELF_CK_1, (uint8_t)(self_check_time)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_SELF_CK_2, (uint8_t)(self_check_time >> 8)), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_trig_base_time(at581x_dev_handle_t handle, uint64_t base_time)
{
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_BASE_1, (uint8_t)(base_time)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_BASE_2, (uint8_t)(base_time >> 8)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_BASE_3, (uint8_t)(base_time >> 16)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_BASE_4, (uint8_t)(base_time >> 24)), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_protect_time(at581x_dev_handle_t handle, uint32_t protect_time)
{
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_UNDET_PORT_1, (uint8_t)(protect_time)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_UNDET_PORT_2, (uint8_t)(protect_time >> 8)), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_distance(at581x_dev_handle_t handle, uint8_t pwr_setting, uint32_t delta, at581x_gain_t gain)
{
    uint8_t reg_burst_time = 0x00;
    uint8_t reg_work_time = 0x00;

    reg_burst_time |= BIT(AT581X_BURST_TIME_DR_BIT);
    reg_burst_time |= ((pwr_setting & 0x0F) << AT581X_BURST_TIME_REG_BIT);

    reg_work_time |= BIT(AT581X_WORK_TIME_DR_BIT);
    reg_work_time |= ((pwr_setting >> 4) << AT581X_WORK_TIME_REG_BIT);

    reg_burst_time |= BIT(AT581X_POWER_TH_DR_BIT);
    reg_work_time |= BIT(AT581X_POWER_TH_REG_BIT);

    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_CNF_BURST_TIME_REG, reg_burst_time), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_CNF_WORK_TIME_REG, reg_work_time), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_SIGNAL_DET_THR_1_1, (uint8_t)(delta)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_SIGNAL_DET_THR_1_2, (uint8_t)(delta >> 8)), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RX_TIA_AGIN_REG, (0x0B | (gain << AT581X_TIA_GAIN_DR_BIT))), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_trig_keep_time(at581x_dev_handle_t handle, uint64_t keep_time)
{
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_OUT_CTRL, 0x01), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_OUT_1, (uint8_t)(keep_time)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_OUT_2, (uint8_t)(keep_time >> 8)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_OUT_3, (uint8_t)(keep_time >> 16)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_TIME_FLAG_OUT_4, (uint8_t)(keep_time >> 24)), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_light_sensor_threshold(at581x_dev_handle_t handle,
                                            bool onoff,
                                            uint32_t light_sensor_value_high,
                                            uint32_t light_sensor_value_low,
                                            uint32_t light_sensor_iniverse)
{
    if (onoff) {
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_CNF_LIGHT_DR, AT581X_CNF_LIGHT_ON), TAG, "I2C read/write error");
    } else {
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_CNF_LIGHT_DR, AT581X_CNF_LIGHT_OFF), TAG, "I2C read/write error");
    }

    uint8_t reg_data = 0x00;
    reg_data = ((light_sensor_value_low >> 8) << AT581X_LIGHT_REF_LOW_BIT);
    reg_data |= ((light_sensor_value_high >> 8) << AT581X_LIGHT_REF_HIGH_BIT);
    reg_data |= (light_sensor_iniverse << AT581X_LIGHT_INV_BIT);

    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_LIGHT_REF_LOW, (uint8_t)(light_sensor_value_low)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_LIGHT_REF_HIGH, (uint8_t)(light_sensor_value_high)), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_LIGHT_REF, reg_data), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_set_rf_onoff(at581x_dev_handle_t handle, bool onoff)
{
    if (onoff) {
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RX_RF2, AT581X_RX_RF2_ON), TAG, "I2C read/write error");
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RX_RF1, AT581X_RX_RF1_ON), TAG, "I2C read/write error");
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RF_CTRL, AT581X_RF_CTRL_ON), TAG, "I2C read/write error");
    } else {
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RX_RF2, AT581X_RX_RF2_OFF), TAG, "I2C read/write error");
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RX_RF1, AT581X_RX_RF1_OFF), TAG, "I2C read/write error");
        ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_RF_CTRL, AT581X_RF_CTRL_OFF), TAG, "I2C read/write error");
    }

    return ESP_OK;
}

esp_err_t at581x_set_detect_window(at581x_dev_handle_t handle, uint8_t window_length, uint8_t window_threshold)
{
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_POWER_DET_WIN_LEN, window_length), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(at581x_write_reg(handle, AT581X_POWER_DET_WIN_THR, window_threshold), TAG, "I2C read/write error");

    return ESP_OK;
}

esp_err_t at581x_register_interrupt_callback(at581x_dev_handle_t handle, at581x_interrupt_callback_t callback)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle can't be NULL");

    at581x_dev_t *sens = (at581x_dev_t *) handle;

    /* Interrupt pin is not selected */
    if (sens->config.int_gpio_num == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_ARG;
    }

    sens->config.interrupt_callback = callback;

    if (callback != NULL) {
        ret = gpio_install_isr_service(0);
        /* ISR service can be installed from user before, then it returns invalid state */
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "GPIO ISR install failed");
            return ret;
        }
        /* Add GPIO ISR handler */
        ret = gpio_intr_enable(sens->config.int_gpio_num);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR install failed");
        ret = gpio_isr_handler_add(sens->config.int_gpio_num, (gpio_isr_t)sens->config.interrupt_callback, &sens->config);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR install failed");
    } else {
        /* Remove GPIO ISR handler */
        ret = gpio_isr_handler_remove(sens->config.int_gpio_num);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR remove handler failed");
        ret = gpio_intr_disable(sens->config.int_gpio_num);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR disable failed");
        gpio_uninstall_isr_service();
    }

    return ESP_OK;
}

esp_err_t at581x_new_sensor(const at581x_i2c_config_t *i2c_conf, at581x_dev_handle_t *handle_out)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "%-15s: %d.%d.%d", CHIP_NAME, AT581X_VER_MAJOR, AT581X_VER_MINOR, AT581X_VER_PATCH);

    ESP_RETURN_ON_FALSE(i2c_conf, ESP_ERR_INVALID_ARG, TAG, "invalid device config pointer");
    ESP_RETURN_ON_FALSE(handle_out, ESP_ERR_INVALID_ARG, TAG, "invalid device handle pointer");

    at581x_dev_t *handle = calloc(1, sizeof(at581x_dev_t));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "memory allocation for device handler failed");

    // Create I2C device
    handle->bus_device_inst = i2c_bus_device_create(i2c_conf->bus_inst, i2c_conf->i2c_addr, 0);
    if (handle->bus_device_inst == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C device");
        free(handle);
        return ESP_FAIL;
    }

    /* Save config */
    memcpy(&handle->config, i2c_conf, sizeof(at581x_i2c_config_t));

    const at581x_default_cfg_t def_cfg = ATH581X_INITIALIZATION_CONFIG();
    const at581x_default_cfg_t *p_def_cfg = &def_cfg;
    if (i2c_conf->def_conf != NULL) {
        p_def_cfg = i2c_conf->def_conf;
    }

    /* Prepare pin for touch interrupt */
    if (handle->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = 1,
            .intr_type = (handle->config.interrupt_level ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(handle->config.int_gpio_num)
        };
        ret = gpio_config(&int_gpio_config);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "GPIO config failed");

        /* Register interrupt callback */
        if (handle->config.interrupt_callback) {
            at581x_register_interrupt_callback((at581x_dev_handle_t)handle, handle->config.interrupt_callback);
        }
    }

    if (handle) {
        ESP_RETURN_ON_ERROR(at581x_set_distance((at581x_dev_handle_t)handle, p_def_cfg->power_cfg, p_def_cfg->delta_cfg, p_def_cfg->gain_cfg), TAG, "set_distance error");
        ESP_RETURN_ON_ERROR(at581x_set_trig_base_time((at581x_dev_handle_t)handle, p_def_cfg->trig_base_tm_cfg), TAG, "set_trig_base_time error");
        ESP_RETURN_ON_ERROR(at581x_set_trig_keep_time((at581x_dev_handle_t)handle, p_def_cfg->trig_keep_tm_cfg), TAG, "set_trig_keep_time error");
        ESP_RETURN_ON_ERROR(at581x_set_self_check_time((at581x_dev_handle_t)handle, p_def_cfg->self_check_tm_cfg), TAG, "set_self_check_time error");
        ESP_RETURN_ON_ERROR(at581x_set_protect_time((at581x_dev_handle_t)handle, p_def_cfg->protect_tm_cfg), TAG, "set_protect_time error");

        ESP_RETURN_ON_ERROR(at581x_write_reg((at581x_dev_handle_t)handle, 0x55, 0x04), TAG, "I2C read/write error");
        ESP_RETURN_ON_ERROR(at581x_soft_reset((at581x_dev_handle_t)handle), TAG, "soft_reset error");
    }
    *handle_out = handle;

err:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (0x%x)! Sensor AT581x initialization failed!", ret);
        if (handle) {
            at581x_del_sensor(handle);
        }
    }

    return ret;
}

esp_err_t at581x_del_sensor(at581x_dev_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid device handle pointer");

    at581x_dev_t *sens = (at581x_dev_t *)handle;
    at581x_register_interrupt_callback(handle, NULL);

    if (sens->bus_device_inst) {
        i2c_bus_device_delete(&sens->bus_device_inst);
    }

    free(handle);
    return ESP_OK;
}
