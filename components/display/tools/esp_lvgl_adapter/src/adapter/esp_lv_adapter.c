/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_lv_adapter_adapter.c
 * @brief LVGL adapter implementation for ESP-IDF
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "esp_lv_adapter.h"
#include "adapter_internal.h"
#include "display_manager.h"
#include "display_bridge.h"

/* Tag for logging */
static const char *TAG = "esp_lvgl:adapter";

/* Global adapter context */
static esp_lv_adapter_context_t s_ctx;

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE && LVGL_VERSION_MAJOR < 9
/* LVGL v8 FreeType library initialization state */
static bool s_freetype_initialized = false;
#endif

/* Forward declarations of internal functions */
static esp_err_t tick_init(void);
static void lvgl_worker(void *arg);

/*****************************************************************************
 *                         Public API Implementation                         *
 *****************************************************************************/

static void adapter_sleep_state_reset(esp_lv_adapter_sleep_state_t *state)
{
    if (state) {
        memset(state, 0, sizeof(*state));
    }
}

static void adapter_detach_display_node(esp_lv_adapter_display_node_t *node)
{
    if (!node) {
        return;
    }

    node->cfg.panel_detached = true;
    node->cfg.base.panel = NULL;
    node->cfg.base.panel_io = NULL;
    memset(node->cfg.frame_buffers, 0, sizeof(node->cfg.frame_buffers));
    node->cfg.frame_buffer_count = 0;

    if (node->bridge && node->bridge->update_panel) {
        esp_err_t ret = node->bridge->update_panel(node->bridge, &node->cfg);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to update bridge during detach (%d)", ret);
        }
    }
}

esp_err_t esp_lv_adapter_init(const esp_lv_adapter_config_t *config)
{
    ESP_RETURN_ON_FALSE(!s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter already initialized");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid adapter configuration");

    /* Initialize context */
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.config = *config;
    s_ctx.task_exit_requested = false;
    s_ctx.paused = false;
    s_ctx.pause_ack = false;

    /* Initialize LVGL library */
    lv_init();

    esp_err_t ret = ESP_OK;

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
    /* Initialize image decoder */
    ret = esp_lv_decoder_init(&s_ctx.decoder_handle);
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "Decoder init failed (%d)", ret);
    ESP_LOGI(TAG, "Decoder initialized successfully");
#endif

    /* Initialize LVGL tick timer */
    ret = tick_init();
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "Tick init failed (%d)", ret);

    /* Create dual recursive mutexes */
    s_ctx.lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_GOTO_ON_FALSE(s_ctx.lvgl_mutex, ESP_ERR_NO_MEM, cleanup, TAG, "Failed to create LVGL mutex");

    s_ctx.dummy_draw_mutex = xSemaphoreCreateRecursiveMutex();
    ESP_GOTO_ON_FALSE(s_ctx.dummy_draw_mutex, ESP_ERR_NO_MEM, cleanup, TAG, "Failed to create dummy draw mutex");

    s_ctx.pause_done_sem = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(s_ctx.pause_done_sem, ESP_ERR_NO_MEM, cleanup, TAG, "Failed to create pause semaphore");

    s_ctx.inited = true;
    ESP_LOGI(TAG, "LVGL adapter initialized successfully");
    return ESP_OK;

cleanup:
    if (s_ctx.pause_done_sem) {
        vSemaphoreDelete(s_ctx.pause_done_sem);
        s_ctx.pause_done_sem = NULL;
    }
    if (s_ctx.dummy_draw_mutex) {
        vSemaphoreDelete(s_ctx.dummy_draw_mutex);
        s_ctx.dummy_draw_mutex = NULL;
    }
    if (s_ctx.lvgl_mutex) {
        vSemaphoreDelete(s_ctx.lvgl_mutex);
        s_ctx.lvgl_mutex = NULL;
    }
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
    if (s_ctx.decoder_handle) {
        esp_lv_decoder_deinit(s_ctx.decoder_handle);
        s_ctx.decoder_handle = NULL;
    }
#endif
    memset(&s_ctx, 0, sizeof(s_ctx));
    return ret;
}

esp_err_t esp_lv_adapter_start(void)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");

    /* Check if task already started */
    if (s_ctx.task) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    /* Determine task core affinity */
    BaseType_t core = (s_ctx.config.task_core_id < 0) ? tskNO_AFFINITY : s_ctx.config.task_core_id;

    /* Create LVGL worker task */
    uint32_t stack_size = s_ctx.config.task_stack_size ? s_ctx.config.task_stack_size : ESP_LV_ADAPTER_DEFAULT_STACK_SIZE;
    UBaseType_t task_priority = s_ctx.config.task_priority ? s_ctx.config.task_priority : ESP_LV_ADAPTER_DEFAULT_TASK_PRIORITY;
    BaseType_t task_ret;
    uint32_t caps = s_ctx.config.stack_in_psram
                    ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                    : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    task_ret = xTaskCreatePinnedToCoreWithCaps(
                   lvgl_worker,
                   "lvgl",
                   stack_size,
                   NULL,
                   task_priority,
                   &s_ctx.task,
                   core,
                   caps
               );

    if (task_ret != pdPASS && s_ctx.config.stack_in_psram) {
        ESP_LOGW(TAG, "LVGL task PSRAM allocation failed, retrying with internal memory");
        caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
        task_ret = xTaskCreatePinnedToCoreWithCaps(
                       lvgl_worker,
                       "lvgl",
                       stack_size,
                       NULL,
                       task_priority,
                       &s_ctx.task,
                       core,
                       caps
                   );
    }

    ESP_GOTO_ON_FALSE(task_ret == pdPASS, ESP_ERR_NO_MEM, fail, TAG, "Failed to create LVGL task");

    s_ctx.task_exit_requested = false;
    s_ctx.paused = false;
    s_ctx.pause_ack = false;

    ESP_LOGI(TAG, "LVGL task started successfully");
    return ESP_OK;

fail:
    s_ctx.task = NULL;
    return ret;
}

bool esp_lv_adapter_is_initialized(void)
{
    return s_ctx.inited;
}

esp_err_t esp_lv_adapter_lock(int32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_ctx.lvgl_mutex, ESP_ERR_INVALID_STATE, TAG, "Adapter LVGL mutex not initialized");

    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    ESP_RETURN_ON_FALSE(xSemaphoreTakeRecursive(s_ctx.lvgl_mutex, ticks) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Failed to acquire LVGL lock");

    return ESP_OK;
}

void esp_lv_adapter_unlock(void)
{
    if (s_ctx.lvgl_mutex) {
        xSemaphoreGiveRecursive(s_ctx.lvgl_mutex);
    }
}

esp_err_t esp_lv_adapter_refresh_now(lv_display_t *disp)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");

    /* Acquire lock before calling LVGL API */
    esp_err_t ret = esp_lv_adapter_lock(-1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    /* Force LVGL to refresh the display now */
    lv_refr_now(disp);

    /* Release lock */
    esp_lv_adapter_unlock();

    return ESP_OK;
}

esp_err_t esp_lv_adapter_pause(int32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    if (s_ctx.paused) {
        return ESP_OK;
    }

    bool called_from_worker = (s_ctx.task && xTaskGetCurrentTaskHandle() == s_ctx.task);

    s_ctx.pause_ack = false;
    s_ctx.paused = true;

    if (called_from_worker) {
        /* Avoid deadlock when pause is requested from the LVGL worker itself */
        s_ctx.pause_ack = true;
        if (s_ctx.pause_done_sem) {
            xSemaphoreGive(s_ctx.pause_done_sem);
        }
        return ESP_OK;
    }

    if (!s_ctx.task || !s_ctx.pause_done_sem) {
        s_ctx.pause_ack = true;
        return ESP_OK;
    }

    xSemaphoreTake(s_ctx.pause_done_sem, 0); /* clear pending */
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(s_ctx.pause_done_sem, ticks) != pdTRUE) {
        s_ctx.paused = false;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t esp_lv_adapter_resume(void)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    if (!s_ctx.paused) {
        return ESP_OK;
    }

    bool was_sleeping = s_ctx.sleep_state.is_sleeping;

    s_ctx.paused = false;
    s_ctx.pause_ack = false;

    if (s_ctx.task) {
        xTaskNotifyGive(s_ctx.task);
    }

    /* If recovering from sleep, trigger full refresh for displays that need it */
    if (was_sleeping) {
        /* Give worker task time to resume */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Acquire LVGL lock before invalidating display areas */
        esp_err_t lock_ret = esp_lv_adapter_lock(-1);
        if (lock_ret == ESP_OK) {
            esp_lv_adapter_display_node_t *node = s_ctx.display_list;
            while (node) {
                if (node->lv_disp && !node->cfg.panel_detached) {
                    /* Only refresh if framebuffers were cleared (RGB/MIPI interfaces) */
                    if (node->cfg.base.profile.interface != ESP_LV_ADAPTER_PANEL_IF_OTHER) {
                        display_manager_request_full_refresh(node->lv_disp);
                    }
                }
                node = node->next;
            }
            esp_lv_adapter_unlock();
        }

        adapter_sleep_state_reset(&s_ctx.sleep_state);
    }

    return ESP_OK;
}

/**
 * @brief Prepare all displays for sleep with automatic recovery on failure
 *
 * This function:
 * 1. Pauses LVGL worker
 * 2. Waits for all pending flushes
 * 3. Detaches LCD panels from displays
 *
 * If any step fails, automatically resumes the worker and cleans up.
 *
 * @return
 *      - ESP_OK: Success, displays are ready for panel deletion
 *      - ESP_ERR_INVALID_STATE: Not initialized or already sleeping
 *      - ESP_ERR_TIMEOUT: Flush wait timeout
 */
esp_err_t esp_lv_adapter_sleep_prepare(void)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Not initialized");
    /* Check if already sleeping */
    if (s_ctx.sleep_state.is_sleeping) {
        ESP_LOGW(TAG, "Already in sleep state");
        return ESP_ERR_INVALID_STATE;
    }

    /* Check if externally paused to prevent state pollution */
    if (s_ctx.paused) {
        ESP_LOGE(TAG, "Cannot sleep_prepare while externally paused. "
                 "Call esp_lv_adapter_resume() first or use pause/resume directly.");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t total_displays = 0;
    for (esp_lv_adapter_display_node_t *node = s_ctx.display_list; node; node = node->next) {
        total_displays++;
        if (total_displays > ESP_LV_ADAPTER_MAX_SLEEP_DISPLAYS) {
            ESP_LOGE(TAG, "Sleep prepare supports up to %d displays (found %u)",
                     ESP_LV_ADAPTER_MAX_SLEEP_DISPLAYS, total_displays);
            return ESP_ERR_INVALID_SIZE;
        }
    }

    adapter_sleep_state_reset(&s_ctx.sleep_state);

    esp_lv_adapter_display_node_t *node = s_ctx.display_list;
    while (node && s_ctx.sleep_state.display_count < ESP_LV_ADAPTER_MAX_SLEEP_DISPLAYS) {
        uint8_t idx = s_ctx.sleep_state.display_count++;
        s_ctx.sleep_state.all_displays[idx] = node->lv_disp;
        s_ctx.sleep_state.display_nodes[idx] = node;

        for (uint8_t i = 0; i < node->sleep.input_count &&
                s_ctx.sleep_state.input_count < ESP_LV_ADAPTER_MAX_SLEEP_INPUTS; i++) {
            s_ctx.sleep_state.all_inputs[s_ctx.sleep_state.input_count++] = node->sleep.associated_inputs[i];
        }

        node = node->next;
    }

    ESP_LOGI(TAG, "Sleep prepare: %u displays, %u inputs",
             s_ctx.sleep_state.display_count, s_ctx.sleep_state.input_count);

    esp_err_t ret = esp_lv_adapter_pause(-1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pause adapter (%d)", ret);
        adapter_sleep_state_reset(&s_ctx.sleep_state);
        return ret;
    }

    for (int i = 0; i < s_ctx.sleep_state.display_count; i++) {
        ret = display_manager_wait_flush_done(s_ctx.sleep_state.all_displays[i], 5000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Display %d flush wait failed (%d), auto-recovering", i, ret);
            esp_lv_adapter_resume();
            adapter_sleep_state_reset(&s_ctx.sleep_state);
            return ret;
        }
    }

    for (int i = 0; i < s_ctx.sleep_state.display_count; i++) {
        esp_lv_adapter_display_node_t *detach_node = s_ctx.sleep_state.display_nodes[i];
        if (!detach_node || !detach_node->cfg.base.panel) {
            ESP_LOGD(TAG, "Display %d already detached or not found", i);
            continue;
        }

        adapter_detach_display_node(detach_node);
    }

    s_ctx.sleep_state.is_sleeping = true;

    ESP_LOGI(TAG, "Sleep prepared successfully (%d displays detached)",
             s_ctx.sleep_state.display_count);
    ESP_LOGI(TAG, "Safe to call esp_lcd_panel_del() for each panel");
    return ESP_OK;
}

/**
 * @brief Recover a display from sleep state
 *
 * Rebinds a new LCD panel to an existing LVGL display after sleep.
 * Automatically resumes LVGL worker when all displays are recovered.
 *
 * Supports partial recovery: Call esp_lv_adapter_sleep_recover() again for
 * displays that become ready later. The adapter resumes automatically once
 * every display is rebound.
 *
 * @param disp LVGL display handle
 * @param panel New LCD panel handle
 * @param panel_io New LCD panel IO handle (can be NULL for RGB/MIPI DSI)
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Not in sleep state
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Failed to rebind panel
 */
esp_err_t esp_lv_adapter_sleep_recover(lv_display_t *disp,
                                       esp_lcd_panel_handle_t panel,
                                       esp_lcd_panel_io_handle_t panel_io)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_ARG, TAG, "Invalid panel handle");
    ESP_RETURN_ON_FALSE(s_ctx.sleep_state.is_sleeping, ESP_ERR_INVALID_STATE, TAG, "Not in sleep state");

    /* Rebind the display with new panel */
    esp_err_t ret = esp_lv_adapter_rebind_lcd_panel_internal(disp, panel, panel_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to rebind panel (%d)", ret);
        return ret;
    }

    /* Check if this display was in the sleep state list */
    bool found = false;
    int recovered_idx = -1;
    for (int i = 0; i < s_ctx.sleep_state.display_count; i++) {
        if (s_ctx.sleep_state.all_displays[i] == disp) {
            found = true;
            recovered_idx = i;
            /* Mark as recovered by clearing the entry */
            s_ctx.sleep_state.all_displays[i] = NULL;
            s_ctx.sleep_state.display_nodes[i] = NULL;
            break;
        }
    }

    if (!found) {
        ESP_LOGW(TAG, "Display not found in sleep state, but rebind succeeded");
    } else {
        ESP_LOGI(TAG, "Display %d/%d recovered successfully",
                 recovered_idx + 1, s_ctx.sleep_state.display_count);
    }

    /* Check if all displays have been recovered */
    int remaining = 0;
    for (int i = 0; i < s_ctx.sleep_state.display_count; i++) {
        if (s_ctx.sleep_state.all_displays[i] != NULL) {
            remaining++;
        }
    }

    if (remaining == 0) {
        /* All displays recovered - exit sleep mode */
        s_ctx.sleep_state.is_sleeping = false;
        adapter_sleep_state_reset(&s_ctx.sleep_state);

        ESP_LOGI(TAG, "All displays recovered, resuming LVGL worker");
        esp_lv_adapter_resume();
    } else {
        ESP_LOGI(TAG, "Waiting for %d more display(s) to recover", remaining);
    }

    return ESP_OK;
}

esp_err_t esp_lv_adapter_detach_lcd_panel_internal(lv_display_t *disp)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");

    esp_lv_adapter_display_node_t *node = display_manager_get_node(disp);
    ESP_RETURN_ON_FALSE(node, ESP_ERR_NOT_FOUND, TAG, "Display not found");
    ESP_RETURN_ON_FALSE(node->cfg.base.panel, ESP_ERR_INVALID_STATE, TAG, "Panel already detached");

    /* Step 1: Pause LVGL worker to prevent new render cycles */
    esp_err_t ret = esp_lv_adapter_pause(-1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to pause adapter");

    /* Step 2: Wait for any ongoing flush operations to complete */
    ret = display_manager_wait_flush_done(disp, 5000);  /* 5 second timeout */
    if (ret != ESP_OK) {
        /* Try to resume on error */
        esp_lv_adapter_resume();
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to wait for flush completion");
    }

    /* Step 3-5: Mark runtime state as detached */
    adapter_detach_display_node(node);

    ESP_LOGI(TAG, "LCD panel detached successfully, LVGL display preserved");
    ESP_LOGI(TAG, "It is now safe to call esp_lcd_panel_del()");
    return ESP_OK;
}

esp_err_t esp_lv_adapter_rebind_lcd_panel_internal(lv_display_t *disp,
                                                   esp_lcd_panel_handle_t panel,
                                                   esp_lcd_panel_io_handle_t panel_io)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_ARG, TAG, "Invalid panel handle");

    esp_lv_adapter_display_node_t *node = display_manager_get_node(disp);
    ESP_RETURN_ON_FALSE(node, ESP_ERR_NOT_FOUND, TAG, "Display not found");
    ESP_RETURN_ON_FALSE(node->cfg.panel_detached, ESP_ERR_INVALID_STATE, TAG, "Panel not detached");

    /* Step 1: Update panel handles in configuration */
    node->cfg.base.panel = panel;
    node->cfg.base.panel_io = panel_io;

    /* Step 2: Refetch framebuffers from new panel and update all references */
    esp_err_t ret = display_manager_refetch_framebuffers(disp);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to refetch framebuffers from new panel");

    ret = display_manager_rebind_draw_buffers(disp);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to rebind LVGL draw buffers");

    /* Step 3: Clear detached flag */
    node->cfg.panel_detached = false;

    /* Step 4: Acquire lock and trigger full screen refresh */
    ret = esp_lv_adapter_lock(-1);
    if (ret == ESP_OK) {
        display_manager_request_full_refresh(disp);
        esp_lv_adapter_unlock();
    } else {
        ESP_LOGW(TAG, "Failed to acquire LVGL lock for full refresh");
    }

    ESP_LOGI(TAG, "LCD panel rebound successfully");
    ESP_LOGI(TAG, "Call esp_lv_adapter_resume() to restart rendering");
    return ESP_OK;
}

esp_err_t esp_lv_adapter_set_dummy_draw(lv_display_t *disp, bool enable)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");
    ESP_RETURN_ON_FALSE(xSemaphoreTakeRecursive(s_ctx.dummy_draw_mutex, portMAX_DELAY) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Failed to acquire dummy draw lock");

    esp_err_t ret = display_manager_set_dummy_draw(disp, enable);

    if (ret == ESP_OK && !enable) {
        esp_err_t lock_ret = esp_lv_adapter_lock((uint32_t) -1);
        if (lock_ret == ESP_OK) {
            display_manager_request_full_refresh(disp);
            esp_lv_adapter_unlock();
        } else {
            ESP_LOGW(TAG, "Failed to acquire LVGL lock for full refresh");
        }
    }

    xSemaphoreGiveRecursive(s_ctx.dummy_draw_mutex);
    return ret;
}

bool esp_lv_adapter_get_dummy_draw_enabled(lv_display_t *disp)
{
    if (!s_ctx.inited) {
        ESP_LOGW(TAG, "Adapter not initialized");
        return false;
    }

    if (!disp) {
        ESP_LOGW(TAG, "Invalid display handle");
        return false;
    }

    bool enabled = false;
    esp_err_t ret = display_manager_get_dummy_draw_state(disp, &enabled);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get dummy draw state");
        return false;
    }

    return enabled;
}

esp_err_t esp_lv_adapter_set_dummy_draw_callbacks(lv_display_t *disp,
                                                  const esp_lv_adapter_dummy_draw_callbacks_t *cbs,
                                                  void *user_ctx)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");

    return display_manager_set_dummy_draw_callbacks(disp, cbs, user_ctx);
}

esp_err_t esp_lv_adapter_dummy_draw_blit(lv_display_t *disp,
                                         int x_start,
                                         int y_start,
                                         int x_end,
                                         int y_end,
                                         const void *frame_buffer,
                                         bool wait)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");
    ESP_RETURN_ON_FALSE(frame_buffer, ESP_ERR_INVALID_ARG, TAG, "Frame buffer cannot be NULL");
    ESP_RETURN_ON_FALSE(x_start >= 0 && y_start >= 0 && x_end > x_start && y_end > y_start,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid coordinates");
    ESP_RETURN_ON_FALSE(xSemaphoreTakeRecursive(s_ctx.dummy_draw_mutex, portMAX_DELAY) == pdTRUE,
                        ESP_ERR_TIMEOUT, TAG, "Failed to acquire dummy draw lock");

    esp_err_t ret = display_manager_dummy_draw_blit(disp, x_start, y_start, x_end, y_end, frame_buffer, wait);

    xSemaphoreGiveRecursive(s_ctx.dummy_draw_mutex);
    return ret;
}

uint8_t esp_lv_adapter_get_required_frame_buffer_count(esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                                                       esp_lv_adapter_rotation_t rotation)
{
    /* This is a pure calculation function that doesn't require adapter initialization */
    return display_manager_required_frame_buffer_count(tear_avoid_mode, rotation);
}

lv_display_t *esp_lv_adapter_register_display(const esp_lv_adapter_display_config_t *config)
{
    /* Validate adapter is initialized and config is valid */
    if (!s_ctx.inited || !config) {
        return NULL;
    }

    /* Delegate to display manager */
    return display_manager_register(config);
}

esp_err_t esp_lv_adapter_unregister_display(lv_display_t *disp)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");

    /* Acquire lock to ensure thread safety */
    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire adapter lock");

    /* Delegate to display manager */
    ret = display_manager_unregister(disp);

    /* Release lock */
    esp_lv_adapter_unlock();

    return ret;
}

esp_err_t esp_lv_adapter_set_area_rounder_cb(lv_display_t *disp,
                                             void (*rounder_cb)(lv_area_t *area, void *user_data),
                                             void *user_data)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(disp, ESP_ERR_INVALID_ARG, TAG, "Invalid display handle");
    return display_manager_set_area_rounder_cb(disp, rounder_cb, user_data);
}

esp_err_t esp_lv_adapter_deinit(void)
{
    /* Check if already deinitialized */
    if (!s_ctx.inited) {
        return ESP_OK;
    }

    /* Request LVGL task to exit gracefully */
    if (s_ctx.task) {
        ESP_LOGI(TAG, "Requesting LVGL task to exit...");
        s_ctx.task_exit_requested = true;
        s_ctx.paused = false;
        s_ctx.pause_ack = false;
        xTaskNotifyGive(s_ctx.task);

        /* Wait for task to exit (max 1 second) */
        uint32_t wait_count = 0;
        const uint32_t max_wait_ms = 1000;
        const uint32_t poll_interval_ms = 10;

        while (s_ctx.task != NULL && wait_count < (max_wait_ms / poll_interval_ms)) {
            vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
            wait_count++;
        }

        /* Force delete if task didn't exit */
        if (s_ctx.task != NULL) {
            ESP_LOGW(TAG, "LVGL task didn't exit gracefully, force deleting");
            vTaskDelete(s_ctx.task);
            s_ctx.task = NULL;
        } else {
            ESP_LOGI(TAG, "LVGL task exited gracefully");
        }
    }

    /* Stop and delete tick timer */
    if (s_ctx.tick_timer) {
        esp_timer_handle_t timer = (esp_timer_handle_t)s_ctx.tick_timer;
        esp_timer_stop(timer);
        esp_timer_delete(timer);
        s_ctx.tick_timer = NULL;
        ESP_LOGI(TAG, "Tick timer stopped and deleted");
    }

    /* Clear all registered displays */
    display_manager_clear();

    /* Clear all registered input devices */
    esp_lv_adapter_input_node_t *input_node = s_ctx.input_list;
    uint32_t input_count = 0;
    while (input_node) {
        esp_lv_adapter_input_node_t *next = input_node->next;

        /* Delete LVGL input device */
        if (input_node->indev) {
#if LVGL_VERSION_MAJOR >= 9
            lv_indev_delete(input_node->indev);
#else
            /* In LVGL v8, input devices are automatically cleaned up with displays */
            /* Just free the user context if it exists */
#endif
        }

        /* Free user context based on input device type */
        if (input_node->user_ctx) {
            switch (input_node->type) {
            case ESP_LV_ADAPTER_INPUT_TYPE_TOUCH:
                ESP_LOGD(TAG, "Cleaning up touch device context");
                free(input_node->user_ctx);
                break;
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_BUTTON
            case ESP_LV_ADAPTER_INPUT_TYPE_BUTTON:
                ESP_LOGD(TAG, "Cleaning up button device context");
                free(input_node->user_ctx);
                break;
#endif
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_KNOB
            case ESP_LV_ADAPTER_INPUT_TYPE_ENCODER:
                ESP_LOGD(TAG, "Cleaning up encoder device context");
                free(input_node->user_ctx);
                break;
#endif
            default:
                ESP_LOGW(TAG, "Unknown input device type %d, freeing context anyway", input_node->type);
                free(input_node->user_ctx);
                break;
            }
        }

        free(input_node);
        input_node = next;
        input_count++;
    }
    s_ctx.input_list = NULL;

    if (input_count > 0) {
        ESP_LOGI(TAG, "%lu input device(s) cleaned up", input_count);
    }

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
    /* Deinitialize all mounted file systems */
    esp_lv_adapter_fs_node_t *fs_node = s_ctx.fs_list;
    while (fs_node) {
        esp_lv_adapter_fs_node_t *next = fs_node->next;
        if (fs_node->handle) {
            esp_lv_fs_desc_deinit(fs_node->handle);
            ESP_LOGI(TAG, "File system deinitialized");
        }
        free(fs_node);
        fs_node = next;
    }
    s_ctx.fs_list = NULL;
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    /* Deinitialize all FreeType fonts */
    uint32_t font_count = 0;
    esp_lv_adapter_ft_font_node_t *font_node = s_ctx.font_list;
    while (font_node) {
        esp_lv_adapter_ft_font_node_t *next = font_node->next;
        if (font_node->initialized) {
#if LVGL_VERSION_MAJOR >= 9
            if (font_node->font) {
                lv_freetype_font_delete(font_node->font);
                font_count++;
            }
#else
            if (font_node->ft_info.font) {
                lv_ft_font_destroy(font_node->ft_info.font);
                font_count++;
            }
#endif
        }
        if (font_node->name_copy) {
            free(font_node->name_copy);
        }
        free(font_node);
        font_node = next;
    }
    s_ctx.font_list = NULL;
    if (font_count > 0) {
        ESP_LOGI(TAG, "%lu FreeType font(s) deinitialized", font_count);
    }

#if LVGL_VERSION_MAJOR < 9
    /* LVGL v8: Deinitialize FreeType library */
    if (s_freetype_initialized) {
        lv_freetype_destroy();
        s_freetype_initialized = false;
        ESP_LOGI(TAG, "FreeType library deinitialized");
    }
#endif
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_DECODER
    /* Deinitialize image decoder */
    if (s_ctx.decoder_handle) {
        esp_lv_decoder_deinit(s_ctx.decoder_handle);
        s_ctx.decoder_handle = NULL;
        ESP_LOGI(TAG, "Decoder deinitialized");
    }
#endif

    /* Cleanup mutexes */
    if (s_ctx.dummy_draw_mutex) {
        vSemaphoreDelete(s_ctx.dummy_draw_mutex);
        s_ctx.dummy_draw_mutex = NULL;
    }
    if (s_ctx.lvgl_mutex) {
        vSemaphoreDelete(s_ctx.lvgl_mutex);
        s_ctx.lvgl_mutex = NULL;
    }
    if (s_ctx.pause_done_sem) {
        vSemaphoreDelete(s_ctx.pause_done_sem);
        s_ctx.pause_done_sem = NULL;
    }

    /* Clear context */
    memset(&s_ctx, 0, sizeof(s_ctx));
    ESP_LOGI(TAG, "LVGL adapter deinitialized successfully");
    return ESP_OK;
}

esp_lv_adapter_context_t *esp_lv_adapter_get_context(void)
{
    return &s_ctx;
}

/*****************************************************************************
 *                      Input Device Management Functions                    *
 *****************************************************************************/

esp_err_t esp_lv_adapter_register_input_device(lv_indev_t *indev,
                                               esp_lv_adapter_input_type_t type,
                                               void *user_ctx)
{
    ESP_RETURN_ON_FALSE(indev, ESP_ERR_INVALID_ARG, TAG, "Input device handle cannot be NULL");

    /* Allocate new input device node */
    esp_lv_adapter_input_node_t *node = (esp_lv_adapter_input_node_t *)calloc(1, sizeof(esp_lv_adapter_input_node_t));
    ESP_RETURN_ON_FALSE(node, ESP_ERR_NO_MEM, TAG, "Failed to allocate input device node");

    /* Initialize node */
    node->indev = indev;
    node->type = type;
    node->user_ctx = user_ctx;

    /* Add to head of list */
    node->next = s_ctx.input_list;
    s_ctx.input_list = node;

    ESP_LOGD(TAG, "Input device registered (type=%d)", type);
    return ESP_OK;
}

esp_err_t esp_lv_adapter_unregister_input_device(lv_indev_t *indev)
{
    ESP_RETURN_ON_FALSE(indev, ESP_ERR_INVALID_ARG, TAG, "Input device handle cannot be NULL");

    /* Find and remove from list */
    esp_lv_adapter_input_node_t **cursor = &s_ctx.input_list;
    while (*cursor) {
        if ((*cursor)->indev == indev) {
            esp_lv_adapter_input_node_t *node = *cursor;
            *cursor = node->next;
            free(node);
            ESP_LOGD(TAG, "Input device unregistered");
            return ESP_OK;
        }
        cursor = &(*cursor)->next;
    }

    return ESP_ERR_NOT_FOUND;
}

/*****************************************************************************
 *                         Internal Functions                                *
 *****************************************************************************/

/**
 * @brief LVGL worker task
 *
 * Main task that periodically calls lv_timer_handler() to process LVGL events.
 * The task will exit gracefully when s_ctx.task_exit_requested is set.
 */
static void lvgl_worker(void *arg)
{
    uint32_t task_delay_ms = s_ctx.config.task_max_delay_ms;

    while (!s_ctx.task_exit_requested) {
        if (s_ctx.paused) {
            if (!s_ctx.pause_ack) {
                s_ctx.pause_ack = true;
                if (s_ctx.pause_done_sem) {
                    xSemaphoreGive(s_ctx.pause_done_sem);
                }
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        /* Process LVGL timers */
        if (esp_lv_adapter_lock(-1) == ESP_OK) {
            task_delay_ms = lv_timer_handler();
            esp_lv_adapter_unlock();
        }

        /* Clamp delay to configured range */
        if (task_delay_ms > s_ctx.config.task_max_delay_ms) {
            task_delay_ms = s_ctx.config.task_max_delay_ms;
        } else if (task_delay_ms < s_ctx.config.task_min_delay_ms) {
            task_delay_ms = s_ctx.config.task_min_delay_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }

    s_ctx.paused = false;
    s_ctx.pause_ack = false;
    /* Task deletes itself */
    s_ctx.task = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief LVGL tick increment callback
 *
 * Called by esp_timer to increment LVGL's internal tick counter
 */
static void tick_increment(void *arg)
{
    uint32_t tick_period_ms = (uint32_t)arg;
    lv_tick_inc(tick_period_ms);
}

/**
 * @brief Initialize LVGL tick timer
 *
 * Creates and starts a periodic timer for LVGL tick updates
 */
static esp_err_t tick_init(void)
{
    uint32_t tick_period_ms = s_ctx.config.tick_period_ms;

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .arg = (void *)tick_period_ms,
        .name = "LVGL tick"
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_err_t ret = esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create tick timer");

    /* Save timer handle to context for cleanup */
    s_ctx.tick_timer = lvgl_tick_timer;

    ret = esp_timer_start_periodic(lvgl_tick_timer, tick_period_ms * 1000);
    if (ret != ESP_OK) {
        esp_timer_delete(lvgl_tick_timer);
        s_ctx.tick_timer = NULL;
        return ret;
    }

    return ESP_OK;
}

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
/*****************************************************************************
 *                   File System Integration Functions                       *
 *****************************************************************************/

esp_err_t esp_lv_adapter_fs_mount(const fs_cfg_t *config, esp_lv_fs_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid FS parameters");

    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire adapter lock (ret=%d)", ret);

    esp_lv_fs_handle_t handle = NULL;
    ret = esp_lv_fs_desc_init(config, &handle);
    if (ret != ESP_OK) {
        esp_lv_adapter_unlock();
        return ret;
    }

    esp_lv_adapter_fs_node_t *node = (esp_lv_adapter_fs_node_t *)calloc(1, sizeof(esp_lv_adapter_fs_node_t));
    if (!node) {
        esp_lv_fs_desc_deinit(handle);
        esp_lv_adapter_unlock();
        ESP_LOGE(TAG, "Failed to allocate FS node");
        return ESP_ERR_NO_MEM;
    }

    node->handle = handle;
    node->next = s_ctx.fs_list;
    s_ctx.fs_list = node;

    esp_lv_adapter_unlock();

    *ret_handle = handle;
    ESP_LOGI(TAG, "File system mounted successfully");
    return ESP_OK;
}

esp_err_t esp_lv_adapter_fs_unmount(esp_lv_fs_handle_t handle)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid FS handle");

    esp_err_t ret = esp_lv_adapter_lock((uint32_t) -1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire adapter lock (ret=%d)", ret);

    esp_lv_adapter_fs_node_t **cursor = &s_ctx.fs_list;
    while (*cursor && (*cursor)->handle != handle) {
        cursor = &(*cursor)->next;
    }

    if (!*cursor) {
        esp_lv_adapter_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    esp_lv_adapter_fs_node_t *node = *cursor;
    *cursor = node->next;

    esp_lv_fs_desc_deinit(handle);
    esp_lv_adapter_unlock();

    free(node);

    ESP_LOGI(TAG, "File system unmounted successfully");
    return ESP_OK;
}
#endif

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS
/*****************************************************************************
 *                      FPS Statistics Functions                             *
 *****************************************************************************/

esp_err_t esp_lv_adapter_fps_stats_enable(lv_display_t *disp, bool enable)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");

    esp_lv_adapter_display_node_t *node = display_manager_get_node(disp);
    ESP_RETURN_ON_FALSE(node, ESP_ERR_INVALID_ARG, TAG, "Display not found");

    esp_err_t ret = esp_lv_adapter_lock(-1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    /* Enable/disable FPS statistics */
    node->fps_stats.enabled = enable;

    /* Reset statistics when enabling */
    if (enable) {
        node->fps_stats.frame_count = 0;
        node->fps_stats.window_start_time = esp_timer_get_time();
        node->fps_stats.current_fps = 0;  /* Use integer to avoid FPU in ISR */
    }

    esp_lv_adapter_unlock();

    return ESP_OK;
}

esp_err_t esp_lv_adapter_get_fps(lv_display_t *disp, uint32_t *fps)
{
    ESP_RETURN_ON_FALSE(fps, ESP_ERR_INVALID_ARG, TAG, "FPS pointer cannot be NULL");
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");

    esp_lv_adapter_display_node_t *node = display_manager_get_node(disp);
    ESP_RETURN_ON_FALSE(node, ESP_ERR_INVALID_ARG, TAG, "Display not found");
    ESP_RETURN_ON_FALSE(node->fps_stats.enabled, ESP_ERR_INVALID_STATE, TAG, "FPS statistics disabled");

    esp_err_t ret = esp_lv_adapter_lock(-1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    /* Return integer FPS directly (no floating point) */
    *fps = node->fps_stats.current_fps;

    esp_lv_adapter_unlock();

    return ESP_OK;
}

esp_err_t esp_lv_adapter_fps_stats_reset(lv_display_t *disp)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");

    esp_lv_adapter_display_node_t *node = display_manager_get_node(disp);
    ESP_RETURN_ON_FALSE(node, ESP_ERR_INVALID_ARG, TAG, "Display not found");

    esp_err_t ret = esp_lv_adapter_lock(-1);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to acquire LVGL lock (ret=%d)", ret);

    node->fps_stats.frame_count = 0;
    node->fps_stats.window_start_time = esp_timer_get_time();
    node->fps_stats.current_fps = 0;  /* Use integer to avoid FPU in ISR */

    esp_lv_adapter_unlock();

    return ESP_OK;
}
#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FPS_STATS */

/*****************************************************************************
 *                      FreeType Font Functions (Optional)                   *
 *****************************************************************************/

#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE

/* LVGL v8: Check FreeType support and define style macros */
#if LVGL_VERSION_MAJOR < 9
#if !defined(LV_USE_FREETYPE) || LV_USE_FREETYPE == 0
#error "LVGL FreeType support must be enabled for adapter FreeType features"
#endif

/* Map LVGL v8 FreeType style constants */
#ifndef FT_FONT_STYLE_NORMAL
#ifdef LV_FT_FONT_STYLE_NORMAL
#define FT_FONT_STYLE_NORMAL    LV_FT_FONT_STYLE_NORMAL
#define FT_FONT_STYLE_ITALIC    LV_FT_FONT_STYLE_ITALIC
#define FT_FONT_STYLE_BOLD      LV_FT_FONT_STYLE_BOLD
#else
#define FT_FONT_STYLE_NORMAL    0
#define FT_FONT_STYLE_ITALIC    1
#define FT_FONT_STYLE_BOLD      2
#endif
#endif

/* LVGL v8: Global FreeType initialization (once per adapter lifecycle) */
static esp_err_t ensure_freetype_initialized(void)
{
    if (s_freetype_initialized) {
        return ESP_OK;
    }

    if (!lv_freetype_init(CONFIG_LV_FREETYPE_CACHE_FT_FACES,
                          CONFIG_LV_FREETYPE_CACHE_FT_SIZES,
                          CONFIG_LV_FREETYPE_CACHE_SIZE)) {
        ESP_LOGE(TAG, "FreeType library initialization failed");
        return ESP_FAIL;
    }

    s_freetype_initialized = true;
    ESP_LOGI(TAG, "FreeType library initialized (v8)");
    return ESP_OK;
}

#endif /* LVGL_VERSION_MAJOR < 9 */

esp_err_t esp_lv_adapter_ft_font_init(const esp_lv_adapter_ft_font_config_t *config,
                                      esp_lv_adapter_ft_font_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE(config->name, ESP_ERR_INVALID_ARG, TAG, "Font name is required");
    ESP_RETURN_ON_FALSE(config->size > 0, ESP_ERR_INVALID_ARG, TAG, "Font size must be > 0");

#if LVGL_VERSION_MAJOR < 9
    /* LVGL v8: Ensure FreeType library is initialized */
    esp_err_t init_ret = ensure_freetype_initialized();
    ESP_RETURN_ON_ERROR(init_ret, TAG, "FreeType library initialization failed");
#endif

    /* Allocate font node */
    esp_lv_adapter_ft_font_node_t *font = (esp_lv_adapter_ft_font_node_t *)calloc(1, sizeof(esp_lv_adapter_ft_font_node_t));
    ESP_RETURN_ON_FALSE(font, ESP_ERR_NO_MEM, TAG, "Failed to allocate font node");

    esp_err_t ret = ESP_OK;
    bool lock_acquired = false;

    /* Copy font name */
    size_t name_len = strlen(config->name) + 1;
    font->name_copy = (char *)malloc(name_len);
    if (!font->name_copy) {
        ESP_LOGE(TAG, "Failed to allocate font name");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memcpy(font->name_copy, config->name, name_len);

#if LVGL_VERSION_MAJOR >= 9
    /* LVGL v9: Setup font_info structure */
    lv_freetype_init_font_info(&font->font_info);
    font->font_info.name = font->name_copy;
    font->font_info.size = config->size;

    /* Map style enum to LVGL v9 FreeType style */
    switch (config->style) {
    case ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL:
        font->font_info.style = LV_FREETYPE_FONT_STYLE_NORMAL;
        break;
    case ESP_LV_ADAPTER_FT_FONT_STYLE_ITALIC:
        font->font_info.style = LV_FREETYPE_FONT_STYLE_ITALIC;
        break;
    case ESP_LV_ADAPTER_FT_FONT_STYLE_BOLD:
        font->font_info.style = LV_FREETYPE_FONT_STYLE_BOLD;
        break;
    case ESP_LV_ADAPTER_FT_FONT_STYLE_BOLD_ITALIC:
        font->font_info.style = LV_FREETYPE_FONT_STYLE_BOLD | LV_FREETYPE_FONT_STYLE_ITALIC;
        break;
    default:
        font->font_info.style = LV_FREETYPE_FONT_STYLE_NORMAL;
        break;
    }

    /* Note: v9 only supports file-based fonts via pathname in font_info.name */
    if (config->mem && config->mem_size > 0) {
        ESP_LOGW(TAG, "LVGL v9 FreeType does not support memory-based fonts, will use file path");
    }

#else  /* LVGL_VERSION_MAJOR < 9 */
    /* LVGL v8: Setup ft_info structure */
    font->ft_info.name = font->name_copy;
    font->ft_info.weight = config->size;

    /* Map style enum to LVGL v8 FreeType style */
    switch (config->style) {
    case ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL:
        font->ft_info.style = FT_FONT_STYLE_NORMAL;
        break;
    case ESP_LV_ADAPTER_FT_FONT_STYLE_ITALIC:
        font->ft_info.style = FT_FONT_STYLE_ITALIC;
        break;
    case ESP_LV_ADAPTER_FT_FONT_STYLE_BOLD:
        font->ft_info.style = FT_FONT_STYLE_BOLD;
        break;
    case ESP_LV_ADAPTER_FT_FONT_STYLE_BOLD_ITALIC:
        font->ft_info.style = FT_FONT_STYLE_BOLD | FT_FONT_STYLE_ITALIC;
        break;
    default:
        font->ft_info.style = FT_FONT_STYLE_NORMAL;
        break;
    }

    /* Setup memory or file mode */
    if (config->mem && config->mem_size > 0) {
        /* Load from memory */
        font->ft_info.mem = (void *)config->mem;
        font->ft_info.mem_size = config->mem_size;
    } else {
        /* Load from file */
        font->ft_info.mem = NULL;
        font->ft_info.mem_size = 0;
    }
#endif /* LVGL_VERSION_MAJOR >= 9 */

    /* Acquire LVGL lock before font initialization */
    ret = esp_lv_adapter_lock(-1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        goto cleanup;
    }
    lock_acquired = true;

    /* Initialize FreeType font */
#if LVGL_VERSION_MAJOR >= 9
    /* LVGL v9: Use new API */
    font->font = lv_freetype_font_create_with_info(&font->font_info);
    if (font->font == NULL) {
        ESP_LOGE(TAG, "FreeType font initialization failed (v9 API)");
        ret = ESP_FAIL;
        goto cleanup;
    }
    font->initialized = true;
#else
    /* LVGL v8: Use legacy API - requires sufficient stack in calling task */
    bool ft_ret = lv_ft_font_init(&font->ft_info);
    if (!ft_ret) {
        ESP_LOGE(TAG, "FreeType font initialization failed (v8 API)");
        ESP_LOGE(TAG, "Font info: name='%s', size=%u, style=0x%02x, mem=%p, mem_size=%zu",
                 font->ft_info.name ? font->ft_info.name : "NULL",
                 font->ft_info.weight, font->ft_info.style,
                 font->ft_info.mem, font->ft_info.mem_size);
        ret = ESP_FAIL;
        goto cleanup;
    }
    font->initialized = true;
#endif

    /* Add to font list */
    font->next = s_ctx.font_list;
    s_ctx.font_list = font;

    /* Release LVGL lock */
    esp_lv_adapter_unlock();
    lock_acquired = false;

    /* Return handle if requested */
    if (handle) {
        *handle = (esp_lv_adapter_ft_font_handle_t)font;
    }

    return ESP_OK;

cleanup:
    if (lock_acquired) {
        esp_lv_adapter_unlock();
    }
    if (font) {
        if (font->name_copy) {
            free(font->name_copy);
        }
        free(font);
    }
    if (handle) {
        *handle = NULL;
    }
    return ret;
}

const lv_font_t *esp_lv_adapter_ft_font_get(esp_lv_adapter_ft_font_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid font handle");
        return NULL;
    }

    esp_lv_adapter_ft_font_node_t *font = (esp_lv_adapter_ft_font_node_t *)handle;
    if (!font->initialized) {
        ESP_LOGE(TAG, "Font not initialized");
        return NULL;
    }

#if LVGL_VERSION_MAJOR >= 9
    return font->font;
#else
    return font->ft_info.font;
#endif
}

esp_err_t esp_lv_adapter_ft_font_deinit(esp_lv_adapter_ft_font_handle_t handle)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid font handle");

    esp_lv_adapter_ft_font_node_t *font = (esp_lv_adapter_ft_font_node_t *)handle;
    ESP_RETURN_ON_FALSE(font->initialized, ESP_ERR_INVALID_STATE, TAG, "Font not initialized");

    esp_err_t ret = ESP_OK;

    /* Find and remove from list */
    esp_lv_adapter_ft_font_node_t **pp = &s_ctx.font_list;
    bool found = false;
    while (*pp) {
        if (*pp == font) {
            *pp = font->next;
            found = true;
            break;
        }
        pp = &(*pp)->next;
    }

    ESP_RETURN_ON_FALSE(found, ESP_ERR_NOT_FOUND, TAG, "Font not found in list");

    /* Acquire LVGL lock before font deinitialization */
    ret = esp_lv_adapter_lock(-1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return ret;
    }

    /* Deinitialize FreeType font */
#if LVGL_VERSION_MAJOR >= 9
    lv_freetype_font_delete(font->font);
    ESP_LOGI(TAG, "FreeType font '%s' deinitialized [LVGL v9]", font->name_copy);
#else
    lv_ft_font_destroy(font->ft_info.font);
    ESP_LOGI(TAG, "FreeType font '%s' deinitialized [LVGL v8]", font->name_copy);
#endif

    /* Release LVGL lock */
    esp_lv_adapter_unlock();

    /* Free resources */
    if (font->name_copy) {
        free(font->name_copy);
    }
    free(font);

    return ESP_OK;
}

#endif /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */
