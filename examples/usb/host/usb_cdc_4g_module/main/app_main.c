/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"
#include "esp_netif_ppp.h"
#include "iot_usbh_modem.h"
#include "app_wifi.h"
#include "iot_eth.h"
#include "ping/ping_sock.h"
#include "at_3gpp_ts_27_007.h"
#include "network_test.h"
#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
#include "modem_http_config.h"
#endif

static const char *TAG = "PPP_4G_main";

static modem_wifi_config_t s_modem_wifi_config = MODEM_WIFI_DEFAULT_CONFIG();
static EventGroupHandle_t s_event_group;
#define EVENT_GOT_IP_BIT                   (BIT0)

// All supported modem IDs
static const usb_modem_id_t usb_modem_id_list[] = {
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x1782, 0x4d11}, 2, -1, "China Mobile, ML302/Fibocom, MC610-EU"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x1E0E, 0x9011}, 5, -1, "SIMCOM, A7600C1/SIMCOM, A7670E"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x1E0E, 0x9205}, 2, -1, "SIMCOM, SIM7080G"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x2CB7, 0x0D01}, 2, 6, "Fibocom, LE270-CN"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x2C7C, 0x6001}, 4, -1, "Quectel, EC600N-CN"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x2C7C, 0x0125}, 2, -1, "Quectel, EC20"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x19D1, 0x1003}, 2, -1, "YUGE, YM310 X09"},
    {.match_id = {USB_DEVICE_ID_MATCH_VID_PID, 0x19D1, 0x0001}, 2, -1, "Luat, Air780E"},
    {.match_id = {0}}, // End of list marker
};

#ifdef CONFIG_DUMP_SYSTEM_STATUS
#define TASK_MAX_COUNT 32
typedef struct {
    uint32_t ulRunTimeCounter;
    uint32_t xTaskNumber;
} taskData_t;

static taskData_t previousSnapshot[TASK_MAX_COUNT];
static int taskTopIndex = 0;
static uint32_t previousTotalRunTime = 0;

static taskData_t *getPreviousTaskData(uint32_t xTaskNumber)
{
    // Try to find the task in the list of tasks
    for (int i = 0; i < taskTopIndex; i++) {
        if (previousSnapshot[i].xTaskNumber == xTaskNumber) {
            return &previousSnapshot[i];
        }
    }
    // Allocate a new entry
    ESP_ERROR_CHECK(!(taskTopIndex < TASK_MAX_COUNT));
    taskData_t *result = &previousSnapshot[taskTopIndex];
    result->xTaskNumber = xTaskNumber;
    taskTopIndex++;
    return result;
}

static void _system_dump()
{
    uint32_t totalRunTime;

    TaskStatus_t taskStats[TASK_MAX_COUNT];
    uint32_t taskCount = uxTaskGetSystemState(taskStats, TASK_MAX_COUNT, &totalRunTime);
    ESP_ERROR_CHECK(!(taskTopIndex < TASK_MAX_COUNT));
    uint32_t totalDelta = totalRunTime - previousTotalRunTime;
    float f = 100.0 / totalDelta;
    // Dumps the the CPU load and stack usage for all tasks
    // CPU usage is since last dump in % compared to total time spent in tasks. Note that time spent in interrupts will be included in measured time.
    // Stack usage is displayed as nr of unused bytes at peak stack usage.

    ESP_LOGI(TAG, "Task dump\n");
    ESP_LOGI(TAG, "Load\tStack left\tName\tPRI\n");

    for (uint32_t i = 0; i < taskCount; i++) {
        TaskStatus_t *stats = &taskStats[i];
        taskData_t *previousTaskData = getPreviousTaskData(stats->xTaskNumber);

        uint32_t taskRunTime = stats->ulRunTimeCounter;
        float load = f * (taskRunTime - previousTaskData->ulRunTimeCounter);
        ESP_LOGI(TAG, "%.2f \t%" PRIu32 "\t%s %" PRIu32 "\t\n", load, stats->usStackHighWaterMark, stats->pcTaskName, (uint32_t)stats->uxBasePriority);

        previousTaskData->ulRunTimeCounter = taskRunTime;
    }
    ESP_LOGI(TAG, "Free heap=%d Free mini=%d bigst=%d, internal=%d bigst=%d",
             heap_caps_get_free_size(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
             heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
             heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    previousTotalRunTime = totalRunTime;
}
#endif

static void iot_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IOT_ETH_EVENT) {
        switch (event_id) {
        case IOT_ETH_EVENT_START:
            ESP_LOGI(TAG, "IOT_ETH_EVENT_START");
            break;
        case IOT_ETH_EVENT_STOP:
            ESP_LOGI(TAG, "IOT_ETH_EVENT_STOP");
            break;
        case IOT_ETH_EVENT_CONNECTED:
            ESP_LOGI(TAG, "IOT_ETH_EVENT_CONNECTED");
            break;
        case IOT_ETH_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "IOT_ETH_EVENT_DISCONNECTED");
            xEventGroupClearBits(s_event_group, EVENT_GOT_IP_BIT);
            stop_ping_timer();
            break;
        default:
            ESP_LOGI(TAG, "IOT_ETH_EVENT_UNKNOWN");
            break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_PPP_GOT_IP) {
            ESP_LOGI(TAG, "GOT_IP");
            xEventGroupSetBits(s_event_group, EVENT_GOT_IP_BIT);
            start_ping_timer();
        } else if (event_id == IP_EVENT_PPP_LOST_IP) {
            ESP_LOGW(TAG, "LOST_IP");
            xEventGroupClearBits(s_event_group, EVENT_GOT_IP_BIT);
            stop_ping_timer();
        }
    }
}

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    // USB mode select host
    const gpio_config_t io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_18, 1));

    // Set host usb dev power mode
    const gpio_config_t power_io_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_17) | BIT64(GPIO_NUM_12) | BIT64(GPIO_NUM_13),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&power_io_config));

    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17, 1)); // Configure the limiter 500mA
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_12, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_13, 0)); // Turn power off
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_12, 1)); // Turn on usb dev power mode
#endif
    /* Initialize NVS for Wi-Fi storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated and needs to be erased
         * Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Initialize default TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_event_group != NULL,, TAG, "Failed to create event group");
    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, iot_event_handle, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, iot_event_handle, NULL);
    esp_event_handler_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, iot_event_handle, NULL);

    // install usbh cdc driver
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = configMAX_PRIORITIES - 1,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    ESP_ERROR_CHECK(usbh_cdc_driver_install(&config));

    /* Waiting for modem powerup */
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "     ESP 4G Cat.1 Wi-Fi Router");
    ESP_LOGI(TAG, "====================================");

    /* Initialize modem board. Dial-up internet */
    usbh_modem_config_t modem_config = {
        .modem_id_list = usb_modem_id_list,
        .at_tx_buffer_size = 256,
        .at_rx_buffer_size = 256,
    };
    usbh_modem_install(&modem_config);
    ESP_LOGI(TAG, "modem board installed");

    xEventGroupWaitBits(s_event_group, EVENT_GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
    modem_http_get_nvs_wifi_config(&s_modem_wifi_config);
    modem_http_init(&s_modem_wifi_config);
#endif
    app_wifi_main(&s_modem_wifi_config);
    esp_netif_set_default_netif(usbh_modem_get_netif());

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_event_group, EVENT_GOT_IP_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
        if (bits & EVENT_GOT_IP_BIT) {
            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a bit for DNS to be ready
            test_query_public_ip(); // Query public IP via HTTP
#if CONFIG_EXAMPLE_USB_PPP_DOWNLOAD_SPEED_TEST
            test_download_speed("http://mirrors.ustc.edu.cn/ros/ubuntu/db/references.db");
#endif
            char str[64] = {0};
            at_cmd_get_manufacturer_id(usbh_modem_get_atparser(), str, sizeof(str));
            ESP_LOGI(TAG, "Manufacturer ID: %s", str);
        }

#ifdef CONFIG_DUMP_SYSTEM_STATUS
        _system_dump();
#endif
    }
    usbh_modem_uninstall();
    usbh_cdc_driver_uninstall();
}
