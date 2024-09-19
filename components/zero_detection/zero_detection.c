/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zero_detection.h"

static const char *TAG = "zero_detect";

/**
 * @brief Structs to store device info
 *
 */
typedef struct zero_cross {

    uint32_t cap_val_begin_of_sample;     //Tick record value captured by the timer
    uint32_t cap_val_end_of_sample;
    uint32_t full_cycle_us;            //Tick value after half a cycle, becomes the entire cycle after multiplying by two

    uint16_t valid_count;            //Count value of valid signals,switching during half a cycle
    uint16_t valid_times;             //Minimum required number of times for detecting signal validity

    uint16_t invalid_count;          //Count value of invalid signals,switching during half a cycle
    uint16_t invalid_times;           //Minimum required number of times for detecting signal invalidity

    int64_t signal_lost_time_us;        //Minimum required duration for detecting signal loss

    bool zero_source_power_invalid;  //Power loss flag when signal source is lost
    bool zero_singal_invaild;        //Signal is in an invalid range

    zero_signal_type_t zero_signal_type;  //Zero crossing signal type
    zero_driver_type_t zero_driver_type;  //Zero crossing driver type

    int32_t capture_pin;

    double freq_range_max_us;      //Tick value calculated after the user inputs the frequency
    double freq_range_min_us;

    zero_cross_cb_t event_callback; //Zero cross event callback
    void *user_data;                //User's data when regsister for callback

    mcpwm_cap_timer_handle_t cap_timer;
    mcpwm_cap_channel_handle_t cap_chan;
#if defined (CONFIG_USE_GPTIMER)
    gptimer_handle_t gptimer;
#else
    esp_timer_handle_t esp_timer;
#endif

} zero_cross_dev_t;

/**
  * @brief  Zero cross detection driver core function
  */
static void IRAM_ATTR zero_cross_handle_interrupt(void *user_data, const mcpwm_capture_event_data_t *edata)
{
    zero_cross_dev_t *zero_cross_dev = user_data;
    int gpio_status = 0;
    if (zero_cross_dev->zero_driver_type == GPIO_TYPE) {
        //Retrieve the current GPIO level and determine the rising or falling edge
        gpio_status = gpio_ll_get_level(&GPIO, zero_cross_dev->capture_pin);
    }
#if defined(SOC_MCPWM_SUPPORTED)
    bool edge_status = (gpio_status && zero_cross_dev->zero_driver_type == GPIO_TYPE) || ((zero_signal_edge_t)edata->cap_edge == CAP_EDGE_POS && zero_cross_dev->zero_driver_type == MCPWM_TYPE);
#else
    bool edge_status = gpio_status && zero_cross_dev->zero_driver_type == GPIO_TYPE;
#endif
    //Clear power down counter
#if defined(CONFIG_USE_GPTIMER)
    gptimer_set_raw_count(zero_cross_dev->gptimer, 0);
#else
    esp_timer_restart(zero_cross_dev->esp_timer, zero_cross_dev->signal_lost_time_us);
#endif
    if (edge_status) {
        if (zero_cross_dev->zero_signal_type == PULSE_WAVE) { //The methods for calculating the periods of pulse signals and square wave signals are different
            zero_cross_dev->cap_val_begin_of_sample = zero_cross_dev->cap_val_end_of_sample; //Save the current value for the next calculation
            zero_cross_dev->cap_val_end_of_sample = esp_timer_get_time();  //Read the count value and calculate the current cycle
            zero_cross_dev->full_cycle_us = (zero_cross_dev->cap_val_end_of_sample - zero_cross_dev->cap_val_begin_of_sample) * 2;
        } else {
            zero_cross_dev->cap_val_begin_of_sample = esp_timer_get_time();
            zero_cross_dev->full_cycle_us = (zero_cross_dev->cap_val_begin_of_sample - zero_cross_dev->cap_val_end_of_sample) * 2;
        }
        if (zero_cross_dev->full_cycle_us >= zero_cross_dev->freq_range_min_us && zero_cross_dev->full_cycle_us <= zero_cross_dev->freq_range_max_us) {
            zero_cross_dev->valid_count++; //Reset to zero, increment and evaluate the counting value
            zero_cross_dev->invalid_count = 0;
            if (zero_cross_dev->valid_count >= zero_cross_dev->valid_times) {
                zero_cross_dev->zero_singal_invaild = false;
                zero_cross_dev->zero_source_power_invalid = false;
                //Enter the user callback function and return detection data and avoid judging upon receiving the first triggering edge
                if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                    zero_detect_cb_param_t param = {0};
                    param.signal_valid_event_data.valid_count = zero_cross_dev->valid_count;
                    param.signal_valid_event_data.full_cycle_us = zero_cross_dev->full_cycle_us;
                    param.signal_valid_event_data.cap_edge = CAP_EDGE_POS;
                    //Add judgments to prevent data overflow
                    if (zero_cross_dev->valid_count >= UINT16_MAX - 1) {
                        zero_cross_dev->valid_count = zero_cross_dev->valid_times;
                    }
                    zero_cross_dev->event_callback(SIGNAL_VALID, &param, zero_cross_dev->user_data);
                }
            }
        }
        if (zero_cross_dev->event_callback) {
            zero_detect_cb_param_t param = {0};
            param.signal_rising_edge_event_data.valid_count = zero_cross_dev->valid_count;
            param.signal_rising_edge_event_data.invalid_count = zero_cross_dev->invalid_count;
            param.signal_rising_edge_event_data.full_cycle_us = zero_cross_dev->full_cycle_us;
            zero_cross_dev->event_callback(SIGNAL_RISING_EDGE, &param, zero_cross_dev->user_data);
        }
    } else if (!edge_status) {
        if (zero_cross_dev->zero_signal_type == SQUARE_WAVE) {  //The falling edge is only used with square wave signals
            //Calculate the interval in the ISR
            zero_cross_dev->cap_val_end_of_sample = esp_timer_get_time();
            zero_cross_dev->full_cycle_us = (zero_cross_dev->cap_val_end_of_sample - zero_cross_dev->cap_val_begin_of_sample) * 2;  //Count value for half a period
            if (zero_cross_dev->full_cycle_us >= zero_cross_dev->freq_range_min_us && zero_cross_dev->full_cycle_us <= zero_cross_dev->freq_range_max_us) {   //Determine whether it is within the frequency range
                zero_cross_dev->valid_count++;
                zero_cross_dev->invalid_count = 0;
                if (zero_cross_dev->valid_count >= zero_cross_dev->valid_times) {
                    zero_cross_dev->zero_singal_invaild = false;
                    zero_cross_dev->zero_source_power_invalid = false;
                    //Enter the user callback function and return detection data and avoid judging upon receiving the first triggering edge
                    if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                        zero_detect_cb_param_t param = {0};
                        param.signal_valid_event_data.valid_count = zero_cross_dev->valid_count;
                        param.signal_valid_event_data.full_cycle_us = zero_cross_dev->full_cycle_us;
                        param.signal_valid_event_data.cap_edge = CAP_EDGE_NEG;
                        if (zero_cross_dev->valid_count >= UINT16_MAX - 1) {
                            zero_cross_dev->valid_count = zero_cross_dev->valid_times;
                        }
                        zero_cross_dev->event_callback(SIGNAL_VALID, &param, zero_cross_dev->user_data);
                    }
                }
            }
        }
        if (zero_cross_dev->event_callback) {
            zero_detect_cb_param_t param = {0};
            param.signal_falling_edge_event_data.valid_count = zero_cross_dev->valid_count;
            param.signal_falling_edge_event_data.invalid_count = zero_cross_dev->invalid_count;
            param.signal_falling_edge_event_data.full_cycle_us = zero_cross_dev->full_cycle_us;
            zero_cross_dev->event_callback(SIGNAL_FALLING_EDGE, &param, zero_cross_dev->user_data);
        }
    }

    if (edge_status || (!edge_status && zero_cross_dev->zero_signal_type == SQUARE_WAVE)) {          //Exclude the case of the falling edge of the pulse signal
        if (zero_cross_dev->full_cycle_us < zero_cross_dev->freq_range_min_us || zero_cross_dev->full_cycle_us > zero_cross_dev->freq_range_max_us) {   //Determine whether it is within the frequency range
            zero_cross_dev->zero_singal_invaild = true;
            zero_cross_dev->valid_count = 0;
            zero_cross_dev->invalid_count++;
            if (zero_cross_dev->invalid_count >= zero_cross_dev->invalid_times) {
                if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                    zero_detect_cb_param_t param = {0};
                    param.signal_invalid_event_data.invalid_count = zero_cross_dev->invalid_count;
                    param.signal_invalid_event_data.full_cycle_us = zero_cross_dev->full_cycle_us;
                    if (edge_status) {
                        param.signal_invalid_event_data.cap_edge = CAP_EDGE_POS;
                    } else {
                        param.signal_invalid_event_data.cap_edge = CAP_EDGE_NEG;
                    }
                    zero_cross_dev->event_callback(SIGNAL_INVALID, &param, zero_cross_dev->user_data);
                }
            }
            if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                zero_detect_cb_param_t param = {0};
                if (edge_status) {
                    param.signal_freq_event_data.cap_edge = CAP_EDGE_POS;
                } else {
                    param.signal_freq_event_data.cap_edge = CAP_EDGE_NEG;
                }
                param.signal_freq_event_data.full_cycle_us = zero_cross_dev->full_cycle_us;
                zero_cross_dev->event_callback(SIGNAL_FREQ_OUT_OF_RANGE, &param, zero_cross_dev->user_data);
            }
        }
    }
}

#if defined(SOC_MCPWM_SUPPORTED)
static IRAM_ATTR bool zero_detect_mcpwm_cb(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data)
{
    zero_cross_handle_interrupt(user_data, edata);
    return false;
}
#endif

static void IRAM_ATTR zero_detect_gpio_cb(void *arg)
{
    mcpwm_capture_event_data_t edata = {0};
    zero_cross_handle_interrupt(arg, &edata);
}

#if defined(CONFIG_USE_GPTIMER)
static IRAM_ATTR bool zero_source_power_invalid_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    zero_cross_dev_t *zero_cross_dev = user_ctx;
    zero_cross_dev->zero_source_power_invalid = true;
    zero_cross_dev->full_cycle_us = 0;
    zero_cross_dev->cap_val_begin_of_sample = 0;
    zero_cross_dev->cap_val_end_of_sample = 0;
    zero_cross_dev->valid_count = 0;
    zero_cross_dev->zero_singal_invaild = 0;
    if (zero_cross_dev->event_callback) {
        zero_cross_dev->event_callback(SIGNAL_LOST, NULL, zero_cross_dev->user_data);
    }
    return false;
}
#else
static void IRAM_ATTR zero_source_power_invalid_cb(void *arg)
{
    zero_cross_dev_t *zero_cross_dev = arg;
    zero_cross_dev->zero_source_power_invalid = true;
    zero_cross_dev->full_cycle_us = 0;
    zero_cross_dev->cap_val_begin_of_sample = 0;
    zero_cross_dev->cap_val_end_of_sample = 0;
    zero_cross_dev->valid_count = 0;
    zero_cross_dev->zero_singal_invaild = 0;
    if (zero_cross_dev->event_callback) {
        zero_cross_dev->event_callback(SIGNAL_LOST, NULL, zero_cross_dev->user_data);
    }
}
#endif

#if defined(SOC_MCPWM_SUPPORTED)
static esp_err_t zero_detect_mcpwm_init(zero_detect_handle_t zcd_handle)
{
    esp_err_t ret = ESP_OK;
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;

    ESP_LOGI(TAG, "Install capture timer");

    mcpwm_capture_timer_config_t cap_conf = {
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
        .group_id = 0,
    };
    ESP_GOTO_ON_ERROR(mcpwm_new_capture_timer(&cap_conf, &zcd->cap_timer), err, TAG, "Install mcpwm capture timer failed");

    ESP_LOGI(TAG, "Install capture channel");

    mcpwm_capture_channel_config_t cap_ch_conf = {
        .gpio_num = zcd->capture_pin,
        .prescale = 1,
        .flags.neg_edge = true,    //Capture on both edge
        .flags.pos_edge = true,
        .flags.pull_up = true,     //Pull up internally
    };
    ESP_GOTO_ON_ERROR(mcpwm_new_capture_channel(zcd->cap_timer, &cap_ch_conf, &zcd->cap_chan), err, TAG, "Mcpwm channel create failed");

    ESP_LOGI(TAG, "Register capture callback");
    mcpwm_capture_event_callbacks_t cbs = {
        .on_cap = zero_detect_mcpwm_cb,
    };
    ESP_GOTO_ON_ERROR(mcpwm_capture_channel_register_event_callbacks(zcd->cap_chan, &cbs, zcd), err, TAG, "Mcpwm callback create failed");   //Create a detect callback

    ESP_LOGI(TAG, "Enable capture channel");
    ESP_GOTO_ON_ERROR(mcpwm_capture_channel_enable(zcd->cap_chan), err, TAG, "Mcpwm capture channel enable failed");

    ESP_LOGI(TAG, "Enable and start capture timer");         //Enable timer
    ESP_GOTO_ON_ERROR(mcpwm_capture_timer_enable(zcd->cap_timer), err, TAG, "Mcpwm capture timer enable failed");

    ESP_GOTO_ON_ERROR(mcpwm_capture_timer_start(zcd->cap_timer), err, TAG, "Mcpwm capture timer start failed");

    return ESP_OK;

err:
    if (zcd) {
        free(zcd);
    }
    return ret;
}
#endif

static esp_err_t zero_detect_gpio_init(zero_detect_handle_t zcd_handle)
{
    esp_err_t ret = ESP_OK;
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;

    gpio_config_t io_conf = {};
    //Interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //Bit mask of the pins
    io_conf.pin_bit_mask = BIT(zcd->capture_pin);
    //Set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //Enable pull-up mode
    io_conf.pull_up_en = 1;
    ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "GPIO config failed");
    //Change gpio interrupt type for one pin
    ESP_GOTO_ON_ERROR(gpio_install_isr_service(ESP_INTR_FLAG_IRAM), err, TAG, "GPIO install isr failed");

    return ESP_OK;

err:
    if (zcd) {
        free(zcd);
    }
    return ret;
}

#if defined(CONFIG_USE_GPTIMER)
static esp_err_t zero_detect_gptime_init(zero_detect_handle_t zcd_handle)
{
    esp_err_t ret = ESP_OK;
    zero_cross_dev_t *zcd = (zero_cross_dev_t *) zcd_handle;

    ESP_LOGI(TAG, "Install gptimer");
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1tick = 1us
    };
    ESP_GOTO_ON_ERROR(gptimer_new_timer(&timer_config, &zcd->gptimer), err, TAG, "Gpttimer create failed");

    ESP_LOGI(TAG, "Install gptimer alarm");
    gptimer_alarm_config_t gptimer_alarm = {
        .alarm_count = zcd->signal_lost_time_us,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_GOTO_ON_ERROR(gptimer_set_alarm_action(zcd->gptimer, &gptimer_alarm), err, TAG, "Set alarm action failed");

    ESP_LOGI(TAG, "Register powerdown callback");
    const  gptimer_event_callbacks_t cbs = {
        .on_alarm = zero_source_power_invalid_cb,
    };
    ESP_GOTO_ON_ERROR(gptimer_register_event_callbacks(zcd->gptimer, &cbs, zcd), err, TAG, "Register callback create failed");

    ESP_GOTO_ON_ERROR(gptimer_enable(zcd->gptimer), err, TAG, "Enable gptimer failed");

    return ESP_OK;

err:
    if (zcd) {
        free(zcd);
    }
    return ret;
}
#else
static esp_err_t zero_detect_esptimer_init(zero_detect_handle_t zcd_handle)
{
    esp_err_t ret = ESP_OK;
    zero_cross_dev_t *zcd = (zero_cross_dev_t *) zcd_handle;
    esp_timer_create_args_t test_once_arg = {
        .callback = &zero_source_power_invalid_cb,
        .arg = zcd,
        .name = "power_detect_call_back"
    };
    ESP_LOGI(TAG, "Install esptimer and register powerdown callback");
    ESP_GOTO_ON_ERROR(esp_timer_create(&test_once_arg, &zcd->esp_timer), err, TAG, "Esp timer create failed");

    return ESP_OK;

err:
    if (zcd) {
        free(zcd);
    }
    return ret;
}
#endif

zero_detect_handle_t zero_detect_create(zero_detect_config_t *config)
{
    ESP_LOGI(TAG, "IoT Zero Detection Version: %d.%d.%d", ZERO_DETECTION_VER_MAJOR, ZERO_DETECTION_VER_MINOR, ZERO_DETECTION_VER_PATCH);
    zero_cross_dev_t *zcd = (zero_cross_dev_t *) calloc(1, sizeof(zero_cross_dev_t));
    if (NULL == zcd) {
        ESP_LOGI(TAG, "Calloc device failed");
        return NULL;
    }

    if (config->freq_range_max_hz < 0) {
        ESP_LOGW(TAG, "The entered freq_range_max_hz should not be negative, setting to the default value of 65 as the current value");
        config->freq_range_max_hz = 65;
    }
    if (config->freq_range_min_hz < 0) {
        ESP_LOGW(TAG, "The entered freq_range_min_hz should not be negative, setting to the default value of 45 as the current value");
        config->freq_range_min_hz = 45;
    }
    if (!GPIO_IS_VALID_GPIO(config->capture_pin)) {
        ESP_LOGW(TAG, "The current GPIO pin is invalid; set to the default GPIO2");
        config->capture_pin = 2;
    }

    zcd->valid_times = config->valid_times;
    zcd->invalid_times = config->invalid_times;
    zcd->signal_lost_time_us = config->signal_lost_time_us;
    zcd->freq_range_max_us = 1000000 / config->freq_range_min_hz;
    zcd->freq_range_min_us = 1000000 / config->freq_range_max_hz;
    zcd->capture_pin = config->capture_pin;
    zcd->zero_signal_type = config->zero_signal_type;
#if defined(SOC_MCPWM_SUPPORTED)
    zcd->zero_driver_type = config->zero_driver_type;
#else
    zcd->zero_driver_type = GPIO_TYPE;
#endif

#if defined(CONFIG_USE_GPTIMER)
    if (zero_detect_gptime_init(zcd) != ESP_OK) {
        ESP_LOGE(TAG, "Gptimer init failed");
    }
#else
    if (zero_detect_esptimer_init(zcd) != ESP_OK) {
        ESP_LOGE(TAG, "Eptimer init failed");
    }
#endif

#if defined(SOC_MCPWM_SUPPORTED)
    if (zcd->zero_driver_type == MCPWM_TYPE) {
        if (zero_detect_mcpwm_init(zcd) != ESP_OK) {
            ESP_LOGE(TAG, "MCPWM_TYPE init failed");
        }
    }
#endif
    if (zcd->zero_driver_type == GPIO_TYPE) {
        if (zero_detect_gpio_init(zcd) != ESP_OK) {
            ESP_LOGE(TAG, "Detect GPIO init failed");
        }
        if (gpio_isr_handler_add(zcd->capture_pin, zero_detect_gpio_cb, zcd) != ESP_OK) {
            ESP_LOGE(TAG, "Isr handler add failed");
        }
    }
    //Prevent power loss before zero-crossing signal is detected.
#if defined(CONFIG_USE_GPTIMER)
    if (gptimer_start(zcd->gptimer) != ESP_OK) {
        ESP_LOGE(TAG, "Gptimer start failed");
    }
#else
    if (esp_timer_start_periodic(zcd->esp_timer, zcd->signal_lost_time_us) != ESP_OK) {
        ESP_LOGE(TAG, "Esptimer start failed");
    }
#endif

    return (zero_detect_handle_t)zcd;
}

esp_err_t zero_detect_delete(zero_detect_handle_t zcd_handle)
{
    esp_err_t ret = ESP_OK;
    if (NULL == zcd_handle) {
        ESP_LOGW(TAG, "Pointer of handle is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;

#if defined(CONFIG_USE_GPTIMER)
    gptimer_stop(zcd->gptimer);
    gptimer_disable(zcd->gptimer);
    ESP_GOTO_ON_ERROR(gptimer_del_timer(zcd->gptimer), err, TAG, "Gptimer delete failed");
#else
    esp_timer_stop(zcd->esp_timer);
    esp_timer_delete(zcd->esp_timer);
#endif

#if defined(SOC_MCPWM_SUPPORTED)
    if (zcd->zero_driver_type == MCPWM_TYPE) {
        mcpwm_capture_timer_stop(zcd->cap_timer);
        mcpwm_capture_timer_disable(zcd->cap_timer);
        mcpwm_capture_channel_disable(zcd->cap_chan);
        ESP_GOTO_ON_ERROR(mcpwm_del_capture_channel(zcd->cap_chan), err, TAG, "Mcpwm channel delete failed");
        ESP_GOTO_ON_ERROR(mcpwm_del_capture_timer(zcd->cap_timer), err, TAG, "Mcpwm capture timer delete failed");
    }
#endif
    if (zcd->zero_driver_type == GPIO_TYPE) {
        ESP_GOTO_ON_ERROR(gpio_isr_handler_remove(zcd->capture_pin), err, TAG, "Isr handler remove failed");
        gpio_uninstall_isr_service();
    }
    //Free memory
    free(zcd);
    return ESP_OK;

err:
    if (zcd) {
        free(zcd);
    }
    return ret;
}

esp_err_t zero_detect_register_cb(zero_detect_handle_t zcd_handle, zero_cross_cb_t cb, void *usr_data)
{
    esp_err_t ret = ESP_OK;
    if (zcd_handle == NULL || cb == NULL) {
        ESP_LOGW(TAG, "arg is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    zcd->user_data = usr_data;
    zcd->event_callback = cb;

    return ret;
}

void zero_show_data(zero_detect_handle_t zcd_handle)
{
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    float pulse_width_us = 0;
    float detect_hz = 0;
    //ESP_LOGI(TAG, "End of Sample:%ld Begin of sample:%ld us:%ld", zcd->cap_val_end_of_sample,zcd->cap_val_begin_of_sample,zcd->full_cycle_us);
    pulse_width_us = zcd->full_cycle_us;
    detect_hz = 1000000 / pulse_width_us;
    //Avoid displaying data upon receiving the first triggering edge.
    if (zcd->cap_val_end_of_sample == 0 || zcd->cap_val_begin_of_sample == 0) {
        ESP_LOGI(TAG, "Waiting for the next triggering edge");
    } else {
        ESP_LOGI(TAG, "Measured Time: %.2fms Hz:%.2f", pulse_width_us / 1000, detect_hz);
    }
}

bool zero_detect_get_power_status(zero_detect_handle_t zcd_handle)
{
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    return zcd->zero_source_power_invalid;
}

zero_signal_type_t zero_detect_get_signal_type(zero_detect_handle_t zcd_handle)
{
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    return zcd->zero_signal_type;
}

bool zero_detect_signal_invaild_status(zero_detect_handle_t zcd_handle)
{
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    return zcd->zero_singal_invaild;
}
