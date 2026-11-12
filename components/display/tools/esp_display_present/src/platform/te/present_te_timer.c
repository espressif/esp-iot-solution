/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "platform/te/present_te_timer.h"
#include "platform/present_os.h"
#include <stdlib.h>

#ifdef PRESENT_HOST
#include <errno.h>
#include <pthread.h>
#include <time.h>
struct present_te_timer {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    pthread_t thread;
    present_te_timer_callback_t callback;
    void *user;
    uint64_t deadline;
    bool armed, closing, exited;
};
static struct timespec to_timespec(uint64_t us)
{
    return (struct timespec) {
        (time_t)(us / 1000000), (long)(us % 1000000) * 1000
    };
}
static void *timer_main(void *user)
{
    present_te_timer_t *timer = user;
    pthread_mutex_lock(&timer->lock);
    while (!timer->closing) {
        if (!timer->armed) {
            pthread_cond_wait(&timer->changed, &timer->lock);
            continue;
        }
        struct timespec deadline = to_timespec(timer->deadline);
        int ret = pthread_cond_timedwait(&timer->changed, &timer->lock, &deadline);
        if (ret == ETIMEDOUT && timer->armed && present_now_us() >= timer->deadline) {
            timer->armed = false;
            pthread_mutex_unlock(&timer->lock);
            timer->callback(timer->user);
            pthread_mutex_lock(&timer->lock);
        }
    }
    timer->exited = true;
    pthread_cond_broadcast(&timer->changed);
    pthread_mutex_unlock(&timer->lock);
    return NULL;
}
esp_err_t present_te_timer_create(present_te_timer_callback_t callback, void *user, present_te_timer_t **out)
{
    if (!callback || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    present_te_timer_t *timer = calloc(1, sizeof(*timer));
    if (!timer) {
        return ESP_ERR_NO_MEM;
    }
    pthread_condattr_t attr;
    if (pthread_mutex_init(&timer->lock, NULL)) {
        free(timer);
        return ESP_FAIL;
    }
    if (pthread_condattr_init(&attr)) {
        pthread_mutex_destroy(&timer->lock);
        free(timer);
        return ESP_FAIL;
    }
    int err = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (!err) {
        err = pthread_cond_init(&timer->changed, &attr);
    }
    pthread_condattr_destroy(&attr);
    if (err) {
        pthread_mutex_destroy(&timer->lock);
        free(timer);
        return ESP_FAIL;
    }
    timer->callback = callback; timer->user = user;
    if (pthread_create(&timer->thread, NULL, timer_main, timer)) {
        pthread_cond_destroy(&timer->changed); pthread_mutex_destroy(&timer->lock); free(timer); return ESP_FAIL;
    }
    *out = timer;
    return ESP_OK;
}
esp_err_t present_te_timer_arm(present_te_timer_t *timer, uint64_t deadline_us)
{
    if (!timer) {
        return ESP_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&timer->lock);
    if (timer->closing) {
        pthread_mutex_unlock(&timer->lock);
        return ESP_ERR_INVALID_STATE;
    }
    timer->deadline = deadline_us; timer->armed = true;
    pthread_cond_signal(&timer->changed);
    pthread_mutex_unlock(&timer->lock);
    return ESP_OK;
}
esp_err_t present_te_timer_destroy(present_te_timer_t *timer, uint32_t timeout_ms)
{
    if (!timer) {
        return ESP_OK;
    }
    pthread_mutex_lock(&timer->lock);
    timer->closing = true; timer->armed = false;
    pthread_cond_signal(&timer->changed);
    uint64_t now = present_now_us();
    uint64_t delta = (uint64_t)timeout_ms * 1000;
    struct timespec deadline = to_timespec(UINT64_MAX - now < delta ? UINT64_MAX : now + delta);
    while (!timer->exited) {
        if (!timeout_ms) {
            pthread_mutex_unlock(&timer->lock);
            return ESP_ERR_TIMEOUT;
        }
        int ret = timeout_ms == UINT32_MAX ? pthread_cond_wait(&timer->changed, &timer->lock) :
                  pthread_cond_timedwait(&timer->changed, &timer->lock, &deadline);
        if (ret == ETIMEDOUT && !timer->exited) {
            pthread_mutex_unlock(&timer->lock);
            return ESP_ERR_TIMEOUT;
        }
    }
    pthread_mutex_unlock(&timer->lock);
    pthread_join(timer->thread, NULL);
    pthread_cond_destroy(&timer->changed); pthread_mutex_destroy(&timer->lock); free(timer);
    return ESP_OK;
}
#else
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
struct present_te_timer {
    esp_timer_handle_t handle;
    present_te_timer_callback_t callback;
    void *user;
    bool closing;
};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 2, 0)
static void timer_callback(void *user)
{
    present_te_timer_t *timer = user;
    timer->callback(timer->user);
}
#endif
esp_err_t present_te_timer_create(present_te_timer_callback_t callback, void *user, present_te_timer_t **out)
{
    if (!callback || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 2, 0)
    return ESP_ERR_NOT_SUPPORTED; /* No bounded join for the timer callback. */
#else
    present_te_timer_t *timer = calloc(1, sizeof(*timer));
    if (!timer) {
        return ESP_ERR_NO_MEM;
    }
    timer->callback = callback; timer->user = user;
    const esp_timer_create_args_t args = {
        .callback = timer_callback,
        .arg = timer,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "present_te",
    };
    esp_err_t error = esp_timer_create(&args, &timer->handle);
    if (error != ESP_OK) {
        free(timer);
        return error;
    }
    *out = timer;
    return ESP_OK;
#endif
}
esp_err_t present_te_timer_arm(present_te_timer_t *timer, uint64_t deadline_us)
{
    if (!timer || timer->closing) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t error = esp_timer_stop(timer->handle);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return error;
    }
    uint64_t now = present_now_us();
    return esp_timer_start_once(timer->handle, deadline_us > now ? deadline_us - now : 1);
}
esp_err_t present_te_timer_destroy(present_te_timer_t *timer, uint32_t timeout_ms)
{
    if (!timer) {
        return ESP_OK;
    }
    timer->closing = true;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 2, 0)
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
#else
    esp_err_t error = esp_timer_stop_blocking(timer->handle, timeout_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    if (error != ESP_OK) {
        return error;
    }
    error = esp_timer_delete(timer->handle);
    if (error != ESP_OK) {
        return error;
    }
    free(timer);
    return ESP_OK;
#endif
}
#endif

size_t present_te_timer_memory(void)
{
    return sizeof(present_te_timer_t);
}
