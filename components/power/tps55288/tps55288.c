/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include <stdlib.h>
#include "esp_check.h"
#include "esp_log.h"
#include "tps55288.h"

static const char *TAG = "tps55288";

#define TPS55288_TIMEOUT_MS         1000

#define TPS55288_REG_REF_LSB        0x00
#define TPS55288_REG_REF_MSB        0x01
#define TPS55288_REG_IOUT_LIMIT     0x02
#define TPS55288_REG_VOUT_FS        0x04
#define TPS55288_REG_CDC            0x05
#define TPS55288_REG_MODE           0x06
#define TPS55288_REG_STATUS         0x07

#define TPS55288_IOUT_LIMIT_EN      (1U << 7)
#define TPS55288_VOUT_FS_FB         (1U << 7)
#define TPS55288_VOUT_FS_INTFB_MASK 0x03
#define TPS55288_CDC_OCP_MASK       (1U << 6)
#define TPS55288_MODE_OE            (1U << 7)

/* VOUT = 0.8 V + REF * step. REF is a 10-bit code. */
#define TPS55288_VOUT_MIN_MV        800
#define TPS55288_REF_MAX_CODE       1023

/* IOUT_LIMIT bit6:0 is 0.5 mV per LSB. I(mA) = code * 500 / R(mohm). */
#define TPS55288_IOUT_LIMIT_LSB_UV  500
#define TPS55288_IOUT_LIMIT_MAX_CODE 127

struct tps55288_dev_t {
    i2c_master_dev_handle_t dev;
    uint32_t sense_resistor_mohm;
    uint16_t vout_step_mv;
    uint32_t vout_max_mv;
};

typedef struct {
    uint8_t intfb;
    uint16_t step_mv;
} tps55288_fb_info_t;

static const tps55288_fb_info_t s_fb_info[] = {
    [TPS55288_FB_RATIO_DEFAULT] = {.intfb = 0x03, .step_mv = 20},
    [TPS55288_FB_RATIO_0_0564] = {.intfb = 0x03, .step_mv = 20},
    [TPS55288_FB_RATIO_0_0752] = {.intfb = 0x02, .step_mv = 15},
    [TPS55288_FB_RATIO_0_1128] = {.intfb = 0x01, .step_mv = 10},
    [TPS55288_FB_RATIO_0_2256] = {.intfb = 0x00, .step_mv = 5},
};

esp_err_t tps55288_read_reg(tps55288_handle_t handle, uint8_t reg, uint8_t *data)
{
    ESP_RETURN_ON_FALSE(handle && data, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return i2c_master_transmit_receive(handle->dev, &reg, 1, data, 1, TPS55288_TIMEOUT_MS);
}

esp_err_t tps55288_write_reg(tps55288_handle_t handle, uint8_t reg, uint8_t data)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(handle->dev, write_buf, sizeof(write_buf), TPS55288_TIMEOUT_MS);
}

static const tps55288_fb_info_t *fb_info_get(tps55288_fb_ratio_t ratio)
{
    if ((unsigned)ratio >= (sizeof(s_fb_info) / sizeof(s_fb_info[0]))) {
        return NULL;
    }
    if (s_fb_info[ratio].step_mv == 0) {
        return NULL;
    }
    return &s_fb_info[ratio];
}

static esp_err_t fb_ratio_apply(tps55288_handle_t handle, tps55288_fb_ratio_t ratio)
{
    const tps55288_fb_info_t *info = fb_info_get(ratio);
    uint8_t vout_fs = 0;

    ESP_RETURN_ON_FALSE(info, ESP_ERR_INVALID_ARG, TAG, "invalid feedback ratio");

    ESP_RETURN_ON_ERROR(tps55288_read_reg(handle, TPS55288_REG_VOUT_FS, &vout_fs), TAG, "read VOUT_FS failed");
    vout_fs &= (uint8_t)~(TPS55288_VOUT_FS_FB | TPS55288_VOUT_FS_INTFB_MASK);
    vout_fs |= info->intfb;
    ESP_RETURN_ON_ERROR(tps55288_write_reg(handle, TPS55288_REG_VOUT_FS, vout_fs), TAG, "write VOUT_FS failed");

    handle->vout_step_mv = info->step_mv;
    handle->vout_max_mv = TPS55288_VOUT_MIN_MV + (TPS55288_REF_MAX_CODE * info->step_mv);
    return ESP_OK;
}

esp_err_t tps55288_create(const tps55288_config_t *config, tps55288_handle_t *ret_handle)
{
    esp_err_t err;

    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(config->sense_resistor_mohm != 0, ESP_ERR_INVALID_ARG, TAG, "sense resistor is 0");
    ESP_RETURN_ON_FALSE(fb_info_get(config->fb_ratio), ESP_ERR_INVALID_ARG, TAG, "invalid feedback ratio");

    tps55288_handle_t tps = calloc(1, sizeof(*tps));
    ESP_RETURN_ON_FALSE(tps, ESP_ERR_NO_MEM, TAG, "no memory for device");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = config->scl_speed_hz,
    };
    err = i2c_master_bus_add_device(config->bus, &dev_config, &tps->dev);
    if (err != ESP_OK) {
        free(tps);
        return err;
    }

    tps->sense_resistor_mohm = config->sense_resistor_mohm;
    err = fb_ratio_apply(tps, config->fb_ratio);
    if (err != ESP_OK) {
        i2c_master_bus_rm_device(tps->dev);
        free(tps);
        return err;
    }

    *ret_handle = tps;
    return ESP_OK;
}

esp_err_t tps55288_delete(tps55288_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    esp_err_t err = i2c_master_bus_rm_device(handle->dev);
    free(handle);
    return err;
}

/* OCP_MASK must be 0 while OE or Current_Limit_EN rises from 0 to 1, then put back. */
static esp_err_t ocp_mask_clear(tps55288_handle_t handle, uint8_t *cdc, bool *was_set)
{
    ESP_RETURN_ON_ERROR(tps55288_read_reg(handle, TPS55288_REG_CDC, cdc), TAG, "read CDC failed");
    *was_set = (*cdc & TPS55288_CDC_OCP_MASK) != 0;
    if (!*was_set) {
        return ESP_OK;
    }
    *cdc &= (uint8_t)~TPS55288_CDC_OCP_MASK;
    return tps55288_write_reg(handle, TPS55288_REG_CDC, *cdc);
}

static esp_err_t ocp_mask_restore(tps55288_handle_t handle, uint8_t cdc, bool was_set)
{
    if (!was_set) {
        return ESP_OK;
    }
    cdc |= TPS55288_CDC_OCP_MASK;
    return tps55288_write_reg(handle, TPS55288_REG_CDC, cdc);
}

esp_err_t tps55288_set_vout_mv(tps55288_handle_t handle, uint32_t vout_mv)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    if ((vout_mv < TPS55288_VOUT_MIN_MV) || (vout_mv > handle->vout_max_mv)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Writing REF_MSB loads both bytes into the DAC. Writing REF_LSB alone does not. */
    uint16_t ref = (vout_mv - TPS55288_VOUT_MIN_MV) / handle->vout_step_mv;
    ESP_RETURN_ON_ERROR(tps55288_write_reg(handle, TPS55288_REG_REF_LSB, ref & 0xFF), TAG, "write REF_LSB failed");
    ESP_RETURN_ON_ERROR(tps55288_write_reg(handle, TPS55288_REG_REF_MSB, (ref >> 8) & 0x03), TAG, "write REF_MSB failed");
    ESP_LOGI(TAG, "VOUT target %" PRIu32 " mV, REF=0x%03" PRIX16, vout_mv, ref);
    return ESP_OK;
}

esp_err_t tps55288_set_iout_limit_ma(tps55288_handle_t handle, uint32_t limit_ma)
{
    uint8_t previous = 0;
    uint8_t cdc = 0;
    uint32_t steps;
    uint32_t actual_ma;
    bool enabling;
    bool ocp_mask_was_set = false;

    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    uint32_t limit_max_ma = (TPS55288_IOUT_LIMIT_MAX_CODE * TPS55288_IOUT_LIMIT_LSB_UV) / handle->sense_resistor_mohm;
    if (limit_ma > limit_max_ma) {
        return ESP_ERR_INVALID_ARG;
    }

    /* code = I(mA) * R(mohm) / 500 uV, rounded to the nearest LSB */
    steps = (limit_ma * handle->sense_resistor_mohm + (TPS55288_IOUT_LIMIT_LSB_UV / 2)) / TPS55288_IOUT_LIMIT_LSB_UV;
    if (steps > TPS55288_IOUT_LIMIT_MAX_CODE) {
        steps = TPS55288_IOUT_LIMIT_MAX_CODE;
    }
    actual_ma = (steps * TPS55288_IOUT_LIMIT_LSB_UV) / handle->sense_resistor_mohm;

    ESP_RETURN_ON_ERROR(tps55288_read_reg(handle, TPS55288_REG_IOUT_LIMIT, &previous), TAG, "read IOUT_LIMIT failed");
    enabling = (previous & TPS55288_IOUT_LIMIT_EN) == 0;
    if (enabling) {
        ESP_RETURN_ON_ERROR(ocp_mask_clear(handle, &cdc, &ocp_mask_was_set), TAG, "clear OCP_MASK failed");
    }

    ESP_RETURN_ON_ERROR(tps55288_write_reg(handle, TPS55288_REG_IOUT_LIMIT, TPS55288_IOUT_LIMIT_EN | (uint8_t)steps),
                        TAG, "write IOUT_LIMIT failed");

    if (enabling) {
        ESP_RETURN_ON_ERROR(ocp_mask_restore(handle, cdc, ocp_mask_was_set), TAG, "restore OCP_MASK failed");
    }

    ESP_LOGI(TAG, "IOUT limit %" PRIu32 " mA", actual_ma);
    return ESP_OK;
}

esp_err_t tps55288_enable_output(tps55288_handle_t handle, bool enable)
{
    uint8_t cdc = 0;
    uint8_t mode = 0;
    bool ocp_mask_was_set = false;

    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    if (enable) {
        ESP_RETURN_ON_ERROR(ocp_mask_clear(handle, &cdc, &ocp_mask_was_set), TAG, "clear OCP_MASK failed");
    }

    ESP_RETURN_ON_ERROR(tps55288_read_reg(handle, TPS55288_REG_MODE, &mode), TAG, "read MODE failed");
    if (enable) {
        mode |= TPS55288_MODE_OE;
    } else {
        mode &= (uint8_t)~TPS55288_MODE_OE;
    }
    ESP_RETURN_ON_ERROR(tps55288_write_reg(handle, TPS55288_REG_MODE, mode), TAG, "write MODE failed");

    if (enable) {
        ESP_RETURN_ON_ERROR(ocp_mask_restore(handle, cdc, ocp_mask_was_set), TAG, "restore OCP_MASK failed");
    }

    ESP_LOGI(TAG, "Output %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

esp_err_t tps55288_read_status(tps55288_handle_t handle, uint8_t *status)
{
    return tps55288_read_reg(handle, TPS55288_REG_STATUS, status);
}
