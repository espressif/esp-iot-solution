/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_ops.h"
#include "pixel/present_executor.h"
#include "platform/present_os.h"
typedef struct {
    esp_display_present_executor_t base;
    present_mutex_t *gate;
} cpu_t;
static esp_err_t cpu_caps(void *context, esp_display_present_executor_caps_t *out)
{
    (void)context;
    *out = (esp_display_present_executor_caps_t) {
        .name = "cpu",
        .format_mask = 3,
        .rotation_mask = 15,
        .byte_swap = true,
        .in_place = true,
        .base_alignment = 1,
        .dst_cap_align = 1,
        .stride_align = 1
    };
    return ESP_OK;
}
static esp_err_t cpu_probe(void *context, const esp_display_present_copy_request_t *request, esp_display_present_executor_reason_t *reason)
{
    (void)context;
    esp_err_t error = esp_display_present_pixel_validate_request(request);
    *reason = error == ESP_OK ? ESP_DISPLAY_PRESENT_EXEC_SUPPORTED : ESP_DISPLAY_PRESENT_EXEC_INVALID_REQUEST;
    return error;
}
static esp_err_t cpu_execute(void *context, const esp_display_present_copy_request_t *request)
{
    cpu_t *cpu = context;
    (void)present_mutex_take(cpu->gate, UINT32_MAX);
    esp_err_t error = esp_display_present_pixel_execute_cpu(request);
    present_mutex_give(cpu->gate);
    return error;
}
static esp_err_t cpu_destroy(void *context)
{
    cpu_t *cpu = context;
    present_mutex_destroy(cpu->gate);
    present_free(cpu);
    return ESP_OK;
}
static const esp_display_present_executor_ops_t cpu_ops = {cpu_caps, cpu_probe, cpu_execute, cpu_destroy};
esp_err_t esp_display_present_executor_cpu_create(esp_display_present_executor_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    cpu_t *cpu = present_alloc(sizeof(*cpu));
    if (!cpu) {
        return ESP_ERR_NO_MEM;
    }
    cpu->gate = present_mutex_create();
    if (!cpu->gate) {
        present_free(cpu);
        return ESP_ERR_NO_MEM;
    }
    cpu->base = (esp_display_present_executor_t) {
        &cpu_ops, cpu
    };
    *out = &cpu->base;
    return ESP_OK;
}
