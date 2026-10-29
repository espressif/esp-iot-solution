/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "quickjs.h"
#include "quickjs-libc.h"

#include <stdbool.h>
#include <stddef.h>

static const char *TAG = "qjs_example";

/* From EMBED_TXTFILES "hello_world.js" — trailing null excluded from script_len. */
extern const char hello_world_js_start[] asm("_binary_hello_world_js_start");
extern const char hello_world_js_end[] asm("_binary_hello_world_js_end");

#define QJS_EX_SCRIPT_NAME "hello_world.js"

/* -------------------------------------------------------------------------- */
/* QuickJS runtime                                                            */
/* -------------------------------------------------------------------------- */

typedef struct {
    int unhandled_rejections;
} qjs_ex_runtime_state_t;

static void qjs_ex_runtime_promise_rejection_tracker(JSContext *ctx, JSValueConst promise, JSValueConst reason,
                                                     bool is_handled, void *opaque)
{
    qjs_ex_runtime_state_t *state = opaque;
    (void)promise;

    if (is_handled) {
        if (state->unhandled_rejections > 0) {
            state->unhandled_rejections--;
        }
        return;
    }

    state->unhandled_rejections++;
    const char *reason_str = JS_ToCString(ctx, reason);
    ESP_LOGE(TAG, "Unhandled promise rejection: %s", reason_str ? reason_str : "<unprintable>");
    if (reason_str) {
        JS_FreeCString(ctx, reason_str);
    }
}

static esp_err_t qjs_ex_runtime_execute_pending_jobs(JSRuntime *rt, qjs_ex_runtime_state_t *state)
{
    JSContext *job_ctx = NULL;
    int err;

    while ((err = JS_ExecutePendingJob(rt, &job_ctx)) > 0) {
    }
    if (err < 0) {
        ESP_LOGE(TAG, "JS pending job failed");
        if (job_ctx) {
            js_std_dump_error(job_ctx);
        }
        return ESP_FAIL;
    }
    if (state->unhandled_rejections > 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t qjs_ex_runtime_eval_embedded(void)
{
    const char *script = hello_world_js_start;
    size_t script_len = (size_t)(hello_world_js_end - hello_world_js_start);
    if (script_len > 0 && script[script_len - 1] == '\0') {
        script_len -= 1;
    }

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        ESP_LOGE(TAG, "JS_NewRuntime failed");
        return ESP_FAIL;
    }
    qjs_ex_runtime_state_t state = { 0 };
    js_std_init_handlers(rt);
    JS_SetHostPromiseRejectionTracker(rt, qjs_ex_runtime_promise_rejection_tracker, &state);
    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader, js_module_check_attributes, NULL);

    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        ESP_LOGE(TAG, "JS_NewContext failed");
        js_std_free_handlers(rt);
        JS_FreeRuntime(rt);
        return ESP_FAIL;
    }

    js_std_add_helpers(ctx, 0, NULL);
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    JSValue v = JS_Eval(ctx, script, script_len, QJS_EX_SCRIPT_NAME, JS_EVAL_TYPE_MODULE);

    esp_err_t out = ESP_OK;
    if (JS_IsException(v)) {
        ESP_LOGE(TAG, "JS eval failed");
        js_std_dump_error(ctx);
        out = ESP_FAIL;
    }
    JS_FreeValue(ctx, v);

    if (out == ESP_OK) {
        out = qjs_ex_runtime_execute_pending_jobs(rt, &state);
    }

    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return out;
}

/* -------------------------------------------------------------------------- */

void app_main(void)
{
    esp_err_t err = qjs_ex_runtime_eval_embedded();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "QuickJS hello finished");
    }
}
