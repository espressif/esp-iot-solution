// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "iot_dac_audio.h"
#include "unity.h"
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
    dac = iot_dac_audio_create(0, 8000, 16, I2S_DAC_CHANNEL_RIGHT_EN, 1024, true);
    while (1) {
        for (int i = 0; i < sizeof(playlist)/sizeof(dac_audio_item_t); i++) {
            printf("playing file[%d]: %s\n", i, playlist[i].name);
            iot_dac_audio_play(dac, playlist[i].data, playlist[i].length, portMAX_DELAY);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
}

TEST_CASE("Dac audio test", "[dac_audio][iot][audio]")
{
    audio_test();
}

