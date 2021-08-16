// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp_modem_recov_helper.h"
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_modem_internal.h"
#include "esp_log.h"

static const char *TAG = "esp_modem_recov_helper";

static void pulse_destroy(esp_modem_recov_gpio_t *pin)
{
    free(pin);
}

static void retry_destroy(esp_modem_recov_resend_t *retry)
{
    free(retry);
}

static void pulse_special(esp_modem_recov_gpio_t * pin, int active_width_ms, int inactive_width_ms)
{
    gpio_set_level(pin->gpio_num, !pin->inactive_level);
    esp_modem_wait_ms(active_width_ms);
    gpio_set_level(pin->gpio_num, pin->inactive_level);
    esp_modem_wait_ms(inactive_width_ms);
}

static void pulse(esp_modem_recov_gpio_t * pin)
{
    gpio_set_level(pin->gpio_num, !pin->inactive_level);
    esp_modem_wait_ms(pin->active_width_ms);
    gpio_set_level(pin->gpio_num, pin->inactive_level);
    esp_modem_wait_ms(pin->inactive_width_ms);
}

static esp_err_t esp_modem_retry_run(esp_modem_recov_resend_t * retry, void *param, void *result)
{
    esp_modem_dce_t *dce = retry->dce;
    int errors = 0;
    int timeouts = 0;
    esp_err_t err = ESP_FAIL;
    while (timeouts <= retry->retries_after_timeout &&
           errors <= retry->retries_after_error) {
        if (timeouts || errors) {
            // provide recovery action based on the defined strategy
            if (retry->recover(retry, err, timeouts, errors) != ESP_OK) {
                // fail the retry mechanism once the recovery fails
                return ESP_FAIL;
            }
        }
        if (retry->command) {
            ESP_LOGD(TAG, "%s(%d): executing:%s...", __func__, __LINE__, retry->command );
        }

        // Execute the command
        err = retry->orig_cmd(dce, param, result);

        // Check for timeout
        if (err == ESP_ERR_TIMEOUT) {
            if (retry->command) {
                ESP_LOGW(TAG, "%s(%d): Command:%s response timeout", __func__, __LINE__, retry->command);
            }
            timeouts++;
            continue;
        // Check for errors
        } else if (err != ESP_OK) {
            if (retry->command) {
                ESP_LOGW(TAG, "%s(%d): Command:%s failed", __func__, __LINE__, retry->command);
            }
            errors++;
            continue;
        }

        // Success
        if (retry->command) {
            ESP_LOGD(TAG, "%s(%d): Command:%s succeeded", __func__, __LINE__, retry->command);
        }
        return ESP_OK;
    }
    return err;

}

esp_modem_recov_resend_t *esp_modem_recov_resend_new(esp_modem_dce_t *dce, dce_command_t orig_cmd, esp_modem_retry_fn_t recover, int max_timeouts, int max_errors)
{
    esp_modem_recov_resend_t * retry = calloc(1, sizeof(esp_modem_recov_resend_t));
    ESP_MODEM_ERR_CHECK(retry, "failed to allocate pin structure", err);
    retry->retries_after_error = max_errors;
    retry->retries_after_timeout = max_timeouts;
    retry->orig_cmd = orig_cmd;
    retry->recover = recover;
    retry->destroy = retry_destroy;
    retry->dce = dce;
    retry->run = esp_modem_retry_run;
    return retry;
err:
    return NULL;
}

esp_modem_recov_gpio_t *esp_modem_recov_gpio_new(int gpio_num, int inactive_level, int active_width_ms, int inactive_width_ms)
{
    gpio_config_t io_config = {
            .pin_bit_mask = BIT64(gpio_num),
            .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io_config);
    gpio_set_level(gpio_num, inactive_level);
    gpio_set_level(gpio_num, inactive_level);

    esp_modem_recov_gpio_t * pin = calloc(1, sizeof(esp_modem_recov_gpio_t));
    ESP_MODEM_ERR_CHECK(pin, "failed to allocate pin structure", err);

    pin->inactive_level = inactive_level;
    pin->active_width_ms = active_width_ms;
    pin->inactive_width_ms = inactive_width_ms;
    pin->gpio_num = gpio_num;
    pin->pulse = pulse;
    pin->pulse_special = pulse_special;
    pin->destroy = pulse_destroy;
    return pin;
err:
    return NULL;
}
