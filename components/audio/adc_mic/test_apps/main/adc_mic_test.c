/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_log.h"
#include "adc_mic.h"
#include "esp_codec_dev.h"

static const char *TAG = "adc_mic_test";

TEST_CASE("adc_mic_test_mono", "[adc_mic][mono]")
{
    audio_codec_adc_cfg_t cfg = DEFAULT_AUDIO_CODEC_ADC_MONO_CFG(ADC_CHANNEL_0, 16000);
    const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .data_if = adc_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&codec_dev_cfg);
    TEST_ASSERT(dev);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };

    int ret = esp_codec_dev_open(dev, &fs);
    TEST_ASSERT(ret == 0);

    uint16_t *audio_buffer = malloc(16000 * sizeof(uint16_t));
    assert(audio_buffer);
    while (1) {
        ret = esp_codec_dev_read(dev, audio_buffer, sizeof(uint16_t) * 16000);
        ESP_LOGI(TAG, "ret: %d, esp_codec_dev_read len: %d\n", ret, sizeof(uint16_t) * 16000);
    }
}

TEST_CASE("adc_mic_test memory leak", "[adc_mic][memory_leak]")
{
    audio_codec_adc_cfg_t cfg = DEFAULT_AUDIO_CODEC_ADC_MONO_CFG(ADC_CHANNEL_0, 16000);
    const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .data_if = adc_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&codec_dev_cfg);
    TEST_ASSERT(dev);
    esp_codec_dev_delete(dev);
    audio_codec_delete_data_if(adc_if);
}

typedef struct {
    esp_codec_dev_handle_t dev;
    esp_codec_dev_sample_info_t fs;
    TaskHandle_t partner;
    int loops;
} cross_task_ctx_t;

static void task_open_then_wait(void *arg)
{
    cross_task_ctx_t *ctx = (cross_task_ctx_t *)arg;

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (int i = 0; i < ctx->loops; ++i) {
        TEST_ASSERT_EQUAL(0, esp_codec_dev_open(ctx->dev, &ctx->fs));
        xTaskNotifyGive(ctx->partner);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    vTaskDelete(NULL);
}

static void task_wait_then_close(void *arg)
{
    cross_task_ctx_t *ctx = (cross_task_ctx_t *)arg;

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (int i = 0; i < ctx->loops; ++i) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        TEST_ASSERT_EQUAL(0, esp_codec_dev_close(ctx->dev));
        xTaskNotifyGive(ctx->partner);
    }
    vTaskDelete(NULL);
}

TEST_CASE("adc_mic_cross_task_enable_disable", "[adc_mic][cross_task]")
{
    audio_codec_adc_cfg_t cfg = DEFAULT_AUDIO_CODEC_ADC_MONO_CFG(ADC_CHANNEL_0, 16000);
    const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .data_if = adc_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&codec_dev_cfg);
    TEST_ASSERT(dev);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };

    cross_task_ctx_t a = {
        .dev = dev,
        .fs = fs,
        .partner = NULL,
        .loops = 10,
    };
    cross_task_ctx_t b = a;

    TaskHandle_t taskA = NULL;
    TaskHandle_t taskB = NULL;
    xTaskCreate(task_open_then_wait, "t_open", 3072, &a, 3, &taskA);
    xTaskCreate(task_wait_then_close, "t_close", 3072, &b, 3, &taskB);
    a.partner = taskB;
    b.partner = taskA;

    xTaskNotifyGive(taskA);
    xTaskNotifyGive(taskB);

    while (eTaskGetState(taskA) != eDeleted || eTaskGetState(taskB) != eDeleted) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_codec_dev_delete(dev);
    audio_codec_delete_data_if(adc_if);
}
