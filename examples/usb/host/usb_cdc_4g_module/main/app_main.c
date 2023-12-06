/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "led_indicator.h"
#include "led_indicator_blink_default.h"
#include "usbh_modem_board.h"
#include "usbh_modem_wifi.h"
#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
#include "modem_http_config.h"
#endif

#ifdef CONFIG_EXAMPLE_PING_NETWORK
#include "ping/ping_sock.h"
#endif

static const char *TAG = "4g_main";
#define LED_RED_SYSTEM_GPIO                 CONFIG_EXAMPLE_LED_RED_SYSTEM_GPIO
#define LED_BLUE_WIFI_GPIO                  CONFIG_EXAMPLE_LED_BLUE_WIFI_GPIO
#define LED_GREEN_4GMODEM_GPIO              CONFIG_EXAMPLE_LED_GREEN_4GMODEM_GPIO
#define LED_ACTIVE_LEVEL                    1

static modem_wifi_config_t s_modem_wifi_config = MODEM_WIFI_DEFAULT_CONFIG();
static led_indicator_handle_t s_led_system_handle = NULL;
static led_indicator_handle_t s_led_wifi_handle = NULL;
static led_indicator_handle_t s_led_4g_handle = NULL;

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

static void _led_indicator_init()
{
    led_indicator_gpio_config_t led_indicator_gpio_config = {
        .is_active_level_high = LED_ACTIVE_LEVEL,
    };

    led_indicator_config_t led_config = {
        .led_indicator_gpio_config = &led_indicator_gpio_config,
        .mode = LED_GPIO_MODE,
    };

    if (LED_RED_SYSTEM_GPIO) {
        led_indicator_gpio_config.gpio_num = LED_RED_SYSTEM_GPIO;
        s_led_system_handle = led_indicator_create(&led_config);
        assert(s_led_system_handle != NULL);
    }
    if (LED_BLUE_WIFI_GPIO) {
        led_indicator_gpio_config.gpio_num = LED_BLUE_WIFI_GPIO;
        s_led_wifi_handle = led_indicator_create(&led_config);
        assert(s_led_wifi_handle != NULL);
        led_indicator_stop(s_led_wifi_handle, BLINK_CONNECTED);
        led_indicator_start(s_led_wifi_handle, BLINK_CONNECTING);
    }
    if (LED_GREEN_4GMODEM_GPIO) {
        led_indicator_gpio_config.gpio_num = LED_GREEN_4GMODEM_GPIO;
        s_led_4g_handle = led_indicator_create(&led_config);
        assert(s_led_4g_handle != NULL);
        led_indicator_stop(s_led_4g_handle, BLINK_CONNECTED);
        led_indicator_start(s_led_4g_handle, BLINK_CONNECTING);
    }
}

static void on_modem_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == MODEM_BOARD_EVENT) {
        if (event_id == MODEM_EVENT_SIMCARD_DISCONN) {
            ESP_LOGW(TAG, "Modem Board Event: SIM Card disconnected");
            led_indicator_start(s_led_system_handle, BLINK_CONNECTED);
        } else if (event_id == MODEM_EVENT_SIMCARD_CONN) {
            ESP_LOGI(TAG, "Modem Board Event: SIM Card Connected");
            led_indicator_stop(s_led_system_handle, BLINK_CONNECTED);
        } else if (event_id == MODEM_EVENT_DTE_DISCONN) {
            ESP_LOGW(TAG, "Modem Board Event: USB disconnected");
            led_indicator_start(s_led_system_handle, BLINK_CONNECTING);
        } else if (event_id == MODEM_EVENT_DTE_CONN) {
            ESP_LOGI(TAG, "Modem Board Event: USB connected");
            led_indicator_stop(s_led_system_handle, BLINK_CONNECTED);
            led_indicator_stop(s_led_system_handle, BLINK_CONNECTING);
        } else if (event_id == MODEM_EVENT_DTE_RESTART) {
            ESP_LOGW(TAG, "Modem Board Event: Hardware restart");
            led_indicator_start(s_led_system_handle, BLINK_CONNECTED);
        } else if (event_id == MODEM_EVENT_DTE_RESTART_DONE) {
            ESP_LOGI(TAG, "Modem Board Event: Hardware restart done");
            led_indicator_stop(s_led_system_handle, BLINK_CONNECTED);
        } else if (event_id == MODEM_EVENT_NET_CONN) {
            ESP_LOGI(TAG, "Modem Board Event: Network connected");
            led_indicator_start(s_led_4g_handle, BLINK_CONNECTED);
        } else if (event_id == MODEM_EVENT_NET_DISCONN) {
            ESP_LOGW(TAG, "Modem Board Event: Network disconnected");
            led_indicator_stop(s_led_4g_handle, BLINK_CONNECTED);
        } else if (event_id == MODEM_EVENT_WIFI_STA_CONN) {
            ESP_LOGI(TAG, "Modem Board Event: Station connected");
            led_indicator_start(s_led_wifi_handle, BLINK_CONNECTED);
        } else if (event_id == MODEM_EVENT_WIFI_STA_DISCONN) {
            ESP_LOGW(TAG, "Modem Board Event: All stations disconnected");
            led_indicator_stop(s_led_wifi_handle, BLINK_CONNECTED);
        }
    }
}

#ifdef CONFIG_EXAMPLE_PING_NETWORK
static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "%"PRIu32" bytes from %s icmp_seq=%u ttl=%u time=%"PRIu32" ms\n", recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW(TAG, "From %s icmp_seq=%u timeout\n", ipaddr_ntoa(&target_addr), seqno);
    // Users can add logic to handle ping timeout
    // Add Wait or Reset logic
}
#endif

void app_main(void)
{
    /* Initialize led indicator */
    _led_indicator_init();
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

    /* Waiting for modem powerup */
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "     ESP 4G Cat.1 Wi-Fi Router");
    ESP_LOGI(TAG, "====================================");

    /* Initialize modem board. Dial-up internet */
    modem_config_t modem_config = MODEM_DEFAULT_CONFIG();
    /* Modem init flag, used to control init process */
#ifndef CONFIG_EXAMPLE_ENTER_PPP_DURING_INIT
    /* if Not enter ppp, modem will enter command mode after init */
    modem_config.flags |= MODEM_FLAGS_INIT_NOT_ENTER_PPP;
    /* if Not waiting for modem ready, just return after modem init */
    modem_config.flags |= MODEM_FLAGS_INIT_NOT_BLOCK;
#endif
    modem_config.handler = on_modem_event;
    modem_board_init(&modem_config);

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
    modem_http_get_nvs_wifi_config(&s_modem_wifi_config);
    modem_http_init(&s_modem_wifi_config);
#endif
    esp_netif_t *ap_netif = modem_wifi_ap_init();
    assert(ap_netif != NULL);
    ESP_ERROR_CHECK(modem_wifi_set(&s_modem_wifi_config));

#ifdef CONFIG_EXAMPLE_PING_NETWORK
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    char *ping_addr_s = NULL;
#ifdef CONFIG_EXAMPLE_PING_MANUAL_ADDR
    // Ping users defined address
    ping_addr_s = CONFIG_EXAMPLE_PING_MANUAL_ADDR;
#else
    // otherwise Ping DNS server
    esp_netif_dns_info_t dns2;
    modem_board_get_dns_info(ESP_NETIF_DNS_MAIN, &dns2);
    ping_addr_s = ip4addr_ntoa((ip4_addr_t *)(&dns2.ip.u_addr.ip4));
#endif
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ipaddr_aton(ping_addr_s, &target_addr);
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = CONFIG_EXAMPLE_PING_TIMEOUT;
    ping_config.task_stack_size = 4096;
    ping_config.count = 1;

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL,
        .cb_args = NULL,
    };
    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
#endif

    uint32_t ap_dns_addr = 0;
    while (1) {

#if !defined(CONFIG_EXAMPLE_ENTER_PPP_DURING_INIT) || defined(CONFIG_MODEM_SUPPORT_SECONDARY_AT_PORT)
        // if you want to send AT command during ppp network working, make sure the modem support secondary AT port,
        // otherwise, the modem interface must switch to command mode before send command
        int rssi = 0, ber = 0;
        modem_board_get_signal_quality(&rssi, &ber);
        ESP_LOGI(TAG, "rssi=%d, ber=%d", rssi, ber);
#endif

        // If manual DNS not defined, set DNS when got address, user better to add a queue to handle this
#ifdef CONFIG_EXAMPLE_AUTO_UPDATE_DNS
        esp_netif_dns_info_t dns;
        modem_board_get_dns_info(ESP_NETIF_DNS_MAIN, &dns);
        uint32_t _ap_dns_addr = dns.ip.u_addr.ip4.addr;
        if (_ap_dns_addr != ap_dns_addr) {
            modem_wifi_set_dns(ap_netif, _ap_dns_addr);
            ap_dns_addr = _ap_dns_addr;
            ESP_LOGI(TAG, "changed: ap dns addr (auto): %s", inet_ntoa(ap_dns_addr));
        }
#endif

#ifdef CONFIG_EXAMPLE_PING_NETWORK
        ESP_LOGI(TAG, "Ping addr %s Restart..", ping_addr_s);
        esp_ping_start(ping);
#endif

#ifdef CONFIG_DUMP_SYSTEM_STATUS
        _system_dump();
#endif
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
