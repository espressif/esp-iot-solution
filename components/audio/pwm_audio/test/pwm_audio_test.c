// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include "unity.h"
#include "test_utils.h"
#include "esp_log.h"
#include "math.h"
#include "pwm_audio.h"

static const char *TAG = "pwm_audio test";


#define LEFT_CHANNEL_GPIO  26
#define RIGHT_CHANNEL_GPIO 25


TEST_CASE("pwm audio sine wave test", "[audio][iot]")
{
    const float PI_2 = 6.283185307179f;
    int8_t *data_buffer;
    const uint32_t size = 240;  /*< f = 48000 / 240 = 200Hz*/

    data_buffer = malloc(size*2);
    TEST_ASSERT_NOT_NULL(data_buffer);

    /**
     * Generating two channel waveforms with phase difference
     */
    for (size_t i = 0; i < size; i++) {
        data_buffer[i*2] = 127.8f * sinf(PI_2 * (float)i / (float)size);
        data_buffer[i*2+1] = 127.8f * cosf(PI_2 * (float)i / (float)size);
    }

    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_8_BIT;
    pac.gpio_num_left      = LEFT_CHANNEL_GPIO;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = RIGHT_CHANNEL_GPIO;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 8;
    TEST_ASSERT(ESP_OK == pwm_audio_init(&pac));
    pwm_audio_set_param(48000, 8, 2);
    pwm_audio_start();

    size_t length = size * 2;
    uint32_t index = 0;
    size_t cnt;
    uint32_t block_w = 256;
    while (1) {
        if (index < length) {
            if ((length - index) < block_w) {
                block_w = length - index;
            }
            pwm_audio_write((uint8_t *)data_buffer + index, block_w, &cnt, 1000 / portTICK_PERIOD_MS);
            index += cnt;
        } else {
            index = 0;
            block_w = 256;
        }
    }
    TEST_ASSERT(ESP_OK == pwm_audio_deinit());
    free(data_buffer);
}

extern const uint8_t wave_array_32000_8_1[];
extern const uint8_t wave_array_32000_8_2[];
extern const uint8_t wave_array_32000_16_1[];
extern const uint8_t wave_array_32000_16_2[];

static void play_audio(const uint8_t *data, size_t wave_size, uint32_t rate, uint32_t bits, uint32_t ch)
{
    uint32_t index = 0;
    size_t cnt;
    uint32_t block_w = 2048;
    ESP_LOGI(TAG, "paramter: samplerate:%d, bits:%d, channel:%d", rate, bits, ch);
    pwm_audio_set_param(rate, bits, ch);
    pwm_audio_start();

    while (1) {
        if (index < wave_size) {
            if ((wave_size - index) < block_w) {
                block_w = wave_size - index;
            }
            pwm_audio_write((uint8_t*)data + index, block_w, &cnt, 1000 / portTICK_PERIOD_MS);
            ESP_LOGD(TAG, "write [%d] [%d]", block_w, cnt);
            index += cnt;
        } else {
            ESP_LOGI(TAG, "play completed");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    pwm_audio_stop();
}

static void play_with_param(ledc_timer_bit_t duty_resolution, uint8_t hw_ch)
{
    pwm_audio_config_t pac;
    pac.duty_resolution    = duty_resolution;
    pac.gpio_num_left      = LEFT_CHANNEL_GPIO;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    if (2 == hw_ch) {
        pac.gpio_num_right     = RIGHT_CHANNEL_GPIO;
    } else {
        pac.gpio_num_right     = -1;
    }
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 8;
    TEST_ASSERT(ESP_OK == pwm_audio_init(&pac));

    ESP_LOGI(TAG, "\n------------------------------");
    ESP_LOGI(TAG, "hw info: resolution: %dbit, ch: %d", duty_resolution, hw_ch);
    pwm_audio_set_volume(0);
    play_audio(wave_array_32000_8_1, 64000, 32000, 8, 1);
    pwm_audio_set_volume(-10);
    play_audio(wave_array_32000_8_2, 128000, 32000, 8, 2);
    pwm_audio_set_volume(15);
    play_audio(wave_array_32000_16_1, 128000, 32000, 16, 1);
    pwm_audio_set_volume(0);
    play_audio(wave_array_32000_16_2, 256000, 32000, 16, 2);
    TEST_ASSERT(ESP_OK == pwm_audio_deinit());

    ESP_LOGI(TAG, "------------------------------\n");
}

TEST_CASE("pwm audio play music test", "[audio][iot]")
{
    play_with_param(LEDC_TIMER_8_BIT, 1);
    play_with_param(LEDC_TIMER_9_BIT, 1);
    play_with_param(LEDC_TIMER_10_BIT, 1);

    play_with_param(LEDC_TIMER_8_BIT, 2);
    play_with_param(LEDC_TIMER_9_BIT, 2);
    play_with_param(LEDC_TIMER_10_BIT, 2);
}

