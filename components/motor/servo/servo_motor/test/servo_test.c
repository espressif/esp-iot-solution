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
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "iot_servo.h"
#include "unity.h"


#define SERVO_GPIO 25

TEST_CASE("Servo_motor test", "[servo][iot]")
{
    iot_servo_init(SERVO_GPIO, 180, 500, 2500, LEDC_CHANNEL_0, LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);

    for (size_t i = 0; i < 18; i++) {
        ESP_LOGI("servo", "aet angle: %d", 10 * i);
        iot_servo_write(10 * i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
