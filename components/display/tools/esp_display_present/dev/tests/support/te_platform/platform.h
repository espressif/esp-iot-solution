/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include "esp_err.h"
#define IRAM_ATTR
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#ifndef ESP_IDF_VERSION
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(6,2,0)
#endif
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(n) (n)
#define pdTRUE 1
#define pdFALSE 0
typedef unsigned TickType_t;
typedef int BaseType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define portENTER_CRITICAL_ISR(p) ((void)(p))
#define portEXIT_CRITICAL_ISR(p) ((void)(p))
#define portENTER_CRITICAL_SAFE(p) ((void)(p))
#define portEXIT_CRITICAL_SAFE(p) ((void)(p))
#define portYIELD_FROM_ISR() ((void)0)
typedef void *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateCounting(unsigned a, unsigned b)
{
    (void)a;
    (void)b;
    return malloc(1);
}
static inline void vSemaphoreDelete(SemaphoreHandle_t p)
{
    free(p);
}
static inline int xSemaphoreTake(SemaphoreHandle_t p, unsigned ticks)
{
    (void)p;
    assert(ticks == 0);
    return 0;
}
static inline int xSemaphoreGive(SemaphoreHandle_t p)
{
    (void)p;
    return 1;
}
static inline void xSemaphoreGiveFromISR(SemaphoreHandle_t p, BaseType_t *yield)
{
    (void)p;
    (void)yield;
}
static int64_t fake_time;
static inline int64_t esp_timer_get_time(void)
{
    return fake_time;
}
static inline void log_ignore(const char *tag, const char *fmt, ...)
{
    (void)tag;
    (void)fmt;
}
#define ESP_LOGI log_ignore
#define ESP_LOGW log_ignore
#define ESP_LOGE log_ignore
static inline const char *esp_err_to_name(int e)
{
    (void)e;
    return "mock";
}
#define ESP_RETURN_ON_FALSE(v,e,...) do {if(!(v)) return (e);} while(0)
#define ESP_GOTO_ON_FALSE(v,e,label,...) do {if(!(v)) {ret=(e);goto label;}} while(0)
#define ESP_GOTO_ON_ERROR(v,label,...) do {ret=(v);if(ret!=ESP_OK) goto label;} while(0)
typedef int gpio_int_type_t;
#define GPIO_INTR_POSEDGE 1
#define GPIO_INTR_NEGEDGE 2
#define GPIO_INTR_ANYEDGE 3
/* ESP32-S3 input GPIO capabilities: 0..21 and 26..48. */
#define GPIO_IS_VALID_GPIO(n) ((n) >= 0 && (n) < 49 && ((n) < 22 || (n) > 25))
#define GPIO_MODE_INPUT 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_PULLUP_ENABLE 1
#define ESP_INTR_FLAG_LOWMED 0
typedef struct {
    int intr_type, mode, pull_down_en, pull_up_en;
    uint64_t pin_bit_mask;
} gpio_config_t;
static unsigned removes;
static inline int gpio_get_level(int pin)
{
    (void)pin;
    return 0;
}
static inline int gpio_config(const gpio_config_t *c)
{
    (void)c;
    return ESP_OK;
}
static inline int gpio_install_isr_service(int flags)
{
    (void)flags;
    return ESP_OK;
}
static inline int gpio_isr_handler_add(int pin, void (*cb)(void*), void *p)
{
    (void)pin;
    (void)cb;
    (void)p;
    return ESP_OK;
}
static inline int gpio_isr_handler_remove(int pin)
{
    (void)pin;
    ++removes;
    return ESP_OK;
}
typedef struct {
    void (*callback)(void*);
    void *arg;
    int dispatch_method;
    const char *name;
} esp_timer_create_args_t;
typedef struct fake_timer {
    esp_timer_create_args_t args;
    uint64_t due;
} *esp_timer_handle_t;
#define ESP_TIMER_TASK 0
static bool timer_busy;
static unsigned stop_budget;
static inline int esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out)
{
    *out = calloc(1, sizeof(**out));
    (*out)->args = *args;
    return ESP_OK;
}
static inline int esp_timer_stop(esp_timer_handle_t t)
{
    t->due = 0;
    return ESP_OK;
}
static inline int esp_timer_start_once(esp_timer_handle_t t, uint64_t delay)
{
    t->due = fake_time + delay;
    return ESP_OK;
}
static inline int esp_timer_stop_blocking(esp_timer_handle_t t, unsigned ticks)
{
    (void)t;
    stop_budget = ticks;
    return timer_busy ? ESP_ERR_TIMEOUT : ESP_OK;
}
static inline int esp_timer_delete(esp_timer_handle_t t)
{
    free(t);
    return ESP_OK;
}
