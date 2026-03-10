/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"
#include "usb_device_uac.h"
#include "uac_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

// Some resources are lazy allocated in GPTimer driver, the threshold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (-300)
#define UAC_LOOPBACK_BUF_SZ       (16 * 1024)

#if MIC_CHANNEL_NUM && SPEAK_CHANNEL_NUM
typedef struct {
    uint8_t buf[UAC_LOOPBACK_BUF_SZ];
    size_t read_pos;
    size_t write_pos;
    size_t data_len;
    portMUX_TYPE mux;
} uac_loopback_t;

static uac_loopback_t s_loopback = {
    .mux = portMUX_INITIALIZER_UNLOCKED,
};
#endif

static esp_err_t uac_device_output_cb(uint8_t *buf, size_t len, void *arg)
{
#if MIC_CHANNEL_NUM && SPEAK_CHANNEL_NUM
    uac_loopback_t *loopback = (uac_loopback_t *)arg;

    portENTER_CRITICAL(&loopback->mux);
    for (size_t i = 0; i < len; i++) {
        if (loopback->data_len == UAC_LOOPBACK_BUF_SZ) {
            loopback->read_pos = (loopback->read_pos + 1) % UAC_LOOPBACK_BUF_SZ;
            loopback->data_len--;
        }
        loopback->buf[loopback->write_pos] = buf[i];
        loopback->write_pos = (loopback->write_pos + 1) % UAC_LOOPBACK_BUF_SZ;
        loopback->data_len++;
    }
    portEXIT_CRITICAL(&loopback->mux);
#else
    (void)buf;
    (void)len;
    (void)arg;
#endif
    return ESP_OK;
}

static esp_err_t uac_device_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
#if MIC_CHANNEL_NUM && SPEAK_CHANNEL_NUM
    uac_loopback_t *loopback = (uac_loopback_t *)arg;

    memset(buf, 0, len);
    portENTER_CRITICAL(&loopback->mux);
    *bytes_read = len;
    for (size_t i = 0; i < len && loopback->data_len > 0; i++) {
        buf[i] = loopback->buf[loopback->read_pos];
        loopback->read_pos = (loopback->read_pos + 1) % UAC_LOOPBACK_BUF_SZ;
        loopback->data_len--;
    }
    portEXIT_CRITICAL(&loopback->mux);
    return ESP_OK;
#else
    (void)buf;
    (void)len;
    (void)arg;
    *bytes_read = 0;
#endif
    return ESP_OK;
}

TEST_CASE("usb_device_uac_test", "[usb_device_uac]")
{
    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .cb_ctx = NULL,
    };
    uac_device_init(&config);
}

#if MIC_CHANNEL_NUM && SPEAK_CHANNEL_NUM
TEST_CASE("usb_device_uac_loopback_test", "[usb_device_uac]")
{
    memset(&s_loopback, 0, sizeof(s_loopback));
    s_loopback.mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .cb_ctx = &s_loopback,
    };
    uac_device_init(&config);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    //   _   _  _   ___   _____ ___ ___ _____
    //  | | | |/_\ / __| |_   _| __/ __|_   _|
    //  | |_| / _ \ (__    | | | _|\__ \ | |
    //   \___/_/ \_\___|   |_| |___|___/ |_|
    printf("  _   _  _   ___   _____ ___ ___ _____ \n");
    printf(" | | | |/_\\ / __| |_   _| __/ __|_   _|\n");
    printf(" | |_| / _ \\ (__    | | | _|\\__ \\ | |  \n");
    printf("  \\___/_/ \\_\\___|   |_| |___|___/ |_|  \n");
    unity_run_menu();
}
