/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "string.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_private/esp_cache_private.h"
#include "driver/gpio.h"
#include "driver/ppa.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_lcd_mipi_dsi.h"
#include "ppa_blend.h"
#include "ppa_blend_private.h"
#include "lvgl_ppa_blend.h"
#include "lcd_ppa_blend_private.h"

#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
#define PPA_BLEND_TASK_STACK_SIZE   (4 * 1024)

static char *TAG = "ppa_blend";

static size_t data_cache_line_size = 0;

static ppa_client_handle_t ppa_client_blend_handle = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;

static ppa_blend_oper_config_t blend_config = { };
static uint8_t blend_task_priority = 0;
static SemaphoreHandle_t blend_queue = NULL;
static SemaphoreHandle_t ppa_blend_done = NULL;
static ppa_blend_done_cb_t blend_done = NULL;
static user_ppa_blend_notify_done_cb_t user_ppa_blend_notify_done = NULL;

static bool ppa_blend_enable_flag = true;
static bool ppa_blend_user_data_flag = false;

static void ppa_blend_task(void *arg);
static esp_err_t ppa_blend_start(void);

esp_err_t ppa_blend_new(ppa_blend_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    ESP_RETURN_ON_ERROR(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size), TAG,
                        "Get cache line size failed");

    ppa_client_config_t ppa_client_config = {
        .oper_type = PPA_OPERATION_BLEND,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_client_config, &ppa_client_blend_handle));

    lcd_panel = config->display_panel;

    blend_queue = xQueueCreate(1, sizeof(ppa_blend_block_t));
    ESP_RETURN_ON_FALSE(blend_queue != NULL, ESP_ERR_NO_MEM, TAG, "Create blend_queue failed");

    ppa_blend_oper_config_t blend_operation_config = {
        .in_bg.buffer = config->bg_layer.buffer,
        .in_bg.pic_w = config->bg_layer.w,
        .in_bg.pic_h = config->bg_layer.h,

        .in_fg.buffer = config->fg_layer.buffer,
        .in_fg.pic_w = config->fg_layer.w,
        .in_fg.pic_h = config->fg_layer.h,

        .out.buffer = config->out_layer.buffer,
        .out.buffer_size = ALIGN_UP_BY(config->out_layer.buffer_size, data_cache_line_size),
            .out.pic_w = config->out_layer.w,
            .out.pic_h = config->out_layer.h,

            .in_bg.blend_cm = config->bg_layer.color_mode,
            .bg_rgb_swap = 0,
            .bg_byte_swap = 0,
            .bg_alpha_update_mode = config->bg_layer.alpha_update_mode,
            .bg_alpha_fix_val = config->bg_layer.alpha_value,

            .in_fg.blend_cm = config->fg_layer.color_mode,
            .fg_rgb_swap = 0,
            .fg_byte_swap = 0,
            .fg_alpha_update_mode = config->fg_layer.alpha_update_mode,
            .fg_alpha_fix_val = config->fg_layer.alpha_value,

            .out.blend_cm = config->out_layer.color_mode,

            .mode = PPA_TRANS_MODE_BLOCKING,
    };
    memcpy(&blend_config, &blend_operation_config, sizeof(ppa_blend_oper_config_t));

    blend_task_priority = config->task_priority;

    ppa_blend_start();

    return ESP_OK;
}

esp_err_t ppa_blend_set_bg_layer(ppa_blend_in_layer_t *bg_layer)
{
    ESP_RETURN_ON_FALSE(bg_layer != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    blend_config.in_bg.buffer = bg_layer->buffer;
    blend_config.in_bg.pic_w = bg_layer->w;
    blend_config.in_bg.pic_h = bg_layer->h;
    blend_config.bg_alpha_update_mode = bg_layer->alpha_update_mode;
    blend_config.in_bg.blend_cm = bg_layer->color_mode;
    blend_config.bg_alpha_fix_val = bg_layer->alpha_value;

    return ESP_OK;
}

esp_err_t ppa_blend_set_fg_layer(ppa_blend_in_layer_t *fg_layer)
{
    ESP_RETURN_ON_FALSE(fg_layer != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    blend_config.in_fg.buffer = fg_layer->buffer;
    blend_config.in_fg.pic_w = fg_layer->w;
    blend_config.in_fg.pic_h = fg_layer->h;
    blend_config.fg_alpha_update_mode = fg_layer->alpha_update_mode;
    blend_config.in_fg.blend_cm = fg_layer->color_mode;
    blend_config.fg_alpha_fix_val = fg_layer->alpha_value;

    return ESP_OK;
}

esp_err_t ppa_blend_set_out_layer(ppa_blend_out_layer_t *out_layer)
{
    ESP_RETURN_ON_FALSE(out_layer != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    blend_config.out.buffer = out_layer->buffer;
    blend_config.out.buffer_size = ALIGN_UP_BY(out_layer->buffer_size, data_cache_line_size);
    blend_config.out.pic_w = out_layer->w;
    blend_config.out.pic_h = out_layer->h;
    blend_config.out.blend_cm = out_layer->color_mode;

    return ESP_OK;
}

static bool ppa_blend_check(void)
{
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGive(ppa_blend_done);
    return (need_yield == pdTRUE);
}

static void ppa_blend_done_callback(ppa_blend_block_t *block)
{
    switch (block->update_type) {
    case PPA_BLEND_UPDATE_FG:
        lvgl_ppa_blend_notify_done(false);
        break;
    case PPA_BLEND_UPDATE_BG:
        if (user_ppa_blend_notify_done) {
            user_ppa_blend_notify_done();
        }
        break;
    default:
        break;
    }

    lcd_ppa_blend_refresh();

    ppa_blend_check();
}

static esp_err_t ppa_blend_register_blend_done_callback(ppa_blend_done_cb_t callback)
{
    blend_done = callback;

    return ESP_OK;
}

static esp_err_t ppa_blend_start(void)
{
    ESP_RETURN_ON_FALSE(blend_queue != NULL, ESP_ERR_INVALID_STATE, TAG, "bsp_ppa isn't initialized");

    ppa_blend_done = xSemaphoreCreateBinary();

    BaseType_t ret = xTaskCreate(ppa_blend_task, "ppa_blend_task", PPA_BLEND_TASK_STACK_SIZE, NULL, blend_task_priority, NULL);
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_ERR_NO_MEM, TAG, "Create ppa_blend_task failed");

    ppa_blend_register_blend_done_callback(ppa_blend_done_callback);

    return ESP_OK;
}

esp_err_t ppa_blend_run(ppa_blend_block_t *block, int timeout_ms)
{
    BaseType_t timeout_tick = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    if (ppa_blend_user_data_flag) {
        (esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, blend_config.in_bg.pic_w, blend_config.in_bg.pic_h, blend_config.in_bg.buffer));

        return ESP_OK;
    }

    if (!ppa_blend_enable_flag) {
        return ESP_FAIL;
    }

    if (xQueueSend(blend_queue, block, timeout_tick) != pdTRUE) {
        ESP_LOGE(TAG, "Send blend block failed");

        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

bool ppa_blend_run_isr(ppa_blend_block_t *block)
{
    BaseType_t need_yield = pdFALSE;

    xQueueSendFromISR(blend_queue, block, &need_yield);

    return (need_yield == pdTRUE);
}

static void ppa_blend_fill_block(ppa_blend_oper_config_t *operation_config, ppa_blend_block_t *blend_config)
{
    operation_config->in_bg.block_offset_x = blend_config->bg_offset_x;
    operation_config->in_bg.block_offset_y = blend_config->bg_offset_y;

    operation_config->in_bg.block_offset_x = blend_config->fg_offset_x;
    operation_config->in_bg.block_offset_y = blend_config->fg_offset_y;

    operation_config->out.block_offset_x = blend_config->out_offset_x;
    operation_config->out.block_offset_y = blend_config->out_offset_y;

    operation_config->in_bg.block_w = blend_config->w;
    operation_config->in_bg.block_h = blend_config->h;
    operation_config->in_fg.block_w = blend_config->w;
    operation_config->in_fg.block_h = blend_config->h;
}

static void ppa_blend_task(void *arg)
{
    ESP_LOGI(TAG, "ppa_blend_task start");

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(ppa_client_blend_handle != NULL, ESP_ERR_INVALID_ARG, err, TAG, "ppa_client_blend_handle is NULL");

    ppa_blend_block_t block;

    while (1) {
        xQueueReceive(blend_queue, &block, portMAX_DELAY);

        if (!ppa_blend_enable_flag) {
            continue;
        }

        if (blend_config.in_bg.buffer == NULL || blend_config.in_fg.buffer == NULL || blend_config.out.buffer == NULL) {
            ESP_LOGE(TAG, "Invalid blend buffer");
            continue;
        }

        ppa_blend_fill_block(&blend_config, &block);

        ret = ppa_do_blend(ppa_client_blend_handle, &blend_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "PPA blend failed");
            continue;
        }

        if (blend_done) {
            blend_done(&block);
        }
    }

err:
    vTaskDelete(NULL);
}

void ppa_blend_check_done(uint32_t timeout)
{
    xSemaphoreTake(ppa_blend_done, 0);
    xSemaphoreTake(ppa_blend_done, pdMS_TO_TICKS(timeout));
}

esp_err_t ppa_blend_register_notify_done_callback(user_ppa_blend_notify_done_cb_t callback)
{
    user_ppa_blend_notify_done = callback;

    lvgl_ppa_blend_set_notify_done_cb(callback);
    return ESP_OK;
}

void ppa_blend_enable(bool flag)
{
    ppa_blend_enable_flag = flag;
}

void ppa_blend_user_data_enable(bool flag)
{
    ppa_blend_user_data_flag = flag;
}

bool ppa_blend_user_data_is_enabled(void)
{
    return ppa_blend_user_data_flag;
}

bool ppa_blend_is_enabled(void)
{
    return ppa_blend_enable_flag;
}
