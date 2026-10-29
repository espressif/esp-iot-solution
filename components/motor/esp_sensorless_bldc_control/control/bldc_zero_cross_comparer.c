/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "bldc_zero_cross_comparer.h"
#include "bldc_common.h"
#include "bldc_snls_lib.h"
#include "hal/gpio_ll.h"

static const char *TAG = "bldc_zero_cross";

typedef struct {
    control_param_t *control_param;
    int alignment_pin[PHASE_MAX];
    int zero_stable_flag;                        /*!< Over-zero stabilization flag bit */
    uint64_t alignment_queue_value[PHASE_MAX];
    uint16_t queue_filter_state[PHASE_MAX];      /*!< State after three-phase filtering */
} bldc_zero_cross_comparer_t;

static void IRAM_ATTR alignment_comparer_get_value(bldc_zero_cross_comparer_t *zero_cross)
{
    for (int i = 0; i < PHASE_MAX; i++) {
        zero_cross->alignment_queue_value[i] = zero_cross->alignment_queue_value[i] << 1;
        zero_cross->alignment_queue_value[i] |= gpio_ll_get_level(&GPIO, zero_cross->alignment_pin[i]);
    }
}

/**
 * @brief emf edge monitoring
 * @param {uint8_t} val
 * @return {*}
 */
static uint8_t bldc_umef_edge(uint8_t val)
{
    static uint8_t oldval = 0;
    if (oldval != val) {
        oldval = val;

        if (val == 0) {
            return 0;
        } else {
            return 1;
        }
    }

    return 2;
}

bool IRAM_ATTR read_comparer_on_full(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_data)
{
    bldc_zero_cross_comparer_t *zero_cross = (bldc_zero_cross_comparer_t *)user_data;
    alignment_comparer_get_value(zero_cross);
    return false;
}

esp_err_t bldc_zero_cross_comparer_init(bldc_zero_cross_comparer_handle_t *handle, bldc_zero_cross_comparer_config_t *config, control_param_t *control_param)
{
    BLDC_CHECK(config != NULL, "bldc zero cross config is NULL", ESP_ERR_INVALID_ARG);
    bldc_zero_cross_comparer_t *zero_cross = (bldc_zero_cross_comparer_t *)calloc(1, sizeof(bldc_zero_cross_comparer_t));
    BLDC_CHECK(zero_cross != NULL, "calloc error", ESP_ERR_NO_MEM);

    zero_cross->control_param = control_param;
    esp_err_t err = ESP_OK;

    for (int i = 0; i < PHASE_MAX; i++) {
        err = bldc_gpio_init(&config->comparer_gpio[i]);
        BLDC_CHECK_GOTO(err == ESP_OK, "gpio init error", deinit);
        zero_cross->alignment_pin[i] = config->comparer_gpio[i].gpio_num;
    }

    *handle = (bldc_zero_cross_comparer_handle_t)zero_cross;
    return ESP_OK;

deinit:
    if (zero_cross != NULL) {
        free(zero_cross);
    }
    return ESP_FAIL;
}

esp_err_t bldc_zero_cross_comparer_deinit(bldc_zero_cross_comparer_handle_t handle)
{
    bldc_zero_cross_comparer_t *zero_cross = (bldc_zero_cross_comparer_t *)handle;
    BLDC_CHECK(zero_cross != NULL, "bldc zero cross handle is NULL", ESP_ERR_INVALID_ARG);

    for (int i = 0; i < PHASE_MAX; i++) {
        gpio_reset_pin(zero_cross->alignment_pin[i]);
    }

    free(zero_cross);
    return ESP_OK;
}

uint8_t bldc_zero_cross_comparer_operation(void *handle)
{
    BLDC_CHECK(handle != NULL, "bldc zero cross handle comparer is NULL", 0);

    bldc_zero_cross_comparer_t *zero_cross = (bldc_zero_cross_comparer_t *)handle;
    uint16_t filterEdge;  /*!< Edge detection after filtering */
    zero_cross->control_param->speed_count++;

    for (int i = 0; i < PHASE_MAX; i++) {
        if ((zero_cross->alignment_queue_value[i] & ZERO_CROSS_DETECTION_ACCURACY) == ZERO_CROSS_DETECTION_ACCURACY) {
            zero_cross->queue_filter_state[i] = 1;
        } else if ((zero_cross->alignment_queue_value[i] & ZERO_CROSS_DETECTION_ACCURACY) == 0) {
            zero_cross->queue_filter_state[i] = 0;
        } else {
            zero_cross->control_param->filter_failed_count++;
            /*!< Direct return */
            return 0;
        }
    }

    /*!< Speed measurement */
    filterEdge = bldc_umef_edge(zero_cross->queue_filter_state[0]);
    if (filterEdge == 0) {
        /*!< Starts counting the time past zero from 1->0. */
        if (zero_cross->zero_stable_flag >= ZERO_STABLE_FLAG_CNT) {
            uint32_t speed_rpm = 0;
            if (zero_cross->control_param->speed_count < SKIP_INVALID_SPEED_CALCULATION) {
                /*!< A speed count of less than SKIP_INVALID_SPEED_CALCULATION means that the speed is too fast or unstable and is not counted */
            } else {
                if (zero_cross->control_param->dir == CCW) {
                    /*!< ft/(2*n*c) * 60; where ft is the counting frequency, that is, the frequency of the cycle interrupt, n is the polar logarithm, and c is the number of times it is memorized. */
                    speed_rpm = (uint32_t)(COMPARER_RPM_CALCULATION_COEFFICIENT / zero_cross->control_param->speed_count);
                } else {
                    speed_rpm = (uint32_t)(COMPARER_RPM_CALCULATION_COEFFICIENT / zero_cross->control_param->speed_count);
                }
                ESP_LOGD(TAG, "speed_rpm: %"PRIu32", speed_count: %"PRIu32"\n", speed_rpm, zero_cross->control_param->speed_count);

                BLDC_LPF(zero_cross->control_param->speed_rpm, speed_rpm, 0.2);
            }
        }
        /*!< The high time is noted as 180 degrees, with 30 degrees of hysteresis, divided by 6 to make the delay time shorter and avoid hardware problems. */
        zero_cross->control_param->filter_delay = zero_cross->control_param->speed_count / ZERO_CROSS_ADVANCE;
        zero_cross->control_param->speed_count = 0;
        zero_cross->control_param->filter_failed_count = 0;
        zero_cross->zero_stable_flag++;
    }

    if (filterEdge == 1) {
        zero_cross->control_param->speed_count = 0;
        zero_cross->control_param->filter_failed_count = 0;
    }

    if (filterEdge == 2) {
        zero_cross->control_param->filter_failed_count++;
    }

    /*!< return CLOSED LOOP after two stabilized revolutions. */
    if (zero_cross->zero_stable_flag >= ZERO_STABLE_FLAG_CNT + 2) {
        zero_cross->zero_stable_flag = ZERO_STABLE_FLAG_CNT + 2;
        uint8_t phase = zero_cross_comparer_get_phase(zero_cross->queue_filter_state, zero_cross->control_param->dir);
        if (phase <= 0 || phase > 6) {
            zero_cross->control_param->error_hall_count++;
            return 0;
        } else {
            zero_cross->control_param->phase_cnt = phase;
            if (zero_cross->control_param->phase_cnt != zero_cross->control_param->phase_cnt_prev && zero_cross->control_param->phase_change_done == true) {
                zero_cross->control_param->phase_change_done = false;
                return 1;
            }
        }
    }

    return 0;
}

esp_err_t bldc_zero_cross_comparer_start(bldc_zero_cross_comparer_handle_t handle)
{
    BLDC_CHECK(handle != NULL, "bldc zero cross handle is NULL", ESP_ERR_INVALID_ARG);
    bldc_zero_cross_comparer_t *zero_cross = (bldc_zero_cross_comparer_t *)handle;
    zero_cross->zero_stable_flag = 0;
    zero_cross->alignment_queue_value[0] = 0;
    zero_cross->alignment_queue_value[1] = 0;
    zero_cross->alignment_queue_value[2] = 0;
    return ESP_OK;
}
