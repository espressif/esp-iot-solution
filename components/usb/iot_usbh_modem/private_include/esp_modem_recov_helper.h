/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_modem_dce.h"

/**
* @brief Utility macro to define a retry method
*
*/
#define DEFINE_RETRY_CMD(name, retry, super_type) \
        esp_err_t name(esp_modem_dce_t *dce, void *param, void *result) \
        { \
            super_type *super = __containerof(dce, super_type, parent); \
            return super->retry->run(super->retry, param, result);      \
        }

/**
 * @brief GPIO helper object used to pull modem IOs
 *
 */
typedef struct esp_modem_gpio_s {
    int gpio_num;
    int inactive_level;
    int active_width_ms;
    int inactive_width_ms;
    void (*pulse)(struct esp_modem_gpio_s *pin);
    void (*pulse_special)(struct esp_modem_gpio_s *pin, int active_width_ms, int inactive_width_ms);
    void (*destroy)(struct esp_modem_gpio_s *pin);
} esp_modem_recov_gpio_t;

/**
 * @brief Recovery helper object used to resend commands if failed or timeouted
 *
 */
typedef struct esp_modem_retry_s esp_modem_recov_resend_t;

/**
 * @brief User recovery function to be called upon modem command failed
 *
 */
typedef esp_err_t (*esp_modem_retry_fn_t)(esp_modem_recov_resend_t *retry_cmd, esp_err_t current_err, int timeouts, int errors);

/**
 * @brief Recovery helper object
 *
 */
struct esp_modem_retry_s {
    const char *command;
    esp_err_t (*run)(struct esp_modem_retry_s *retry, void *param, void *result);
    dce_command_t orig_cmd;
    esp_modem_retry_fn_t recover;
    esp_modem_dce_t *dce;
    int retries_after_timeout;                      //!< Retry strategy: numbers of resending the same command on timeout
    int retries_after_error;                        //!< Retry strategy: numbers of resending the same command on error
    void (*destroy)(struct esp_modem_retry_s *this_recov);
};

/**
 * @brief Create new resend object
 *
 */
esp_modem_recov_resend_t *esp_modem_recov_resend_new(esp_modem_dce_t *dce, dce_command_t orig_cmd, esp_modem_retry_fn_t recover, int max_timeouts, int max_errors);

/**
 * @brief Create new gpio object
 *
 */
esp_modem_recov_gpio_t *esp_modem_recov_gpio_new(int gpio_num, int inactive_level, int active_width_ms, int inactive_width_ms);
