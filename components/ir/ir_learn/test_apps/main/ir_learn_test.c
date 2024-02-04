/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* C includes */
#include <stdio.h>
#include <string.h>

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

/* ESP32 includes */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "unity.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

/* IR learn includes */
#include "ir_learn.h"
#include "ir_encoder.h"

static const int NEED_DELETE    = BIT0;
static const int DELETE_END     = BIT1;

#define TEST_MEMORY_LEAK_THRESHOLD      (-400)

#define IR_RESOLUTION_HZ                1000000 // 1MHz resolution, 1 tick = 1us
#define IR_TX_GPIO_NUM                  GPIO_NUM_39
#define IR_RX_GPIO_NUM                  GPIO_NUM_38
#define IR_RX_CTRL_GPIO_NUM             GPIO_NUM_44
#define IR_TX_DETECT_IO                 GPIO_NUM_0

#define IR_LEARN_COUNT                  4
#define IR_OUT_SYMBOL_LEN               20

static const char *TAG = "ir_learn_test";

static ir_learn_handle_t handle = NULL;                 /**< IR learn handle */
static struct ir_learn_sub_list_head ir_test_result;    /**< IR learn test result */

const uint16_t ir_learn_tx_map[] = {
    9000,   4500,   550,    1660,   550,    550,    550,    550,    550,    1660,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    1660,   550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    1660,   550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    1660,
    550,    550,    550,    1660,   550,    550,    550,    550,    550,    1660,
    550,    550,    610,    20194,

    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    1660,   550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    1660,   550,    550,
    550,    550,    550,    1660,   610,    40455,

    9000,   4500,   550,    1660,   550,    550,    550,    550,    550,    1660,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    1660,   550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    1660,   550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    1660,
    550,    1660,   550,    1660,   550,    550,    550,    550,    550,    1660,
    550,    550,    610,    20194,

    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    550,    550,    550,
    550,    550,    550,    550,    550,    550,    550,    1660,   550,    1660,
    550,    1660,   550,    550,    550,    0,
};

static void detect_to_handler(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)(arg);
    BaseType_t task_woken = pdFALSE;

    if (gpio_get_level(IR_TX_DETECT_IO)) {
        if (!SLIST_EMPTY(&ir_test_result)) {
            esp_rom_printf(DRAM_STR("send rmt out\r\n"));
            xQueueSendFromISR(queue, &ir_test_result, &task_woken);
        } else {
            esp_rom_printf(DRAM_STR("ir learn cmd empty\r\n"));
        }
    }

    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void ir_learn_save_result(struct ir_learn_sub_list_head *data_save, struct ir_learn_sub_list_head *data_src)
{
    assert(data_src && "rmt_symbols is null");

    struct ir_learn_sub_list_t *sub_it;
    struct ir_learn_sub_list_t *last;

    last = SLIST_FIRST(data_src);
    while ((sub_it = SLIST_NEXT(last, next)) != NULL) {
        last = sub_it;
    }
    ir_learn_add_sub_list_node(data_save, last->timediff, &last->symbols);

    return;
}

static void ir_learn_test_tx_raw(struct ir_learn_sub_list_head *rmt_out)
{
    rmt_tx_channel_config_t tx_channel_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = 128,
        .trans_queue_depth = 4,
        .gpio_num = IR_TX_GPIO_NUM,
    };
    rmt_channel_handle_t tx_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));

    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.33,
        .frequency_hz = 38000, // 38KHz
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

    rmt_transmit_config_t transmit_cfg = {
        .loop_count = 0, // no loop
    };

    ir_encoder_config_t raw_encoder_cfg = {
        .resolution = IR_RESOLUTION_HZ,
    };
    rmt_encoder_handle_t raw_encoder = NULL;
    ESP_ERROR_CHECK(ir_encoder_new(&raw_encoder_cfg, &raw_encoder));

    ESP_ERROR_CHECK(rmt_enable(tx_channel));

    struct ir_learn_sub_list_t *sub_it;
    SLIST_FOREACH(sub_it, rmt_out, next) {
        vTaskDelay(pdMS_TO_TICKS(sub_it->timediff / 1000));

        rmt_symbol_word_t *rmt_symbols = sub_it->symbols.received_symbols;
        size_t symbol_num = sub_it->symbols.num_symbols;

        ESP_ERROR_CHECK(rmt_transmit(tx_channel, raw_encoder, rmt_symbols, symbol_num, &transmit_cfg));
        rmt_tx_wait_all_done(tx_channel, -1);
    }

    rmt_disable(tx_channel);
    rmt_del_channel(tx_channel);
    raw_encoder->del(raw_encoder);
}

static void ir_learn_manual_tx_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    struct ir_learn_sub_list_head tx_data;
    QueueHandle_t tx_queue = NULL;

    const gpio_config_t io_enable_cfg = {
        .pin_bit_mask = BIT64(IR_RX_CTRL_GPIO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = true,
    };

    const gpio_config_t io_detect_cfg = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = BIT64(IR_TX_DETECT_IO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    EventGroupHandle_t event = (EventGroupHandle_t)(arg);
    ESP_GOTO_ON_FALSE(event, ESP_FAIL, err, TAG, "Pointer of event is invalid");

    tx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    ESP_GOTO_ON_FALSE(tx_queue, ESP_FAIL, err, TAG, "Queue creation failed");

    ESP_ERROR_CHECK(gpio_config(&io_enable_cfg));
    gpio_set_level(IR_RX_CTRL_GPIO_NUM, 0);

    ESP_ERROR_CHECK(gpio_config(&io_detect_cfg));
    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(gpio_isr_handler_add(IR_TX_DETECT_IO, detect_to_handler, tx_queue));

    while (1) {
        if ((NEED_DELETE & xEventGroupGetBits(event))) {
            break;
        }

        if (xQueueReceive(tx_queue, &tx_data, pdMS_TO_TICKS(500)) == pdPASS) {
            ir_learn_test_tx_raw(&tx_data);
            xEventGroupSetBits(event, NEED_DELETE);
        }
    }

err:
    ESP_LOGI(TAG, "manual tx task exit, ret:%d", ret);

    xEventGroupSetBits(event, DELETE_END);
    if (tx_queue) {
        vQueueDelete(tx_queue);
    }
    gpio_isr_handler_remove(0);
    gpio_uninstall_isr_service();

    vTaskDelete(NULL);
}

static void ir_learn_auto_tx_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    int count = 0;
    size_t symbol_num;
    size_t total_symbol_num;
    size_t duration_part;
    rmt_symbol_word_t *out_symbols = NULL;

    static struct ir_learn_sub_list_head ir_auto_test_forward;      /**< IR learn auto test send data */

    const gpio_config_t io_enable_cfg = {
        .pin_bit_mask = BIT64(IR_RX_CTRL_GPIO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = true,
    };

    EventGroupHandle_t event = (EventGroupHandle_t)(arg);
    ESP_GOTO_ON_FALSE(event, ESP_ERR_INVALID_ARG, err, TAG, "Pointer of event is invalid");

    ESP_ERROR_CHECK(gpio_config(&io_enable_cfg));
    gpio_set_level(IR_RX_CTRL_GPIO_NUM, 0);

    total_symbol_num = 0;
    symbol_num = (sizeof(ir_learn_tx_map)) / (sizeof(uint16_t)) / 2;

    out_symbols = (rmt_symbol_word_t *)malloc(symbol_num * sizeof(rmt_symbol_word_t));
    ESP_GOTO_ON_FALSE(out_symbols, ESP_ERR_NO_MEM, err, TAG, "create tx symbols failed");

    for (int i = 0; i < (sizeof(ir_learn_tx_map)) / (sizeof(uint16_t)) / 2; i++) {
        out_symbols[total_symbol_num].duration0 = ir_learn_tx_map[2 * i + 0];
        out_symbols[total_symbol_num].level0 = 1;

        if (ir_learn_tx_map[2 * i + 1] > 0x7FFF) {
            duration_part = ir_learn_tx_map[2 * i + 1] / 0x7FFF + ((ir_learn_tx_map[2 * i + 1] % 0x7FFF) ? 1 : 0);
            symbol_num += duration_part / 2;
            ESP_LOGI(TAG, "[%d] level-1 overflow:[%d], duration part:[%d], need add symbol:[%d -> %d]", \
                     i,
                     ir_learn_tx_map[2 * i + 1],
                     duration_part,
                     duration_part / 2,
                     symbol_num);

            out_symbols = (rmt_symbol_word_t *)realloc(out_symbols, symbol_num * sizeof(rmt_symbol_word_t));
            ESP_GOTO_ON_FALSE(out_symbols, ESP_ERR_NO_MEM, err, TAG, "realloc tx symbols failed");

            out_symbols[total_symbol_num].duration1 = 0x7FFF;
            out_symbols[total_symbol_num].level1 = 0;
            total_symbol_num++;

            for (int j = 0; j < duration_part / 2; j++) {
                if (j == (duration_part / 2 - 1)) {
                    out_symbols[total_symbol_num].duration0 = (ir_learn_tx_map[2 * i + 1] - 0x7FFF - 0x7FFF * j * 2) / 2;
                    out_symbols[total_symbol_num].level0 = 0;

                    out_symbols[total_symbol_num].duration1 = (ir_learn_tx_map[2 * i + 1] - 0x7FFF - 0x7FFF * j * 2) / 2;
                    out_symbols[total_symbol_num].level1 = 0;
                } else {
                    out_symbols[total_symbol_num].duration0 = 0x7FFF;
                    out_symbols[total_symbol_num].level0 = 0;

                    out_symbols[total_symbol_num].duration1 = 0x7FFF;
                    out_symbols[total_symbol_num].level1 = 0;
                }
                total_symbol_num++;
            }
            continue;
        } else {
            out_symbols[total_symbol_num].duration1 = ir_learn_tx_map[2 * i + 1];
            out_symbols[total_symbol_num].level1 = 0;
        }
        total_symbol_num++;
    }

    rmt_rx_done_event_data_t symbol_package = {
        .received_symbols = out_symbols,
        .num_symbols = total_symbol_num,
    };
    ir_learn_add_sub_list_node(&ir_auto_test_forward, 0, &symbol_package);

    while (1) {
        if ((NEED_DELETE & xEventGroupGetBits(event))) {
            break;
        }

        if (count < IR_LEARN_COUNT) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            ir_learn_test_tx_raw(&ir_auto_test_forward);
            count++;
        } else {
            xEventGroupSetBits(event, NEED_DELETE);
        }
    }

err:
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "auto tx task exit, ret:%d", ret);

    if (out_symbols) {
        free(out_symbols);
    }
    ir_learn_clean_sub_data(&ir_auto_test_forward);
    xEventGroupSetBits(event, DELETE_END);
    vTaskDelete(NULL);
}

static void ir_learn_manual_send_cb(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
{
    switch (state) {
    case IR_LEARN_STATE_READY:
        ESP_LOGI(TAG, "IR Learn ready");
        break;
    case IR_LEARN_STATE_EXIT:
        ESP_LOGI(TAG, "IR Learn exit");
        break;
    case IR_LEARN_STATE_END:
        ESP_LOGI(TAG, "IR Learn end");
        ir_learn_save_result(&ir_test_result, data);
        ir_learn_print_raw(&ir_test_result);
        ir_learn_stop(&handle);
        break;
    case IR_LEARN_STATE_FAIL:
        ESP_LOGE(TAG, "IR Learn faield, retry");
        ir_learn_restart(handle);
        break;
    case IR_LEARN_STATE_STEP:
    default:
        ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
        break;
    }
    return;
}

static void ir_learn_manual_keep_learn_cb(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
{
    switch (state) {
    case IR_LEARN_STATE_READY:
        ESP_LOGI(TAG, "IR Learn ready");
        break;
    case IR_LEARN_STATE_EXIT:
        ESP_LOGI(TAG, "IR Learn exit");
        break;
    case IR_LEARN_STATE_END:
        ESP_LOGI(TAG, "IR Learn end");
        ir_learn_print_raw(data);
        ir_learn_restart(handle);
        break;
    case IR_LEARN_STATE_FAIL:
        ESP_LOGI(TAG, "IR Learn faield, retry");
        ir_learn_restart(handle);
        break;
    case IR_LEARN_STATE_STEP:
    default:
        ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
        break;
    }
    return;
}

static void ir_learn_auto_learn_cb(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
{
    switch (state) {
    case IR_LEARN_STATE_READY:
        ESP_LOGI(TAG, "IR Learn ready");
        break;
    case IR_LEARN_STATE_EXIT:
        ESP_LOGI(TAG, "IR Learn exit");
        break;
    case IR_LEARN_STATE_END:
        ESP_LOGI(TAG, "IR Learn end");
        ir_learn_save_result(&ir_test_result, data);
        ir_learn_print_raw(data);
        ir_learn_stop(&handle);
        break;
    case IR_LEARN_STATE_FAIL:
        ESP_LOGI(TAG, "IR Learn faield, retry");
        break;
    case IR_LEARN_STATE_STEP:
    default:
        ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
        break;
    }
    return;
}

static esp_err_t ir_learn_manual_test(ir_learn_result_cb cb)
{
    esp_err_t ret = ESP_OK;

    EventGroupHandle_t learn_event_group = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(learn_event_group, ESP_FAIL, IR_LEARN_END, TAG, "create event group failed");

    xTaskCreate(ir_learn_manual_tx_task, "Manual tx task", 1024 * 4, (void *)learn_event_group, 10, NULL);

    const ir_learn_cfg_t config = {
        .learn_count = IR_LEARN_COUNT,
        .learn_gpio = IR_RX_GPIO_NUM,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution = IR_RESOLUTION_HZ,

        .task_stack = 4096,
        .task_priority = 5,
        .task_affinity = 1,
        .callback = cb,
    };

    ESP_ERROR_CHECK(ir_learn_new(&config, &handle));
    xEventGroupWaitBits(learn_event_group, DELETE_END, true, true, portMAX_DELAY);

    if (handle) {
        ir_learn_stop(&handle);
    }
    ir_learn_clean_sub_data(&ir_test_result);
    vEventGroupDelete(learn_event_group);

IR_LEARN_END:
    vTaskDelay(pdMS_TO_TICKS(1000));
    return ret;
}

static esp_err_t ir_learn_auto_test(ir_learn_result_cb cb)
{
    esp_err_t ret = ESP_OK;

    EventGroupHandle_t learn_event_group = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(learn_event_group, ESP_FAIL, IR_LEARN_END, TAG, "create event group failed");

    xTaskCreate(ir_learn_auto_tx_task, "Auto tx task", 1024 * 4, (void *)learn_event_group, 10, NULL);

    const ir_learn_cfg_t config = {
        .learn_count = IR_LEARN_COUNT,
        .learn_gpio = IR_RX_GPIO_NUM,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution = IR_RESOLUTION_HZ,

        .task_stack = 4096,
        .task_priority = 5,
        .task_affinity = 1,
        .callback = cb,
    };

    ESP_ERROR_CHECK(ir_learn_new(&config, &handle));
    xEventGroupWaitBits(learn_event_group, DELETE_END, true, true, portMAX_DELAY);

    ret = SLIST_EMPTY(&ir_test_result) ? ESP_FAIL : ESP_OK;

    if (handle) {
        ir_learn_stop(&handle);
    }
    ir_learn_clean_sub_data(&ir_test_result);
    vEventGroupDelete(learn_event_group);

IR_LEARN_END:
    vTaskDelay(pdMS_TO_TICKS(1000));
    return ret;
}

TEST_CASE("IR learn and send test", "[IR][IOT][Manual]")
{
    TEST_ASSERT(ESP_OK == ir_learn_manual_test(ir_learn_manual_send_cb));
}

TEST_CASE("IR keep learn test", "[IR][IOT][Manual]")
{
    TEST_ASSERT(ESP_OK == ir_learn_manual_test(ir_learn_manual_keep_learn_cb));
}

TEST_CASE("IR self-receiver test", "[IR][IOT][Auto]")
{
    TEST_ASSERT(ESP_OK == ir_learn_auto_test(ir_learn_auto_learn_cb));
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("IR learn TEST \n");
    unity_run_menu();
}
