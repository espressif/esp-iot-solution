/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bldc_helper.h"
#include "bldc_six_step.h"
#include "bldc_driver.h"
#include "bldc_snls_lib.h"
#include "bldc_config.h"

static const char *TAG = "bldc_six_step";

typedef struct bldc_six_step {
    bool _can_be_enable;
    bool _if_enable;
    bool enable_level;
    bool lower_active_level;
    control_param_t *control_param;
    control_type_t upper_switch_type;
    control_type_t lower_switch_type;
    void *phase_upper_pin[PHASE_MAX];
    void *phase_lower_pin[PHASE_MAX];
    void *phase_enable_pin[PHASE_MAX];
    esp_err_t (*set_upper_pwm)(void *phase, uint32_t duty);
    esp_err_t (*set_lower_pwm)(void *phase, uint32_t duty);
    esp_err_t (*set_gpio)(void *phase, uint32_t duty);
    void (*set_phase_duty)(struct bldc_six_step *six_step, phase_enum_t phase, uint32_t H_duty, uint32_t L_duty);
    uint8_t pos_check_stage;
    void (**six_step_cw_array)(struct bldc_six_step *six_step, uint32_t Hduty, uint32_t Lduty);
    void (**six_step_ccw_array)(struct bldc_six_step *six_step, uint32_t Hduty, uint32_t Lduty);
} bldc_six_step_t;

static void bldc_six_step_set_Hpwm_Lgpio(bldc_six_step_t *six_step, phase_enum_t phase, uint32_t H_duty, uint32_t L_duty)
{
    six_step->set_upper_pwm(six_step->phase_upper_pin[phase], H_duty);
    six_step->set_gpio(six_step->phase_lower_pin[phase], L_duty);
}

static void bldc_six_step_set_Hpwm_Lpwm(bldc_six_step_t *six_step, phase_enum_t phase, uint32_t H_duty, uint32_t L_duty)
{
    six_step->set_upper_pwm(six_step->phase_upper_pin[phase], H_duty);
    six_step->set_lower_pwm(six_step->phase_lower_pin[phase], L_duty);
}

static void bldc_six_step_UVW_stop(bldc_six_step_t *six_step, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, Lduty);
}

static void bldc_six_step_UphaseH_VphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, !Lduty);
    six_step->control_param->adc_bemf_phase = PHASE_W;
}

static void bldc_six_step_VphaseH_UphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, !Lduty);
    six_step->control_param->adc_bemf_phase = PHASE_W;
}

static void bldc_six_step_WphaseH_UphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, Hduty, !Lduty);
    six_step->control_param->adc_bemf_phase = PHASE_V;
}

static void bldc_six_step_WphaseH_VphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, Hduty, !Lduty);
    six_step->control_param->adc_bemf_phase = PHASE_U;
}

static void bldc_six_step_UphaseH_WphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, Lduty);
    six_step->control_param->adc_bemf_phase = PHASE_V;
}

static void bldc_six_step_VphaseH_WphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, Lduty);
    six_step->control_param->adc_bemf_phase = PHASE_U;
}

static void bldc_six_step_UVphaseH_WphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, Lduty);
}

static void bldc_six_step_WphaseH_UVphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, Hduty, !Lduty);
}

static void bldc_six_step_UWphaseH_VphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, Hduty, !Lduty);
}

static void bldc_six_step_VphaseH_UWphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, Lduty);
}

static void bldc_six_step_VWphaseH_UphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, Hduty, !Lduty);
}

static void bldc_six_step_UphaseH_VWphaseL(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty)
{
    six_step->set_phase_duty(six_step, PHASE_U, Hduty, !Lduty);
    six_step->set_phase_duty(six_step, PHASE_V, 0, Lduty);
    six_step->set_phase_duty(six_step, PHASE_W, 0, Lduty);
}

/**
 * @brief Pulse injection sequence
 *
 */
static void (*injectArray[6])(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty) = {
    &bldc_six_step_UVphaseH_WphaseL,
    &bldc_six_step_WphaseH_UVphaseL,
    &bldc_six_step_UWphaseH_VphaseL,
    &bldc_six_step_VphaseH_UWphaseL,
    &bldc_six_step_VWphaseH_UphaseL,
    &bldc_six_step_UphaseH_VWphaseL,
};

uint8_t bldc_six_step_inject(bldc_six_step_handle_t *handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;

    six_step->control_param->charge_time = 0;
    /*!< The value of adc is being read. Skip processing. */
    if (six_step->control_param->inject_adc_read) {
        six_step->control_param->inject_adc_read = false;
        return 0;
    }

    switch (six_step->pos_check_stage) {
    case 0:
        six_step->pos_check_stage = 10;
        goto charge;
        break;
    case 10:
        six_step->pos_check_stage = 1;
        goto inject;
        break;
    case 1:
        six_step->pos_check_stage = 20;
        goto charge;
        break;
    case 20:
        six_step->pos_check_stage = 2;
        goto inject;
        break;
    case 2:
        six_step->pos_check_stage = 30;
        goto charge;
        break;
    case 30:
        six_step->pos_check_stage = 3;
        goto inject;
        break;
    case 3:
        six_step->pos_check_stage = 40;
        goto charge;
        break;
    case 40:
        six_step->pos_check_stage = 4;
        goto inject;
        break;
    case 4:
        six_step->pos_check_stage = 50;
        goto charge;
        break;
    case 50:
        six_step->pos_check_stage = 5;
        goto inject;
        break;
    case 5:
        six_step->pos_check_stage = 60;
        goto charge;
        break;
    case 60:
        six_step->pos_check_stage = 6;
        goto inject;
        break;
    case 6:
        bldc_six_step_UVW_stop(six_step, !six_step->lower_active_level);
        six_step->control_param->phase_cnt = inject_get_phase(six_step->control_param->inject_adc_value);
        ESP_LOGD(TAG, "inject detect phase: %d", six_step->control_param->phase_cnt);
        ESP_LOGD(TAG, "inject_adc_value: %"PRId32" %"PRId32" %"PRId32" %"PRId32" %"PRId32" %"PRId32"",
                 six_step->control_param->inject_adc_value[0], six_step->control_param->inject_adc_value[1],
                 six_step->control_param->inject_adc_value[2], six_step->control_param->inject_adc_value[3],
                 six_step->control_param->inject_adc_value[4], six_step->control_param->inject_adc_value[5]);
        if (six_step->control_param->phase_cnt < 1 || six_step->control_param->phase_cnt > 6) {
            //TODO: error handle make again
            six_step->control_param->phase_cnt = 1;
        }
        six_step->pos_check_stage = 0;
        six_step->control_param->inject_count = 0;
        return 1;
        break;
    }
    return 0;

charge:
    bldc_six_step_UVW_stop(six_step, !six_step->lower_active_level);
    six_step->control_param->charge_time = CHARGE_TIME;
    return 0;
inject:
    six_step->control_param->inject_count++;
    injectArray[six_step->control_param->inject_count - 1](six_step, DUTY_MAX * 0.95, six_step->lower_active_level);
    six_step->control_param->inject_adc_read = true;
    six_step->control_param->charge_time = CHARGE_TIME;
    return 0;
}

/**
 * @brief six step comparer cw sequence
 *
 */
static void (*six_step_comparer_cw_array[6])(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty) = {
    &bldc_six_step_UphaseH_VphaseL,
    &bldc_six_step_WphaseH_VphaseL,
    &bldc_six_step_WphaseH_UphaseL,
    &bldc_six_step_VphaseH_UphaseL,
    &bldc_six_step_VphaseH_WphaseL,
    &bldc_six_step_UphaseH_WphaseL,
};

/**
 * @brief six step comparer ccw sequence
 *
 */
static void (*six_step_comparer_ccw_array[6])(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty) = {
    &bldc_six_step_UphaseH_VphaseL,
    &bldc_six_step_UphaseH_WphaseL,
    &bldc_six_step_VphaseH_WphaseL,
    &bldc_six_step_VphaseH_UphaseL,
    &bldc_six_step_WphaseH_UphaseL,
    &bldc_six_step_WphaseH_VphaseL,
};

/**
 * @brief six step cw sequence
 *
 */
static void (*six_step_adc_cw_array[6])(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty) = {
    &bldc_six_step_WphaseH_VphaseL,
    &bldc_six_step_WphaseH_UphaseL,
    &bldc_six_step_VphaseH_UphaseL,
    &bldc_six_step_VphaseH_WphaseL,
    &bldc_six_step_UphaseH_WphaseL,
    &bldc_six_step_UphaseH_VphaseL,
};

/**
 * @brief six step ccw sequence
 *
 */
static void (*six_step_adc_ccw_array[6])(bldc_six_step_t *six_step, uint32_t Hduty, uint32_t Lduty) = {
    &bldc_six_step_UphaseH_VphaseL,
    &bldc_six_step_UphaseH_WphaseL,
    &bldc_six_step_VphaseH_WphaseL,
    &bldc_six_step_VphaseH_UphaseL,
    &bldc_six_step_WphaseH_UphaseL,
    &bldc_six_step_WphaseH_VphaseL,
};

void bldc_six_step_turn(bldc_six_step_handle_t *handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;
    if (six_step->control_param->phase_cnt > 6) {
        six_step->control_param->phase_cnt = 1;
    } else if (six_step->control_param->phase_cnt < 1) {
        six_step->control_param->phase_cnt = 6;
    }

    if (six_step->control_param->dir == CW) {
        six_step->six_step_cw_array[six_step->control_param->phase_cnt - 1](six_step, six_step->control_param->duty, six_step->lower_active_level);
    } else if (six_step->control_param->dir == CCW) {
        six_step->six_step_ccw_array[six_step->control_param->phase_cnt - 1](six_step, six_step->control_param->duty, six_step->lower_active_level);
    }
    six_step->control_param->phase_cnt_prev = six_step->control_param->phase_cnt;
    six_step->control_param->phase_change_done = true;
}

esp_err_t bldc_six_step_init(bldc_six_step_handle_t *handle, bldc_six_step_config_t *config, control_param_t *control_param)
{
    BLDC_CHECK(config != NULL, "six_step_config can not be NULL", ESP_ERR_INVALID_ARG);
    BLDC_CHECK(config->upper_switch_config.control_type != CONTROL_TYPE_GPIO, "upper_switch_config can not be GPIO", ESP_ERR_INVALID_ARG);
    BLDC_CHECK(control_param != NULL, "control_param can not be NULL", ESP_ERR_INVALID_ARG);
    bldc_six_step_t *six_step = (bldc_six_step_t *)calloc(1, sizeof(bldc_six_step_t));
    BLDC_CHECK(six_step != NULL, "calloc error", ESP_ERR_NO_MEM);

    esp_err_t err = ESP_OK;
    six_step->control_param = control_param;
    /*<! init upper switch */
    switch (config->upper_switch_config.control_type) {
    case CONTROL_TYPE_LEDC: {
        err = bldc_ledc_init(&config->upper_switch_config.bldc_ledc);
        BLDC_CHECK_GOTO(err == ESP_OK, "ledc init error", deinit);
        for (int i = 0; i < PHASE_MAX; i++) {
            six_step->phase_upper_pin[i] = (void *)config->upper_switch_config.bldc_ledc.ledc_channel[i];
        }
        six_step->set_upper_pwm = &bldc_ledc_set_duty;
        six_step->control_param->duty_max = DUTY_MAX;
    }
    break;
#if CONFIG_SOC_MCPWM_SUPPORTED
    case CONTROL_TYPE_MCPWM:
        err = bldc_mcpwm_init(&config->upper_switch_config.bldc_mcpwm, six_step->phase_upper_pin);
        BLDC_CHECK_GOTO(err == ESP_OK, "mcpwm init error", deinit);
        six_step->set_upper_pwm = &bldc_mcpwm_set_duty;
        six_step->control_param->duty_max = DUTY_MAX;
        break;
#endif
    default:
        return ESP_ERR_INVALID_STATE;
    }
    six_step->upper_switch_type = config->upper_switch_config.control_type;

    /*<! init lower switch */
    switch (config->lower_switch_config.control_type) {
    case CONTROL_TYPE_LEDC: {
        err = bldc_ledc_init(&config->lower_switch_config.bldc_ledc);
        BLDC_CHECK_GOTO(err == ESP_OK, "gpio init error", deinit);
        for (int i = 0; i < PHASE_MAX; i++) {
            six_step->phase_lower_pin[i] = (void *)config->lower_switch_config.bldc_ledc.ledc_channel[i];
        }
        six_step->set_lower_pwm = &bldc_ledc_set_duty;
        six_step->set_phase_duty = &bldc_six_step_set_Hpwm_Lpwm;
    }
    break;
#if CONFIG_SOC_MCPWM_SUPPORTED
    case CONTROL_TYPE_MCPWM:
        err = bldc_mcpwm_init(&config->lower_switch_config.bldc_mcpwm, six_step->phase_lower_pin);
        BLDC_CHECK_GOTO(err == ESP_OK, "mcpwm init error", deinit);
        six_step->set_lower_pwm = &bldc_mcpwm_set_duty;
        six_step->set_phase_duty = &bldc_six_step_set_Hpwm_Lpwm;
        break;
#endif
    case CONTROL_TYPE_GPIO: {
        for (int i = 0; i < PHASE_MAX; i++) {
            err = bldc_gpio_init(&config->lower_switch_config.bldc_gpio[i]);
            BLDC_CHECK_GOTO(err == ESP_OK, "gpio init error", deinit);
            six_step->phase_lower_pin[i] = (void *)config->lower_switch_config.bldc_gpio[i].gpio_num;
        }
        six_step->set_gpio = &bldc_gpio_set_level;
        six_step->set_phase_duty = &bldc_six_step_set_Hpwm_Lgpio;
    }
    break;
    default:
        return ESP_ERR_INVALID_STATE;
    }

    six_step->lower_switch_type = config->lower_switch_config.control_type;
    six_step->lower_active_level = config->lower_switch_active_level;

    if (config->mos_en_config.has_enable) {
        for (int i = 0; i < PHASE_MAX; i++) {
            err = bldc_gpio_init(&config->mos_en_config.en_gpio[i]);
            BLDC_CHECK_GOTO(err == ESP_OK, "gpio init error", deinit);
            six_step->phase_enable_pin[i] = (void *)config->mos_en_config.en_gpio[i].gpio_num;
        }
        six_step->enable_level = config->mos_en_config.en_level;
        six_step->_can_be_enable = true;
    } else {
        six_step->_can_be_enable = false;
    }

    if (six_step->control_param->alignment_mode == ALIGNMENT_COMPARER) {
        six_step->six_step_cw_array = six_step_comparer_cw_array;
        six_step->six_step_ccw_array = six_step_comparer_ccw_array;
    } else {
        six_step->six_step_cw_array = six_step_adc_cw_array;
        six_step->six_step_ccw_array = six_step_adc_ccw_array;
    }

    *handle = (bldc_six_step_handle_t)six_step;

    return ESP_OK;
deinit:
    if (six_step) {
        free(six_step);
    }
    return ESP_FAIL;
}

esp_err_t bldc_six_step_deinit(bldc_six_step_handle_t handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;
    BLDC_CHECK(six_step != NULL, "six_step has not been init", ESP_ERR_INVALID_ARG);
    esp_err_t err = ESP_OK;
    switch (six_step->upper_switch_type) {
#if CONFIG_SOC_MCPWM_SUPPORTED
    case CONTROL_TYPE_MCPWM:
        err = bldc_mcpwm_deinit(six_step->phase_upper_pin);
        BLDC_CHECK(err == ESP_OK, "mcpwm deinit error", ESP_FAIL);
        break;
#endif
    default:
        break;
    }

    switch (six_step->lower_switch_type) {
#if CONFIG_SOC_MCPWM_SUPPORTED
    case CONTROL_TYPE_MCPWM:
        err = bldc_mcpwm_deinit(six_step->phase_lower_pin);
        BLDC_CHECK(err == ESP_OK, "mcpwm deinit error", ESP_FAIL);
        break;
#endif
    case CONTROL_TYPE_GPIO:
        if (six_step->_can_be_enable) {
            for (int i = 0; i < PHASE_MAX; i++) {
                err = bldc_gpio_deinit((uint32_t)six_step->phase_enable_pin[i]);
                BLDC_CHECK(err == ESP_OK, "gpio deinit error", ESP_FAIL);
            }
        }
    default:
        break;
    }

    free(six_step);
    return ESP_OK;
}

static esp_err_t bldc_six_step_enable(bldc_six_step_handle_t handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;
    BLDC_CHECK(six_step != NULL, "six_step has not been init", ESP_ERR_INVALID_STATE);
    if (six_step->_can_be_enable) {
        six_step->set_gpio(six_step->phase_enable_pin[PHASE_U], six_step->enable_level);
        six_step->set_gpio(six_step->phase_enable_pin[PHASE_V], six_step->enable_level);
        six_step->set_gpio(six_step->phase_enable_pin[PHASE_W], six_step->enable_level);
    }
    six_step->_if_enable = true;
    return ESP_OK;
}

static esp_err_t bldc_six_step_disable(bldc_six_step_handle_t handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;
    BLDC_CHECK(six_step != NULL, "six_step has not been init", ESP_ERR_INVALID_STATE);
    if (six_step->_can_be_enable) {
        six_step->set_gpio(six_step->phase_enable_pin[PHASE_U], six_step->enable_level);
        six_step->set_gpio(six_step->phase_enable_pin[PHASE_V], six_step->enable_level);
        six_step->set_gpio(six_step->phase_enable_pin[PHASE_W], six_step->enable_level);
    }
    six_step->_if_enable = false;
    return ESP_OK;
}

esp_err_t bldc_six_step_start(bldc_six_step_handle_t handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;
    BLDC_CHECK(six_step != NULL, "six_step has not been init", ESP_ERR_INVALID_ARG);
    BLDC_CHECK(six_step->_if_enable == false, "six_step has been enable", ESP_ERR_INVALID_ARG);

    bldc_six_step_enable(handle);
    bldc_six_step_UVW_stop(six_step, !six_step->lower_active_level);
    six_step->control_param->phase_cnt = 0;
    six_step->control_param->phase_cnt_prev = 0;
    six_step->control_param->duty = RAMP_DUTY_STA;
    six_step->control_param->filter_delay = RAMP_TIM_STA;
    six_step->pos_check_stage = 0;
    return ESP_OK;
}

esp_err_t bldc_six_step_stop(bldc_six_step_handle_t handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;
    BLDC_CHECK(six_step != NULL, "six_step has not been init", ESP_ERR_INVALID_ARG);

    bldc_six_step_UVW_stop(six_step, !six_step->lower_active_level);
    bldc_six_step_disable(handle);
    return ESP_OK;
}

uint8_t bldc_six_step_operation(void *handle)
{
    bldc_six_step_t *six_step = (bldc_six_step_t *)handle;
    if (six_step->control_param->status == CLOSED_LOOP) {
        if (six_step->control_param->phase_cnt <= 0 || six_step->control_param->phase_cnt > 6) {
            return 0;
        }

        if (six_step->control_param->phase_cnt_prev != six_step->control_param->phase_cnt) {
            bldc_six_step_turn(handle);
        }
    } else if (six_step->control_param->status == DRAG) {
        six_step->control_param->duty += RAMP_DUTY_INC;
        six_step->control_param->drag_time -= (six_step->control_param->drag_time / RAMP_TIM_STEP) + 1;
        if (six_step->control_param->drag_time < RAMP_TIM_END) {
            /*!< Zero crossing time interval */
            six_step->control_param->drag_time = RAMP_TIM_END;
        }
        if (six_step->control_param->duty < RAMP_DUTY_STA) {
            six_step->control_param->duty = RAMP_DUTY_STA;
        } else if (six_step->control_param->duty > RAMP_DUTY_END) {
            six_step->control_param->duty = RAMP_DUTY_END;
        }

        /*!< Starting strong drag phase change */
        if (++six_step->control_param->phase_cnt >= 7) {
            six_step->control_param->phase_cnt = 1;
        }
        bldc_six_step_turn(handle);
    } else if (six_step->control_param->status == ALIGNMENT) {
        six_step->control_param->duty = ALIGNMENTDUTY;
        /*!< Aligning the initial phase */
        bldc_six_step_turn(handle);
        six_step->control_param->duty = RAMP_DUTY_STA;
        six_step->control_param->drag_time = RAMP_TIM_STA;
    } else if (six_step->control_param->status == INJECT) {
        /*!< Returns 1 if the initial phase is obtained after six injections, 0 otherwise. */
        return bldc_six_step_inject(handle);
    } else if (six_step->control_param->status == BLOCKED || six_step->control_param->status == STOP) {
        bldc_six_step_UVW_stop(six_step, !six_step->lower_active_level);
    }
    return 0;
}
