// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
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

#include "buzzer.h"
#include "esp_log.h"

#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE

void buzzer_driver_install(gpio_num_t buzzer_pin)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,  //Resolution of PWM duty
        .freq_hz = 5000,                       //Frequency of PWM signal
        .speed_mode = LEDC_LS_MODE,            //Timer mode
        .timer_num = LEDC_LS_TIMER,            //Timer index
        .clk_cfg = LEDC_AUTO_CLK,              //Auto select the source clock (REF_TICK or APB_CLK or RTC_8M_CLK)
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_LS_CH0_CHANNEL,
        .duty       = 4096,
        .gpio_num   = buzzer_pin,
        .speed_mode = LEDC_LS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_LS_TIMER            //Let timer associate with LEDC channel (Timer1)
    };
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
}

void buzzer_set_voice(bool en)
{
    uint32_t freq = en ? 5000 : 0;
    ledc_set_duty(LEDC_LS_MODE, LEDC_LS_CH0_CHANNEL, freq);
    ledc_update_duty(LEDC_LS_MODE, LEDC_LS_CH0_CHANNEL);
}
