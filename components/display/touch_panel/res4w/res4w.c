// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


// 4 Wire resistive touch panels can be controlled directly using
//  microcontroller pins.
// It requires the use of 2 ADC pins (also used as digital output pins) and
//  2 Digital Output pins.
// The four pins are labelled X+ (XP), X- (XM), Y+ (YP) and Y- (YM)
// All four pins pins must be able to be driven high and driven low
// To read an axis, the corresponding pins on the opposite axis are set high
//  and low respectively. The ADC value is read from the input.
// On fast microcontrollers, a small delay may need to be introduced between
//  setting up the outputs and reading the ADC.
// This delay is achieved by sampling the ADC 3 times and discarding the first
//  2 results.


#include <string.h>
#include "esp_log.h"
#include "res4w.h"
#include "driver/gpio.h"
#include "touch_calibration.h"
#include "driver/adc.h"

static const char *TAG = "RES4W";

#define TOUCH_CHECK(a, str, ret)  if(!(a)) {                                      \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                                   \
    }

#define RES4W_SMP_SIZE  8

#define TOUCH_SAMPLE_MAX 4000
#define TOUCH_SAMPLE_MIN 100

#define RES4W_THRESHOLD_Z CONFIG_TOUCH_PANEL_THRESHOLD_PRESS

typedef struct {
    uint16_t x;
    uint16_t y;
} position_t;

typedef struct {
    touch_panel_dir_t direction;
    uint16_t width;
    uint16_t height;
    int io_YP;    // Analog and Digital capable
    int io_YP_ADC;
    int io_YP_CH;
    int io_YM;    // Digital capable
    int io_XP;    // Digital capable
    int io_XM;    // Analog and Digital capable
    int io_XM_ADC;
    int io_XM_CH;
} res4w_dev_t;

static res4w_dev_t g_dev = {0};

touch_panel_driver_t res4w_default_driver = {
    .init = res4w_init,
    .deinit = res4w_deinit,
    .calibration_run = res4w_calibration_run,
    .set_direction = res4w_set_direction,
    .read_point_data = res4w_sample,
};


// Helper function to get ADC Unit number from GPIO pin number
static int get_gpio_adc_module(int gpio)
{
    switch (gpio) {
    case 0:
    case 2:
    case 4:
    case 12:
    case 13:
    case 14:
    case 15:
    case 25:
    case 26:
    case 27:
        return ADC_UNIT_2;
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
        return ADC_UNIT_1;

    default:
        break;
    }
    return -1;
}


// Helper function to get ADC channel number from GPIO pin number
static int get_gpio_adc_channel(int gpio)
{
    switch (gpio) {
    // ADC1
    case 0:
        return ADC_CHANNEL_1;
    case 2:
        return ADC_CHANNEL_2;
    case 4:
        return ADC_CHANNEL_0;
    case 12:
        return ADC_CHANNEL_5;
    case 13:
        return ADC_CHANNEL_4;
    case 14:
        return ADC_CHANNEL_6;
    case 15:
        return ADC_CHANNEL_3;
    case 25:
        return ADC_CHANNEL_8;
    case 26:
        return ADC_CHANNEL_9;
    case 27:
        return ADC_CHANNEL_7;

    // ADC2
    case 32:
        return ADC_CHANNEL_4;
    case 33:
        return ADC_CHANNEL_5;
    case 34:
        return ADC_CHANNEL_6;
    case 35:
        return ADC_CHANNEL_7;
    case 36:
        return ADC_CHANNEL_0;
    case 37:
        return ADC_CHANNEL_1;
    case 38:
        return ADC_CHANNEL_2;
    case 39:
        return ADC_CHANNEL_3;

    default:
        break;
    }

    return -1;
}


// Helper function to test if GPIO is output capable
static bool get_gpio_output_capable(int gpio)
{
    switch (gpio) {
    case 34:
    case 35:
    case 36:
    case 39:
        return false;
    default:
        return true;
    }
}


// Helper functions to control the pins - set them high and low and read the ADC
//==============================================================================

// Set Y+ pin high
static void set_yp_high(void)
{
    gpio_reset_pin(g_dev.io_YP);
    gpio_set_direction(g_dev.io_YP, GPIO_MODE_OUTPUT);
    gpio_set_level(g_dev.io_YP, 1);
}

// Set Y+ pin ADC input and read
static uint32_t set_yp_input(void)
{
    uint32_t retval = 0;
    // Reset pin to high impedance (it may be configured as an output and driven high)
    gpio_reset_pin(g_dev.io_YP);
    // For ESP32 internal ADC's, the Full Scale voltage depends on the configured attenuation.
    // I use 11dB attentuation to get a full scale voltage of 3.9V
    if (ADC_UNIT_1 == g_dev.io_YP_ADC) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten((adc1_channel_t) g_dev.io_YP_CH, ADC_ATTEN_DB_11);
        retval = adc1_get_raw((adc1_channel_t) g_dev.io_YP_CH);
    } else if (ADC_UNIT_2 == g_dev.io_YP_ADC) {
        int raw = 0;
        adc2_config_channel_atten((adc2_channel_t) g_dev.io_YP_CH, ADC_ATTEN_DB_11);
        adc2_get_raw((adc2_channel_t) g_dev.io_YP_CH, ADC_WIDTH_BIT_12, &raw);
        retval = raw;
    }

    return retval;
}

// Set Y- pin high
static void set_ym_high(void)
{
    gpio_reset_pin(g_dev.io_YM);
    gpio_set_direction(g_dev.io_YM, GPIO_MODE_OUTPUT);
    gpio_set_level(g_dev.io_YM, 1);
}

// Set Y- pin low
static void set_ym_low(void)
{
    gpio_reset_pin(g_dev.io_YM);
    gpio_set_direction(g_dev.io_YM, GPIO_MODE_OUTPUT);
    gpio_set_level(g_dev.io_YM, 0);
}

// Set Y- pin high impedance
static void set_ym_hiz(void)
{
    gpio_reset_pin(g_dev.io_YM);
    gpio_set_direction(g_dev.io_YM, GPIO_MODE_INPUT);
}

// Set X+ pin high
static void set_xp_high(void)
{
    gpio_reset_pin(g_dev.io_XP);
    gpio_set_direction(g_dev.io_XP, GPIO_MODE_OUTPUT);
    gpio_set_level(g_dev.io_XP, 1);
}

// Set X+ pin low
static void set_xp_low(void)
{
    gpio_reset_pin(g_dev.io_XP);
    gpio_set_direction(g_dev.io_XP, GPIO_MODE_OUTPUT);
    gpio_set_level(g_dev.io_XP, 0);
}

// Set X+ pin high impedance
static void set_xp_hiz(void)
{
    gpio_reset_pin(g_dev.io_XP);
    gpio_set_direction(g_dev.io_XP, GPIO_MODE_INPUT);
}

// Set X- pin low
static void set_xm_low(void)
{
    gpio_reset_pin(g_dev.io_XM);
    gpio_set_direction(g_dev.io_XM, GPIO_MODE_OUTPUT);
    gpio_set_level(g_dev.io_XM, 0);
}

// Set X- pin ADC input and read
static uint32_t set_xm_input(void)
{
    uint32_t retval = 0;
    // Reset pin to high impedance (it may be configured as an output and driven low)
    gpio_reset_pin(g_dev.io_XM);
    // For ESP32 internal ADC's, the Full Scale voltage depends on the configured attenuation.
    // I use 11dB attentuation to get a full scale voltage of 3.9V
    if (ADC_UNIT_1 == g_dev.io_XM_ADC) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten((adc1_channel_t) g_dev.io_XM_CH, ADC_ATTEN_DB_11);
        retval = adc1_get_raw((adc1_channel_t) g_dev.io_XM_CH);
    } else if (ADC_UNIT_2 == g_dev.io_XM_ADC) {
        int raw = 0;
        adc2_config_channel_atten((adc2_channel_t) g_dev.io_YP_CH, ADC_ATTEN_DB_11);
        adc2_get_raw((adc2_channel_t) g_dev.io_YP_CH, ADC_WIDTH_BIT_12, &raw);
        retval = raw;
    }

    return retval;
}



// Get the X value of the touchscreen
// Set Y- high impedance
// Set X+ high
// Set X- low
// Read Y+ ADC (discard first 2 results)
static uint32_t get_x_val(void)
{
    uint32_t yp = 0;
    uint32_t x = 0;

    // Set pin status
    set_ym_hiz();
    set_xp_high();
    set_xm_low();

    // Wait for levels to settle
    set_yp_input();
    set_yp_input();
    // Read input
    yp = set_yp_input();

    x = (4096 - yp);
    return x;
}

// Get the Y value of the touchscreen
// Set X+ high impedance
// Set Y+ high
// Set Y- low
// Read X- ADC (discard first 2 results)
static uint32_t get_y_val(void)
{
    uint32_t xm = 0;
    uint32_t y = 0;

    // Set pin status
    set_xp_hiz();
    set_yp_high();
    set_ym_low();

    // Wait for levels to settle
    set_xm_input();
    set_xm_input();

    // Read input
    xm = set_xm_input();

    y = (4096 - xm);
    return y;
}

// Get the Z value of the touchscreen
// Set X+ low
// Set Y- high
// Set X- high impedance (adc)
// Read Y+ ADC (discard first results)
// Read X- ADC (discard first 2 results)
static uint32_t get_z_val(void)
{
    uint32_t xm = 0;
    uint32_t yp = 0;
    uint32_t z = 0;

    // Set pin status
    set_xp_low();
    set_ym_high();
    set_xm_input();
    set_yp_input();

    // Wait for levels to settle
    set_xm_input();

    // Read inputs
    xm = set_xm_input();
    yp = set_yp_input();

    z = (4096 - (yp - xm));
    return z;
}


// Initialise the touchscreen pins according to the supplied configuration
esp_err_t res4w_init(const touch_panel_config_t *config)
{
    TOUCH_CHECK(NULL != config, "Pointer invalid", ESP_ERR_INVALID_ARG);

    TOUCH_CHECK((-1 != get_gpio_adc_module(config->interface_res4w.pin_num_yp)), "YP Pin not ADC", ESP_ERR_INVALID_ARG);
    TOUCH_CHECK((-1 != get_gpio_adc_module(config->interface_res4w.pin_num_xm)), "XM Pin not ADC", ESP_ERR_INVALID_ARG);

    TOUCH_CHECK((get_gpio_output_capable(config->interface_res4w.pin_num_yp)), "YP Pin not Output", ESP_ERR_INVALID_ARG);
    TOUCH_CHECK((get_gpio_output_capable(config->interface_res4w.pin_num_ym)), "YM Pin not Output", ESP_ERR_INVALID_ARG);
    TOUCH_CHECK((get_gpio_output_capable(config->interface_res4w.pin_num_xp)), "XP Pin not Output", ESP_ERR_INVALID_ARG);
    TOUCH_CHECK((get_gpio_output_capable(config->interface_res4w.pin_num_xm)), "XM Pin not Output", ESP_ERR_INVALID_ARG);

    g_dev.io_YP = config->interface_res4w.pin_num_yp;
    g_dev.io_YM = config->interface_res4w.pin_num_ym;
    g_dev.io_XP = config->interface_res4w.pin_num_xp;
    g_dev.io_XM = config->interface_res4w.pin_num_xm;

    g_dev.io_YP_ADC = get_gpio_adc_module(g_dev.io_YP);
    g_dev.io_YP_CH = get_gpio_adc_channel(g_dev.io_YP);
    g_dev.io_XM_ADC = get_gpio_adc_module(g_dev.io_XM);
    g_dev.io_XM_CH = get_gpio_adc_channel(g_dev.io_XM);

    res4w_set_direction(config->direction);
    g_dev.width = config->width;
    g_dev.height = config->height;

    ESP_LOGI(TAG, "Touch panel size width: %d, height: %d", g_dev.width, g_dev.height);
    ESP_LOGI(TAG, "Initial successful | dir:%d", config->direction);

    return ESP_OK;
}

esp_err_t res4w_deinit(void)
{
    memset(&g_dev, 0, sizeof(res4w_dev_t));
    return ESP_OK;
}

int res4w_is_pressed(void)
{
    /**
     * @note There are two ways to determine whether the touch panel is pressed
     * 1. Read the IRQ line of touch controller
     * 2. Read value of z axis
     * Only the second method is used here, so the IRQ line is not used.
     */
    uint16_t z;
    z = get_z_val();
    if (z > RES4W_THRESHOLD_Z) { /** If z more than threshold, it is considered as pressed */
        return 1;
    }

    return 0;
}

esp_err_t res4w_set_direction(touch_panel_dir_t dir)
{
    if (TOUCH_DIR_MAX < dir) {
        dir >>= 5;
    }
    g_dev.direction = dir;
    return ESP_OK;
}

esp_err_t res4w_get_rawdata(uint16_t *x, uint16_t *y)
{
    position_t samples[RES4W_SMP_SIZE];
    uint32_t aveX = 0;
    uint32_t aveY = 0;
    int valid_count = 0;

    for (int i = 0; i < RES4W_SMP_SIZE; i++) {
        samples[i].x = get_x_val();
        samples[i].y = get_y_val();

        // Only add the samples to the average if they are valid
        if ((samples[i].x >= TOUCH_SAMPLE_MIN) && (samples[i].x <= TOUCH_SAMPLE_MAX) &&
            (samples[i].y >= TOUCH_SAMPLE_MIN) && (samples[i].y <= TOUCH_SAMPLE_MAX)) {
            aveX += samples[i].x;
            aveY += samples[i].y;
            valid_count++;
        }
    }

    // If we don't have at least 50% valid samples, there was no valid touch.
    if (valid_count >= (RES4W_SMP_SIZE / 2)) {
        aveX /= valid_count;
        aveY /= valid_count;
    } else {
        aveX = 1;
        aveY = 1;
    }

    *x = aveX;
    *y = aveY;

    return ESP_OK;
}

static void res4w_apply_rotate(uint16_t *x, uint16_t *y)
{
    uint16_t _x = *x;
    uint16_t _y = *y;

    switch (g_dev.direction) {
    case TOUCH_DIR_LRTB:
        *x = _x;
        *y = _y;
        break;
    case TOUCH_DIR_LRBT:
        *x = _x;
        *y = g_dev.height - _y;
        break;
    case TOUCH_DIR_RLTB:
        *x = g_dev.width - _x;
        *y = _y;
        break;
    case TOUCH_DIR_RLBT:
        *x = g_dev.width - _x;
        *y = g_dev.height - _y;
        break;
    case TOUCH_DIR_TBLR:
        *x = _y;
        *y = _x;
        break;
    case TOUCH_DIR_BTLR:
        *x = _y;
        *y = g_dev.width - _x;
        break;
    case TOUCH_DIR_TBRL:
        *x = g_dev.height - _y;
        *y = _x;
        break;
    case TOUCH_DIR_BTRL:
        *x = g_dev.height - _y;
        *y = g_dev.width - _x;
        break;

    default:
        break;
    }
}

esp_err_t res4w_sample(touch_panel_points_t *info)
{
    TOUCH_CHECK(NULL != info, "Pointer invalid", ESP_FAIL);

    esp_err_t ret;
    uint16_t x, y;
    info->curx[0] = 0;
    info->cury[0] = 0;
    info->event = TOUCH_EVT_RELEASE;
    info->point_num = 0;

    int state = res4w_is_pressed();
    if (0 == state) {
        return ESP_OK;
    }

    ret = res4w_get_rawdata(&x, &y);
    TOUCH_CHECK(ret == ESP_OK, "Get raw data failed", ESP_FAIL);

    /**< If the data is not in the normal range, it is considered that it is not pressed */
    if ((x < TOUCH_SAMPLE_MIN) || (x > TOUCH_SAMPLE_MAX) ||
            (y < TOUCH_SAMPLE_MIN) || (y > TOUCH_SAMPLE_MAX)) {
        return ESP_OK;
    }

    int32_t _x = x;
    int32_t _y = y;
    ret = touch_calibration_transform(&_x, &_y);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Transform raw data failed. Maybe the xxx.calibration_run() function has been not call");
        _x = _y = 0;
        return ESP_FAIL;
    }

    x = _x;
    y = _y;

    res4w_apply_rotate(&x, &y);

    info->curx[0] = x;
    info->cury[0] = y;
    info->event = state ? TOUCH_EVT_PRESS : TOUCH_EVT_RELEASE;
    info->point_num = 1;
    return ESP_OK;
}

esp_err_t res4w_calibration_run(const scr_driver_t *screen, bool recalibrate)
{
    return touch_calibration_run(screen, res4w_is_pressed, res4w_get_rawdata, recalibrate);
}
