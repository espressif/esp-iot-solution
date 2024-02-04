/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/queue.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

#include "ir_learn.h"
#include "ir_encoder.h"
#include "ir_learn_err_check.h"

static const char *TAG = "ir learn";

#define RMT_RX_MEM_BLOCK_SIZE       CONFIG_RMT_MEM_BLOCK_SYMBOLS
#define RMT_DECODE_MARGIN           CONFIG_RMT_DECODE_MARGIN_US
#define RMT_MAX_RANGE_TIME          CONFIG_RMT_SINGLE_RANGE_MAX_US

static const int LEARN_TASK_DELETE  = BIT0;

typedef struct {
    rmt_channel_handle_t channel_rx;    /*!< rmt rx channel handler */
    rmt_rx_done_event_data_t rmt_rx;    /*!< received RMT symbols */

    struct ir_learn_list_head learn_list;
    struct ir_learn_sub_list_head learn_result;

    EventGroupHandle_t  learn_event;
    SemaphoreHandle_t   rmt_mux;
    QueueHandle_t   receive_queue;          /*!< A queue used to send the raw data to the task from the ISR */
    bool            running;
    uint32_t        pre_time;

    uint8_t        learn_count;
    uint8_t        learned_count;
    uint8_t        learned_sub;
} ir_learn_t;

typedef struct {
    ir_learn_result_cb user_cb;
    ir_learn_t *ctx;
} ir_learn_common_param_t;

const static rmt_receive_config_t ir_learn_rmt_rx_cfg = {
    .signal_range_min_ns = 1000,
    .signal_range_max_ns = RMT_MAX_RANGE_TIME * 1000,
};

static bool ir_learn_list_lock(ir_learn_t *ctx, uint32_t timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(ctx->rmt_mux, timeout_ticks) == pdTRUE;
}

static void ir_learn_list_unlock(ir_learn_t *ctx)
{
    xSemaphoreGiveRecursive(ctx->rmt_mux);
}

static bool ir_learn_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t task_woken = pdFALSE;
    ir_learn_t *ir_learn = (ir_learn_t *)user_data;

    xQueueSendFromISR(ir_learn->receive_queue, edata, &task_woken);
    return (task_woken == pdTRUE);
}

esp_err_t ir_learn_print_raw(struct ir_learn_sub_list_head *cmd_list)
{
    IR_LEARN_CHECK(cmd_list, "list pointer can't be NULL!", ESP_ERR_INVALID_ARG);

    uint8_t sub_num = 0;
    struct ir_learn_sub_list_t *sub_it;
    rmt_symbol_word_t *p_symbols;

    SLIST_FOREACH(sub_it, cmd_list, next) {
        ESP_LOGI(TAG, "sub_it:[%d], timediff:%03d ms, symbols:%03d",
                 sub_num++,
                 sub_it->timediff / 1000,
                 sub_it->symbols.num_symbols);

        p_symbols = sub_it->symbols.received_symbols;
        for (int i = 0; i < sub_it->symbols.num_symbols; i++) {
            printf("symbol:[%03d] %04d| %04d\r\n",
                   i, p_symbols->duration0, p_symbols->duration1);
            p_symbols++;
        }
    }
    return ESP_OK;
}

static esp_err_t ir_learn_remove_all_symbol(ir_learn_t *ctx)
{
    ir_learn_list_lock(ctx, 0);
    ir_learn_clean_data(&ctx->learn_list);
    ir_learn_clean_sub_data(&ctx->learn_result);
    ir_learn_list_unlock(ctx);

    return ESP_OK;
}

static esp_err_t ir_learn_destroy(ir_learn_t *ctx)
{
    ir_learn_remove_all_symbol(ctx);

    if (ctx->channel_rx) {
        rmt_disable(ctx->channel_rx);
        rmt_del_channel(ctx->channel_rx);
    }

    if (ctx->receive_queue) {
        vQueueDelete(ctx->receive_queue);
    }

    if (ctx->rmt_mux) {
        vSemaphoreDelete(ctx->rmt_mux);
    }

    if (ctx->rmt_rx.received_symbols) {
        free(ctx->rmt_rx.received_symbols);
    }

    free(ctx);

    return ESP_OK;
}

esp_err_t ir_learn_clean_sub_data(struct ir_learn_sub_list_head *sub_head)
{
    IR_LEARN_CHECK(sub_head, "list pointer can't be NULL!", ESP_ERR_INVALID_ARG);

    struct ir_learn_sub_list_t *result_it;

    while (!SLIST_EMPTY(sub_head)) {
        result_it = SLIST_FIRST(sub_head);
        if (result_it->symbols.received_symbols) {
            heap_caps_free(result_it->symbols.received_symbols);
        }
        SLIST_REMOVE_HEAD(sub_head, next);
        if (result_it) {
            heap_caps_free(result_it);
        }
    }
    SLIST_INIT(sub_head);

    return ESP_OK;
}

esp_err_t ir_learn_clean_data(struct ir_learn_list_head *learn_head)
{
    IR_LEARN_CHECK(learn_head, "list pointer can't be NULL!", ESP_ERR_INVALID_ARG);

    struct ir_learn_list_t *learn_list;

    while (!SLIST_EMPTY(learn_head)) {
        learn_list = SLIST_FIRST(learn_head);

        ir_learn_clean_sub_data(&learn_list->cmd_sub_node);

        SLIST_REMOVE_HEAD(learn_head, next);
        if (learn_list) {
            heap_caps_free(learn_list);
        }
    }
    SLIST_INIT(learn_head);

    return ESP_OK;
}

static void ir_learn_task(void *arg)
{
    int64_t cur_time;
    size_t period;

    ir_learn_common_param_t *learn_param = (ir_learn_common_param_t *) arg;
    rmt_rx_done_event_data_t learn_data;

    learn_param->ctx->running = true;
    learn_param->ctx->learned_count = 0;
    learn_param->ctx->learned_sub = 0;

    rmt_receive(learn_param->ctx->channel_rx,
                learn_param->ctx->rmt_rx.received_symbols, learn_param->ctx->rmt_rx.num_symbols, &ir_learn_rmt_rx_cfg);

    while (1) {

        if ((LEARN_TASK_DELETE & xEventGroupGetBits(learn_param->ctx->learn_event))) {
            learn_param->ctx->running = false;
            vEventGroupDelete(learn_param->ctx->learn_event);
            learn_param->user_cb(IR_LEARN_STATE_EXIT, 0, NULL);
            ir_learn_destroy(learn_param->ctx);
            vTaskDelete(NULL);
        }

        if (xQueueReceive(learn_param->ctx->receive_queue, &learn_data, pdMS_TO_TICKS(500)) == pdPASS) {

            if (learn_data.num_symbols < 5) {
                rmt_receive(learn_param->ctx->channel_rx,
                            learn_param->ctx->rmt_rx.received_symbols, learn_param->ctx->rmt_rx.num_symbols, &ir_learn_rmt_rx_cfg);
                continue;
            }

            cur_time = esp_timer_get_time();
            period   = cur_time - learn_param->ctx->pre_time;
            learn_param->ctx->pre_time = esp_timer_get_time();

            if ((period < 500 * 1000)) {
                learn_param->ctx->learned_sub++;
            } else {
                period = 0;
                learn_param->ctx->learned_sub = 1;
                learn_param->ctx->learned_count++;
            }

            if (learn_param->ctx->learned_count <= learn_param->ctx->learn_count) {
                ir_learn_list_lock(learn_param->ctx, 0);
                if (1 == learn_param->ctx->learned_sub) {
                    ir_learn_add_list_node(&learn_param->ctx->learn_list);
                }

                struct ir_learn_list_t *main_it;
                struct ir_learn_list_t *last = SLIST_FIRST(&learn_param->ctx->learn_list);
                while ((main_it = SLIST_NEXT(last, next)) != NULL) {
                    last = main_it;
                }

                ir_learn_add_sub_list_node(&last->cmd_sub_node, period, &learn_data);
                ir_learn_list_unlock(learn_param->ctx);
                if (learn_param->user_cb) {
                    learn_param->user_cb(learn_param->ctx->learned_count, learn_param->ctx->learned_sub, &last->cmd_sub_node);
                }
            }

            rmt_receive(learn_param->ctx->channel_rx, \
                        learn_param->ctx->rmt_rx.received_symbols, learn_param->ctx->rmt_rx.num_symbols, &ir_learn_rmt_rx_cfg);
        } else if (learn_param->ctx->learned_sub) {
            learn_param->ctx->learned_sub = 0;

            if (learn_param->ctx->learned_count == learn_param->ctx->learn_count) {
                if (learn_param->user_cb) {
                    ir_learn_list_lock(learn_param->ctx, 0);
                    esp_err_t ret = ir_learn_check_valid(&learn_param->ctx->learn_list, &learn_param->ctx->learn_result);
                    ir_learn_list_unlock(learn_param->ctx);
                    if (ESP_OK == ret) {
                        learn_param->user_cb(IR_LEARN_STATE_END, 0, &learn_param->ctx->learn_result);
                    } else {
                        learn_param->user_cb(IR_LEARN_STATE_FAIL, 0, NULL);
                    }
                }
            }
        }
    }
}

esp_err_t ir_learn_restart(ir_learn_handle_t ir_learn_hdl)
{
    IR_LEARN_CHECK(ir_learn_hdl, "learn task not executed!", ESP_ERR_INVALID_ARG);
    ir_learn_t *ctx = (ir_learn_t *)ir_learn_hdl;

    ir_learn_remove_all_symbol(ctx);
    ctx->learned_count = 0;
    return ESP_OK;
}

esp_err_t ir_learn_stop(ir_learn_handle_t *ir_learn_hdl)
{
    IR_LEARN_CHECK(ir_learn_hdl && *ir_learn_hdl, "learn task not executed!", ESP_ERR_INVALID_ARG);
    ir_learn_t *handle = (ir_learn_t *)(*ir_learn_hdl);

    if (handle->running) {
        *ir_learn_hdl = NULL;
        xEventGroupSetBits(handle->learn_event, LEARN_TASK_DELETE);
    } else {
        ESP_LOGI(TAG, "not running");
    }

    return ESP_OK;
}

esp_err_t ir_learn_add_sub_list_node(struct ir_learn_sub_list_head *sub_head, uint32_t timediff, const rmt_rx_done_event_data_t *symbol)
{
    IR_LEARN_CHECK(sub_head, "list pointer can't be NULL!", ESP_ERR_INVALID_ARG);

    esp_err_t ret = ESP_OK;

    struct ir_learn_sub_list_t *item = (struct ir_learn_sub_list_t *)malloc(sizeof(struct ir_learn_sub_list_t));
    IR_LEARN_CHECK_GOTO(item, "no mem to store received RMT symbols", ESP_ERR_NO_MEM, err);

    item->timediff = timediff;
    item->symbols.num_symbols = symbol->num_symbols;
    item->symbols.received_symbols = malloc(symbol->num_symbols * sizeof(rmt_symbol_word_t));
    IR_LEARN_CHECK_GOTO(item->symbols.received_symbols, "no mem to store received RMT symbols", ESP_ERR_NO_MEM, err);

    memcpy(item->symbols.received_symbols, symbol->received_symbols, symbol->num_symbols * sizeof(rmt_symbol_word_t));
    item->next.sle_next = NULL;

    struct ir_learn_sub_list_t *last = SLIST_FIRST(sub_head);
    if (last == NULL) {
        SLIST_INSERT_HEAD(sub_head, item, next);
    } else {
        struct ir_learn_sub_list_t *sub_it;
        while ((sub_it = SLIST_NEXT(last, next)) != NULL) {
            last = sub_it;
        }
        SLIST_INSERT_AFTER(last, item, next);
    }
    return ret;

err:
    if (item) {
        free(item);
    }

    if (item->symbols.received_symbols) {
        free(item->symbols.received_symbols);
        item->symbols.received_symbols = NULL;
    }
    return ret;
}

esp_err_t ir_learn_add_list_node(struct ir_learn_list_head *learn_head)
{
    IR_LEARN_CHECK(learn_head, "list pointer can't be NULL!", ESP_ERR_INVALID_ARG);

    esp_err_t ret = ESP_OK;

    struct ir_learn_list_t *item = (struct ir_learn_list_t *)malloc(sizeof(struct ir_learn_list_t));
    IR_LEARN_CHECK_GOTO(item, "no mem to store received RMT symbols", ESP_ERR_NO_MEM, err);

    SLIST_INIT(&item->cmd_sub_node);
    item->next.sle_next = NULL;

    struct ir_learn_list_t *last = SLIST_FIRST(learn_head);
    if (last == NULL) {
        SLIST_INSERT_HEAD(learn_head, item, next);
    } else {
        struct ir_learn_list_t *it;
        while ((it = SLIST_NEXT(last, next)) != NULL) {
            last = it;
        }
        SLIST_INSERT_AFTER(last, item, next);
    }
    return ret;

err:
    if (item) {
        free(item);
    }
    return ret;
}

static esp_err_t ir_learn_check_duration(
    struct ir_learn_list_head *learn_head,
    struct ir_learn_sub_list_head *result_out,
    uint8_t sub_cmd_offset,
    uint32_t sub_num_symbols,
    uint32_t timediff)
{
    esp_err_t ret = ESP_OK;

    uint32_t duration_average0 = 0;
    uint32_t duration_average1 = 0;
    uint8_t learn_total_num = 0;

    struct ir_learn_list_t *main_it;
    rmt_symbol_word_t *p_symbols, *p_learn_symbols = NULL;
    rmt_rx_done_event_data_t add_symbols;

    add_symbols.num_symbols = sub_num_symbols;
    add_symbols.received_symbols = malloc(sub_num_symbols * sizeof(rmt_symbol_word_t));
    p_learn_symbols = add_symbols.received_symbols;
    IR_LEARN_CHECK_GOTO(p_learn_symbols, "no mem to store received RMT symbols", ESP_ERR_NO_MEM, err);

    for (int i = 0; i < sub_num_symbols; i++) {
        p_symbols = NULL;
        ret = ESP_OK;
        duration_average0 = 0;
        duration_average1 = 0;
        learn_total_num = 0;

        SLIST_FOREACH(main_it, learn_head, next) {

            struct ir_learn_sub_list_t *sub_it = SLIST_FIRST(&main_it->cmd_sub_node);
            for (int j = 0; j < sub_cmd_offset; j++) {
                sub_it = SLIST_NEXT(sub_it, next);
            }

            p_symbols = sub_it->symbols.received_symbols;
            p_symbols += i;

            if (duration_average0) {
                if ((p_symbols->duration0 > (duration_average0 / learn_total_num + RMT_DECODE_MARGIN)) ||
                        (p_symbols->duration0 < (duration_average0 / learn_total_num - RMT_DECODE_MARGIN))) {
                    ret = ESP_FAIL;
                }
            }
            if (duration_average1) {
                if ((p_symbols->duration1 > (duration_average1 / learn_total_num + RMT_DECODE_MARGIN)) ||
                        (p_symbols->duration1 < (duration_average1 / learn_total_num - RMT_DECODE_MARGIN))) {
                    ret = ESP_FAIL;
                }
            }
            IR_LEARN_CHECK_GOTO((ESP_OK == ret), "add cmd duration error", ESP_ERR_INVALID_ARG, err);

            duration_average0 += p_symbols->duration0;
            duration_average1 += p_symbols->duration1;
            learn_total_num++;
        }

        if (learn_total_num && p_symbols) {
            p_learn_symbols->duration0 = duration_average0 / learn_total_num;
            p_learn_symbols->duration1 = duration_average1 / learn_total_num;
            p_learn_symbols->level0 = p_symbols->level1;
            p_learn_symbols->level1 = p_symbols->level0;
            p_learn_symbols++;
        }
    }
    ir_learn_add_sub_list_node(result_out, timediff, &add_symbols);

    if (add_symbols.received_symbols) {
        free(add_symbols.received_symbols);
    }
    return ESP_OK;
err:
    if (add_symbols.received_symbols) {
        free(add_symbols.received_symbols);
    }
    return ESP_FAIL;
}

esp_err_t ir_learn_check_valid(struct ir_learn_list_head *learn_head, struct ir_learn_sub_list_head *result_out)
{
    IR_LEARN_CHECK(learn_head, "list pointer can't be NULL!", ESP_ERR_INVALID_ARG);

    esp_err_t ret = ESP_OK;
    struct ir_learn_list_t *learned_it;
    struct ir_learn_sub_list_t *sub_it;

    uint8_t expect_sub_cmd_num = 0xFF;
    uint8_t sub_cmd_num = 0;
    uint8_t learned_num = 0;

    SLIST_FOREACH(learned_it, learn_head, next) {
        sub_cmd_num = 0;
        learned_num++;
        SLIST_FOREACH(sub_it, &learned_it->cmd_sub_node, next) {
            sub_cmd_num++;
        }
        if (0xFF == expect_sub_cmd_num) {
            expect_sub_cmd_num = sub_cmd_num;
        }
        ESP_LOGI(TAG, "list:%d-%d", learned_num, sub_cmd_num);
        IR_LEARN_CHECK(expect_sub_cmd_num == sub_cmd_num, "cmd num mismatch", ESP_ERR_INVALID_SIZE);
    }

    uint16_t sub_num_symbols;
    uint32_t time_diff;

    for (int i = 0 ; i < sub_cmd_num; i++) {
        sub_num_symbols = 0xFFFF;
        time_diff = 0xFFFF;
        SLIST_FOREACH(learned_it, learn_head, next) {

            struct ir_learn_sub_list_t *sub_item = SLIST_FIRST(&learned_it->cmd_sub_node);
            for (int j = 0; j < i; j++) {
                sub_item = SLIST_NEXT(sub_item, next);
            }
            if (0xFFFF == sub_num_symbols) {
                sub_num_symbols = sub_item->symbols.num_symbols;
            }
            if (0xFFFF == time_diff) {
                time_diff = sub_item->timediff;
            } else {
                time_diff += sub_item->timediff;
            }
            IR_LEARN_CHECK(sub_num_symbols == sub_item->symbols.num_symbols, "sub symbol mismatch", ESP_ERR_INVALID_SIZE);
        }
        ret = ir_learn_check_duration(learn_head, result_out, i, sub_num_symbols, time_diff / learned_num);
        IR_LEARN_CHECK((ESP_OK == ret), "symbol add failed", ESP_ERR_INVALID_SIZE);
    }
    return ESP_OK;
}

esp_err_t ir_learn_new(const ir_learn_cfg_t *cfg, ir_learn_handle_t *handle_out)
{
    ESP_LOGI(TAG, "IR learn Version: %d.%d.%d", IR_LEARN_VER_MAJOR, IR_LEARN_VER_MINOR, IR_LEARN_VER_PATCH);

    BaseType_t res;
    esp_err_t ret = ESP_OK;
    IR_LEARN_CHECK(cfg && handle_out, "invalid argument", ESP_ERR_INVALID_ARG);
    IR_LEARN_CHECK(cfg->learn_count < IR_LEARN_STATE_READY, "learn count too larger", ESP_ERR_INVALID_ARG);

    ir_learn_t *ir_learn_ctx = calloc(1, sizeof(ir_learn_t));
    IR_LEARN_CHECK(ir_learn_ctx, "no mem for ir_learn_ctx", ESP_ERR_NO_MEM);

    ir_learn_common_param_t learn_param = {
        .user_cb = cfg->callback,
        .ctx = ir_learn_ctx,
    };

    rmt_rx_channel_config_t rx_channel_cfg = {
        .clk_src = cfg->clk_src,
        .gpio_num = cfg->learn_gpio,
        .resolution_hz = cfg->resolution,
        .mem_block_symbols = RMT_RX_MEM_BLOCK_SIZE,
        .flags.with_dma = true,
    };
    ret = rmt_new_rx_channel(&rx_channel_cfg, &ir_learn_ctx->channel_rx);
    IR_LEARN_CHECK_GOTO((ESP_OK == ret), "create rmt rx channel failed", ESP_FAIL, err);

    SLIST_INIT(&ir_learn_ctx->learn_list);
    ir_learn_ctx->learn_count = cfg->learn_count;
    ir_learn_ctx->rmt_rx.num_symbols = RMT_RX_MEM_BLOCK_SIZE * 4;
    ir_learn_ctx->rmt_rx.received_symbols = (rmt_symbol_word_t *)heap_caps_malloc(\
                                                                                  ir_learn_ctx->rmt_rx.num_symbols * sizeof(rmt_symbol_word_t), \
                                                                                  MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    IR_LEARN_CHECK_GOTO(ir_learn_ctx->rmt_rx.received_symbols, "no mem to store received RMT symbols", ESP_ERR_NO_MEM, err);

    ir_learn_ctx->receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    IR_LEARN_CHECK_GOTO(ir_learn_ctx->receive_queue, "create rmt receive queue failed", ESP_FAIL, err);

    ir_learn_ctx->rmt_mux = xSemaphoreCreateRecursiveMutex();
    IR_LEARN_CHECK_GOTO(ir_learn_ctx->rmt_mux, "create rmt mux failed", ESP_FAIL, err);

    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = ir_learn_rx_done_callback,
    };
    ret = rmt_rx_register_event_callbacks(ir_learn_ctx->channel_rx, &cbs, ir_learn_ctx);
    IR_LEARN_CHECK_GOTO((ESP_OK == ret), "register rmt rx callback failed", ESP_FAIL, err);

    ret = rmt_enable(ir_learn_ctx->channel_rx);
    IR_LEARN_CHECK_GOTO((ESP_OK == ret), "enable rmt rx channel failed", ESP_FAIL, err);

    ir_learn_ctx->learn_event = xEventGroupCreate();
    IR_LEARN_CHECK_GOTO(ir_learn_ctx->learn_event, "create event group failed", ESP_FAIL, err);

    if (cfg->task_affinity < 0) {
        res = xTaskCreate(ir_learn_task, "ir learn task", cfg->task_stack, &learn_param, cfg->task_priority, NULL);
    } else {
        res = xTaskCreatePinnedToCore(ir_learn_task, "ir learn task", cfg->task_stack, &learn_param, cfg->task_priority, NULL, cfg->task_affinity);
    }
    IR_LEARN_CHECK_GOTO(res == pdPASS, "create ir_learn task fail!", ESP_FAIL, err);

    if (cfg->callback) {
        cfg->callback(IR_LEARN_STATE_READY, 0, NULL);
    }

    *handle_out = ir_learn_ctx;
    return ret;
err:
    if (ir_learn_ctx) {
        ir_learn_destroy(ir_learn_ctx);
    }
    return ret;
}
