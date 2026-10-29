/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "iot_usbh_cdc.h"
#include "iot_usbh_modem.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_heap_caps.h"

static const char *TAG = "test_usbh_modem";

#define TEST_MEMORY_LEAK_THRESHOLD (-20000)
#define TEST_TIMEOUT_MS (10000)
#define TEST_SHORT_TIMEOUT_MS (100)

static const usb_modem_id_t usb_modem_id_list[] = {
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x1782, 0x4d11}, 2, -1, "China Mobile, ML302/Fibocom, MC610-EU"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x1E0E, 0x9011}, 5, -1, "SIMCOM, A7600C1/SIMCOM, A7670E"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x1E0E, 0x9205}, 2, -1, "SIMCOM, SIM7080G"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x2CB7, 0x0D01}, 2, -1, "Fibocom, LE270-CN"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x2C7C, 0x6001}, 4, -1, "Quectel, EC600N-CN"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x2C7C, 0x0125}, 2, -1, "Quectel, EC20"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x19D1, 0x1003}, 2, -1, "YUGE, YM310 X09"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x19D1, 0x0001}, 2, -1, "Luat, Air780E"},
    {.match_id = {0}}, // End of list marker
};

static void install()
{
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = 5,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };

    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_install(&config));
}

static void uninstall()
{
    TEST_ASSERT_EQUAL(ESP_OK, usbh_cdc_driver_uninstall());
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

// ==================== 边界测试用例 ====================

TEST_CASE("usbh modem install with NULL config", "[iot_usbh_modem][auto][boundary]")
{
    install();
    esp_err_t ret = usbh_modem_install(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    uninstall();
}

TEST_CASE("usbh modem install with NULL modem_id_list", "[iot_usbh_modem][auto][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = NULL,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    esp_err_t ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    uninstall();
}

TEST_CASE("usbh modem install with zero tx buffer size", "[iot_usbh_modem][auto][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 0,
        .at_rx_buffer_size = 256,
    };
    esp_err_t ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    uninstall();
}

TEST_CASE("usbh modem install with zero rx buffer size", "[iot_usbh_modem][auto][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 0,
    };
    esp_err_t ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    uninstall();
}

TEST_CASE("usbh modem install with minimal buffer sizes", "[iot_usbh_modem][auto][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 1,
        .at_rx_buffer_size = 1,
    };
    esp_err_t ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
    vTaskDelay(pdMS_TO_TICKS(1000));
    usbh_modem_uninstall();
    uninstall();
}

TEST_CASE("usbh modem double install", "[iot_usbh_modem][auto][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    esp_err_t ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // 尝试重复安装
    ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    usbh_modem_uninstall();
    uninstall();
}

TEST_CASE("usbh modem uninstall without install", "[iot_usbh_modem][auto][boundary]")
{
    install();
    // 未安装就卸载应该返回错误
    esp_err_t ret = usbh_modem_uninstall();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    uninstall();
}

TEST_CASE("usbh modem get_netif without install", "[iot_usbh_modem][auto][boundary]")
{
    install();
    esp_netif_t *netif = usbh_modem_get_netif();
    TEST_ASSERT_NULL(netif);
    uninstall();
}

TEST_CASE("usbh modem get_atparser without install", "[iot_usbh_modem][auto][boundary]")
{
    install();
    at_handle_t atparser = usbh_modem_get_atparser();
    TEST_ASSERT_NULL(atparser);
    uninstall();
}

TEST_CASE("usbh modem ppp_auto_connect without install", "[iot_usbh_modem][auto][boundary]")
{
    install();
    esp_err_t ret = usbh_modem_ppp_auto_connect(true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    uninstall();
}

TEST_CASE("usbh modem ppp_start without install", "[iot_usbh_modem][auto][boundary]")
{
    install();
    esp_err_t ret = usbh_modem_ppp_start(pdMS_TO_TICKS(TEST_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    uninstall();
}

TEST_CASE("usbh modem ppp_stop without install", "[iot_usbh_modem][auto][boundary]")
{
    install();
    esp_err_t ret = usbh_modem_ppp_stop();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
    uninstall();
}

TEST_CASE("usbh modem ppp_start with auto_connect enabled", "[iot_usbh_modem][auto][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_install(&modem_config));

    // 默认auto_connect是启用的，手动启动应该失败
    esp_err_t ret = usbh_modem_ppp_start(pdMS_TO_TICKS(TEST_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    usbh_modem_uninstall();
    uninstall();
}

TEST_CASE("usbh modem ppp_stop when not running", "[iot_usbh_modem][auto][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_install(&modem_config));

    // 禁用自动连接
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_ppp_auto_connect(false));
    vTaskDelay(pdMS_TO_TICKS(2000)); // 等待进入IDLE状态

    // PPP未运行时停止应该失败
    esp_err_t ret = usbh_modem_ppp_stop();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    usbh_modem_uninstall();
    uninstall();
}

TEST_CASE("usbh modem ppp_start with zero timeout", "[iot_usbh_modem][boundary]")
{
    install();
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_install(&modem_config));
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_ppp_auto_connect(false));

    // 零超时应该立即返回超时错误
    esp_err_t ret = usbh_modem_ppp_start(0);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, ret);
    vTaskDelay(pdMS_TO_TICKS(2000));
    usbh_modem_uninstall();
    uninstall();
}

TEST_CASE("usbh modem install with empty modem_id_list", "[iot_usbh_modem][boundary]")
{
    install();
    // 创建一个只有结束标记的空列表
    static const usb_modem_id_t empty_list[] = {
        {.match_id = {0}}, // End of list marker
    };
    usbh_modem_config_t modem_config = {
        .modem_id_list = empty_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    // 空列表应该允许安装，但无法匹配设备
    esp_err_t ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    vTaskDelay(pdMS_TO_TICKS(1000));
    usbh_modem_uninstall();
    uninstall();
}

// ==================== 功能测试用例 ====================

TEST_CASE("usbh modem basic install uninstall", "[iot_usbh_modem][function]")
{
    install();

    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    esp_err_t ret = usbh_modem_install(&modem_config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "Modem installed successfully");

    vTaskDelay(pdMS_TO_TICKS(2000));

    ret = usbh_modem_uninstall();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "Modem uninstalled successfully");

    uninstall();
}

TEST_CASE("usbh modem get_netif after install", "[iot_usbh_modem][function]")
{
    install();

    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_install(&modem_config));

    esp_netif_t *netif = usbh_modem_get_netif();
    TEST_ASSERT_NOT_NULL(netif);
    ESP_LOGI(TAG, "Got netif: %p", netif);

    usbh_modem_uninstall();
    uninstall();
}

TEST_CASE("usbh modem get_atparser after install", "[iot_usbh_modem][function]")
{
    install();

    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_install(&modem_config));

    // 等待设备连接（如果有设备）
    vTaskDelay(pdMS_TO_TICKS(3000));

    at_handle_t atparser = usbh_modem_get_atparser();
    // 如果没有设备连接，atparser可能为NULL
    if (atparser) {
        ESP_LOGI(TAG, "Got AT parser: %p", atparser);
    } else {
        ESP_LOGW(TAG, "AT parser is NULL (no device connected)");
    }

    usbh_modem_uninstall();
    uninstall();
}

TEST_CASE("usbh modem multiple install uninstall cycles", "[iot_usbh_modem][function]")
{
    install();

    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };

    // 多次安装卸载循环
    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "Install/Uninstall cycle %d", i + 1);
        esp_err_t ret = usbh_modem_install(&modem_config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        vTaskDelay(pdMS_TO_TICKS(1000));

        ret = usbh_modem_uninstall();
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    uninstall();
}

TEST_CASE("usbh modem state transitions", "[iot_usbh_modem][function]")
{
    install();

    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_install(&modem_config));

    // 禁用自动连接以便手动控制
    TEST_ASSERT_EQUAL(ESP_OK, usbh_modem_ppp_auto_connect(false));

    // 等待进入IDLE状态
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 尝试手动启动PPP（如果设备已连接）
    esp_err_t ret = usbh_modem_ppp_start(pdMS_TO_TICKS(TEST_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PPP started successfully");
        vTaskDelay(pdMS_TO_TICKS(2000));

        // 停止PPP
        ret = usbh_modem_ppp_stop();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "PPP stopped successfully");
        } else {
            ESP_LOGW(TAG, "PPP stop returned: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "PPP start returned: %s (device may not be connected)", esp_err_to_name(ret));
    }

    usbh_modem_uninstall();
    uninstall();
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before: %u bytes free, After: %u bytes free (delta:%d)\n", type, before_free, after_free, delta);
    if (!(delta >= TEST_MEMORY_LEAK_THRESHOLD)) {
        ESP_LOGE(TAG, "Memory leak detected, delta: %d bytes, threshold: %d bytes", delta, TEST_MEMORY_LEAK_THRESHOLD);
    }
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
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    printf("IOT USBH MODEM TEST \n");
    unity_run_menu();
    // esp_netif_deinit();
    // esp_event_loop_delete_default();
}
