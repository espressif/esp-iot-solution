/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "bq27220.h"
#include "priv_include/bq27220_reg.h"
#include "priv_include/bq27220_data_memory.h"

static const char *TAG = "bq27220";

// device addr
#define BQ27220_I2C_ADDRESS 0x55
// device id
#define BQ27220_DEVICE_ID 0x0220
#define delay_ms(x) vTaskDelay(pdMS_TO_TICKS(x))

#define WAIT_OPERATION_DONE(handle, condition) do { \
    operation_status_t status = {}; \
    uint32_t timeout = 20; \
    while ((condition) && (timeout-- > 0)) { \
        bq27220_get_operation_status(handle, &status); \
        delay_ms(100); \
    } \
    if (timeout == 0) { \
        ESP_LOGE(TAG, "Timeout"); \
        ret = false; \
    } \
    ret = true; \
} while(0)

#define PRINT_ERROR(err) \
    if (err != ESP_OK) { \
        ESP_LOGE(TAG, "(%s:%d) Error: %s", __func__, __LINE__, esp_err_to_name(err)); \
    }

typedef struct {
    i2c_bus_device_handle_t i2c_device_handle;
} bq27220_data_t;

static uint16_t bq27220_read_u16(bq27220_handle_t bq_handle, uint8_t address)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;
    uint16_t buf = 0;

    esp_err_t res = i2c_bus_read_bytes(bq_data->i2c_device_handle, address, 2, (uint8_t *)&buf);
    PRINT_ERROR(res);
    return buf;
}

static esp_err_t bq27220_control(bq27220_handle_t bq_handle, uint16_t control)
{
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;
    esp_err_t res = i2c_bus_write_bytes(bq_data->i2c_device_handle, COMMAND_CONTROL, 2, (uint8_t *)&control);
    PRINT_ERROR(res);
    return res;
}

static uint8_t bq27220_get_checksum(uint8_t *data, uint16_t len)
{
    ESP_RETURN_ON_FALSE(data != NULL, 0, TAG, "Invalid data");
    uint8_t ret = 0;
    for (uint16_t i = 0; i < len; i++) {
        ret += data[i];
    }
    return 0xFF - ret;
}

static uint16_t getDeviceNumber(bq27220_handle_t bq_handle)
{
    uint16_t devid = 0;
    // Request device number
    bq27220_control(bq_handle, CONTROL_DEVICE_NUMBER);
    delay_ms(15);
    // Read id data from MAC scratch space
    devid = bq27220_read_u16(bq_handle, COMMAND_MAC_DATA);
    return devid;
}

uint16_t bq27220_get_fw_version(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    uint16_t ret = 0;
    bq27220_control(bq_handle, CONTROL_FW_VERSION);
    delay_ms(15);
    ret = bq27220_read_u16(bq_handle, COMMAND_MAC_DATA);
    return ret;
}

uint16_t bq27220_get_hw_version(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    uint16_t ret = 0;
    bq27220_control(bq_handle, CONTROL_HW_VERSION);
    delay_ms(15);
    ret = bq27220_read_u16(bq_handle, COMMAND_MAC_DATA);
    return ret;
}

esp_err_t bq27220_set_parameter_u16(bq27220_handle_t bq_handle, uint16_t address, uint16_t value)
{
    ESP_RETURN_ON_FALSE(bq_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;
    esp_err_t res;
    uint8_t buffer[4];

    buffer[0] = address & 0xFF;
    buffer[1] = (address >> 8) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
    res = i2c_bus_write_bytes(bq_data->i2c_device_handle, COMMAND_SELECT_SUBCLASS, 4, buffer);
    PRINT_ERROR(res);
    delay_ms(10);

    uint8_t checksum = bq27220_get_checksum(buffer, 4);
    buffer[0] = checksum;
    buffer[1] = 6;
    res = i2c_bus_write_bytes(bq_data->i2c_device_handle, COMMAND_MAC_DATA_SUM, 2, buffer);
    PRINT_ERROR(res);
    delay_ms(10);
    return res;
}

uint16_t bq27220_get_parameter_u16(bq27220_handle_t bq_handle, uint16_t address)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;
    esp_err_t res;
    uint8_t buffer[4];

    buffer[0] = address & 0xFF;
    buffer[1] = (address >> 8) & 0xFF;
    res = i2c_bus_write_bytes(bq_data->i2c_device_handle, COMMAND_SELECT_SUBCLASS, 2, buffer);
    PRINT_ERROR(res);
    delay_ms(10);

    res = i2c_bus_read_bytes(bq_data->i2c_device_handle, COMMAND_MAC_DATA, 2, buffer);
    PRINT_ERROR(res);
    return ((uint16_t)buffer[0] << 8) | buffer[1];
}

esp_err_t bq27220_seal(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    operation_status_t status = {};
    bq27220_get_operation_status(bq_handle, &status);
    if (status.SEC == OPERATION_STATUS_SEC_SEALED) {
        return ESP_OK; // Already sealed
    }
    bq27220_control(bq_handle, CONTROL_SEALED);
    delay_ms(10);
    bq27220_get_operation_status(bq_handle, &status);
    if (status.SEC == OPERATION_STATUS_SEC_SEALED) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t bq27220_unseal(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    operation_status_t status = {};
    bq27220_get_operation_status(bq_handle, &status);
    if (status.SEC == OPERATION_STATUS_SEC_UNSEALED) {
        return ESP_OK; // Already unsealed
    }
    bq27220_control(bq_handle, UNSEALKEY1);
    delay_ms(10);
    bq27220_control(bq_handle, UNSEALKEY2);
    delay_ms(10);
    bq27220_get_operation_status(bq_handle, &status);
    if (status.SEC == OPERATION_STATUS_SEC_UNSEALED) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

bq27220_handle_t bq27220_create(const bq27220_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, NULL, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(config->i2c_bus != NULL, NULL, TAG, "Invalid i2c_bus");
    ESP_RETURN_ON_FALSE(config->cedv != NULL, NULL, TAG, "Invalid cedv");
    ESP_RETURN_ON_FALSE(config->cfg != NULL, NULL, TAG, "Invalid gauging config");

    esp_err_t ret = 0;
    bq27220_data_t *handle = (bq27220_data_t *)calloc(1, sizeof(bq27220_data_t));
    if (!handle) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }
    handle->i2c_device_handle = i2c_bus_device_create(config->i2c_bus, BQ27220_I2C_ADDRESS, 0);
    ESP_GOTO_ON_FALSE(handle->i2c_device_handle, 0, err, TAG, "i2c_bus_device_create failed");

    uint16_t data = getDeviceNumber(handle);
    ESP_GOTO_ON_FALSE(data == BQ27220_DEVICE_ID, 0, err, TAG, "Invalid Device Number %04x != 0x0220", data);

    data = bq27220_get_fw_version(handle);
    ESP_LOGI(TAG, "Firmware Version %04x", data);
    data = bq27220_get_hw_version(handle);
    ESP_LOGI(TAG, "Hardware Version %04x", data);

    ESP_GOTO_ON_ERROR(bq27220_unseal(handle), err, TAG, "Failed to unseal");

    uint16_t design_cap = bq27220_get_design_capacity(handle);
    uint16_t EMF = bq27220_get_parameter_u16(handle, dm_table[BQ_DM_EMF].address);
    uint16_t t0 = bq27220_get_parameter_u16(handle, dm_table[BQ_DM_T0].address);
    uint16_t dod20 = bq27220_get_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_20_DOD].address);
    ESP_LOGI(TAG, "Design Capacity: %d, EMF: %d, T0: %d, DOD20: %d", design_cap, EMF, t0, dod20);
    const parameter_cedv_t *cedv = config->cedv;
    if (cedv->design_cap == design_cap && cedv->EMF == EMF && cedv->T0 == t0 && cedv->DOD20 == dod20) {
        ESP_LOGI(TAG, "Skip battery profile update");
        return handle;
    }
    ESP_LOGW(TAG, "Start updating battery profile");
    delay_ms(10);
    bq27220_control(handle, CONTROL_ENTER_CFG_UPDATE);
    WAIT_OPERATION_DONE(handle, (status.CFGUPDATE == true));
    ESP_GOTO_ON_FALSE(ret == true, 0, err, TAG, "Battery profile update failed");

    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_GAUGING_CONFIGURATION].address, config->cfg->full);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_FULL_CHARGE_CAPACITY].address, cedv->full_charge_cap);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_DESIGN_CAPACITY].address, cedv->design_cap);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_NEAR_FULL].address, cedv->near_full);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_SELF_DISCHARGE_RATE].address, cedv->self_discharge_rate);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_RESERVE_CAPACITY].address, cedv->reserve_cap);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_EMF].address, cedv->EMF);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_C0].address, cedv->C0);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_R0].address, cedv->R0);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_T0].address, cedv->T0);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_R1].address, cedv->R1);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_TC].address, (cedv->TC) << 8 | cedv->C1);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_0_DOD].address, cedv->DOD0);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_10_DOD].address, cedv->DOD10);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_20_DOD].address, cedv->DOD20);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_30_DOD].address, cedv->DOD30);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_40_DOD].address, cedv->DOD40);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_50_DOD].address, cedv->DOD40);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_60_DOD].address, cedv->DOD60);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_70_DOD].address, cedv->DOD70);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_80_DOD].address, cedv->DOD80);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_90_DOD].address, cedv->DOD90);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_VOLTAGE_100_DOD].address, cedv->DOD100);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_FIXED_EDV_0].address, cedv->EDV0);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_FIXED_EDV_1].address, cedv->EDV1);
    bq27220_set_parameter_u16(handle, dm_table[BQ_DM_FIXED_EDV_2].address, cedv->EDV2);
    bq27220_control(handle, CONTROL_EXIT_CFG_UPDATE_REINIT);
    delay_ms(10);
    WAIT_OPERATION_DONE(handle, (status.CFGUPDATE != true));
    ESP_GOTO_ON_FALSE(ret == true, 0, err, TAG, "Battery profile update failed");
    delay_ms(10);
    design_cap = bq27220_get_design_capacity(handle);
    if (cedv->design_cap == design_cap) {
        ESP_LOGI(TAG, "Battery profile update success");
    } else {
        ESP_LOGE(TAG, "Battery profile update failed");
        goto err;
    }
    bq27220_seal(handle);
    return handle;
err:
    if (handle->i2c_device_handle) {
        i2c_bus_device_delete(&handle->i2c_device_handle);
    }
    free(handle);
    return NULL;
}

esp_err_t bq27220_delete(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;

    if (bq_data->i2c_device_handle) {
        ESP_RETURN_ON_ERROR(i2c_bus_device_delete(&bq_data->i2c_device_handle), TAG, "Failed to delete i2c device");
    }
    free(bq_handle);
    return ESP_OK;
}

uint16_t bq27220_get_voltage(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_VOLTAGE);
}

int16_t bq27220_get_current(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_CURRENT);
}

int16_t bq27220_get_avgcurrent(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_AVERAGE_CURRENT);
}

esp_err_t bq27220_get_battery_status(bq27220_handle_t bq_handle, battery_status_t *battery_status)
{
    uint16_t data = bq27220_read_u16(bq_handle, COMMAND_BATTERY_STATUS);
    *(uint16_t *)battery_status = data;
    return ESP_OK;
}

esp_err_t bq27220_get_operation_status(bq27220_handle_t bq_handle, operation_status_t *operation_status)
{
    uint16_t data = bq27220_read_u16(bq_handle, COMMAND_OPERATION_STATUS);
    *(uint16_t *)operation_status = data;
    return ESP_OK;
}

uint16_t bq27220_get_temperature(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_TEMPERATURE);
}

uint16_t bq27220_get_cycle_count(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_CYCLE_COUNT);
}

uint16_t bq27220_get_full_charge_capacity(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_FULL_CHARGE_CAPACITY);
}

uint16_t bq27220_get_design_capacity(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_DESIGN_CAPACITY);
}

uint16_t bq27220_get_remaining_capacity(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_REMAINING_CAPACITY);
}

uint16_t bq27220_get_state_of_charge(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_STATE_OF_CHARGE);
}

uint16_t bq27220_get_state_of_health(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_STATE_OF_HEALTH);
}

uint16_t bq27220_get_charge_voltage(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_CHARGE_VOLTAGE);
}

uint16_t bq27220_get_charge_current(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_CHARGE_CURRENT);
}

int16_t bq27220_get_average_power(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_AVERAGE_POWER);
}

uint16_t bq27220_get_time_to_empty(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_TIME_TO_EMPTY);
}

uint16_t bq27220_get_time_to_full(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_TIME_TO_FULL);
}

int16_t bq27220_get_maxload_current(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_MAX_LOAD_CURRENT);
}

int16_t bq27220_get_standby_current(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_STANDBY_CURRENT);
}
