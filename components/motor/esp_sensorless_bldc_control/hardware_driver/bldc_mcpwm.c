/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bldc_common.h"
#include "bldc_mcpwm.h"

static const char *TAG = "bldc_mcpwm";

typedef struct {
    mcpwm_timer_handle_t timer;
    mcpwm_oper_handle_t operators[MCPWM_MAX_COMPARATOR];
    mcpwm_cmpr_handle_t comparators[MCPWM_MAX_COMPARATOR];
    mcpwm_gen_handle_t  generators[MCPWM_MAX_COMPARATOR];
} bldc_mcpwm_ctx_t;

esp_err_t bldc_mcpwm_init(bldc_mcpwm_config_t *mcpwm_config, void **cmprs)
{
    BLDC_CHECK(mcpwm_config, "mcpwm_config is NULL", ESP_ERR_INVALID_ARG);
    esp_err_t err = ESP_OK;
    bldc_mcpwm_ctx_t *bldc_mcpwm_ctx = calloc(1, sizeof(bldc_mcpwm_ctx_t));
    BLDC_CHECK(bldc_mcpwm_ctx, "calloc failed", ESP_ERR_NO_MEM);

    mcpwm_timer_config_t timer_config = MCPWM_TIMER_CONFIG_DEFAULT(mcpwm_config->group_id);
    err = mcpwm_new_timer(&timer_config, &bldc_mcpwm_ctx->timer);
    BLDC_CHECK(err == ESP_OK, "mcpwm_new_timer failed: %s", err, esp_err_to_name(err));

    mcpwm_operator_config_t operator_config = {
        .group_id = mcpwm_config->group_id,
    };

    for (int i = 0; i < 3; i++) {
        err = mcpwm_new_operator(&operator_config, &bldc_mcpwm_ctx->operators[i]);
        BLDC_CHECK(err == ESP_OK, "mcpwm_new_operator failed: %s", err, esp_err_to_name(err));
        err = mcpwm_operator_connect_timer(bldc_mcpwm_ctx->operators[i], bldc_mcpwm_ctx->timer);
        BLDC_CHECK(err == ESP_OK, "mcpwm_operator_connect_timer failed: %s", err, esp_err_to_name(err));
    }

    mcpwm_comparator_config_t compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    for (int i = 0; i < 3; i++) {
        err = mcpwm_new_comparator(bldc_mcpwm_ctx->operators[i], &compare_config, &bldc_mcpwm_ctx->comparators[i]);
        BLDC_CHECK(err == ESP_OK, "mcpwm_new_comparator failed: %s", err, esp_err_to_name(err));
        // set compare value to 0, we will adjust the speed in a period timer callback
        err = mcpwm_comparator_set_compare_value(bldc_mcpwm_ctx->comparators[i], 0);
        BLDC_CHECK(err == ESP_OK, "mcpwm_comparator_set_compare_value failed: %s", err, esp_err_to_name(err));
    }

    mcpwm_generator_config_t gen_config = {};
    for (int i = 0; i < 3; i++) {
        gen_config.gen_gpio_num = mcpwm_config->gpio_num[i];
        err = mcpwm_new_generator(bldc_mcpwm_ctx->operators[i], &gen_config, &bldc_mcpwm_ctx->generators[i]);
        BLDC_CHECK(err == ESP_OK, "mcpwm_new_generator failed: %s", err, esp_err_to_name(err));
    }

    for (int i = 0; i < 3; i++) {
        err = mcpwm_generator_set_actions_on_compare_event(bldc_mcpwm_ctx->generators[i],
                                                           MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, bldc_mcpwm_ctx->comparators[i], MCPWM_GEN_ACTION_LOW),
                                                           MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, bldc_mcpwm_ctx->comparators[i], MCPWM_GEN_ACTION_HIGH),
                                                           MCPWM_GEN_COMPARE_EVENT_ACTION_END());
        BLDC_CHECK(err == ESP_OK, "mcpwm_generator_set_action_on_compare_event failed: %s", err, esp_err_to_name(err));
    }

    if (mcpwm_config->cbs) {
        err = mcpwm_timer_register_event_callbacks(bldc_mcpwm_ctx->timer, mcpwm_config->cbs, mcpwm_config->timer_cb_user_data);
        BLDC_CHECK(err == ESP_OK, "mcpwm_timer_register_event_callbacks failed: %s", err, esp_err_to_name(err));
    }

    err = mcpwm_timer_enable(bldc_mcpwm_ctx->timer);
    BLDC_CHECK(err == ESP_OK, "mcpwm_timer_enable failed: %s", err, esp_err_to_name(err));

    err = mcpwm_timer_start_stop(bldc_mcpwm_ctx->timer, MCPWM_TIMER_START_NO_STOP);
    BLDC_CHECK(err == ESP_OK, "mcpwm_timer_start_stop failed: %s", err, esp_err_to_name(err));

    cmprs[0] = bldc_mcpwm_ctx->comparators[0];
    cmprs[1] = bldc_mcpwm_ctx->comparators[1];
    cmprs[2] = bldc_mcpwm_ctx->comparators[2];

    return ESP_OK;
}

esp_err_t bldc_mcpwm_deinit(void *cmprs)
{
    BLDC_CHECK(cmprs, "cmprs is NULL", ESP_ERR_INVALID_ARG);
    bldc_mcpwm_ctx_t *mcpwm_ctx = __containerof(cmprs, bldc_mcpwm_ctx_t, comparators);
    esp_err_t err = ESP_OK;
    err = mcpwm_timer_disable(mcpwm_ctx->timer);
    BLDC_CHECK(err == ESP_OK, "mcpwm_timer_disable failed: %s", err, esp_err_to_name(err));
    for (int i = 0; i < 3; i++) {
        err = mcpwm_del_generator(mcpwm_ctx->generators[i]);
        BLDC_CHECK(err == ESP_OK, "mcpwm_del_generator failed: %s", err, esp_err_to_name(err));
        err = mcpwm_del_comparator(mcpwm_ctx->comparators[i]);
        BLDC_CHECK(err == ESP_OK, "mcpwm_del_comparator failed: %s", err, esp_err_to_name(err));
        err = mcpwm_del_operator(mcpwm_ctx->operators[i]);
        BLDC_CHECK(err == ESP_OK, "mcpwm_del_operator failed: %s", err, esp_err_to_name(err));
    }
    err = mcpwm_del_timer(mcpwm_ctx->timer);
    BLDC_CHECK(err == ESP_OK, "mcpwm_del_timer failed: %s", err, esp_err_to_name(err));
    return ESP_OK;
}

esp_err_t bldc_mcpwm_set_duty(void *cmprs, uint32_t duty)
{
    esp_err_t err = ESP_OK;
    err = mcpwm_comparator_set_compare_value(cmprs, duty);
    return err;
}
