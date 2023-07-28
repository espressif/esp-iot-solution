/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "ccomp_timer.h"

#include "ccomp_timer_impl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_intr_alloc.h"

#if CONFIG_IDF_TARGET_ESP32
#include "esp32/clk.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/clk.h"
#endif


static const char TAG[] = "ccomp_timer";

esp_err_t ccomp_timer_start(void)
{
    esp_err_t err = ESP_OK;

    ccomp_timer_impl_lock();
    if (ccomp_timer_impl_is_init()) {
        if (ccomp_timer_impl_is_active()) {
            err = ESP_ERR_INVALID_STATE;
        }
    }
    else {
        err = ccomp_timer_impl_init();
    }
    ccomp_timer_impl_unlock();

    if (err != ESP_OK) {
        goto fail;
    }

    err = ccomp_timer_impl_reset();

    if (err != ESP_OK) {
        goto fail;
    }

    err = ccomp_timer_impl_start();

    if (err == ESP_OK) {
        return ESP_OK;
    }

fail:
    ESP_LOGE(TAG, "Unable to start performance timer");
    return err;
}

int64_t IRAM_ATTR ccomp_timer_stop(void)
{
    esp_err_t err = ESP_OK;
    ccomp_timer_impl_lock();
    if (!ccomp_timer_impl_is_active()) {
        err = ESP_ERR_INVALID_STATE;
    }
    ccomp_timer_impl_unlock();

    if (err != ESP_OK) {
        goto fail;
    }

    err = ccomp_timer_impl_stop();
    if (err != ESP_OK) {
        goto fail;
    }

    int64_t t = ccomp_timer_get_time();

    err = ccomp_timer_impl_deinit();

    if (err == ESP_OK && t != -1) {
        return t;
    }

fail:
    ESP_LOGE(TAG, "Unable to stop performance timer");
    return -1;
}

int64_t IRAM_ATTR ccomp_timer_get_time(void)
{
    return ccomp_timer_impl_get_time();
}
