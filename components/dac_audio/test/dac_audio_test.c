/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "dac_audio.h"

#include "ja_alert_e2.h"
#include "ja_alert.h"
#include "ja_off_e2.h"
#include "ja_off.h"
#include "ja_on_e2.h"
#include "ja_on.h"
#include "ja_press_e2.h"
#include "ja_press.h"
#include "ja_press3x_e2.h"
#include "ja_press3x.h"
#include "ja_prompt_e2.h"
#include "ja_prompt.h"

typedef struct {
    char* name;
    const uint8_t* data;
    int length;
} dac_audio_item_t;

dac_audio_handle_t dac = NULL;
void audio_test()
{
    dac_audio_item_t playlist[] = {
            {"alert_01", audio_alert, sizeof(audio_alert)},
            {"alert_02", audio_alert_e2, sizeof(audio_alert_e2)},
            {"audio_off", audio_off, sizeof(audio_off)},
            {"audio_off_02", audio_off_e2, sizeof(audio_off_e2)},
            {"audio_on", audio_on, sizeof(audio_on)},
            {"audio_on_02", audio_on_e2, sizeof(audio_on_e2)},
            {"audio_press", audio_press, sizeof(audio_press)},
            {"audio_press_02", audio_press_e2, sizeof(audio_press_e2)},
            {"audio_press3x", audio_press3x, sizeof(audio_press3x)},
            {"audio_press3x_02", audio_press3x_e2, sizeof(audio_press3x_e2)},
            {"audio_prompt", audio_prompt, sizeof(audio_prompt)},
            {"audio_prompt_02", audio_prompt_e2, sizeof(audio_prompt_e2)},

    };
    dac = dac_audio_create(0, 8000, 16, I2S_DAC_CHANNEL_RIGHT_EN, 1024, true);
    while (1) {
        for (int i = 0; i < sizeof(playlist)/sizeof(dac_audio_item_t); i++) {
            printf("playing file[%d]: %s\n", i, playlist[i].name);
            dac_audio_play(dac, playlist[i].data, playlist[i].length, portMAX_DELAY);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
}


