/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_ops.h"
#include "pixel/present_executor.h"
#include <string.h>
static bool valid(const esp_display_present_executor_t *executor)
{
    return executor && executor->ops && executor->ops->get_caps && executor->ops->probe && executor->ops->execute && executor->ops->destroy;
}
esp_err_t esp_display_present_executor_get_caps(const esp_display_present_executor_t *executor, esp_display_present_executor_caps_t *caps)
{
    if (!caps) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(caps, 0, sizeof(*caps));
    return valid(executor) ? executor->ops->get_caps(executor->context, caps) : ESP_ERR_INVALID_ARG;
}
esp_err_t esp_display_present_executor_probe(const esp_display_present_executor_t *executor, const esp_display_present_copy_request_t *request, esp_display_present_executor_reason_t *reason)
{
    if (!reason) {
        return ESP_ERR_INVALID_ARG;
    }
    *reason = ESP_DISPLAY_PRESENT_EXEC_INVALID_REQUEST;
    if (!valid(executor)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t error = esp_display_present_pixel_validate_request(request);
    return error == ESP_OK ? executor->ops->probe(executor->context, request, reason) : error;
}
esp_err_t esp_display_present_executor_execute(void *context, const esp_display_present_copy_request_t *request)
{
    esp_display_present_executor_t *executor = context;
    esp_display_present_executor_reason_t reason;
    esp_err_t error = esp_display_present_executor_probe(executor, request, &reason);
    return error == ESP_OK ? executor->ops->execute(executor->context, request) : error;
}
esp_err_t esp_display_present_executor_destroy(esp_display_present_executor_t *executor)
{
    return valid(executor) ? executor->ops->destroy(executor->context) : ESP_ERR_INVALID_ARG;
}
esp_err_t esp_display_present_executor_select(const esp_display_present_executor_dispatch_t *dispatch, const esp_display_present_copy_request_t *request, esp_display_present_executor_selection_t *selection)
{
    if (!selection) {
        return ESP_ERR_INVALID_ARG;
    }
    *selection = (esp_display_present_executor_selection_t) {
        .primary_reason = ESP_DISPLAY_PRESENT_EXEC_INVALID_REQUEST
    };
    if (!dispatch) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t error = esp_display_present_executor_probe(dispatch->primary, request, &selection->primary_reason);
    if (error == ESP_OK) {
        selection->executor = dispatch->primary;
        return ESP_OK;
    }
    if (error != ESP_ERR_NOT_SUPPORTED || !dispatch->fallback) {
        return error;
    }
    error = esp_display_present_executor_probe(dispatch->fallback, request, &selection->fallback_reason);
    if (error == ESP_OK) {
        selection->executor = dispatch->fallback;
        selection->used_fallback = true;
    }
    return error;
}
esp_err_t esp_display_present_executor_run(const esp_display_present_executor_dispatch_t *dispatch, const esp_display_present_copy_request_t *request, esp_display_present_executor_selection_t *out)
{
    esp_display_present_executor_selection_t local;
    esp_display_present_executor_selection_t *selection = out ? out : &local;
    esp_err_t error = esp_display_present_executor_select(dispatch, request, selection);
    return error == ESP_OK ? esp_display_present_executor_execute(selection->executor, request) : error;
}
esp_err_t esp_display_present_executor_dispatch_execute(void *context, const esp_display_present_copy_request_t *request)
{
    return esp_display_present_executor_run(context, request, NULL);
}
