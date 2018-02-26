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

#ifndef _IOT_STEPPER_A4988_H_
#define _IOT_STEPPER_A4988_H_
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/pcnt.h"
#include "freertos/timers.h"

/**
 *
 * Controls signals for 2 control wires(single phase or dual phase inverted polarity externally):
 *
 * step_num  A+  A-/B+
 *    1      1     0
 *    2      1     1
 *    3      0     1
 *    4      0     0
 *
 *
 * Control signals for 4 control wires(2 phase single on):
 *
 * step_num  A+   B+   A-   B-
 *    1      1    0    0    0
 *    2      0    1    0    0
 *    3      0    0    1    0
 *    4      0    0    0    1
 *
 *
 * Control signals for 4 control wires(2 phase dual on):
 *
 * step_num  A+   A-   B+   B-
 *    1      1    0    1    0
 *    2      0    1    1    0
 *    3      0    1    0    1
 *    4      1    0    0    1
 *
 *
 * Control signals for 5 control wires(4 phase - one wire connected to +VCC):
 *
 * step_num  A    B    C    D
 *    1      1    0    0    0
 *    2      1    1    0    0
 *    3      0    1    0    0
 *    4      0    1    1    0
 *    5      0    0    1    0
 *    6      0    0    1    1
 *    7      0    0    0    1
 *    8      1    0    0    1
 *
 *
 */

#ifdef __cplusplus

class CA4988Stepper
{
private:
    void* m_stepper;

    /**
     * prevent copy constructing
     */
    CA4988Stepper(const CA4988Stepper&);
    CA4988Stepper& operator =(const CA4988Stepper&);

public:
    /**
     * @brief Constructor for CA4988Stepper class
     * @param step_io Output GPIO for step signal
     * @param dir_io Output GPIO for direction signal
     * @param number_of_steps The number of steps in one revolution of the stepper motor.
     * @param speed_mode LEDC mode
     * @param tim_idx LEDC time index
     * @param chn LEDC channel index
     * @param pcnt_unit PCNT unit index
     * @param pcnt_chn PCNT channel index
     *
     * @note Different stepper object must use different LEDC CHANNELS and PCNT UNITS.
     *       Make sure the LEDC CHANNELS and PCNT UNITS you are using do not conflict with other modules.
     */
    CA4988Stepper(int step_io, int dir_io, int number_of_steps = 200, ledc_mode_t speed_mode = LEDC_HIGH_SPEED_MODE,
            ledc_timer_t tim_idx = LEDC_TIMER_0, ledc_channel_t chn = LEDC_CHANNEL_0,
            pcnt_unit_t pcnt_unit = PCNT_UNIT_0, pcnt_channel_t pcnt_chn = PCNT_CHANNEL_0);

    /**
     * @brief To turn the motor a specific number of steps.
     * @param steps The number of steps to turn, negative or positive values determine the direction
     * @param ticks_to_wait block time for this function
     * @param instant whether to switch to the current speed directly,
     *        if false, the driver would increase the speed gradually
     * @return ESP_OK if success
     *         ESP_ERR_TIMEOUT if operation timeout
     */
    esp_err_t step(int steps, TickType_t ticks_to_wait = portMAX_DELAY, bool instant = false);

    /**
     * @brief Make the motor run towards a direction.
     * @param dir the direction to run, positive or negative values.
     * @param instant whether to switch to the current speed directly,
     *        if false, the driver would increase the speed gradually
     * @return ESP_OK if success
     */
    esp_err_t run(int dir, bool instant = false);

    /**
     * @brief Stop the motor
     * @param instant whether to stop the motor immediately
     * @return ESP_OK if success
     */
    esp_err_t stop(bool instant = true);

    /**
     * @brief Wait the steps to finish
     * @return ESP_OK if success
     */
    esp_err_t wait();

    /**
     * @brief Set the target speed, in RPM
     * @param rpm Rounds per minute
     * @param instant whether to change the frequency directly
     * @return ESP_OK if success
     */
    esp_err_t setSpeedRpm(int rpm, bool instant = false);

    /**
     * @brief Set the speed, immediately
     * @param rpm Rounds per minute
     * @return ESP_OK if success
     */
    esp_err_t setSpeedRpmCur(int rpm);

    /**
     * @brief Get the current motor speed, in rpm
     * @return rpm
     */
    int getSpeedRpm();

    /**
     * @brief Get the target speed, in rpm
     * @note After setting the target speed, the driver might increase the current speed gradually
     * @return target rpm
     */
    int getSpeedRpmTarget();

    /**
     * @brief Destructor function of CA4988Stepper object
     */
    ~CA4988Stepper(void);
};

#endif

#endif /* _IOT_STEPPER_A4988_H_ */
