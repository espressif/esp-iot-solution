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

#ifdef __cplusplus

class CServo
{
private:
    ledc_channel_t m_chn;
    ledc_mode_t m_mode;
    ledc_timer_t m_timer;
    int m_full_duty;
    int m_freq;
    float m_angle;
    uint32_t m_max_angle;
    uint32_t m_min_us;
    uint32_t m_max_us;

    /**
     * prevent copy constructing
     */
    CServo(const CServo&);
    CServo& operator =(const CServo&);
public:
    /**
     * @brief Constructor for CServo class
     * @param servo_io GPIO incdex for servo signal
     * @param max_angle Max angle of servo motor, positive value.
     * @param min_width_us Minimum pulse width, corresponding to 0 position.
     * @param max_width_us Maximum pulse width, corresponding to max_angle position.
     * @param chn LEDC hardware channel
     * @param speed_mode Speed mode for LEDC hardware
     * @param tim_idx Timer index for LEDC hardware
     */
    CServo(int servo_io, int max_angle = 180, int min_width_us = 500, int max_width_us = 2500, ledc_channel_t chn = LEDC_CHANNEL_0,
            ledc_mode_t speed_mode = LEDC_HIGH_SPEED_MODE, ledc_timer_t tim_idx = LEDC_TIMER_0);

    /**
     * @brief Set the servo motor to a certain position
     * @param angle the angle to go
     * @return ESP_OK if success
     */
    esp_err_t write(float angle);

    /**
     * @brief Destructor function of CServo object
     */
    ~CServo(void);
};

#endif

#endif /* _IOT_STEPPER_A4988_H_ */
