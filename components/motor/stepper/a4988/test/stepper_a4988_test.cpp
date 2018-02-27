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
#include "iot_a4988.h"
#include "unity.h"

TEST_CASE("Stepper A4988 test", "[stepper][a4988][iot]")
{
    int steps = 800*3;
    int rpm = 60*5;

    CA4988Stepper stepper(2, 15, 800);
    CA4988Stepper stepper2(21, 18, 800, LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_1, PCNT_UNIT_1, PCNT_CHANNEL_0);

    stepper.setSpeedRpm(rpm);
    stepper.step(steps);
    vTaskDelay(1000/portTICK_RATE_MS);
    stepper.step(-1*steps);
    vTaskDelay(1000/portTICK_RATE_MS);

    stepper.setSpeedRpm(rpm);
    stepper.step(steps);
    vTaskDelay(1000/portTICK_RATE_MS);
    stepper.step(-1*steps);
    vTaskDelay(1000/portTICK_RATE_MS);

    stepper2.setSpeedRpm(rpm);
    stepper2.step(steps);
    vTaskDelay(1000/portTICK_RATE_MS);
    stepper2.step(-1*steps);
    vTaskDelay(1000/portTICK_RATE_MS);

    stepper2.setSpeedRpm(rpm);
    stepper2.step(steps);
    vTaskDelay(1000/portTICK_RATE_MS);
    stepper2.step(-1*steps);
    vTaskDelay(1000/portTICK_RATE_MS);

    stepper.setSpeedRpm(60*3);
    stepper2.setSpeedRpm(60*3);
    stepper.run(1);
    stepper2.run(1);
    vTaskDelay(5000/portTICK_RATE_MS);
    stepper.run(-1);
    stepper2.run(-1);
    vTaskDelay(5000/portTICK_RATE_MS);
    stepper.stop();
    stepper2.stop();

}
