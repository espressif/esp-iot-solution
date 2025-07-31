/* SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_idf_version.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "button_adc.h"
#include "button_interface.h"

static const char *TAG = "adc_button";

#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   CONFIG_ADC_BUTTON_SAMPLE_TIMES     //Multisampling

/*!< Using atten bigger than 6db by default, it will be 11db or 12db in different target */
#define DEFAULT_ADC_ATTEN         (ADC_ATTEN_DB_6 + 1)

#define ADC_BUTTON_WIDTH          SOC_ADC_RTC_MAX_BITWIDTH
#define ADC_BUTTON_CHANNEL_MAX    SOC_ADC_MAX_CHANNEL_NUM
#define ADC_BUTTON_ATTEN          DEFAULT_ADC_ATTEN

#define ADC_BUTTON_MAX_CHANNEL  CONFIG_ADC_BUTTON_MAX_CHANNEL
#define ADC_BUTTON_MAX_BUTTON   CONFIG_ADC_BUTTON_MAX_BUTTON_PER_CHANNEL

// ESP32C3 ADC2 it has been deprecated.
#if (SOC_ADC_PERIPH_NUM >= 2) && !CONFIG_IDF_TARGET_ESP32C3
#define ADC_UNIT_NUM 2
#else
#define ADC_UNIT_NUM 1
#endif

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

typedef enum {
    ADC_NONE_INIT = 0,
    ADC_INIT_BY_ADC_BUTTON,
    ADC_INIT_BY_USER,
} adc_init_info_t;

typedef struct {
    adc_init_info_t is_configured;
    adc_cali_handle_t adc_cali_handle;
    adc_oneshot_unit_handle_t adc_handle;
    btn_adc_channel_t ch[ADC_BUTTON_MAX_CHANNEL];
    uint8_t ch_num;
} btn_adc_unit_t;

typedef struct {
    btn_adc_unit_t unit[ADC_UNIT_NUM];
} button_adc_t;
typedef struct {
    button_driver_t base;
    adc_unit_t unit_id;
    uint32_t ch;
    uint32_t index;
} button_adc_obj;

static button_adc_t g_button = {0};

static int find_unused_channel(adc_unit_t unit_id)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (0 == g_button.unit[unit_id].ch[i].is_init) {
            return i;
        }
    }
    return -1;
}

static int find_channel(adc_unit_t unit_id, uint8_t channel)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (channel == g_button.unit[unit_id].ch[i].channel) {
            return i;
        }
    }
    return -1;
}

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;
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
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Calibration not supported");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static bool adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (adc_cali_delete_scheme_curve_fitting(handle) == ESP_OK) {
        return true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (adc_cali_delete_scheme_line_fitting(handle) == ESP_OK) {
        return true;
    }
#endif

    return false;
}

esp_err_t button_adc_del(button_driver_t *button_driver)
{
    button_adc_obj *adc_btn = __containerof(button_driver, button_adc_obj, base);
    ESP_RETURN_ON_FALSE(adc_btn->ch < ADC_BUTTON_CHANNEL_MAX, ESP_ERR_INVALID_ARG, TAG, "channel out of range");
    ESP_RETURN_ON_FALSE(adc_btn->index < ADC_BUTTON_MAX_BUTTON, ESP_ERR_INVALID_ARG, TAG, "button_index out of range");

    int ch_index = find_channel(adc_btn->unit_id, adc_btn->ch);
    ESP_RETURN_ON_FALSE(ch_index >= 0, ESP_ERR_INVALID_ARG, TAG, "can't find the channel");

    g_button.unit[adc_btn->unit_id].ch[ch_index].btns[adc_btn->index].max = 0;
    g_button.unit[adc_btn->unit_id].ch[ch_index].btns[adc_btn->index].min = 0;

    /** check button usage on the channel*/
    uint8_t unused_button = 0;
    for (size_t i = 0; i < ADC_BUTTON_MAX_BUTTON; i++) {
        if (0 == g_button.unit[adc_btn->unit_id].ch[ch_index].btns[i].max) {
            unused_button++;
        }
    }
    if (unused_button == ADC_BUTTON_MAX_BUTTON && g_button.unit[adc_btn->unit_id].ch[ch_index].is_init) {  /**< if all button is unused, deinit the channel */
        g_button.unit[adc_btn->unit_id].ch[ch_index].is_init = 0;
        g_button.unit[adc_btn->unit_id].ch[ch_index].channel = ADC_BUTTON_CHANNEL_MAX;
        ESP_LOGD(TAG, "all button is unused on channel%d, deinit the channel", g_button.unit[adc_btn->unit_id].ch[ch_index].channel);
    }

    /** check channel usage on the adc*/
    uint8_t unused_ch = 0;
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (0 == g_button.unit[adc_btn->unit_id].ch[i].is_init) {
            unused_ch++;
        }
    }
    if (unused_ch == ADC_BUTTON_MAX_CHANNEL && g_button.unit[adc_btn->unit_id].is_configured) { /**< if all channel is unused, deinit the adc */
        if (g_button.unit[adc_btn->unit_id].is_configured == ADC_INIT_BY_ADC_BUTTON) {
            esp_err_t ret = adc_oneshot_del_unit(g_button.unit[adc_btn->unit_id].adc_handle);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "adc oneshot del unit fail");
            adc_calibration_deinit(g_button.unit[adc_btn->unit_id].adc_cali_handle);
        }

        g_button.unit[adc_btn->unit_id].is_configured = ADC_NONE_INIT;
        memset(&g_button.unit[adc_btn->unit_id], 0, sizeof(btn_adc_unit_t));
        ESP_LOGD(TAG, "all channel is unused, , deinit adc");
    }
    free(adc_btn);

    return ESP_OK;
}

static uint32_t get_adc_voltage(adc_unit_t unit_id, uint8_t channel)
{
    uint32_t adc_reading = 0;
    int adc_raw = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_oneshot_read(g_button.unit[unit_id].adc_handle, channel, &adc_raw);
        adc_reading += adc_raw;
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    int voltage = 0;
    adc_cali_raw_to_voltage(g_button.unit[unit_id].adc_cali_handle, adc_reading, &voltage);
    ESP_LOGV(TAG, "Raw: %"PRIu32"\tVoltage: %dmV", adc_reading, voltage);
    return voltage;
}

uint8_t button_adc_get_key_level(button_driver_t *button_driver)
{
    button_adc_obj *adc_btn = __containerof(button_driver, button_adc_obj, base);
    static uint16_t vol = 0;
    uint32_t ch = adc_btn->ch;
    uint32_t index = adc_btn->index;
    ESP_RETURN_ON_FALSE(ch < ADC_BUTTON_CHANNEL_MAX, 0, TAG, "channel out of range");
    ESP_RETURN_ON_FALSE(index < ADC_BUTTON_MAX_BUTTON, 0, TAG, "button_index out of range");

    int ch_index = find_channel(adc_btn->unit_id, ch);
    ESP_RETURN_ON_FALSE(ch_index >= 0, 0, TAG, "The button_index is not init");

    /** It starts only when the elapsed time is more than 1ms */
    if ((esp_timer_get_time() - g_button.unit[adc_btn->unit_id].ch[ch_index].last_time) > 1000) {
        vol = get_adc_voltage(adc_btn->unit_id, ch);
        g_button.unit[adc_btn->unit_id].ch[ch_index].last_time = esp_timer_get_time();
    }

    if (vol <= g_button.unit[adc_btn->unit_id].ch[ch_index].btns[index].max &&
            vol >= g_button.unit[adc_btn->unit_id].ch[ch_index].btns[index].min) {
        return BUTTON_ACTIVE;
    }
    return BUTTON_INACTIVE;
}

esp_err_t iot_button_new_adc_device(const button_config_t *button_config, const button_adc_config_t *adc_config, button_handle_t *ret_button)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(button_config && adc_config && ret_button, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(adc_config->unit_id < ADC_UNIT_NUM, ESP_ERR_INVALID_ARG, TAG, "adc_handle out of range");
    ESP_RETURN_ON_FALSE(adc_config->adc_channel < ADC_BUTTON_CHANNEL_MAX, ESP_ERR_INVALID_ARG, TAG, "channel out of range");
    ESP_RETURN_ON_FALSE(adc_config->button_index < ADC_BUTTON_MAX_BUTTON, ESP_ERR_INVALID_ARG, TAG, "button_index out of range");
    ESP_RETURN_ON_FALSE(adc_config->max > 0, ESP_ERR_INVALID_ARG, TAG, "key max voltage invalid");
    button_adc_obj *adc_btn = calloc(1, sizeof(button_adc_obj));
    ESP_RETURN_ON_FALSE(adc_btn, ESP_ERR_NO_MEM, TAG, "calloc fail");
    adc_btn->unit_id = adc_config->unit_id;

    int ch_index = find_channel(adc_btn->unit_id, adc_config->adc_channel);
    if (ch_index >= 0) { /**< the channel has been initialized */
        ESP_GOTO_ON_FALSE(g_button.unit[adc_btn->unit_id].ch[ch_index].btns[adc_config->button_index].max == 0, ESP_ERR_INVALID_STATE, err, TAG, "The button_index has been used");
    } else { /**< this is a new channel */
        int unused_ch_index = find_unused_channel(adc_config->unit_id);
        ESP_GOTO_ON_FALSE(unused_ch_index >= 0, ESP_ERR_INVALID_STATE, err, TAG, "exceed max channel number, can't create a new channel");
        ch_index = unused_ch_index;
    }

    /** initialize adc */
    if (0 == g_button.unit[adc_btn->unit_id].is_configured) {
        esp_err_t ret;
        if (NULL == adc_config->adc_handle) {
            //ADC1 Init
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = adc_btn->unit_id,
            };
            ret = adc_oneshot_new_unit(&init_config, &g_button.unit[adc_btn->unit_id].adc_handle);
            ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "adc oneshot new unit fail!");
            g_button.unit[adc_btn->unit_id].is_configured = ADC_INIT_BY_ADC_BUTTON;
        } else {
            g_button.unit[adc_btn->unit_id].adc_handle = *adc_config->adc_handle;
            ESP_LOGI(TAG, "ADC1 has been initialized");
            g_button.unit[adc_btn->unit_id].is_configured = ADC_INIT_BY_USER;
        }

    }

    /** initialize adc channel */
    if (0 == g_button.unit[adc_btn->unit_id].ch[ch_index].is_init) {
        //ADC1 Config
        adc_oneshot_chan_cfg_t oneshot_config = {
            .bitwidth = ADC_BUTTON_WIDTH,
            .atten = ADC_BUTTON_ATTEN,
        };
        esp_err_t ret = adc_oneshot_config_channel(g_button.unit[adc_btn->unit_id].adc_handle, adc_config->adc_channel, &oneshot_config);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "adc oneshot config channel fail!");
        //-------------ADC1 Calibration Init---------------//
        adc_calibration_init(adc_btn->unit_id, ADC_BUTTON_ATTEN, &g_button.unit[adc_btn->unit_id].adc_cali_handle);
        g_button.unit[adc_btn->unit_id].ch[ch_index].channel = adc_config->adc_channel;
        g_button.unit[adc_btn->unit_id].ch[ch_index].is_init = 1;
        g_button.unit[adc_btn->unit_id].ch[ch_index].last_time = 0;
    }
    g_button.unit[adc_btn->unit_id].ch[ch_index].btns[adc_config->button_index].max = adc_config->max;
    g_button.unit[adc_btn->unit_id].ch[ch_index].btns[adc_config->button_index].min = adc_config->min;
    g_button.unit[adc_btn->unit_id].ch_num++;

    adc_btn->ch = adc_config->adc_channel;
    adc_btn->index = adc_config->button_index;
    adc_btn->base.get_key_level = button_adc_get_key_level;
    adc_btn->base.del = button_adc_del;
    ret = iot_button_create(button_config, &adc_btn->base, ret_button);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Create button failed");

    return ESP_OK;
err:
    if (adc_btn) {
        free(adc_btn);
    }
    return ret;
}
