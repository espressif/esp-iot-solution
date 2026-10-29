/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "as5600.h"
#include "driver/i2c_master.h"

/** Device I2C address (7-bit)*/
#define AS5600_I2C_ADDRESS              0x36
#define AS5600_I2C_SPEED_DEFAULT_HZ     100000
#define AS5600_I2C_TIMEOUT_DEFAULT_MS   100

/* AS5600 register addresses*/
#define AS5600_REG_ZPOS_H         0x01
#define AS5600_REG_ZPOS_L         0x02
#define AS5600_REG_CONF_H         0x07
#define AS5600_REG_CONF_L         0x08
#define AS5600_REG_RAW_ANGLE_H    0x0C
#define AS5600_REG_RAW_ANGLE_L    0x0D
#define AS5600_REG_ANGLE_H        0x0E
#define AS5600_REG_ANGLE_L        0x0F
#define AS5600_REG_STATUS         0x0B

/* CONF register bit fields */
#define AS5600_CONF_MASK          0x3FFF  /* 14-bit */
#define AS5600_CONF_PM_SHIFT      0
#define AS5600_CONF_PM_MASK       0x0003U
#define AS5600_CONF_HYST_SHIFT    2
#define AS5600_CONF_HYST_MASK     0x000CU
#define AS5600_CONF_OUTS_SHIFT    4
#define AS5600_CONF_OUTS_MASK     0x0030U
#define AS5600_CONF_PWMF_SHIFT    6
#define AS5600_CONF_PWMF_MASK     0x00C0U
#define AS5600_CONF_SF_SHIFT      8
#define AS5600_CONF_SF_MASK       0x0300U
#define AS5600_CONF_FTH_SHIFT     10
#define AS5600_CONF_FTH_MASK      0x1C00U
#define AS5600_CONF_WD_SHIFT      13
#define AS5600_CONF_WD_MASK       0x2000U

/* STATUS register bits */
#define AS5600_STATUS_MD          0x20    /* Magnet detected */
#define AS5600_STATUS_ML          0x10    /* Magnet too weak */
#define AS5600_STATUS_MH          0x08    /* Magnet too strong */

/** AS5600 12-bit resolution, 0-4095 corresponds to 0-360 degrees / 0-2π radians */
#define AS5600_RESOLUTION         4096
#define AS5600_DEGREES_PER_LSB    360.0f / AS5600_RESOLUTION
#define AS5600_RADIANS_PER_LSB    6.28318530717958647692f / AS5600_RESOLUTION

#define AS5600_ANGLE_MASK         0x0FFF  /* 12-bit mask */

typedef struct as5600_dev_t {
    i2c_master_dev_handle_t dev_handle;
    uint32_t timeout_ms;
} as5600_dev_t;

esp_err_t as5600_new_sensor(i2c_master_bus_handle_t bus, const as5600_i2c_config_t *i2c_conf, as5600_handle_t *handle_out)
{
    if (bus == NULL || i2c_conf == NULL || handle_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    as5600_dev_t *dev = (as5600_dev_t *) calloc(1, sizeof(as5600_dev_t));
    if (dev == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint32_t scl_speed_hz = i2c_conf->scl_speed_hz ? i2c_conf->scl_speed_hz : AS5600_I2C_SPEED_DEFAULT_HZ;
    uint32_t timeout_ms = i2c_conf->timeout_ms ? i2c_conf->timeout_ms : AS5600_I2C_TIMEOUT_DEFAULT_MS;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AS5600_I2C_ADDRESS,
        .scl_speed_hz = scl_speed_hz,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_config, &dev->dev_handle);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

    dev->timeout_ms = timeout_ms;
    *handle_out = dev;
    return ESP_OK;
}

esp_err_t as5600_del_sensor(as5600_handle_t sensor)
{
    if (sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    as5600_dev_t *dev = sensor;
    esp_err_t ret = i2c_master_bus_rm_device(dev->dev_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    free(dev);
    return ESP_OK;
}

static inline esp_err_t s_as5600_read_reg(as5600_handle_t sensor, uint8_t reg, uint8_t *data, size_t len)
{
    as5600_dev_t *dev = sensor;
    return i2c_master_transmit_receive(dev->dev_handle, &reg, 1, data, len, dev->timeout_ms);
}

static esp_err_t s_as5600_write_reg(as5600_handle_t sensor, const uint8_t *data, size_t len)
{
    as5600_dev_t *dev = sensor;
    return i2c_master_transmit(dev->dev_handle, data, len, dev->timeout_ms);
}

static esp_err_t s_as5600_read_angle_regs(as5600_handle_t sensor, uint8_t reg_h, uint16_t *angle)
{
    uint8_t buf[2];
    esp_err_t ret = s_as5600_read_reg(sensor, reg_h, buf, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    *angle = (((uint16_t)buf[0] << 8) | buf[1]) & AS5600_ANGLE_MASK;
    return ESP_OK;
}

static esp_err_t s_as5600_get_angle_raw_no_offset(as5600_handle_t sensor, uint16_t *raw_angle)
{
    return s_as5600_read_angle_regs(sensor, AS5600_REG_RAW_ANGLE_H, raw_angle);
}

esp_err_t as5600_get_angle_raw(as5600_handle_t sensor, uint16_t *raw_angle)
{
    if (sensor == NULL || raw_angle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return s_as5600_read_angle_regs(sensor, AS5600_REG_ANGLE_H, raw_angle);
}

esp_err_t as5600_get_angle_degrees(as5600_handle_t sensor, float *degrees)
{
    if (sensor == NULL || degrees == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t raw;
    esp_err_t ret = s_as5600_read_angle_regs(sensor, AS5600_REG_ANGLE_H, &raw);
    if (ret != ESP_OK) {
        return ret;
    }
    *degrees = (float) raw * AS5600_DEGREES_PER_LSB;
    return ESP_OK;
}

esp_err_t as5600_get_angle_radians(as5600_handle_t sensor, float *radians)
{
    if (sensor == NULL || radians == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t raw;
    esp_err_t ret = s_as5600_read_angle_regs(sensor, AS5600_REG_ANGLE_H, &raw);
    if (ret != ESP_OK) {
        return ret;
    }
    *radians = (float) raw * AS5600_RADIANS_PER_LSB;
    return ESP_OK;
}

esp_err_t as5600_get_magnet_status(as5600_handle_t sensor, as5600_magnet_status_t *status)
{
    if (sensor == NULL || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t reg;
    esp_err_t ret = s_as5600_read_reg(sensor, AS5600_REG_STATUS, &reg, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!(reg & AS5600_STATUS_MD)) {
        *status = AS5600_MAGNET_NOT_DETECTED;
    } else if (reg & AS5600_STATUS_MH) {
        *status = AS5600_MAGNET_TOO_STRONG;
    } else if (reg & AS5600_STATUS_ML) {
        *status = AS5600_MAGNET_TOO_WEAK;
    } else {
        *status = AS5600_MAGNET_OK;
    }
    return ESP_OK;
}

static esp_err_t s_as5600_write_zpos_registers(as5600_handle_t sensor, uint16_t zpos_raw)
{
    if (sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((zpos_raw & ~AS5600_ANGLE_MASK) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buf[3] = {
        AS5600_REG_ZPOS_H,
        (uint8_t)((zpos_raw >> 8) & 0x0F),
        (uint8_t)(zpos_raw & 0xFF),
    };
    return s_as5600_write_reg(sensor, buf, sizeof(buf));
}

esp_err_t as5600_set_conf(as5600_handle_t sensor, const as5600_conf_t *conf)
{
    if (sensor == NULL || conf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t val = 0;
    val |= ((uint16_t)conf->power_mode   & 0x03U) << AS5600_CONF_PM_SHIFT;
    val |= ((uint16_t)conf->hysteresis   & 0x03U) << AS5600_CONF_HYST_SHIFT;
    val |= ((uint16_t)conf->output_stage & 0x03U) << AS5600_CONF_OUTS_SHIFT;
    val |= ((uint16_t)conf->pwm_freq     & 0x03U) << AS5600_CONF_PWMF_SHIFT;
    val |= ((uint16_t)conf->slow_filter  & 0x03U) << AS5600_CONF_SF_SHIFT;
    val |= ((uint16_t)conf->fast_filter  & 0x07U) << AS5600_CONF_FTH_SHIFT;
    val |= conf->watchdog ? AS5600_CONF_WD_MASK : 0U;
    uint8_t buf[3] = {
        AS5600_REG_CONF_H,
        (uint8_t)((val >> 8) & 0x3F),
        (uint8_t)(val & 0xFF),
    };
    return s_as5600_write_reg(sensor, buf, sizeof(buf));
}

esp_err_t as5600_get_conf(as5600_handle_t sensor, as5600_conf_t *conf)
{
    if (sensor == NULL || conf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buf[2];
    esp_err_t ret = s_as5600_read_reg(sensor, AS5600_REG_CONF_H, buf, 2);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t val = (((uint16_t)buf[0] << 8) | buf[1]) & AS5600_CONF_MASK;
    conf->power_mode   = (as5600_power_mode_t)((val & AS5600_CONF_PM_MASK)   >> AS5600_CONF_PM_SHIFT);
    conf->hysteresis   = (as5600_hysteresis_t)((val & AS5600_CONF_HYST_MASK) >> AS5600_CONF_HYST_SHIFT);
    conf->output_stage = (as5600_output_stage_t)((val & AS5600_CONF_OUTS_MASK) >> AS5600_CONF_OUTS_SHIFT);
    conf->pwm_freq     = (as5600_pwm_freq_t)((val & AS5600_CONF_PWMF_MASK) >> AS5600_CONF_PWMF_SHIFT);
    conf->slow_filter  = (as5600_slow_filter_t)((val & AS5600_CONF_SF_MASK)   >> AS5600_CONF_SF_SHIFT);
    conf->fast_filter  = (as5600_fast_filter_t)((val & AS5600_CONF_FTH_MASK)  >> AS5600_CONF_FTH_SHIFT);
    conf->watchdog     = (val & AS5600_CONF_WD_MASK) ? true : false;
    return ESP_OK;
}

esp_err_t as5600_set_zero_position(as5600_handle_t sensor)
{
    if (sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t raw;
    esp_err_t ret = s_as5600_get_angle_raw_no_offset(sensor, &raw);
    if (ret != ESP_OK) {
        return ret;
    }
    return s_as5600_write_zpos_registers(sensor, raw);
}
