/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#else
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#endif
#include "button_adc.h"

static const char *TAG = "adc button";

#define ADC_BTN_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   CONFIG_ADC_BUTTON_SAMPLE_TIMES     //Multisampling

/*!< Using atten bigger than 6db by default, it will be 11db or 12db in different target */
#define DEFAULT_ADC_ATTEN (ADC_ATTEN_DB_6 + 1)

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define ADC_BUTTON_WIDTH        SOC_ADC_RTC_MAX_BITWIDTH
#define ADC1_BUTTON_CHANNEL_MAX SOC_ADC_MAX_CHANNEL_NUM
#define ADC_BUTTON_ATTEN        DEFAULT_ADC_ATTEN
#else
#define ADC_BUTTON_WIDTH        ADC_WIDTH_MAX-1
#define ADC1_BUTTON_CHANNEL_MAX ADC1_CHANNEL_MAX
#define ADC_BUTTON_ATTEN        DEFAULT_ADC_ATTEN
#endif
#define ADC_BUTTON_ADC_UNIT     ADC_UNIT_1
#define ADC_BUTTON_MAX_CHANNEL  CONFIG_ADC_BUTTON_MAX_CHANNEL
#define ADC_BUTTON_MAX_BUTTON   CONFIG_ADC_BUTTON_MAX_BUTTON_PER_CHANNEL

typedef struct {
    uint16_t min;
    uint16_t max;
} button_data_t;

typedef struct {
    uint8_t channel;
    uint8_t is_init;
    button_data_t btns[ADC_BUTTON_MAX_BUTTON];  /* all button on the channel */
    uint64_t last_time;  /* the last time of adc sample */
} btn_adc_channel_t;

typedef struct {
    bool is_configured;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    adc_cali_handle_t adc1_cali_handle;
    adc_oneshot_unit_handle_t adc1_handle;
#else
    esp_adc_cal_characteristics_t adc_chars;
#endif
    btn_adc_channel_t ch[ADC_BUTTON_MAX_CHANNEL];
    uint8_t ch_num;
} adc_button_t;

static adc_button_t g_button = {0};

static int find_unused_channel(void)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (0 == g_button.ch[i].is_init) {
            return i;
        }
    }
    return -1;
}

static int find_channel(uint8_t channel)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (channel == g_button.ch[i].channel) {
            return i;
        }
    }
    return -1;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static esp_err_t adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BUTTON_WIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BUTTON_WIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated ? ESP_OK : ESP_FAIL;
}
#endif

esp_err_t button_adc_init(const button_adc_config_t *config)
{
    ADC_BTN_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    ADC_BTN_CHECK(config->adc_channel < ADC1_BUTTON_CHANNEL_MAX, "channel out of range", ESP_ERR_NOT_SUPPORTED);
    ADC_BTN_CHECK(config->button_index < ADC_BUTTON_MAX_BUTTON, "button_index out of range", ESP_ERR_NOT_SUPPORTED);
    ADC_BTN_CHECK(config->max > 0, "key max voltage invalid", ESP_ERR_INVALID_ARG);

    int ch_index = find_channel(config->adc_channel);
    if (ch_index >= 0) { /**< the channel has been initialized */
        ADC_BTN_CHECK(g_button.ch[ch_index].btns[config->button_index].max == 0, "The button_index has been used", ESP_ERR_INVALID_STATE);
    } else { /**< this is a new channel */
        int unused_ch_index = find_unused_channel();
        ADC_BTN_CHECK(unused_ch_index >= 0, "exceed max channel number, can't create a new channel", ESP_ERR_INVALID_STATE);
        ch_index = unused_ch_index;
    }

    /** initialize adc */
    if (0 == g_button.is_configured) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        esp_err_t ret;
        if (NULL == config->adc_handle) {
            //ADC1 Init
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,
            };
            ret = adc_oneshot_new_unit(&init_config, &g_button.adc1_handle);
            ADC_BTN_CHECK(ret == ESP_OK, "adc oneshot new unit fail!", ESP_FAIL);
        } else {
            g_button.adc1_handle = *config->adc_handle ;
            ESP_LOGI(TAG, "ADC1 has been initialized");
        }
#else
        //Configure ADC
        adc1_config_width(ADC_BUTTON_WIDTH);
        //Characterize ADC
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_BUTTON_ADC_UNIT, ADC_BUTTON_ATTEN, ADC_BUTTON_WIDTH, DEFAULT_VREF, &g_button.adc_chars);
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            ESP_LOGI(TAG, "Characterized using Two Point Value");
        } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            ESP_LOGI(TAG, "Characterized using eFuse Vref");
        } else {
            ESP_LOGI(TAG, "Characterized using Default Vref");
        }
#endif
        g_button.is_configured = 1;
    }

    /** initialize adc channel */
    if (0 == g_button.ch[ch_index].is_init) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        //ADC1 Config
        adc_oneshot_chan_cfg_t oneshot_config = {
            .bitwidth = ADC_BUTTON_WIDTH,
            .atten = ADC_BUTTON_ATTEN,
        };
        esp_err_t ret = adc_oneshot_config_channel(g_button.adc1_handle, config->adc_channel, &oneshot_config);
        ADC_BTN_CHECK(ret == ESP_OK, "adc oneshot config channel fail!", ESP_FAIL);
        //-------------ADC1 Calibration Init---------------//
        ret = adc_calibration_init(ADC_BUTTON_ADC_UNIT, ADC_BUTTON_ATTEN, &g_button.adc1_cali_handle);
        ADC_BTN_CHECK(ret == ESP_OK, "ADC1 Calibration Init False", 0);
#else
        adc1_config_channel_atten(config->adc_channel, ADC_BUTTON_ATTEN);
#endif
        g_button.ch[ch_index].channel = config->adc_channel;
        g_button.ch[ch_index].is_init = 1;
        g_button.ch[ch_index].last_time = 0;
    }
    g_button.ch[ch_index].btns[config->button_index].max = config->max;
    g_button.ch[ch_index].btns[config->button_index].min = config->min;
    g_button.ch_num++;

    return ESP_OK;
}

esp_err_t button_adc_deinit(uint8_t channel, int button_index)
{
    ADC_BTN_CHECK(channel < ADC1_BUTTON_CHANNEL_MAX, "channel out of range", ESP_ERR_INVALID_ARG);
    ADC_BTN_CHECK(button_index < ADC_BUTTON_MAX_BUTTON, "button_index out of range", ESP_ERR_INVALID_ARG);

    int ch_index = find_channel(channel);
    ADC_BTN_CHECK(ch_index >= 0, "can't find the channel", ESP_ERR_INVALID_ARG);

    g_button.ch[ch_index].btns[button_index].max = 0;
    g_button.ch[ch_index].btns[button_index].min = 0;

    /** check button usage on the channel*/
    uint8_t unused_button = 0;
    for (size_t i = 0; i < ADC_BUTTON_MAX_BUTTON; i++) {
        if (0 == g_button.ch[ch_index].btns[i].max) {
            unused_button++;
        }
    }
    if (unused_button == ADC_BUTTON_MAX_BUTTON && g_button.ch[ch_index].is_init) {  /**< if all button is unused, deinit the channel */
        g_button.ch[ch_index].is_init = 0;
        g_button.ch[ch_index].channel = ADC1_BUTTON_CHANNEL_MAX;
        ESP_LOGD(TAG, "all button is unused on channel%d, deinit the channel", g_button.ch[ch_index].channel);
    }

    /** check channel usage on the adc*/
    uint8_t unused_ch = 0;
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (0 == g_button.ch[i].is_init) {
            unused_ch++;
        }
    }
    if (unused_ch == ADC_BUTTON_MAX_CHANNEL && g_button.is_configured) { /**< if all channel is unused, deinit the adc */
        g_button.is_configured = false;
        memset(&g_button, 0, sizeof(adc_button_t));
        ESP_LOGD(TAG, "all channel is unused, , deinit adc");
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_err_t ret = adc_oneshot_del_unit(g_button.adc1_handle);
    ADC_BTN_CHECK(ret == ESP_OK, "adc oneshot deinit fail", ESP_FAIL);
#endif
    return ESP_OK;
}

static uint32_t get_adc_volatge(uint8_t channel)
{
    uint32_t adc_reading = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    int adc_raw = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_oneshot_read(g_button.adc1_handle, channel, &adc_raw);
        adc_reading += adc_raw;
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    int voltage = 0;
    adc_cali_raw_to_voltage(g_button.adc1_cali_handle, adc_reading, &voltage);
    ESP_LOGV(TAG, "Raw: %"PRIu32"\tVoltage: %dmV", adc_reading, voltage);
#else
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(channel);
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &g_button.adc_chars);
    ESP_LOGV(TAG, "Raw: %"PRIu32"\tVoltage: %"PRIu32"mV", adc_reading, voltage);
#endif
    return voltage;
}

uint8_t button_adc_get_key_level(void *button_index)
{
    static uint16_t vol = 0;
    uint32_t ch = ADC_BUTTON_SPLIT_CHANNEL(button_index);
    uint32_t index = ADC_BUTTON_SPLIT_INDEX(button_index);
    ADC_BTN_CHECK(ch < ADC1_BUTTON_CHANNEL_MAX, "channel out of range", 0);
    ADC_BTN_CHECK(index < ADC_BUTTON_MAX_BUTTON, "button_index out of range", 0);
    int ch_index = find_channel(ch);
    ADC_BTN_CHECK(ch_index >= 0, "The button_index is not init", 0);

    /** It starts only when the elapsed time is more than 1ms */
    if ((esp_timer_get_time() - g_button.ch[ch_index].last_time) > 1000) {
        vol = get_adc_volatge(ch);
        g_button.ch[ch_index].last_time = esp_timer_get_time();
    }

    if (vol <= g_button.ch[ch_index].btns[index].max &&
            vol >= g_button.ch[ch_index].btns[index].min) {
        return 1;
    }
    return 0;
}
