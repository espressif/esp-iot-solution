/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include "iic.h"
#include "kp18058.h"

static const char *TAG = "kp18058";

#define KP18058_CHECK(a, str, action, ...)                                  \
    if (unlikely(!(a))) {                                                   \
        ESP_LOGE(TAG, str, ##__VA_ARGS__);                                  \
        action;                                                             \
    }

#if CONFIG_ENABLE_DRIVER_DEBUG_LOG_OUTPUT
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#endif

#define INVALID_ADDR            0xFF
#define IIC_BASE_UNIT_HZ        1000
#define KP18058_MAX_PIN         5
#define KP18058_MAX_CMD_LEN     13

/**
 * KP18058 register start address - Byte0
 */
/* B[7] */
#define BASE_ADDR           0x80

/* B[6:5] */
#define BIT_STANDBY         0x00
#define BIT_RGB_CHANNEL     0x20
#define BIT_CW_CHANNEL      0x40
#define BIT_ALL_CHANNEL     0x60

/* B[4:1] */
#define BIT_NEXT_BYTE1      0x00
#define BIT_NEXT_BYTE4      0x06
#define BIT_NEXT_BYTE6      0x0A
#define BIT_NEXT_BYTE8      0x0E
#define BIT_NEXT_BYTE10     0x12
#define BIT_NEXT_BYTE12     0x16

/* B[0] */
//Parity Check

/**
 * KP18058 register baseline voltage compensation address - Byte1
 */
/* B[7] */
#define BIT_ENABLE_COMPENSATION     0x80
#define BIT_DISABLE_COMPENSATION    0x00

/* B[6:3] */
#define BIT_DEFAULT_COMPENSATION_VOLTAGE    0x20

/* B[2:1] */
#define BIT_DEFAULT_SLOPE_LEVEL             0x02

/* B[0] */
//Parity Check

/**
 * KP18058 register OUT1 - OUT3 Max current and Chopping frequency - Byte2
 */
/* B[7:6] */
#define BIT_DEFAULT_CHOPPING_FREQ_1KHZ      0x80

/* B[5:1] */
#define BIT_DEFAULT_OUT1_3_CURRENT          0x00

/* B[0] */
//Parity Check

/**
 * KP18058 register OUT4 - OUT5 Max current and Chopping frequency - Byte3
 */
/* B[7] */
#define BIT_ENABLE_CHOPPING_CONTROL         0x80
#define BIT_DISABLE_CHOPPING_CONTROL        0x00

/* B[6] */
#define BIT_ENABLE_RC_FILTER                0x40
#define BIT_DISABLE_RC_FILTER               0x00

/* B[5:1] */
#define BIT_DEFAULT_OUT4_5_CURRENT          0x00

typedef struct {
    bool init_done;
    uint8_t mapping_addr[KP18058_MAX_PIN];
    /* Mark Byte1 Byte2 Byte3, these parameters will not change after initialization. */
    uint8_t fixed_bit[3];
} kp18058_handle_t;

static kp18058_handle_t *s_kp18058 = NULL;

static IRAM_ATTR uint8_t parity_check(uint8_t input)
{
#if 0
    uint8_t result = input;
    result ^= result >> 8;
    result ^= result >> 4;
    result ^= result >> 2;
    result ^= result >> 1;

    return (result % 2 == 0) ? input : (input |= 0x01);
#else
    static uint8_t table[256] = {
        0,   0,   3,   3,   5,   5,   6,   6,   9,   9,   10,  10,  12,  12,  15,  15,
        17,  17,  18,  18,  20,  20,  23,  23,  24,  24,  27,  27,  29,  29,  30,  30,
        33,  33,  34,  34,  36,  36,  39,  39,  40,  40,  43,  43,  45,  45,  46,  46,
        48,  48,  51,  51,  53,  53,  54,  54,  57,  57,  58,  58,  60,  60,  63,  63,
        65,  65,  66,  66,  68,  68,  71,  71,  72,  72,  75,  75,  77,  77,  78,  78,
        80,  80,  83,  83,  85,  85,  86,  86,  89,  89,  90,  90,  92,  92,  95,  95,
        96,  96,  99,  99,  101, 101, 102, 102, 105, 105, 106, 106, 108, 108, 111, 111,
        113, 113, 114, 114, 116, 116, 119, 119, 120, 120, 123, 123, 125, 125, 126, 126,
        129, 129, 130, 130, 132, 132, 135, 135, 136, 136, 139, 139, 141, 141, 142, 142,
        144, 144, 147, 147, 149, 149, 150, 150, 153, 153, 154, 154, 156, 156, 159, 159,
        160, 160, 163, 163, 165, 165, 166, 166, 169, 169, 170, 170, 172, 172, 175, 175,
        177, 177, 178, 178, 180, 180, 183, 183, 184, 184, 187, 187, 189, 189, 190, 190,
        192, 192, 195, 195, 197, 197, 198, 198, 201, 201, 202, 202, 204, 204, 207, 207,
        209, 209, 210, 210, 212, 212, 215, 215, 216, 216, 219, 219, 221, 221, 222, 222,
        225, 225, 226, 226, 228, 228, 231, 231, 232, 232, 235, 235, 237, 237, 238, 238,
        240, 240, 243, 243, 245, 245, 246, 246, 249, 249, 250, 250, 252, 252, 255, 255,
    };
    return (table[input]);
#endif
}

static uint8_t get_mapping_addr(kp18058_channel_t channel)
{
    uint8_t addr[] = { BIT_NEXT_BYTE4, BIT_NEXT_BYTE6, BIT_NEXT_BYTE8, BIT_NEXT_BYTE10, BIT_NEXT_BYTE12 };
    uint8_t result = addr[s_kp18058->mapping_addr[channel]];

    return result;
}

static esp_err_t _write(uint8_t addr, uint8_t *data_wr, size_t size)
{
    addr = parity_check(addr);
    for (int i = 0; i < size; i++) {
        data_wr[i] = parity_check(data_wr[i]);
    }

    return iic_driver_write(addr, data_wr, size);
}

static esp_err_t set_init_data(void)
{
    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_NEXT_BYTE1;
    uint8_t value[3] = { 0 };

    memcpy(&value[0], s_kp18058->fixed_bit, sizeof(value));

    return _write(addr, value, sizeof(value));
}

esp_err_t kp18058_set_standby_mode(bool enable_standby)
{
    KP18058_CHECK(s_kp18058, "not init", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_STANDBY | BIT_NEXT_BYTE4;
    uint8_t value[13] = { 0 };

    if (!enable_standby) {
        addr |= BIT_ALL_CHANNEL;
    }
    memcpy(&value[0], s_kp18058->fixed_bit, 3);

    return _write(addr, value, sizeof(value));
}

esp_err_t kp18058_set_shutdown(void)
{
    return kp18058_set_standby_mode(true);
}

esp_err_t kp18058_regist_channel(kp18058_channel_t channel, kp18058_out_pin_t pin)
{
    KP18058_CHECK(s_kp18058, "not init", return ESP_ERR_INVALID_STATE);
    KP18058_CHECK(channel < KP18058_CHANNEL_MAX, "check channel fail", return ESP_ERR_INVALID_ARG);
    KP18058_CHECK(pin < KP18058_PIN_OUT_MAX, "check out pin fail", return ESP_ERR_INVALID_ARG);

    s_kp18058->mapping_addr[channel] = pin;
    return ESP_OK;
}

esp_err_t kp18058_set_channel(kp18058_channel_t channel, uint16_t value)
{
    KP18058_CHECK(s_kp18058, "not init", return ESP_ERR_INVALID_STATE);
    KP18058_CHECK(s_kp18058->mapping_addr[channel] != INVALID_ADDR, "channel:%d not regist", return ESP_ERR_INVALID_STATE, channel);
    KP18058_CHECK(value <= 1023, "value out of range", return ESP_ERR_INVALID_ARG);

    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | get_mapping_addr(channel);
    uint8_t _value[2] = { 0 };

    if (!s_kp18058->init_done) {
        set_init_data();
        s_kp18058->init_done = true;
    }

    _value[0] = (value >> 5) << 1;
    _value[1] = (value & 0x1F) << 1;

    return _write(addr, _value, sizeof(_value));
}

esp_err_t kp18058_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b)
{
    KP18058_CHECK(s_kp18058, "not init", return ESP_ERR_INVALID_STATE);
    KP18058_CHECK(s_kp18058->mapping_addr[0] != INVALID_ADDR || s_kp18058->mapping_addr[1] != INVALID_ADDR || s_kp18058->mapping_addr[2] != INVALID_ADDR, "color channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_NEXT_BYTE4;
    uint8_t _value[6] = { 0 };

    if (!s_kp18058->init_done) {
        set_init_data();
        s_kp18058->init_done = true;
    }

    _value[s_kp18058->mapping_addr[0] * 2 + 0] = (value_r >> 5) << 1;
    _value[s_kp18058->mapping_addr[0] * 2 + 1] = (value_r & 0x1F) << 1;

    _value[s_kp18058->mapping_addr[1] * 2 + 0] = (value_g >> 5) << 1;
    _value[s_kp18058->mapping_addr[1] * 2 + 1] = (value_g & 0x1F) << 1;

    _value[s_kp18058->mapping_addr[2] * 2 + 0] = (value_b >> 5) << 1;
    _value[s_kp18058->mapping_addr[2] * 2 + 1] = (value_b & 0x1F) << 1;

    return _write(addr, _value, sizeof(_value));
}

esp_err_t kp18058_set_cw_channel(uint16_t value_c, uint16_t value_w)
{
    KP18058_CHECK(s_kp18058, "not init", return ESP_ERR_INVALID_STATE);
    KP18058_CHECK(s_kp18058->mapping_addr[3] != INVALID_ADDR || s_kp18058->mapping_addr[4] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_NEXT_BYTE10;
    uint8_t _value[4] = { 0 };

    if (!s_kp18058->init_done) {
        set_init_data();
        s_kp18058->init_done = true;
    }

    _value[(s_kp18058->mapping_addr[3] - 3) * 2 + 0] = (value_c >> 5) << 1;
    _value[(s_kp18058->mapping_addr[3] - 3) * 2 + 1] = (value_c & 0x1F) << 1;

    _value[(s_kp18058->mapping_addr[4] - 3) * 2 + 0] = (value_w >> 5) << 1;
    _value[(s_kp18058->mapping_addr[4] - 3) * 2 + 1] = (value_w & 0x1F) << 1;

    return _write(addr, _value, sizeof(_value));
}

esp_err_t kp18058_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w)
{
    KP18058_CHECK(s_kp18058, "not init", return ESP_ERR_INVALID_STATE);
    KP18058_CHECK(s_kp18058->mapping_addr[3] != INVALID_ADDR || s_kp18058->mapping_addr[4] != INVALID_ADDR, "white channel not regist", return ESP_ERR_INVALID_STATE);

    uint8_t addr = BASE_ADDR | BIT_ALL_CHANNEL | BIT_NEXT_BYTE4;
    uint8_t _value[10] = { 0 };

    if (!s_kp18058->init_done) {
        set_init_data();
        s_kp18058->init_done = true;
    }

    _value[s_kp18058->mapping_addr[0] * 2 + 0] = (value_r >> 5) << 1;
    _value[s_kp18058->mapping_addr[0] * 2 + 1] = (value_r & 0x1F) << 1;

    _value[s_kp18058->mapping_addr[1] * 2 + 0] = (value_g >> 5) << 1;
    _value[s_kp18058->mapping_addr[1] * 2 + 1] = (value_g & 0x1F) << 1;

    _value[s_kp18058->mapping_addr[2] * 2 + 0] = (value_b >> 5) << 1;
    _value[s_kp18058->mapping_addr[2] * 2 + 1] = (value_b & 0x1F) << 1;

    _value[s_kp18058->mapping_addr[3] * 2 + 0] = (value_c >> 5) << 1;
    _value[s_kp18058->mapping_addr[3] * 2 + 1] = (value_c & 0x1F) << 1;

    _value[s_kp18058->mapping_addr[4] * 2 + 0] = (value_w >> 5) << 1;
    _value[s_kp18058->mapping_addr[4] * 2 + 1] = (value_w & 0x1F) << 1;

    return _write(addr, _value, sizeof(_value));
}

esp_err_t kp18058_init(driver_kp18058_t *config, void(*hook_func)(void *))
{
    esp_err_t err = ESP_OK;

    KP18058_CHECK(config, "config is null", return ESP_ERR_INVALID_ARG);
    KP18058_CHECK(!s_kp18058, "already init done", return ESP_ERR_INVALID_ARG);
    KP18058_CHECK(config->cw_current_multiple < 31 && config->cw_current_multiple >= 1, "cw channel current data check failed", return ESP_ERR_INVALID_ARG);
    KP18058_CHECK(config->rgb_current_multiple < 31 && config->rgb_current_multiple >= 1, "rgb channel current data check failed", return ESP_ERR_INVALID_ARG);

    s_kp18058 = calloc(1, sizeof(kp18058_handle_t));
    KP18058_CHECK(s_kp18058, "alloc fail", return ESP_ERR_NO_MEM);
    memset(s_kp18058->mapping_addr, INVALID_ADDR, KP18058_MAX_PIN);

    // The following configuration defaults value are from the KP18058 data sheet
    // Byte 1
    if (config->enable_custom_param && config->custom_param.disable_voltage_compensation) {
        s_kp18058->fixed_bit[0] |= BIT_DISABLE_COMPENSATION;
    } else {
        s_kp18058->fixed_bit[0] |= BIT_ENABLE_COMPENSATION;
        s_kp18058->fixed_bit[0] |= config->custom_param.compensation << 3;
        s_kp18058->fixed_bit[0] |= config->custom_param.slope << 1;
    }

    // Byte 2
    if (config->enable_custom_param && config->custom_param.disable_chopping_dimming) {
        s_kp18058->fixed_bit[1] |= BIT_DEFAULT_CHOPPING_FREQ_1KHZ;
    } else {
        s_kp18058->fixed_bit[1] |= config->custom_param.chopping_freq << 6;
    }
    s_kp18058->fixed_bit[1] = (config->rgb_current_multiple - 1) << 1;

    // Byte 3
    if (config->enable_custom_param && config->custom_param.disable_chopping_dimming) {
        s_kp18058->fixed_bit[2] |= BIT_DISABLE_CHOPPING_CONTROL;
    } else {
        s_kp18058->fixed_bit[2] |= BIT_ENABLE_CHOPPING_CONTROL;
    }
    if (config->enable_custom_param && config->custom_param.enable_rc_filter) {
        s_kp18058->fixed_bit[2] |= BIT_ENABLE_RC_FILTER;
    } else {
        s_kp18058->fixed_bit[2] |= BIT_DISABLE_RC_FILTER;
    }
    s_kp18058->fixed_bit[2] = (config->cw_current_multiple - 1) << 1;

    if (config->iic_freq_khz > 300) {
        config->iic_freq_khz = 300;
        ESP_LOGW(TAG, "The frequency is too high, adjust it to 300khz");
    }

    err |= iic_driver_init(I2C_NUM_0, config->iic_sda, config->iic_clk, config->iic_freq_khz * IIC_BASE_UNIT_HZ);
    KP18058_CHECK(err == ESP_OK, "i2c master init fail", goto EXIT);

    if (config->enable_iic_queue) {
        err |= iic_driver_send_task_create();
        KP18058_CHECK(err == ESP_OK, "task create fail", goto EXIT);
    }

    return err;
EXIT:

    if (s_kp18058) {
        free(s_kp18058);
        s_kp18058 = NULL;
    }
    return err;
}

esp_err_t kp18058_deinit(void)
{
    KP18058_CHECK(s_kp18058, "not init", return ESP_ERR_INVALID_STATE);

    kp18058_set_shutdown();
    iic_driver_deinit();
    iic_driver_task_destroy();
    free(s_kp18058);
    s_kp18058 = NULL;
    return ESP_OK;
}
