/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "zero_detection.h"

static const char *TAG = "zero_detect";

typedef struct zero_cross {

    uint32_t cap_val_begin_of_sample;//Tick record value captured by the timer
    uint32_t cap_val_end_of_sample;  
    uint32_t full_cycle_ticks;            //Tick value after half a cycle, becomes the entire cycle after multiplying by two

    uint16_t valid_count;            //Count value of valid signals,Switching during half a cycle
    uint16_t valid_time;             //Number of valid signal verifications

    uint16_t invalid_count;          //Count value of invalid signals,Switching during half a cycle
    uint16_t invalid_time;           //Number of invalid signal verifications

    bool zero_source_power_invalid;  //Power loss flag when signal source is lost
    bool zero_singal_invaild;        //Signal is in an invalid range

    zero_signal_type_t zero_signal_type; //Zero Crossing Signal Type

    uint32_t capture_pin;

    double freq_range_max_tick;      //Tick value calculated after the user inputs the frequency
    double freq_range_min_tick;

    esp_zero_detect_cb_t event_callback;

    mcpwm_cap_timer_handle_t cap_timer;
    mcpwm_cap_channel_handle_t cap_chan;
    #if defined (CONFIG_USE_GPTIMER)
    gptimer_handle_t gptimer;
    #else
    esp_timer_handle_t esp_timer;
    #endif

} zero_cross_dev_t;

static IRAM_ATTR bool zero_detect_cb(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data)
{
    zero_cross_dev_t *zero_cross_dev = user_data;
    //Clear power down counter
    #if defined(CONFIG_USE_GPTIMER)
    gptimer_set_raw_count(zero_cross_dev->gptimer, 0); 
    #else
    esp_timer_restart(zero_cross_dev->esp_timer, 100000);
    #endif
    // Store the timestamp when pos edge is detected
    if (edata->cap_edge == MCPWM_CAP_EDGE_POS) {
        if (zero_cross_dev->zero_signal_type == PULSE_WAVE) { //The methods for calculating the periods of pulse signals and square wave signals are different
            zero_cross_dev->cap_val_begin_of_sample = zero_cross_dev->cap_val_end_of_sample; //Save the current value for the next calculation
            zero_cross_dev->cap_val_end_of_sample = edata->cap_value;  //Read the count value and calculate the current cycle
            zero_cross_dev->full_cycle_ticks = (zero_cross_dev->cap_val_end_of_sample - zero_cross_dev->cap_val_begin_of_sample)*2;
        } else {
            zero_cross_dev->cap_val_begin_of_sample = edata->cap_value;
            zero_cross_dev->full_cycle_ticks = (zero_cross_dev->cap_val_begin_of_sample - zero_cross_dev->cap_val_end_of_sample)*2; 
        }
        if (zero_cross_dev->full_cycle_ticks >= zero_cross_dev->freq_range_min_tick && zero_cross_dev->full_cycle_ticks <= zero_cross_dev->freq_range_max_tick) {
            zero_cross_dev->valid_count++; //Reset to zero, increment and evaluate the counting value
            zero_cross_dev->invalid_count = 0;
            if (zero_cross_dev->valid_count >= zero_cross_dev->valid_time) {
                zero_cross_dev->zero_singal_invaild = false;
                zero_cross_dev->zero_source_power_invalid = false;  
                //Enter the user callback function and return detection data
                if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                    zero_detect_cb_param_t param = {0};
                    param.signal_valid_event_data_t.valid_count = zero_cross_dev->valid_count;
                    param.signal_valid_event_data_t.full_cycle_ticks = zero_cross_dev->full_cycle_ticks;
                    param.signal_valid_event_data_t.cap_edge = edata->cap_edge;
                    zero_cross_dev->event_callback(SIGNAL_VALID, &param); 
                }
            }
        }
        if (zero_cross_dev->event_callback) {
            zero_detect_cb_param_t param = {0};
            param.signal_rising_edge_event_data_t.valid_count = zero_cross_dev->valid_count;
            param.signal_rising_edge_event_data_t.invalid_count = zero_cross_dev->invalid_count;
            param.signal_rising_edge_event_data_t.full_cycle_ticks = zero_cross_dev->full_cycle_ticks;
            zero_cross_dev->event_callback(SIGNAL_RISING_EDGE, &param);
        }
    } else if (edata->cap_edge == MCPWM_CAP_EDGE_NEG) {
        if (zero_cross_dev->zero_signal_type == SQUARE_WAVE) {  //The falling edge is only used with square wave signals
            //Calculate the interval in the ISR
            zero_cross_dev->cap_val_end_of_sample = edata->cap_value;
            zero_cross_dev->full_cycle_ticks = (zero_cross_dev->cap_val_end_of_sample - zero_cross_dev->cap_val_begin_of_sample)*2;  //Count value for half a period
            if (zero_cross_dev->full_cycle_ticks >= zero_cross_dev->freq_range_min_tick && zero_cross_dev->full_cycle_ticks <= zero_cross_dev->freq_range_max_tick) {   //Determine whether it is within the frequency range
                zero_cross_dev->valid_count++;
                zero_cross_dev->invalid_count = 0;
                if (zero_cross_dev->valid_count >= zero_cross_dev->valid_time) {
                    zero_cross_dev->zero_singal_invaild = false;
                    zero_cross_dev->zero_source_power_invalid = false;  
                    //Enter the user callback function and return detection data
                    if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                        zero_detect_cb_param_t param = {0};
                        param.signal_valid_event_data_t.valid_count = zero_cross_dev->valid_count;
                        param.signal_valid_event_data_t.full_cycle_ticks = zero_cross_dev->full_cycle_ticks;
                        param.signal_valid_event_data_t.cap_edge = edata->cap_edge;
                        zero_cross_dev->event_callback(SIGNAL_VALID, &param); 
                    }
                }
            }
        }
        if (zero_cross_dev->event_callback) {
            zero_detect_cb_param_t param = {0};
            param.signal_falling_edge_event_data_t.valid_count = zero_cross_dev->valid_count;
            param.signal_falling_edge_event_data_t.invalid_count = zero_cross_dev->invalid_count;
            param.signal_falling_edge_event_data_t.full_cycle_ticks = zero_cross_dev->full_cycle_ticks;
            zero_cross_dev->event_callback(SIGNAL_FALLING_EDGE, &param); 
        }
    }

    if (edata->cap_edge == MCPWM_CAP_EDGE_POS || (edata->cap_edge == MCPWM_CAP_EDGE_NEG && zero_cross_dev->zero_signal_type == SQUARE_WAVE)) {          //Exclude the case of the falling edge of the pulse signal
        if (zero_cross_dev->full_cycle_ticks < zero_cross_dev->freq_range_min_tick || zero_cross_dev->full_cycle_ticks > zero_cross_dev->freq_range_max_tick) {   //Determine whether it is within the frequency range
            zero_cross_dev->zero_singal_invaild = true; 
            zero_cross_dev->valid_count = 0;  
            zero_cross_dev->invalid_count++; 
            if (zero_cross_dev->invalid_count >= zero_cross_dev->invalid_time) {
                if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                    zero_detect_cb_param_t param = {0};
                    param.signal_invalid_event_data_t.invalid_count = zero_cross_dev->invalid_count;
                    param.signal_invalid_event_data_t.full_cycle_ticks = zero_cross_dev->full_cycle_ticks;
                    param.signal_invalid_event_data_t.cap_edge = edata->cap_edge;
                    zero_cross_dev->event_callback(SIGNAL_INVALID, &param);
                }
            }
            if (zero_cross_dev->event_callback && (zero_cross_dev->cap_val_end_of_sample != 0) && (zero_cross_dev->cap_val_begin_of_sample != 0)) {
                zero_detect_cb_param_t param = {0};
                param.signal_freq_event_data_t.cap_edge = edata->cap_edge;
                param.signal_freq_event_data_t.full_cycle_ticks = zero_cross_dev->full_cycle_ticks;
                zero_cross_dev->event_callback(SIGNAL_FREQ_OUT_OF_RANGE, &param);
            }
        }
    }
    return false;
}

#if defined(CONFIG_USE_GPTIMER)
static IRAM_ATTR bool zero_source_power_invalid_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    zero_cross_dev_t *zero_cross_dev = user_ctx;
    zero_cross_dev->zero_source_power_invalid = true;
    zero_cross_dev->full_cycle_ticks = 0;
    zero_cross_dev->cap_val_begin_of_sample = 0;
    zero_cross_dev->cap_val_end_of_sample = 0;
    zero_cross_dev->valid_count = 0;
    zero_cross_dev->zero_singal_invaild = 0;
    zero_cross_dev->event_callback(SIGNAL_LOST, NULL);

    return false;
}
#else
void IRAM_ATTR zero_source_power_invalid_cb(void *arg)
{
    zero_cross_dev_t *zero_cross_dev = arg;
    zero_cross_dev->zero_source_power_invalid = true;
    zero_cross_dev->full_cycle_ticks = 0;
    zero_cross_dev->cap_val_begin_of_sample = 0;
    zero_cross_dev->cap_val_end_of_sample = 0;
    zero_cross_dev->valid_count = 0;
    zero_cross_dev->zero_singal_invaild = 0;
    zero_cross_dev->event_callback(SIGNAL_LOST, NULL);
}
#endif


esp_err_t zero_detect_mcpwn_init(zero_detect_handle_t zcd_handle)
{
    esp_err_t ret = ESP_OK;
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;

    ESP_LOGI(TAG, "Install capture timer");
    
    mcpwm_capture_timer_config_t cap_conf = {
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
        .group_id = 0,
    };
    ESP_GOTO_ON_ERROR(mcpwm_new_capture_timer(&cap_conf, &zcd->cap_timer), err , TAG, "Install mcpwn capture timer failed");

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
        .on_cap = zero_detect_cb,
    };
    ESP_GOTO_ON_ERROR(mcpwm_capture_channel_register_event_callbacks(zcd->cap_chan, &cbs, zcd), err, TAG, "Mcpwm callback create failed");   //Craete a detect callback

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

#if defined(CONFIG_USE_GPTIMER)
esp_err_t zero_detect_gptime_init(zero_detect_handle_t zcd_handle)
{
    esp_err_t ret = ESP_OK;
    zero_cross_dev_t *zcd = (zero_cross_dev_t *) zcd_handle;

    ESP_LOGI(TAG, "Install gptimer");
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
    };
    ESP_GOTO_ON_ERROR(gptimer_new_timer(&timer_config, &zcd->gptimer), err, TAG, "Gpttimer create failed");

    ESP_LOGI(TAG, "Install gptimer alarm");
    gptimer_alarm_config_t gptimer_alarm = {
        .alarm_count = 100000,            //100ms
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
esp_err_t zero_detect_esptimer_init(zero_detect_handle_t zcd_handle)
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
    zero_cross_dev_t *zcd = (zero_cross_dev_t *) calloc(1, sizeof(zero_cross_dev_t));
    
    zcd->valid_time = config->valid_time;
    zcd->invalid_time = config->invalid_time;
    zcd->freq_range_max_tick = esp_clk_apb_freq() / config->freq_range_min_hz;   
    zcd->freq_range_min_tick = esp_clk_apb_freq() / config->freq_range_max_hz;
    zcd->capture_pin = config->capture_pin;
    zcd->event_callback = config->event_callback;
    zcd->zero_signal_type = config->zero_signal_type;

    #if defined(CONFIG_USE_GPTIMER)
    zero_detect_gptime_init(zcd);
    #else
    zero_detect_esptimer_init(zcd);
    #endif
    zero_detect_mcpwn_init(zcd);
    //Prevent power loss before zero-crossing signal is detected.
    #if defined(CONFIG_USE_GPTIMER)
    gptimer_start(zcd->gptimer);  
    #else
    esp_timer_start_periodic(zcd->esp_timer, 100000);  
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

    mcpwm_capture_timer_stop(zcd->cap_timer);
    mcpwm_capture_timer_disable(zcd->cap_timer);

    mcpwm_capture_channel_disable(zcd->cap_chan);
    ESP_GOTO_ON_ERROR(mcpwm_del_capture_channel(zcd->cap_chan), err, TAG, "Mcpwm channel delete failed");
    
    ESP_GOTO_ON_ERROR(mcpwm_del_capture_timer(zcd->cap_timer), err, TAG, "Mcpwm capture timer delete failed");
    //Free memory
    free(zcd);
    return ESP_OK;

err:
    if (zcd) {
        free(zcd);
    }
    return ret;
}

void zero_show_data(zero_detect_handle_t zcd_handle)
{
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    float pulse_width_us = 0;
    float detect_hz = 0;
    //ESP_LOGI(TAG, "End of Sample:%ld Begin of sample:%ld ticks:%ld", zcd->cap_val_end_of_sample,zcd->cap_val_begin_of_sample,zcd->full_cycle_ticks);
    pulse_width_us = zcd->full_cycle_ticks * (1000000.0 / esp_clk_apb_freq());
    detect_hz = 1000000 / pulse_width_us;
    ESP_LOGI(TAG, "Measured Time: %.2fms Hz:%.2f", pulse_width_us / 1000, detect_hz);
}

bool zero_detect_get_power_status(zero_detect_handle_t zcd_handle)
{
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    return zcd->zero_source_power_invalid;
}

bool zero_detect_signal_invaild_status(zero_detect_handle_t zcd_handle)
{
    zero_cross_dev_t *zcd = (zero_cross_dev_t *)zcd_handle;
    return zcd->zero_singal_invaild;
}