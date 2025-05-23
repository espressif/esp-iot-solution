/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "drv10987.h"
#include "driver/gpio.h"
#include "drv10987_reg.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "DRV10987";

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t i2c_addr;
    drv10987_config_reg_t drv10987_config_reg;
    gpio_num_t dir_pin;
} drv10987_t;

static esp_err_t drv10987_read_reg(drv10987_t *drv10987, uint8_t reg, uint16_t *data)
{
    uint8_t drv10987_data[2] = {0};
    esp_err_t ret = i2c_bus_read_bytes(drv10987->i2c_dev, reg, 2, &drv10987_data[0]);
    *data = ((drv10987_data[0] << 8 | drv10987_data[1]));
    return ret;
}

static esp_err_t drv10987_write_reg(drv10987_t *drv10987, uint8_t reg, uint16_t data)
{
    uint8_t buffer[2] = {0};
    buffer[0] = data >> 8;
    buffer[1] = data;
    esp_err_t ret = i2c_bus_write_bytes(drv10987->i2c_dev, reg, 2, buffer);
    return ret;
}

esp_err_t drv10987_create(drv10987_handle_t *handle, drv10987_config_t *config)
{
    esp_err_t err = ESP_OK;
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Config is NULL");

    drv10987_t *drv10987 = (drv10987_t *)calloc(1, sizeof(drv10987_t));
    ESP_RETURN_ON_FALSE(drv10987, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for drv10987");

    drv10987->i2c_addr = config->dev_addr;
    drv10987->i2c_dev = i2c_bus_device_create(config->bus, drv10987->i2c_addr, i2c_bus_get_current_clk_speed(config->bus));

    drv10987->dir_pin = config->dir_pin;
    gpio_config_t io_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ull << drv10987->dir_pin),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    err = gpio_config(&io_cfg);

    if (drv10987->i2c_dev == NULL) {
        free(drv10987);
        return ESP_FAIL;
    }

    *handle = (drv10987_handle_t)drv10987;
    return err;
}

esp_err_t drv10987_delete(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_OK;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    i2c_bus_device_delete(&drv10987->i2c_dev);
    free(drv10987);
    return ESP_OK;
}

esp_err_t drv10987_disable_control(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    return drv10987_write_reg(drv10987, DRV10987_EECTRL_REGISTER_ADDR, EECTRL_REGISTER_VALUE(0X01));                                                           /*!< Set the highest bit of the electrl register to 1 to turn off the motor */
}

esp_err_t drv10987_enable_control(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    return drv10987_write_reg(drv10987, DRV10987_EECTRL_REGISTER_ADDR, EECTRL_REGISTER_VALUE(0X00));                                                           /*!< Set the highest bit of the electrl register to 0 to turn on the motor */
}

esp_err_t drv10987_write_config_register(drv10987_handle_t handle, drv10987_config_register_addr_t config_register_addr, void *config)
{
    if (handle == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    uint16_t value = *(uint16_t *)config;                                                                                                                      /*!< Get the value of config without saving it separately to drv10987.  */
    return drv10987_write_reg(drv10987, config_register_addr, value);
}

drv10987_eeprom_status_t drv10987_read_eeprom_status(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    uint16_t result = 0;
    esp_err_t ret;
    ret = drv10987_read_reg(drv10987, DRV10987_EEPROM_PROGRAMMING2_REGISTER_ADDR, &result);                                                                    /*!< Get EEPROM status bits */

    if (ret != ESP_OK) {
        return EEPROM_NO_READY;
    }

    if (result == 0x0001) {
        return EEPROM_READY;                                                                                                                                   /*!< 1: EEPROM ready for read/write access */
    }
    return EEPROM_NO_READY;                                                                                                                                    /*!< 0: EEPROM not ready for read/write access */
}

esp_err_t drv10987_write_access_to_eeprom(drv10987_handle_t handle, uint8_t maximum_failure_count)
{
    if (handle == NULL || maximum_failure_count < 1) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    esp_err_t ret;
    drv10987_disable_control(handle);
    ret = drv10987_write_reg(drv10987, DRV10987_EEPROM_PROGRAMMING1_REGISTER_ADDR, 0x0000);
    ret = drv10987_write_reg(drv10987, DRV10987_EEPROM_PROGRAMMING1_REGISTER_ADDR, 0XC0DE);

    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    while (maximum_failure_count--) {
        if (drv10987_read_eeprom_status(handle) == EEPROM_READY) {                                                                                             /*!< Within the maximum number of failures, an attempt is made to determine if the eeprom can allow reads and writes. */
            return ESP_OK;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    ESP_LOGE(TAG, "EEPROM not ready for read/write access");
    return ESP_FAIL;
}

esp_err_t drv10987_mass_write_acess_to_eeprom(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    return drv10987_write_reg(drv10987, DRV10987_EEPROM_PROGRAMMING5_REGISTER_ADDR, EEPROM_PROGRAMMING5_REGISTER_VALUE(0x02, true, 0x00, true));               /*!< Config EEPROM_PROGRAMMING5_REGISTER */
}

esp_err_t drv10987_write_speed(drv10987_handle_t handle, uint16_t speed)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    if (speed >= 511) {
        speed = 511;                                                                                                                                           /*!< In I2C mode, the speed is 9 bits, so the maximum value is 511 */
    }

    return drv10987_write_reg(drv10987, DRV10987_SPEEDCTRL_REGISTER_ADDR, 0x8000 | (speed & 0x1FF));
}

esp_err_t drv10987_read_driver_status(drv10987_handle_t handle, drv10987_fault_t *fault)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    return drv10987_read_reg(drv10987, DRV10987_FAULT_ADDR, &fault->code);                                                                                     /*!< Get Fail Code */
}

drv10987_config_reg_t drv10987_read_config_register(drv10987_handle_t handle)
{
    drv10987_config_reg_t invalid_config;
    memset(&invalid_config, 0, sizeof(drv10987_config_reg_t));

    if (handle == NULL) {
        return invalid_config;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    drv10987_read_reg(drv10987, DRV10987_CONFIG1_ADDR, &drv10987->drv10987_config_reg.config1.result);
    drv10987_read_reg(drv10987, DRV10987_CONFIG2_ADDR, &drv10987->drv10987_config_reg.config2.result);
    drv10987_read_reg(drv10987, DRV10987_CONFIG3_ADDR, &drv10987->drv10987_config_reg.config3.result);
    drv10987_read_reg(drv10987, DRV10987_CONFIG4_ADDR, &drv10987->drv10987_config_reg.config4.result);
    drv10987_read_reg(drv10987, DRV10987_CONFIG5_ADDR, &drv10987->drv10987_config_reg.config5.result);
    drv10987_read_reg(drv10987, DRV10987_CONFIG6_ADDR, &drv10987->drv10987_config_reg.config6.result);
    drv10987_read_reg(drv10987, DRV10987_CONFIG7_ADDR, &drv10987->drv10987_config_reg.config7.result);

    return drv10987->drv10987_config_reg;
}

esp_err_t drv10987_read_phase_resistance_and_kt(drv10987_handle_t handle, float *phase_res, float *kt)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    esp_err_t ret;

    ret = drv10987_read_reg(drv10987, DRV10987_CONFIG1_ADDR, &drv10987->drv10987_config_reg.config1.result);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    ret = drv10987_read_reg(drv10987, DRV10987_CONFIG2_ADDR, &drv10987->drv10987_config_reg.config2.result);
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }

    *phase_res = 1.0f * (drv10987->drv10987_config_reg.config1.rm_value << drv10987->drv10987_config_reg.config1.rm_shift) * 0.00967f;
    *kt = 1.0f * (drv10987->drv10987_config_reg.config2.kt_value << drv10987->drv10987_config_reg.config2.kt_shift) / 1090.0f * 1000.0f;

    return ESP_OK;

}

esp_err_t drv10987_clear_driver_fault(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    return drv10987_write_reg(drv10987, DRV10987_FAULT_ADDR, 0xFFFF);                                                                                          /*!< Clear all faults. It is important to note that all faults should be set to 1 to clear them. */
}

esp_err_t drv10987_set_direction_uvw(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    return gpio_set_level(drv10987->dir_pin, 0);                                                                                                               /*!< When low, phase driving sequence is U → V → W. */
}

esp_err_t drv10987_set_direction_uwv(drv10987_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    drv10987_t *drv10987 = (drv10987_t *)handle;
    return gpio_set_level(drv10987->dir_pin, 1);                                                                                                               /*!< When low, phase driving sequence is U → V → W. */
}

esp_err_t drv10987_eeprom_test(drv10987_handle_t handle, drv10987_config1_t config1, drv10987_config2_t config2)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    drv10987_t *drv10987 = (drv10987_t *)handle;
    ret = drv10987_write_access_to_eeprom(handle, 5);                                                                                                          /*!< Enable access to EEPROM and check if EEPROM is ready for read/write access. */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "EEPROM not ready for read/write access");
        return ESP_FAIL;
    }

    ret = drv10987_write_config_register(handle, DRV10987_CONFIG1_ADDR, &config1);                                                                             /*!< Write data to the CONFIG1 register. */
    ret = drv10987_write_config_register(handle, DRV10987_CONFIG2_ADDR, &config2);
    ret = drv10987_mass_write_acess_to_eeprom(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to EEPROM");
        return ESP_FAIL;
    }

    while (1) {
        if (drv10987_read_eeprom_status(handle) == EEPROM_READY) {
            ESP_LOGI(TAG, "EEPROM is now updated with the contents of the shadow registers.");
            break;
        }
    }

    drv10987->drv10987_config_reg = drv10987_read_config_register(handle);
    ESP_LOGI(TAG, "CONFIG1:0x%04x, Read:0x%04x", config1.result, drv10987->drv10987_config_reg.config1.result);
    ESP_LOGI(TAG, "CONFIG1 RMValue:%02x", drv10987->drv10987_config_reg.config1.rm_value);
    ESP_LOGI(TAG, "CONFIG1 RMShift:%02x", drv10987->drv10987_config_reg.config1.rm_shift);
    ESP_LOGI(TAG, "CONFIG1 ClkCycleAdjust:%02x", drv10987->drv10987_config_reg.config1.clk_cycle_adjust);
    ESP_LOGI(TAG, "CONFIG1 FGCycle:%02x", drv10987->drv10987_config_reg.config1.fg_cycle);
    ESP_LOGI(TAG, "CONFIG1 FGOLSel:%02x", drv10987->drv10987_config_reg.config1.fg_open_loop_sel);
    ESP_LOGI(TAG, "CONFIG1 SSMConfig:%02x", drv10987->drv10987_config_reg.config1.ssm_config);
    ESP_LOGI(TAG, "--------------------------");
    ESP_LOGI(TAG, "CONFIG2:0x%04x, Read:0x%04x", config2.result, drv10987->drv10987_config_reg.config2.result);
    ESP_LOGI(TAG, "CONFIG2 TCtrlAdvValue:%02x", drv10987->drv10987_config_reg.config2.tctrladv);
    ESP_LOGI(TAG, "CONFIG2 TCtrlAdvShift:%02x", drv10987->drv10987_config_reg.config2.tctrladv_shift);
    ESP_LOGI(TAG, "CONFIG2 CommAdvMode:%02x", drv10987->drv10987_config_reg.config2.commadv_mode);
    ESP_LOGI(TAG, "CONFIG2 KtValue:%02x", drv10987->drv10987_config_reg.config2.kt_value);
    ESP_LOGI(TAG, "CONFIG2 KtShift:%02x", drv10987->drv10987_config_reg.config2.kt_shift);
    return ESP_OK;
}
