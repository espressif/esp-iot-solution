/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <assert.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "bq27220.h"
#include "priv_include/bq27220_reg.h"
#include "priv_include/bq27220_data_memory.h"

static const char *TAG = "bq27220";

// device addr
#define BQ27220_I2C_ADDRESS 0x55
// device id
#define BQ27220_DEVICE_ID 0x0220
#define delay_ms(x) vTaskDelay(pdMS_TO_TICKS(x))
#define BQ27220_CFG_UPDATE_ENTER_DELAY_MS 1100
#define BQ27220_CFG_UPDATE_EXIT_SETTLE_MS 2000
#define BQ27220_CFG_UPDATE_POLL_MS 20
#define BQ27220_CFG_UPDATE_POLL_COUNT 100
#define BQ27220_DM_BLOCK_MAX_SIZE 32
#define BQ27220_DM_ACCESS_DELAY_MS 10
#define BQ27220_DM_READY_POLL_US 1000
#define BQ27220_DM_READY_POLL_COUNT 20
#define BQ27220_I2C_PACKET_GAP_US 100
#define BQ27220_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#define PRINT_ERROR(err) \
    if (err != ESP_OK) { \
        ESP_LOGE(TAG, "(%s:%d) Error: %s", __func__, __LINE__, esp_err_to_name(err)); \
    }

typedef struct {
    i2c_bus_device_handle_t i2c_device_handle;
} bq27220_data_t;

typedef struct {
    const char *name;
    uint16_t address;
    uint16_t value;
    uint8_t size;
} bq27220_profile_field_t;

typedef struct {
    uint16_t address;
    uint8_t size;
    uint8_t data[BQ27220_DM_BLOCK_MAX_SIZE];
} bq27220_profile_block_t;

typedef struct {
    bq27220_data_memory_name_t first_field;
    uint8_t size;
} bq27220_profile_block_desc_t;

static esp_err_t bq27220_read_u16_checked(bq27220_handle_t bq_handle, uint8_t address, uint16_t *value)
{
    ESP_RETURN_ON_FALSE(bq_handle != NULL && value != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid read arguments");
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;
    esp_err_t ret = i2c_bus_read_bytes(bq_data->i2c_device_handle, address, 2, (uint8_t *)value);
    ESP_RETURN_ON_ERROR(ret, TAG, "Read command 0x%02x failed", address);
    return ESP_OK;
}

static uint16_t bq27220_read_u16(bq27220_handle_t bq_handle, uint8_t address)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    uint16_t value = 0;
    (void)bq27220_read_u16_checked(bq_handle, address, &value);
    return value;
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

static esp_err_t bq27220_read_dm_block(bq27220_handle_t bq_handle, uint16_t address, uint8_t *data, uint8_t size)
{
    ESP_RETURN_ON_FALSE(bq_handle != NULL && data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid block read arguments");
    ESP_RETURN_ON_FALSE(size > 0 && size <= BQ27220_DM_BLOCK_MAX_SIZE, ESP_ERR_INVALID_SIZE, TAG, "Invalid block read size");
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;
    uint8_t select[2] = {address & 0xFF, (address >> 8) & 0xFF};

    esp_err_t ret = i2c_bus_write_bytes(bq_data->i2c_device_handle, COMMAND_SELECT_SUBCLASS, sizeof(select), select);
    ESP_RETURN_ON_ERROR(ret, TAG, "Select data memory block 0x%04x failed", address);

    uint8_t buffer[BQ27220_DM_BLOCK_MAX_SIZE + 2];
    uint16_t echoed_address = 0;
    for (uint8_t i = 0; i < BQ27220_DM_READY_POLL_COUNT; ++i) {
        esp_rom_delay_us(BQ27220_DM_READY_POLL_US);
        ret = i2c_bus_read_bytes(bq_data->i2c_device_handle, COMMAND_SELECT_SUBCLASS, size + 2, buffer);
        ESP_RETURN_ON_ERROR(ret, TAG, "Read data memory block 0x%04x failed", address);
        esp_rom_delay_us(BQ27220_I2C_PACKET_GAP_US);
        echoed_address = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
        if (echoed_address == address) {
            memcpy(data, &buffer[2], size);
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "Data memory address timeout: requested=0x%04x echoed=0x%04x", address, echoed_address);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t bq27220_write_dm_block(bq27220_handle_t bq_handle, uint16_t address, const uint8_t *data, uint8_t size)
{
    ESP_RETURN_ON_FALSE(bq_handle != NULL && data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid block write arguments");
    ESP_RETURN_ON_FALSE(size > 0 && size <= BQ27220_DM_BLOCK_MAX_SIZE, ESP_ERR_INVALID_SIZE, TAG, "Invalid block write size");
    bq27220_data_t *bq_data = (bq27220_data_t *)bq_handle;
    uint8_t buffer[BQ27220_DM_BLOCK_MAX_SIZE + 2] = {address & 0xFF, (address >> 8) & 0xFF};
    memcpy(&buffer[2], data, size);

    esp_err_t ret = i2c_bus_write_bytes(bq_data->i2c_device_handle, COMMAND_SELECT_SUBCLASS, size + 2, buffer);
    ESP_RETURN_ON_ERROR(ret, TAG, "Stage data memory block 0x%04x failed", address);
    delay_ms(BQ27220_DM_ACCESS_DELAY_MS);
    uint8_t commit[2] = {bq27220_get_checksum(buffer, size + 2), size + 4};
    ret = i2c_bus_write_bytes(bq_data->i2c_device_handle, COMMAND_MAC_DATA_SUM, sizeof(commit), commit);
    ESP_RETURN_ON_ERROR(ret, TAG, "Commit data memory block 0x%04x failed", address);
    delay_ms(BQ27220_DM_ACCESS_DELAY_MS);
    return ESP_OK;
}

static uint16_t bq27220_read_control_data(bq27220_handle_t bq_handle, uint16_t control)
{
    bq27220_control(bq_handle, control);
    delay_ms(15);
    return bq27220_read_u16(bq_handle, COMMAND_MAC_DATA);
}

uint16_t bq27220_get_fw_version(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    return bq27220_read_control_data(bq_handle, CONTROL_FW_VERSION);
}

uint16_t bq27220_get_hw_version(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    return bq27220_read_control_data(bq_handle, CONTROL_HW_VERSION);
}

esp_err_t bq27220_set_parameter_u16(bq27220_handle_t bq_handle, uint16_t address, uint16_t value)
{
    ESP_RETURN_ON_FALSE(bq_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    uint8_t data[2] = {(value >> 8) & 0xFF, value & 0xFF};
    return bq27220_write_dm_block(bq_handle, address, data, (uint8_t)sizeof(data));
}

static esp_err_t bq27220_get_parameter_checked(bq27220_handle_t bq_handle, uint16_t address, uint8_t size, uint16_t *value)
{
    ESP_RETURN_ON_FALSE(bq_handle != NULL && value != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid parameter read arguments");
    ESP_RETURN_ON_FALSE(size == 1 || size == 2, ESP_ERR_INVALID_ARG, TAG, "Invalid parameter size");
    uint8_t data[2];
    ESP_RETURN_ON_ERROR(bq27220_read_dm_block(bq_handle, address, data, size), TAG, "Read parameter 0x%04x failed", address);
    *value = size == 2 ? ((uint16_t)data[0] << 8) | data[1] : data[0];
    return ESP_OK;
}

uint16_t bq27220_get_parameter_u16(bq27220_handle_t bq_handle, uint16_t address)
{
    ESP_RETURN_ON_FALSE(bq_handle, 0, TAG, "Invalid handle");
    uint16_t value = 0;
    (void)bq27220_get_parameter_checked(bq_handle, address, 2, &value);
    return value;
}

esp_err_t bq27220_seal(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    operation_status_t status = {};
    bq27220_get_operation_status(bq_handle, &status);
    if (status.SEC != OPERATION_STATUS_SEC_SEALED) {
        bq27220_control(bq_handle, CONTROL_SEALED);
    }
    delay_ms(CONFIG_BQ27220_SEAL_SETTLE_MS);
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
    if (status.SEC != OPERATION_STATUS_SEC_SEALED) {
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

static esp_err_t bq27220_enter_full_access(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_ERROR(bq27220_control(bq_handle, FULLACCESSKEY), TAG, "First full-access key failed");
    delay_ms(10);
    ESP_RETURN_ON_ERROR(bq27220_control(bq_handle, FULLACCESSKEY), TAG, "Second full-access key failed");
    delay_ms(1000);

    uint16_t raw_status = 0;
    ESP_RETURN_ON_ERROR(bq27220_read_u16_checked(bq_handle, COMMAND_OPERATION_STATUS, &raw_status), TAG, "Read full-access status failed");
    operation_status_t status = {};
    memcpy(&status, &raw_status, sizeof(status));
    ESP_RETURN_ON_FALSE(status.SEC == OPERATION_STATUS_SEC_FULL, ESP_FAIL, TAG, "Full access rejected, SEC=%u", status.SEC);
    return ESP_OK;
}

#define BQ27220_PROFILE_FIELD_COUNT 26
static const bq27220_profile_block_desc_t s_profile_read_blocks[] = {
    {.first_field = BQ_DM_SELF_DISCHARGE_RATE, .size = 7},
    {.first_field = BQ_DM_GAUGING_CONFIGURATION, .size = 31},
    {.first_field = BQ_DM_FIXED_EDV_2, .size = 25},
};
static const bq27220_profile_block_desc_t s_profile_write_blocks[] = {
    {.first_field = BQ_DM_SELF_DISCHARGE_RATE, .size = 1},
    {.first_field = BQ_DM_NEAR_FULL, .size = 4},
    {.first_field = BQ_DM_GAUGING_CONFIGURATION, .size = 6},
    {.first_field = BQ_DM_EMF, .size = 12},
    {.first_field = BQ_DM_FIXED_EDV_0, .size = 2},
    {.first_field = BQ_DM_FIXED_EDV_1, .size = 2},
    {.first_field = BQ_DM_FIXED_EDV_2, .size = 2},
    {.first_field = BQ_DM_VOLTAGE_0_DOD, .size = 22},
};

#define BQ27220_PROFILE_READ_BLOCK_COUNT BQ27220_ARRAY_SIZE(s_profile_read_blocks)
#define BQ27220_PROFILE_WRITE_BLOCK_COUNT BQ27220_ARRAY_SIZE(s_profile_write_blocks)

static void bq27220_init_profile_blocks(bq27220_profile_block_t *blocks, const bq27220_profile_block_desc_t *descs,
                                        size_t block_count)
{
    for (size_t i = 0; i < block_count; ++i) {
        blocks[i] = (bq27220_profile_block_t) {
            .address = bq27220_dm_table[descs[i].first_field].address,
            .size = descs[i].size,
        };
    }
}

static esp_err_t bq27220_read_profile_blocks(bq27220_handle_t handle,
                                             bq27220_profile_block_t blocks[BQ27220_PROFILE_READ_BLOCK_COUNT])
{
    bq27220_init_profile_blocks(blocks, s_profile_read_blocks, BQ27220_PROFILE_READ_BLOCK_COUNT);
    for (size_t i = 0; i < BQ27220_PROFILE_READ_BLOCK_COUNT; ++i) {
        esp_err_t ret = bq27220_read_dm_block(handle, blocks[i].address, blocks[i].data, blocks[i].size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Profile block read failed at 0x%04x: %s", blocks[i].address, esp_err_to_name(ret));
            return ret;
        }
    }
    return ESP_OK;
}

static bq27220_profile_block_t *bq27220_find_profile_block(bq27220_profile_block_t *blocks, size_t block_count,
                                                           uint16_t address, uint8_t size)
{
    for (size_t i = 0; i < block_count; ++i) {
        uint32_t block_end = (uint32_t)blocks[i].address + blocks[i].size;
        if (address >= blocks[i].address && (uint32_t)address + size <= block_end) {
            return &blocks[i];
        }
    }
    return NULL;
}

static esp_err_t bq27220_get_block_field(bq27220_profile_block_t blocks[BQ27220_PROFILE_READ_BLOCK_COUNT],
                                         const bq27220_profile_field_t *field, uint16_t *value)
{
    ESP_RETURN_ON_FALSE(field != NULL && value != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid profile field arguments");
    bq27220_profile_block_t *block = bq27220_find_profile_block(blocks, BQ27220_PROFILE_READ_BLOCK_COUNT, field->address, field->size);
    ESP_RETURN_ON_FALSE(block != NULL, ESP_ERR_NOT_FOUND, TAG, "No block for profile field %s", field->name);
    size_t offset = field->address - block->address;
    *value = field->size == 2 ? ((uint16_t)block->data[offset] << 8) | block->data[offset + 1] : block->data[offset];
    return ESP_OK;
}

static esp_err_t bq27220_set_block_field(bq27220_profile_block_t blocks[BQ27220_PROFILE_WRITE_BLOCK_COUNT],
                                         const bq27220_profile_field_t *field)
{
    ESP_RETURN_ON_FALSE(field != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid profile field");
    bq27220_profile_block_t *block = bq27220_find_profile_block(blocks, BQ27220_PROFILE_WRITE_BLOCK_COUNT, field->address, field->size);
    ESP_RETURN_ON_FALSE(block != NULL, ESP_ERR_NOT_FOUND, TAG, "No block for profile field %s", field->name);
    size_t offset = field->address - block->address;
    if (field->size == 2) {
        block->data[offset] = (field->value >> 8) & 0xFF;
        block->data[offset + 1] = field->value & 0xFF;
    } else {
        block->data[offset] = field->value & 0xFF;
    }
    return ESP_OK;
}

static void bq27220_get_profile_fields(const bq27220_config_t *config, bq27220_profile_field_t fields[BQ27220_PROFILE_FIELD_COUNT])
{
    const parameter_cedv_t *cedv = config->cedv;
    size_t i = 0;
#define PROFILE_FIELD(field_name, dm_name, field_value, field_size) \
    fields[i++] = (bq27220_profile_field_t){field_name, bq27220_dm_table[dm_name].address, field_value, field_size}
    PROFILE_FIELD("gauging_config", BQ_DM_GAUGING_CONFIGURATION, config->cfg->full, 2);
    PROFILE_FIELD("full_charge_cap", BQ_DM_FULL_CHARGE_CAPACITY, cedv->full_charge_cap, 2);
    PROFILE_FIELD("design_cap", BQ_DM_DESIGN_CAPACITY, cedv->design_cap, 2);
    PROFILE_FIELD("near_full", BQ_DM_NEAR_FULL, cedv->near_full, 2);
    PROFILE_FIELD("self_discharge_rate", BQ_DM_SELF_DISCHARGE_RATE, cedv->self_discharge_rate, 1);
    PROFILE_FIELD("reserve_cap", BQ_DM_RESERVE_CAPACITY, cedv->reserve_cap, 2);
    PROFILE_FIELD("EMF", BQ_DM_EMF, cedv->EMF, 2);
    PROFILE_FIELD("C0", BQ_DM_C0, cedv->C0, 2);
    PROFILE_FIELD("R0", BQ_DM_R0, cedv->R0, 2);
    PROFILE_FIELD("T0", BQ_DM_T0, cedv->T0, 2);
    PROFILE_FIELD("R1", BQ_DM_R1, cedv->R1, 2);
    PROFILE_FIELD("TC_C1", BQ_DM_TC, ((uint16_t)cedv->TC << 8) | cedv->C1, 2);
    PROFILE_FIELD("DOD0", BQ_DM_VOLTAGE_0_DOD, cedv->DOD0, 2);
    PROFILE_FIELD("DOD10", BQ_DM_VOLTAGE_10_DOD, cedv->DOD10, 2);
    PROFILE_FIELD("DOD20", BQ_DM_VOLTAGE_20_DOD, cedv->DOD20, 2);
    PROFILE_FIELD("DOD30", BQ_DM_VOLTAGE_30_DOD, cedv->DOD30, 2);
    PROFILE_FIELD("DOD40", BQ_DM_VOLTAGE_40_DOD, cedv->DOD40, 2);
    PROFILE_FIELD("DOD50", BQ_DM_VOLTAGE_50_DOD, cedv->DOD50, 2);
    PROFILE_FIELD("DOD60", BQ_DM_VOLTAGE_60_DOD, cedv->DOD60, 2);
    PROFILE_FIELD("DOD70", BQ_DM_VOLTAGE_70_DOD, cedv->DOD70, 2);
    PROFILE_FIELD("DOD80", BQ_DM_VOLTAGE_80_DOD, cedv->DOD80, 2);
    PROFILE_FIELD("DOD90", BQ_DM_VOLTAGE_90_DOD, cedv->DOD90, 2);
    PROFILE_FIELD("DOD100", BQ_DM_VOLTAGE_100_DOD, cedv->DOD100, 2);
    PROFILE_FIELD("EDV0", BQ_DM_FIXED_EDV_0, cedv->EDV0, 2);
    PROFILE_FIELD("EDV1", BQ_DM_FIXED_EDV_1, cedv->EDV1, 2);
    PROFILE_FIELD("EDV2", BQ_DM_FIXED_EDV_2, cedv->EDV2, 2);
#undef PROFILE_FIELD
    assert(i == BQ27220_PROFILE_FIELD_COUNT);
}

static esp_err_t bq27220_wait_cfg_update(bq27220_handle_t handle, bool expected)
{
    for (uint32_t i = 0; i < BQ27220_CFG_UPDATE_POLL_COUNT; ++i) {
        uint16_t raw_status = 0;
        ESP_RETURN_ON_ERROR(bq27220_read_u16_checked(handle, COMMAND_OPERATION_STATUS, &raw_status), TAG,
                            "Read operation status while waiting for CFGUPDATE failed");
        operation_status_t status = {};
        memcpy(&status, &raw_status, sizeof(status));
        if (status.CFGUPDATE == expected) {
            ESP_LOGI(TAG, "CFGUPDATE=%u confirmed, OperationStatus=0x%04x", expected, raw_status);
            return ESP_OK;
        }
        delay_ms(BQ27220_CFG_UPDATE_POLL_MS);
    }
    ESP_LOGE(TAG, "Timed out waiting for CFGUPDATE=%u", expected);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t bq27220_write_profile(bq27220_handle_t handle, const bq27220_config_t *config)
{
    bq27220_profile_field_t fields[BQ27220_PROFILE_FIELD_COUNT];
    bq27220_profile_block_t blocks[BQ27220_PROFILE_WRITE_BLOCK_COUNT];
    bq27220_get_profile_fields(config, fields);
    bq27220_init_profile_blocks(blocks, s_profile_write_blocks, BQ27220_PROFILE_WRITE_BLOCK_COUNT);
    for (size_t i = 0; i < BQ27220_PROFILE_FIELD_COUNT; ++i) {
        esp_err_t ret = bq27220_set_block_field(blocks, &fields[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Profile staging failed at %s (0x%04x): %s", fields[i].name, fields[i].address, esp_err_to_name(ret));
            return ret;
        }
    }
    for (size_t i = 0; i < BQ27220_PROFILE_WRITE_BLOCK_COUNT; ++i) {
        esp_err_t ret = bq27220_write_dm_block(handle, blocks[i].address, blocks[i].data, blocks[i].size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Profile block write failed at 0x%04x: %s", blocks[i].address, esp_err_to_name(ret));
            return ret;
        }
    }
    ESP_LOGI(TAG, "Battery profile written in %u targeted blocks", (unsigned)BQ27220_PROFILE_WRITE_BLOCK_COUNT);
    return ESP_OK;
}

static esp_err_t bq27220_verify_profile(bq27220_handle_t handle, const bq27220_config_t *config, bool include_learned_capacity)
{
    bq27220_profile_field_t fields[BQ27220_PROFILE_FIELD_COUNT];
    bq27220_profile_block_t blocks[BQ27220_PROFILE_READ_BLOCK_COUNT];
    bq27220_get_profile_fields(config, fields);
    ESP_RETURN_ON_ERROR(bq27220_read_profile_blocks(handle, blocks), TAG, "Failed to read profile for verification");
    size_t verified = 0;
    for (size_t i = 0; i < BQ27220_PROFILE_FIELD_COUNT; ++i) {
        if (!include_learned_capacity && fields[i].address == bq27220_dm_table[BQ_DM_FULL_CHARGE_CAPACITY].address) {
            continue;
        }
        uint16_t actual = 0;
        esp_err_t ret = bq27220_get_block_field(blocks, &fields[i], &actual);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Profile verify read failed at %s (0x%04x): %s", fields[i].name, fields[i].address,
                     esp_err_to_name(ret));
            return ret;
        }
        if (actual != fields[i].value) {
            ESP_LOGW(TAG, "Profile verify mismatch at %s (0x%04x): expected=%u actual=%u", fields[i].name,
                     fields[i].address, fields[i].value, actual);
            return ESP_ERR_INVALID_STATE;
        }
        ++verified;
    }
    ESP_LOGI(TAG, "Battery profile readback verified (%u fields, learned_capacity=%s)", (unsigned)verified,
             include_learned_capacity ? "included" : "excluded");
    return ESP_OK;
}

bq27220_handle_t bq27220_create(const bq27220_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, NULL, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(config->i2c_bus != NULL, NULL, TAG, "Invalid i2c_bus");
    ESP_RETURN_ON_FALSE(config->cedv != NULL, NULL, TAG, "Invalid cedv");
    ESP_RETURN_ON_FALSE(config->cfg != NULL, NULL, TAG, "Invalid gauging config");

    esp_err_t ret = ESP_OK;
    bq27220_data_t *handle = (bq27220_data_t *)calloc(1, sizeof(bq27220_data_t));
    if (!handle) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }
    handle->i2c_device_handle = i2c_bus_device_create(config->i2c_bus, BQ27220_I2C_ADDRESS, 0);
    ESP_GOTO_ON_FALSE(handle->i2c_device_handle, 0, err, TAG, "i2c_bus_device_create failed");

    uint16_t data = bq27220_read_control_data(handle, CONTROL_DEVICE_NUMBER);
    ESP_GOTO_ON_FALSE(data == BQ27220_DEVICE_ID, 0, err, TAG, "Invalid Device Number %04x != 0x0220", data);

    data = bq27220_get_fw_version(handle);
    ESP_LOGI(TAG, "Firmware Version %04x", data);
    data = bq27220_get_hw_version(handle);
    ESP_LOGI(TAG, "Hardware Version %04x", data);

    ESP_GOTO_ON_ERROR(bq27220_unseal(handle), err, TAG, "Failed to unseal");

    ret = bq27220_verify_profile(handle, config, false);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Skip battery profile update: verification=full");
        ESP_GOTO_ON_ERROR(bq27220_seal(handle), err, TAG, "Failed to seal after profile check");
        return handle;
    }
    ESP_LOGW(TAG, "Stored battery profile requires update: %s", esp_err_to_name(ret));
    ESP_LOGW(TAG, "Start updating battery profile");
    ESP_GOTO_ON_ERROR(bq27220_enter_full_access(handle), err, TAG, "Failed to enter full access");
    ESP_GOTO_ON_ERROR(bq27220_control(handle, CONTROL_ENTER_CFG_UPDATE), err, TAG, "Failed to request CONFIG UPDATE mode");
    delay_ms(BQ27220_CFG_UPDATE_ENTER_DELAY_MS);
    ESP_GOTO_ON_ERROR(bq27220_wait_cfg_update(handle, true), exit_cfg_update, TAG, "Failed to enter CONFIG UPDATE mode");
    ESP_GOTO_ON_ERROR(bq27220_write_profile(handle, config), exit_cfg_update, TAG, "Battery profile write failed");
    ESP_GOTO_ON_ERROR(bq27220_verify_profile(handle, config, true), exit_cfg_update, TAG, "Battery profile immediate verification failed");
    ESP_GOTO_ON_ERROR(bq27220_control(handle, CONTROL_EXIT_CFG_UPDATE_REINIT), err, TAG, "Failed to exit CONFIG UPDATE mode");
    delay_ms(BQ27220_CFG_UPDATE_EXIT_SETTLE_MS);
    ESP_GOTO_ON_ERROR(bq27220_wait_cfg_update(handle, false), err, TAG, "CONFIG UPDATE exit did not complete");
    ESP_GOTO_ON_ERROR(bq27220_seal(handle), err, TAG, "Failed to seal after profile update");
    ESP_LOGI(TAG, "Battery profile update success: verification=full");
    return handle;

exit_cfg_update:
    if (bq27220_control(handle, CONTROL_EXIT_CFG_UPDATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to leave CONFIG UPDATE mode after write error");
    }
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
