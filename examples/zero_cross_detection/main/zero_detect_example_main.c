/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zero_detection.h"

#define ZERO_DETECTION_RELAY_CONFIG_DEFAULT() { \
    .relay_suspend = 0, \
    .relay_on_off = 0, \
    .relay_active_level = 1,\
    .relay_status_after_power_loss = 1,\
    .relay_out_of_range = RELAY_DEFAULT_LOW,\
    .control_pin = CONFIG_ZERO_DETECT_OUTPUT_GPIO,\
}

static const char *TAG = "example";

typedef enum {
    RELAY_CONTROL_BY_USER = 1,
    RELAY_DEFAULT_HIGH,
    RELAY_DEFAULT_LOW,
} zero_out_range_default_t;

typedef struct {
    bool relay_suspend;        //Pending user actions，This action will be executed when the zero-crossing condition is met
    bool relay_on_off;         //Current user switch command
    bool relay_active_level;   //Active level for controlling the relay
    bool relay_status_after_power_loss;  //Status After power loss
    int32_t control_pin;
    zero_out_range_default_t relay_out_of_range; //Actions taken when the signal is out of the frequency range
} zero_cross_relay_t;

uint16_t DRAM_ATTR freq_out_of_range_count = 0;
uint16_t DRAM_ATTR relay_off_count = 0;
uint16_t DRAM_ATTR relay_open_count = 0;

static zero_cross_relay_t zcd = ZERO_DETECTION_RELAY_CONFIG_DEFAULT(); //Default parameter
static zero_detect_handle_t *g_zcds;
zero_detect_config_t config = ZERO_DETECTION_INIT_CONFIG_DEFAULT(); //Default parameter

void IRAM_ATTR zero_detection_event_cb(zero_detect_event_t zero_detect_event, zero_detect_cb_param_t *param, void *usr_data)  //User's callback API
{
    switch (zero_detect_event) {
    case SIGNAL_FREQ_OUT_OF_RANGE:
        freq_out_of_range_count++;
        if (param->signal_freq_event_data.cap_edge == CAP_EDGE_POS) {
            if (zcd.relay_out_of_range == RELAY_DEFAULT_HIGH) {
                gpio_ll_set_level(&GPIO, zcd.control_pin, zcd.relay_active_level);
            } else if ((CONFIG_ZERO_DETECT_SIGNAL_TYPE == PULSE_WAVE) && (zcd.relay_out_of_range == RELAY_DEFAULT_LOW)) {  //The relay off performed on the rising edge of the pulse signal.
                gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
            }
        } else {
            if (zcd.relay_out_of_range == RELAY_DEFAULT_LOW) {
                gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
            }
        }
        break;
    case SIGNAL_VALID:
        if (param->signal_valid_event_data.cap_edge == CAP_EDGE_POS) {
            if (zcd.relay_suspend) {
                if (zcd.relay_on_off) {
                    gpio_ll_set_level(&GPIO, zcd.control_pin, zcd.relay_active_level);
                    zcd.relay_suspend = false;
                    relay_open_count++;
                }
                if (CONFIG_ZERO_DETECT_SIGNAL_TYPE == PULSE_WAVE) {
                    if (zcd.relay_on_off == false) {
                        gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
                        zcd.relay_suspend = false;
                        relay_off_count++;
                    }
                }
            }
        } else if (param->signal_valid_event_data.cap_edge == CAP_EDGE_NEG) {
            if (zcd.relay_suspend) {
                if (zcd.relay_on_off == false) {
                    gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
                    zcd.relay_suspend = false;
                    relay_off_count++;
                }
            }
        }
        break;
    case SIGNAL_LOST:
        if (zcd.relay_status_after_power_loss) {
            gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
            zcd.relay_suspend = false;
        }
        break;
    case SIGNAL_INVALID:
        if (param->signal_invalid_event_data.cap_edge == CAP_EDGE_POS) {
            if (zcd.relay_out_of_range == RELAY_CONTROL_BY_USER) {
                if (zcd.relay_suspend) { //Operation is suspend, on or off
                    if (zcd.relay_on_off == true) {
                        gpio_ll_set_level(&GPIO, zcd.control_pin, zcd.relay_active_level);
                        relay_open_count++;
                        zcd.relay_suspend = false;
                    }
                    if (CONFIG_ZERO_DETECT_SIGNAL_TYPE == PULSE_WAVE) {
                        if (zcd.relay_on_off == false) {
                            gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
                            relay_off_count++;
                            zcd.relay_suspend = false;
                        }
                    }
                }
            }
        } else if (param->signal_invalid_event_data.cap_edge == CAP_EDGE_NEG) {
            if (zcd.relay_out_of_range == RELAY_CONTROL_BY_USER) {
                if (zcd.relay_suspend) { //Operation is suspend, on or off
                    if (zcd.relay_on_off == false) {
                        gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
                        relay_off_count++;
                        zcd.relay_suspend = false;
                    }
                }
            }
        }
        break;
    default:
        break;
    }
}

esp_err_t relay_on_off(bool on_off)
{
    zcd.relay_on_off = on_off;
    zcd.relay_suspend = true;
    return ESP_OK;
}

void app_main(void)
{
    config.capture_pin = CONFIG_ZERO_DETECT_INPUT_GPIO;
    config.freq_range_max_hz = 65;  //Hz
    config.freq_range_min_hz = 45;  //Hz
    config.signal_lost_time_us = 100000; //Us
    config.valid_times = 6;
    config.zero_signal_type = CONFIG_ZERO_DETECT_SIGNAL_TYPE;
#if defined(SOC_MCPWM_SUPPORTED)
    config.zero_driver_type = MCPWM_TYPE;
#else
    config.zero_driver_type = GPIO_TYPE;
#endif
    g_zcds = zero_detect_create(&config);
    relay_on_off(true);  //User API to control the relay

    //Init the output GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(zcd.control_pin);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_ll_set_level(&GPIO, zcd.control_pin, !zcd.relay_active_level);
    gpio_config(&io_conf);
    zero_detect_register_cb(g_zcds, zero_detection_event_cb, NULL);

    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        zero_show_data(g_zcds);           //Show zero cross data
        printf("EVENT: OUT OF RANGE COUNT:%d OFF COUNT:%d OPEN COUNT:%d\n", freq_out_of_range_count, relay_off_count, relay_open_count);

        // Test pause and resume function
        if (freq_out_of_range_count > 5) {
            ESP_LOGI(TAG, "Pausing zero detection......");
            zero_detect_pause(g_zcds);
        }

        if (relay_open_count > 5) {
            ESP_LOGI(TAG, "Resuming zero detection......");
            zero_detect_resume(g_zcds);
            relay_open_count = 0;
        }

        if (zcd.relay_suspend) {
            ESP_LOGW(TAG, "Process suspened, please wait till relay open");
        }
        if (zcd.relay_on_off && zcd.relay_suspend) {
            ESP_LOGI(TAG, "Set control relay on");
        }
        if ((zcd.relay_on_off == false) && zcd.relay_suspend) {
            ESP_LOGI(TAG, "Set control relay off");
        }
        if (zero_detect_get_power_status(g_zcds)) {
            ESP_LOGE(TAG, "Source power invalid");
            relay_on_off(true);    //Try to open relay until the signal back to normal
        }
        // g_zcds = zero_detect_create(&config);           //Delete function test
        // printf("After Create%d\n",heap_caps_get_free_size(MALLOC_CAP_8BIT));
        // zero_detect_delete(g_zcds);
        // printf("After Delete%d\n",heap_caps_get_free_size(MALLOC_CAP_8BIT));
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
