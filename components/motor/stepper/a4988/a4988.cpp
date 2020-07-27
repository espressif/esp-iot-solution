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
#ifdef __cplusplus
extern "C" {
#endif

#include <esp_types.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/gpio.h"
#include "driver/ledc.h"
#include "driver/pcnt.h"
#include "soc/ledc_struct.h"
#include "iot_a4988.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

static const char* STEPPER_A4988_TAG = "stepper_a4988";
#define STEPPER_A4988_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(STEPPER_A4988_TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }

#define STEPPER_CNT_H  10000
#define STEPPER_CNT_L  -10000
#define STEPPER_LEDC_INIT_BITS ((ledc_timer_bit_t)7)
#define STEPPER_LEDC_DUTY  ((1ULL << STEPPER_LEDC_INIT_BITS) / 2 - 1)
#define STEPPER_LEDC_INIT_FREQ 1000
#define STEPPER_INIT_RPM_DEFAULT   (60)
#define STEPPER_SPEED_UP_DELAY_MS     (20)
#define STEPPER_START_LIMIT_RPM       (60*3)
#define STEPPER_INC_GAP               (60)
typedef struct {
    int step_io;
    int dir_io;
    ledc_timer_t ledc_timer;
    ledc_channel_t ledc_channel;
    ledc_mode_t ledc_mode;
    pcnt_channel_t pcnt_channel;
    pcnt_unit_t pcnt_unit;
    TimerHandle_t tmr;
    int rpm;
    int rpm_target;
    bool stop;
    int number_of_steps;
    int steps_left;
    int direction;
    SemaphoreHandle_t sem;
    SemaphoreHandle_t mux;
} stepper_dev_t;

static void IRAM_ATTR stepper_pcnt_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    portBASE_TYPE HPTaskAwoken = pdFALSE;
    stepper_dev_t* dev = ((stepper_dev_t*) arg);
    int i = dev->pcnt_unit;
    if (intr_status & (BIT(i))) {
        PCNT.int_clr.val = BIT(i);
        int16_t cnt = (int16_t) PCNT.cnt_unit[i].cnt_val;
        dev->steps_left -= cnt;
        if (dev->steps_left <= 0) {
            LEDC.channel_group[dev->ledc_mode].channel[dev->ledc_channel].conf0.idle_lv = 0;
            LEDC.channel_group[dev->ledc_mode].channel[dev->ledc_channel].conf0.sig_out_en = 0;
            LEDC.channel_group[dev->ledc_mode].channel[dev->ledc_channel].conf1.duty_start = 0;
            if (dev->ledc_mode == LEDC_LOW_SPEED_MODE) {
                LEDC.channel_group[dev->ledc_mode].channel[dev->ledc_channel].conf0.low_speed_update = 1;
            }
            dev->steps_left = 0;
            dev->rpm = 0;
            if (dev->tmr){
                xTimerStopFromISR(dev->tmr, &HPTaskAwoken);
            }
            xSemaphoreGiveFromISR(dev->sem, &HPTaskAwoken);
        } else {
            int steps = dev->steps_left >= STEPPER_CNT_H ? STEPPER_CNT_H - 1 : dev->steps_left;
            PCNT.conf_unit[dev->pcnt_unit].conf1.cnt_thres0 = steps;
            PCNT.conf_unit[dev->pcnt_unit].conf0.thr_thres0_en = 1;
            // Counter reset
            uint32_t reset_bit = BIT(PCNT_PLUS_CNT_RST_U0_S + (dev->pcnt_unit * 2));
            PCNT.ctrl.val |= reset_bit;
            PCNT.ctrl.val &= ~reset_bit;
            ESP_EARLY_LOGD(STEPPER_A4988_TAG, "thres cnt: %d\n", PCNT.conf_unit[dev->pcnt_unit].conf1.cnt_thres0);
        }
        ESP_EARLY_LOGD(STEPPER_A4988_TAG, "cnt: %d,l:%d\n", cnt, dev->steps_left);
    }
    if (HPTaskAwoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void stepper_speed_up_cb(xTimerHandle tmr)
{
    CA4988Stepper* pobj = (CA4988Stepper*) pvTimerGetTimerID(tmr);
    int rpm_target = pobj->getSpeedRpmTarget();
    int rpm_cur = pobj->getSpeedRpm();
    ESP_LOGD(STEPPER_A4988_TAG, "tmr: rpm cur: %d; target: %d\n", rpm_cur, rpm_target);
    if (rpm_cur == rpm_target) {
        return;
    }
    if (rpm_cur < rpm_target) {
        rpm_cur = (rpm_cur + STEPPER_INC_GAP > rpm_target) ? rpm_target : rpm_cur + STEPPER_INC_GAP;
    } else if (rpm_cur > rpm_target) {
        rpm_cur = (rpm_cur - STEPPER_INC_GAP < rpm_target) ? rpm_target : rpm_cur - STEPPER_INC_GAP;
    }
    pobj->setSpeedRpmCur(rpm_cur);
    xTimerReset(tmr, portMAX_DELAY);
}

CA4988Stepper::CA4988Stepper(int step_io, int dir_io, int number_of_steps, ledc_mode_t speed_mode, ledc_timer_t tim_idx, ledc_channel_t chn,
        pcnt_unit_t pcnt_unit, pcnt_channel_t pcnt_chn)
{
    esp_log_level_set(STEPPER_A4988_TAG, ESP_LOG_DEBUG);
    stepper_dev_t *pstepper = (stepper_dev_t*) calloc(sizeof(stepper_dev_t), 1);
    pstepper->step_io = step_io;
    pstepper->dir_io = dir_io;
    pstepper->ledc_timer = tim_idx;
    pstepper->ledc_channel = chn;
    pstepper->ledc_mode = speed_mode;
    pstepper->steps_left = 0;
    pstepper->pcnt_channel = pcnt_chn;
    pstepper->pcnt_unit = pcnt_unit;
    pstepper->direction = 0;
    pstepper->tmr = NULL;
    pstepper->rpm = STEPPER_INIT_RPM_DEFAULT;
    pstepper->rpm_target = STEPPER_INIT_RPM_DEFAULT;
    pstepper->number_of_steps = number_of_steps;
    pstepper->sem = xSemaphoreCreateBinary();
    pstepper->mux = xSemaphoreCreateMutex();
    m_stepper = pstepper;

    ledc_timer_config_t ledc_timer;
    ledc_timer.duty_resolution = STEPPER_LEDC_INIT_BITS;   // resolution of PWM duty
    ledc_timer.freq_hz = STEPPER_LEDC_INIT_FREQ;           // frequency of PWM signal
    ledc_timer.speed_mode = pstepper->ledc_mode;           // timer mode
    ledc_timer.timer_num = pstepper->ledc_timer;           // timer index
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_ch={0};
    ledc_ch.channel    = pstepper->ledc_channel;
    ledc_ch.duty       = 0;
    ledc_ch.gpio_num   = pstepper->step_io;
    ledc_ch.speed_mode = pstepper->ledc_mode;
    ledc_ch.timer_sel  = pstepper->ledc_timer;
    ledc_channel_config(&ledc_ch);

    if (pstepper->dir_io > 0) {
        gpio_config_t dir;
        dir.intr_type = GPIO_INTR_DISABLE;
        dir.mode = GPIO_MODE_OUTPUT;
        dir.pin_bit_mask = (1LL << pstepper->dir_io);
        dir.pull_down_en = GPIO_PULLDOWN_DISABLE;
        dir.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&dir);
    }

    pcnt_config_t pcnt;
    pcnt.pulse_gpio_num = pstepper->step_io;
    pcnt.ctrl_gpio_num = (-1);
    pcnt.channel = pstepper->pcnt_channel;
    pcnt.unit = pstepper->pcnt_unit;
    pcnt.pos_mode = PCNT_COUNT_DIS;
    pcnt.neg_mode = PCNT_COUNT_INC;
    pcnt.lctrl_mode = PCNT_MODE_KEEP;
    pcnt.hctrl_mode = PCNT_MODE_KEEP;
    pcnt.counter_h_lim = STEPPER_CNT_H;
    pcnt.counter_l_lim = STEPPER_CNT_L;
    pcnt_unit_config(&pcnt);
    pcnt_isr_service_install(0);
    pcnt_isr_handler_add(pstepper->pcnt_unit, stepper_pcnt_intr_handler, (void*)this->m_stepper);

    // Set signal input and output
    int sig_base  = (pstepper->pcnt_channel == 0) ? PCNT_SIG_CH0_IN0_IDX  : PCNT_SIG_CH1_IN0_IDX;
    if (pstepper->pcnt_unit > 4) {
        sig_base  += 12; // GPIO matrix assignments have a gap between units 4 & 5
    }
    int input_sig_index = sig_base  + (4 * pstepper->pcnt_unit);
    gpio_set_direction((gpio_num_t) pstepper->step_io, GPIO_MODE_INPUT_OUTPUT);
    gpio_matrix_out(pstepper->step_io, LEDC_HS_SIG_OUT0_IDX + pstepper->ledc_channel, 0, 0);
    gpio_matrix_in(pstepper->step_io, input_sig_index, 0);
    this->setSpeedRpmCur(pstepper->rpm);
}

esp_err_t CA4988Stepper::stop(bool instant)
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    if (instant || pstepper->rpm <= STEPPER_START_LIMIT_RPM) {
        pstepper->steps_left = 0;
        pstepper->rpm = 0;
        if (pstepper->tmr) {
            xTimerStop(pstepper->tmr, portMAX_DELAY);
        }
        pcnt_intr_disable(pstepper->pcnt_unit);
        ledc_stop(pstepper->ledc_mode, pstepper->ledc_channel, 0);
        xSemaphoreGive(pstepper->sem);
    } else {
        setSpeedRpmCur(STEPPER_START_LIMIT_RPM);
        vTaskDelay(100 / portTICK_RATE_MS);
        this->stop(true);
    }
    return ESP_OK;
}

int CA4988Stepper::getSpeedRpm()
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    return pstepper->rpm;
}

int CA4988Stepper::getSpeedRpmTarget()
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    return pstepper->rpm_target;
}

esp_err_t CA4988Stepper::setSpeedRpm(int rpm, bool instant)
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    pstepper->rpm_target = rpm;
    if (instant) {
        setSpeedRpmCur(rpm);
    } else {
        if (pstepper->steps_left > 0) {
            if (pstepper->tmr == NULL) {
                pstepper->tmr = xTimerCreate("Stepper", STEPPER_SPEED_UP_DELAY_MS / portTICK_RATE_MS, pdFALSE, this,
                        stepper_speed_up_cb);
            }
            xTimerStart(pstepper->tmr, portMAX_DELAY);
        }
    }
    return ESP_OK;
}

esp_err_t CA4988Stepper::setSpeedRpmCur(int rpm)
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    pstepper->rpm = rpm;
    int freq = rpm * pstepper->number_of_steps / 60;
    if (freq > 0) {
        ESP_LOGI(STEPPER_A4988_TAG, "set freq: %d\n", freq);
        ledc_set_freq(pstepper->ledc_mode, pstepper->ledc_timer, freq);
    } else {
        ESP_LOGE(STEPPER_A4988_TAG, "LEDC freq error.");
    }
    return ESP_OK;
}

esp_err_t CA4988Stepper::wait()
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    xSemaphoreTake(pstepper->mux, portMAX_DELAY);
    if (pstepper->steps_left != 0) {
        xSemaphoreTake(pstepper->sem, portMAX_DELAY);
    }
    xSemaphoreGive(pstepper->mux);
    return ESP_OK;
}

esp_err_t CA4988Stepper::run(int dir, bool instant)
{
    ESP_LOGI(STEPPER_A4988_TAG, "Stepper run, dir: %d \n", dir);
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    xSemaphoreTake(pstepper->mux, portMAX_DELAY);
    xSemaphoreTake(pstepper->sem, 0);
    pstepper->steps_left = 0x7fffffff;
    pstepper->direction = dir >= 0 ? 1 : -1;
    if (pstepper->direction > 0) {
        gpio_set_level((gpio_num_t)pstepper->dir_io, 1);
    } else {
        gpio_set_level((gpio_num_t)pstepper->dir_io, 0);
    }

    ESP_LOGD(STEPPER_A4988_TAG, "target rpm: %d; rpm: %d\n", pstepper->rpm_target, pstepper->rpm);
    if (instant || pstepper->rpm_target <= STEPPER_START_LIMIT_RPM) {
        setSpeedRpmCur(pstepper->rpm_target);
    } else {
        if (pstepper->tmr == NULL) {
            pstepper->tmr = xTimerCreate("Stepper", STEPPER_SPEED_UP_DELAY_MS / portTICK_RATE_MS, pdFALSE,
                    this, stepper_speed_up_cb);
        }
        xTimerStop(pstepper->tmr, portMAX_DELAY);
        if (pstepper->rpm < STEPPER_START_LIMIT_RPM || pstepper->steps_left == 0) {
            ESP_LOGD(STEPPER_A4988_TAG, "run: set rpm cur: %d\n", STEPPER_START_LIMIT_RPM);
            setSpeedRpmCur(STEPPER_START_LIMIT_RPM);
        }
        xTimerStart(pstepper->tmr, portMAX_DELAY);
    }
    ledc_set_duty(pstepper->ledc_mode, pstepper->ledc_channel, STEPPER_LEDC_DUTY);
    ledc_update_duty(pstepper->ledc_mode, pstepper->ledc_channel);
    xSemaphoreGive(pstepper->mux);
    return ESP_OK;
}

esp_err_t CA4988Stepper::step(int steps, TickType_t ticks_to_wait, bool instant)
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    xSemaphoreTake(pstepper->mux, portMAX_DELAY);
    pstepper->steps_left = steps >= 0 ? steps : (-1 * steps);

    if (steps >= 0) {
        pstepper->direction = 1;
        gpio_set_level((gpio_num_t)pstepper->dir_io, 1);
    } else {
        pstepper->direction = -1;
        gpio_set_level((gpio_num_t)pstepper->dir_io, 0);
    }

    steps = pstepper->steps_left;
    if (steps > STEPPER_CNT_H) {
        steps = STEPPER_CNT_H - 1;
    }
    pcnt_set_event_value(pstepper->pcnt_unit, PCNT_EVT_THRES_0, steps);
    pcnt_event_enable(pstepper->pcnt_unit, PCNT_EVT_THRES_0);
    pcnt_counter_clear(pstepper->pcnt_unit);
    xSemaphoreTake(pstepper->sem, 0);
    pcnt_intr_enable(pstepper->pcnt_unit);
    if (instant || pstepper->rpm_target <= STEPPER_START_LIMIT_RPM) {
        setSpeedRpmCur(pstepper->rpm_target);
    } else {
        if (pstepper->tmr == NULL) {
            pstepper->tmr = xTimerCreate("Stepper", STEPPER_SPEED_UP_DELAY_MS / portTICK_RATE_MS, pdFALSE, this,
                    stepper_speed_up_cb);
        }
        xTimerStop(pstepper->tmr, portMAX_DELAY);
        if (pstepper->rpm < STEPPER_START_LIMIT_RPM || pstepper->steps_left == 0) {
            setSpeedRpmCur(STEPPER_START_LIMIT_RPM);
        }
        xTimerStart(pstepper->tmr, portMAX_DELAY);
    }

    ledc_set_duty(pstepper->ledc_mode, pstepper->ledc_channel, STEPPER_LEDC_DUTY);
    ledc_update_duty(pstepper->ledc_mode, pstepper->ledc_channel);

    BaseType_t res = xSemaphoreTake(pstepper->sem, ticks_to_wait);
    xSemaphoreGive(pstepper->mux);
    if (res == pdFALSE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

CA4988Stepper::~CA4988Stepper()
{
    stepper_dev_t *pstepper = (stepper_dev_t*) m_stepper;
    pcnt_intr_disable(pstepper->pcnt_unit);
    ledc_stop(pstepper->ledc_mode, pstepper->ledc_channel, 0);
    vSemaphoreDelete(pstepper->mux);
    vSemaphoreDelete(pstepper->sem);
    if (pstepper->tmr) {
        xTimerStop(pstepper->tmr, portMAX_DELAY);
        xTimerDelete(pstepper->tmr, portMAX_DELAY);
        pstepper->tmr = NULL;
    }
    free(pstepper);
    m_stepper = NULL;
}

#ifdef __cplusplus
}
#endif

