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

#ifndef _IOT_SERVO_H_
#define _IOT_SERVO_H_
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"

/**
 * @brief Constructor for CServo class
 * 
 * @param servo_io GPIO incdex for servo signal
 * @param max_angle Max angle of servo motor, positive value.
 * @param min_width_us Minimum pulse width, corresponding to 0 position.
 * @param max_width_us Maximum pulse width, corresponding to max_angle position.
 * @param chn LEDC hardware channel
 * @param speed_mode Speed mode for LEDC hardware
 * @param tim_idx Timer index for LEDC hardware
 * 
 * @return ESP_OK if success
 */
esp_err_t iot_servo_init(int servo_io, int max_angle, int min_width_us, int max_width_us, ledc_channel_t chn, ledc_mode_t speed_mode, ledc_timer_t tim_idx);

/**
 * @brief Set the servo motor to a certain position
 * 
 * @param angle the angle to go
 * 
 * @return ESP_OK if success
 */
esp_err_t iot_servo_write(float angle);


#endif /* _IOT_STEPPER_A4988_H_ */
