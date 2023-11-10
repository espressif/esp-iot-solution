/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
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

static const int NEED_DELETE = BIT0;
static const int DELETE_END = BIT1;
static EventGroupHandle_t learn_event_group;

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

#define NEC_IR_RESOLUTION_HZ 1000000 // 1MHz resolution, 1 tick = 1us

/** ESP33_C3*/
// #define NEC_IR_TX_GPIO_NUM          4
// #define NEC_IR_RX_GPIO_NUM          3
// #define NEC_IR_DETECT_NUM           GPIO_NUM_9

/** ESP33_S3*/
#define NEC_IR_TX_GPIO_NUM          39
#define NEC_IR_RX_GPIO_NUM          38
#define NEC_IR_DETECT_NUM           GPIO_NUM_0

#define NEC_IR_RX_CTRL_GPIO_NUM     44

static const char *TAG = "ir_learn_test";

static QueueHandle_t rmt_out_queue = NULL;
static ir_learn_handle_t ir_learn_handle = NULL;

/**
 * @brief result
 *
 */
struct ir_learn_sub_list_head ir_leran_data_cmd;
static bool rmt_enable_flag = false;

void boot_send_btn_handler(void *arg)
{
    if (gpio_get_level(NEC_IR_DETECT_NUM)) {
        if (1) {
            // if (!SLIST_EMPTY(&ir_leran_data_cmd)) {
            esp_rom_printf(DRAM_STR("send rmt out\r\n"));
            if(false == rmt_enable_flag){
                xQueueSendFromISR(rmt_out_queue, &ir_leran_data_cmd, 0);
            }
        } else {
            esp_rom_printf(DRAM_STR("ir learn cmd empty\r\n"));
        }
    }
}

esp_err_t ir_lean_test_send_detect(void)
{
    gpio_config_t io_conf_key;
    io_conf_key.intr_type = GPIO_INTR_POSEDGE;
    io_conf_key.mode = GPIO_MODE_INPUT;
    io_conf_key.pin_bit_mask = 1ULL << NEC_IR_DETECT_NUM;
    io_conf_key.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf_key.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf_key));

    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(gpio_isr_handler_add(NEC_IR_DETECT_NUM, boot_send_btn_handler, NULL));
    return ESP_OK;
}

static void ir_learn_test_save_result(struct ir_learn_sub_list_head *data_save, struct ir_learn_sub_list_head *data_src)
{
    assert(data_src && "rmt_symbols is null");

    ir_learn_sub_list_t *sub_it;
    ir_learn_sub_list_t *last;

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
        .resolution_hz = NEC_IR_RESOLUTION_HZ,
        .mem_block_symbols = 128,
        .trans_queue_depth = 4,
        .gpio_num = NEC_IR_TX_GPIO_NUM,
        // .flags.invert_out = true,
    };
    rmt_channel_handle_t tx_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));

    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.33,
        .frequency_hz = 38000, // 38KHz
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0, // no loop
    };

    ir_encoder_config_t nec_encoder_cfg = {
        .resolution = NEC_IR_RESOLUTION_HZ,
    };
    rmt_encoder_handle_t nec_encoder = NULL;
    ESP_ERROR_CHECK(ir_encoder_new(&nec_encoder_cfg, &nec_encoder));

    ESP_LOGE(TAG, "rmt_enable");
    ESP_ERROR_CHECK(rmt_enable(tx_channel));

#if 0
    ir_learn_sub_list_t *sub_it;
    SLIST_FOREACH(sub_it, rmt_out, next) {
        ESP_LOGI(TAG, "timediff:%" PRIu32 " ms, symbols:%03u, duration0:%02X[%d]. duration1:%02X[%d]",

                 sub_it->timediff / 1000,
                 sub_it->symbols.num_symbols,
                 sub_it->symbols.received_symbols->duration0,
                 sub_it->symbols.received_symbols->level0,
                 sub_it->symbols.received_symbols->duration1,
                 sub_it->symbols.received_symbols->level1);

        vTaskDelay(pdMS_TO_TICKS(sub_it->timediff / 1000));

        rmt_symbol_word_t *rmt_nec_symbols = sub_it->symbols.received_symbols;
        size_t symbol_num = sub_it->symbols.num_symbols;

        ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, rmt_nec_symbols, symbol_num, &transmit_config));
        rmt_tx_wait_all_done(tx_channel, -1);
    }
#else

    #define SYMBOL_SEND     50
    static rmt_symbol_word_t received_symbols[SYMBOL_SEND];

    for(int i = 0; i< SYMBOL_SEND; i++){
        received_symbols[i].duration0 = 100 + 100*(i%20);
        received_symbols[i].level0 = 1;

        received_symbols[i].duration1 = 1100 + 100*(i%20);
        received_symbols[i].level1 = 0;
    };

    // ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, &received_symbols, sizeof(received_symbols), &transmit_config));

    size_t symbol_num = SYMBOL_SEND;
    rmt_symbol_word_t *rmt_nec_symbols = &received_symbols[0];
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, rmt_nec_symbols, symbol_num*4, &transmit_config));

    rmt_tx_wait_all_done(tx_channel, -1);
    ESP_LOGI(TAG, "rmt_tx_wait_all_done,symbol_num:%d", symbol_num);
#endif
    rmt_disable(tx_channel);
    rmt_del_channel(tx_channel);
    nec_encoder->del(nec_encoder);
}

void ir_learn_test_tx_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    struct ir_learn_sub_list_head tx_data;

    rmt_out_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    ESP_GOTO_ON_FALSE(rmt_out_queue, ESP_ERR_NO_MEM, err, TAG, "receive queue creation failed");

    ir_lean_test_send_detect();

    while (1) {
        if ((NEED_DELETE & xEventGroupGetBits(learn_event_group))) {
            xEventGroupSetBits(learn_event_group, DELETE_END);
            if (rmt_out_queue) {
                vQueueDelete(rmt_out_queue);
                rmt_out_queue = NULL;
            }
            gpio_isr_handler_remove(0);
            gpio_uninstall_isr_service();
            vTaskDelete(NULL);
        }

        if (xQueueReceive(rmt_out_queue, &tx_data, pdMS_TO_TICKS(500)) == pdPASS) {
            rmt_enable_flag = true;
            ir_learn_test_tx_raw(&tx_data);
            rmt_enable_flag = false;
            // xEventGroupSetBits(learn_event_group, NEED_DELETE);
        }
    }
err:
    ESP_LOGI(TAG, "ir_learn_test_tx_task exit:%d", ret);
    vTaskDelete(NULL);
}

void ir_learn_learn_send_callback(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
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
        // ir_learn_test_save_result(&ir_leran_data_cmd, data);
        // ir_learn_stop(&ir_learn_handle);
        ir_learn_restart(ir_learn_handle);
        break;
    case IR_LEARN_STATE_FAIL:
        ESP_LOGE(TAG, "IR Learn faield, retry");
        ir_learn_restart(ir_learn_handle);
        break;
    case IR_LEARN_STATE_STEP:
    default:
        ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
        ir_learn_print_raw(data);
        break;
    }
    return;
}

void ir_learn_keep_learn_callback(ir_learn_state_t state, uint8_t sub_step, struct ir_learn_sub_list_head *data)
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
        ir_learn_restart(ir_learn_handle);
        break;
    case IR_LEARN_STATE_FAIL:
        ESP_LOGI(TAG, "IR Learn faield, retry");
        ir_learn_restart(ir_learn_handle);
        break;
    case IR_LEARN_STATE_STEP:
    default:
        ESP_LOGI(TAG, "IR Learn step:[%d][%d]", state, sub_step);
        break;
    }
    return;
}

esp_err_t ir_learn_test(ir_learn_result_cb cb)
{
    esp_err_t ret = ESP_OK;

    learn_event_group = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(learn_event_group, ESP_FAIL, IR_LEARN_END, TAG, "create event group failed");

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = BIT64(NEC_IR_RX_CTRL_GPIO_NUM);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = true;
    gpio_config(&io_conf);
    gpio_set_level(NEC_IR_RX_CTRL_GPIO_NUM, 0); // enable IR TX

    xTaskCreate(ir_learn_test_tx_task, "ir_learn_test_tx_task", 1024 * 8, NULL, 10, NULL);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ir_learn_cfg_t ir_learn_config = {
        .learn_count = 4,
        .learn_gpio = NEC_IR_RX_GPIO_NUM,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution = NEC_IR_RESOLUTION_HZ,

        .task_stack = 4096,
        .task_priority = 5,
        .task_affinity = 1,
        .callback = cb,
    };

    ESP_ERROR_CHECK(ir_learn_new(&ir_learn_config, &ir_learn_handle));
    xEventGroupWaitBits(learn_event_group, DELETE_END, true, true, portMAX_DELAY);

    ir_learn_stop(&ir_learn_handle);
    ir_learn_clean_sub_data(&ir_leran_data_cmd);
    vEventGroupDelete(learn_event_group);

IR_LEARN_END:
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Process end");
    return ret;
}

TEST_CASE("IR learn and send test", "[IR][IOT]")
{
    ir_learn_test(ir_learn_learn_send_callback);
}

TEST_CASE("IR keep learn test", "[IR][IOT]")
{
    ir_learn_test(ir_learn_keep_learn_callback);
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
    // unity_run_menu();
    ir_learn_test(ir_learn_learn_send_callback);
}
