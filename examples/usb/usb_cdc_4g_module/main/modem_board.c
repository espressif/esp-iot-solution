// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_modem.h"
#include "esp_netif_ppp.h"
#include "lwip/lwip_napt.h"

#include "esp_modem_recov_helper.h"
#include "esp_modem_dce.h"
#include "esp_modem_dce_common_commands.h"
#include "led_indicator.h"
#include "modem_board.h"

#define ESP_MODEM_EXAMPLE_CHECK(a, str, goto_tag, ...)                                \
    do                                                                                \
    {                                                                                 \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                            \
        }                                                                             \
    } while (0)

#define LED_RED_SYSTEM_GPIO                 CONFIG_LED_RED_SYSTEM_GPIO
#define LED_BLUE_WIFI_GPIO                  CONFIG_LED_BLUE_WIFI_GPIO
#define LED_GREEN_4GMODEM_GPIO              CONFIG_LED_GREEN_4GMODEM_GPIO
#define MODEM_POWER_GPIO                    CONFIG_MODEM_POWER_GPIO
#define MODEM_RESET_GPIO                    CONFIG_MODEM_RESET_GPIO
#define MODEM_POWER_GPIO_INACTIVE_LEVEL     1
#define MODEM_POWER_GPIO_ACTIVE_MS          500
#define MODEM_POWER_GPIO_INACTIVE_MS        8000
#define MODEM_RESET_GPIO_INACTIVE_LEVEL     1
#define MODEM_RESET_GPIO_ACTIVE_MS          200
#define MODEM_RESET_GPIO_INACTIVE_MS        5000

static const char *TAG = "modem_board";
static const int CONNECT_BIT = BIT0;
static const int DISCONNECT_BIT = BIT1;

static led_indicator_handle_t led_system_handle = NULL;
static led_indicator_handle_t led_wifi_handle = NULL;
static led_indicator_handle_t led_4g_handle = NULL;
static int active_station_num = 0;

#define MODEM_EVENT_TASK_PRIORITY   4
#define MODEM_EVENT_TASK_STACK_SIZE 3072

typedef struct {
    esp_modem_dce_t parent;
    esp_modem_recov_gpio_t *power_pin;
    esp_modem_recov_gpio_t *reset_pin;
    esp_err_t (*reset)(esp_modem_dce_t *dce);
    esp_err_t (*power_up)(esp_modem_dce_t *dce);
    esp_err_t (*power_down)(esp_modem_dce_t *dce);
    esp_modem_recov_resend_t *re_sync;
    esp_modem_recov_resend_t *re_store_profile;
} modem_board_t;

static esp_err_t modem_board_handle_powerup(esp_modem_dce_t *dce, const char *line)
{
    if (strstr(line, "PB DONE")) {
        ESP_LOGI(TAG, "Board ready after hard reset/power-cycle");
    } else {
        ESP_LOGI(TAG, "xxxxxxxxxxxxxx Board ready after hard reset/power-cycle");
    }
    return ESP_OK;
}

static esp_err_t modem_board_reset(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_reset!");
    dce->handle_line = modem_board_handle_powerup;
    board->reset_pin->pulse(board->reset_pin);
    return ESP_OK;
}

static esp_err_t modem_board_power_up(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_power_up!");
    dce->handle_line = modem_board_handle_powerup;
    board->power_pin->pulse(board->power_pin);
    return ESP_OK;
}

static esp_err_t modem_board_power_down(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_power_down!");
    /* power down sequence (typical values for SIM7600 Toff=min2.5s, Toff-status=26s) */
    dce->handle_line = modem_board_handle_powerup;
    board->power_pin->pulse_special(board->power_pin, 3000, 26000);
    return ESP_OK;
}

static esp_err_t my_recov(esp_modem_recov_resend_t *retry_cmd, esp_err_t err, int timeouts, int errors)
{
    esp_modem_dce_t *dce = retry_cmd->dce;
    ESP_LOGI(TAG, "Current timeouts: %d and errors: %d", timeouts, errors);
    if (err == ESP_ERR_TIMEOUT) {
        if (timeouts < 2) {
            // first timeout, try to exit data mode and sync again
            dce->set_command_mode(dce, NULL, NULL);
            esp_modem_dce_sync(dce, NULL, NULL);
        } else if (timeouts < 3) {
            // try to reset with GPIO if resend didn't help
            ESP_LOGI(TAG, "Restart to connect modem........");
            led_indicator_start(led_system_handle, BLINK_FACTORY_RESET);
            modem_board_t *board = __containerof(dce, modem_board_t, parent);
            board->reset(dce);
            esp_restart(); //restart system
        } else {
            // otherwise power-cycle the board
            modem_board_t *board = __containerof(dce, modem_board_t, parent);
            board->power_down(dce);
            esp_modem_dce_sync(dce, NULL, NULL);
        }
    } else {
        // check if a PIN needs to be supplied in case of a failure
        bool ready = false;
        esp_modem_dce_read_pin(dce, NULL, &ready);
        if (!ready) {
            esp_modem_dce_set_pin(dce, "1234", NULL);
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_modem_dce_read_pin(dce, NULL, &ready);
        if (!ready) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static DEFINE_RETRY_CMD(re_sync_fn, re_sync, modem_board_t)

static DEFINE_RETRY_CMD(re_store_profile_fn, re_store_profile, modem_board_t)

static esp_err_t modem_board_start_up(esp_modem_dce_t *dce)
{
    ESP_MODEM_EXAMPLE_CHECK(re_sync_fn(dce, NULL, NULL) == ESP_OK, "sending sync failed", err);
    ESP_MODEM_EXAMPLE_CHECK(dce->set_echo(dce, (void*)false, NULL) == ESP_OK, "set_echo failed", err);
    ESP_MODEM_EXAMPLE_CHECK(dce->set_flow_ctrl(dce, (void*)ESP_MODEM_FLOW_CONTROL_NONE, NULL) == ESP_OK, "set_flow_ctrl failed", err);
    ESP_MODEM_EXAMPLE_CHECK(dce->store_profile(dce, NULL, NULL) == ESP_OK, "store_profile failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;

}

static esp_err_t modem_board_deinit(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    board->power_pin->destroy(board->power_pin);
    board->reset_pin->destroy(board->reset_pin);
    esp_err_t err = esp_modem_command_list_deinit(&board->parent);
    if (err == ESP_OK) {
        free(dce);
    }
    //TODO: Full Clean
    return err;
}

static esp_modem_dce_t *modem_board_create(esp_modem_dce_config_t *config)
{
    modem_board_t *board = calloc(1, sizeof(modem_board_t));
    ESP_MODEM_EXAMPLE_CHECK(board, "failed to allocate board-sim7600 object", err);
    ESP_MODEM_EXAMPLE_CHECK(esp_modem_dce_init(&board->parent, config) == ESP_OK, "Failed to init sim7600", err);
    // /* power on sequence (typical values for SIM7600 Ton=500ms, Ton-status=16s) */
    board->power_pin = esp_modem_recov_gpio_new(MODEM_POWER_GPIO, MODEM_POWER_GPIO_INACTIVE_LEVEL,
                                                MODEM_POWER_GPIO_ACTIVE_MS, MODEM_POWER_GPIO_INACTIVE_MS);
    // /* reset sequence (typical values for SIM7600 Treset=200ms, wait 10s after reset */
    board->reset_pin = esp_modem_recov_gpio_new(MODEM_RESET_GPIO, MODEM_RESET_GPIO_INACTIVE_LEVEL,
                                                MODEM_RESET_GPIO_ACTIVE_MS, MODEM_RESET_GPIO_INACTIVE_MS);
    board->parent.deinit = modem_board_deinit;
    board->reset = modem_board_reset;
    board->power_up = modem_board_power_up;
    board->power_down = modem_board_power_down;
    board->re_sync = esp_modem_recov_resend_new(&board->parent, board->parent.sync, my_recov, 5, 1);
    board->parent.start_up = modem_board_start_up;
    board->re_store_profile = esp_modem_recov_resend_new(&board->parent, board->parent.store_profile, my_recov, 2, 3);
    board->parent.store_profile = re_store_profile_fn;

    return &board->parent;
err:
    return NULL;
}

static void on_modem_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    EventGroupHandle_t connection_events = arg;
    if (event_base == IP_EVENT) {
        ESP_LOGI(TAG, "IP event! %d", event_id);
        if (event_id == IP_EVENT_PPP_GOT_IP) {
            esp_netif_dns_info_t dns_info;

            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            esp_netif_t *netif = event->esp_netif;

            ESP_LOGI(TAG, "Modem Connected to PPP Server");
            ESP_LOGI(TAG, "%s ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR, esp_netif_get_desc(netif),
                     IP2STR(&event->ip_info.ip),
                     IP2STR(&event->ip_info.netmask),
                     IP2STR(&event->ip_info.gw));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
            ESP_LOGI(TAG, "Main DNS: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            ESP_LOGI(TAG, "Backup DNS: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            led_indicator_start(led_4g_handle, BLINK_CONNECTED);
            xEventGroupSetBits(connection_events, CONNECT_BIT);

        } else if (event_id == IP_EVENT_PPP_LOST_IP) {
            ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
            led_indicator_stop(led_4g_handle, BLINK_CONNECTED);
            led_indicator_start(led_4g_handle, BLINK_CONNECTING);
            xEventGroupSetBits(connection_events, DISCONNECT_BIT);
        } else if (event_id == IP_EVENT_GOT_IP6) {
            ESP_LOGI(TAG, "GOT IPv6 event!");
            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        }
    } else if (event_base == ESP_MODEM_EVENT) {
        ESP_LOGI(TAG, "Modem event! %d", event_id);
        switch (event_id) {
            case ESP_MODEM_EVENT_PPP_START:
                ESP_LOGI(TAG, "Modem PPP Started");
                break;
            case ESP_MODEM_EVENT_PPP_STOP:
                ESP_LOGI(TAG, "Modem PPP Stopped");
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        //wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED:
                if (++active_station_num > 0) {
                    led_indicator_start(led_wifi_handle, BLINK_CONNECTED);
                }
                //ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
            break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                if (--active_station_num == 0) {
                    led_indicator_stop(led_wifi_handle, BLINK_CONNECTED);
                    led_indicator_start(led_wifi_handle, BLINK_CONNECTING);
                }
                //ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
            break;
            default:
            break;
        }
    } else if (event_base == NETIF_PPP_STATUS) {
        ESP_LOGI(TAG, "PPP netif event! %d", event_id);
        if (event_id == NETIF_PPP_ERRORCONNECT) {
            led_indicator_stop(led_4g_handle, BLINK_CONNECTED);
            led_indicator_start(led_4g_handle, BLINK_CONNECTING);
            xEventGroupSetBits(connection_events, DISCONNECT_BIT);
        }
    }
}

esp_netif_t *modem_board_init(modem_config_t *config)
{
    led_indicator_config_t led_config = {
        .off_level = 0,
        .mode = LED_GPIO_MODE,
    };

    led_system_handle = led_indicator_create(LED_RED_SYSTEM_GPIO, &led_config);
    led_wifi_handle = led_indicator_create(LED_BLUE_WIFI_GPIO, &led_config);
    led_4g_handle = led_indicator_create(LED_GREEN_4GMODEM_GPIO, &led_config);

    EventGroupHandle_t connection_events = xEventGroupCreate();

    // init the DTE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.rx_buffer_size = config->rx_buffer_size; //rx ringbuffer for usb transfer
    dte_config.tx_buffer_size = config->tx_buffer_size; //tx ringbuffer for usb transfer
    dte_config.event_queue_size = config->event_queue_size;
    dte_config.line_buffer_size = config->line_buffer_size;
    dte_config.event_task_stack_size = MODEM_EVENT_TASK_STACK_SIZE; //task to handle usb rx data
    dte_config.event_task_priority = MODEM_EVENT_TASK_PRIORITY; //task to handle usb rx data
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
    dce_config.populate_command_list = true;
    esp_netif_config_t ppp_netif_config = ESP_NETIF_DEFAULT_PPP();

    // Initialize esp-modem units, DTE, DCE, ppp-netif
    esp_modem_dte_t *dte = esp_modem_dte_new(&dte_config);
    assert(dte != NULL);
    esp_modem_dce_t *dce = modem_board_create(&dce_config);
    assert(dce != NULL);
    esp_netif_t *ppp_netif = esp_netif_new(&ppp_netif_config);
    assert(ppp_netif != NULL);

    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, on_modem_event, ESP_EVENT_ANY_ID, connection_events));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_modem_event, connection_events));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_modem_event, connection_events));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_modem_event, connection_events));

    led_indicator_stop(led_4g_handle, BLINK_CONNECTED);
    led_indicator_start(led_4g_handle, BLINK_CONNECTING);
    led_indicator_stop(led_wifi_handle, BLINK_CONNECTED);
    led_indicator_start(led_wifi_handle, BLINK_CONNECTING);

    ESP_ERROR_CHECK(esp_modem_default_attach(dte, dce, ppp_netif));
    ESP_ERROR_CHECK(esp_modem_default_start(dte));

    esp_err_t ret = ESP_OK;
    int dial_retry_times = CONFIG_MODEM_DIAL_RERTY_TIMES;

    do {
        vTaskDelay(pdMS_TO_TICKS(3000));
        ret = esp_modem_start_ppp(dte);
    } while ( ret != ESP_OK && --dial_retry_times > 0 );

    if (ret != ESP_OK) {
        led_indicator_start(led_4g_handle, BLINK_CONNECTED);
    } else {
        led_indicator_start(led_system_handle, BLINK_CONNECTED);//solid red, internal error
    }

    /* Wait for the first connection */
    EventBits_t bits;
    do {
        bits = xEventGroupWaitBits(connection_events, (CONNECT_BIT | DISCONNECT_BIT), pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits&DISCONNECT_BIT) {
            // restart the PPP mode in DTE
            ESP_ERROR_CHECK(esp_modem_stop_ppp(dte));
            ESP_ERROR_CHECK(esp_modem_start_ppp(dte));
            ESP_LOGW(TAG, "Restart PPP");
        }
    } while ((bits&CONNECT_BIT) == 0);

    return ppp_netif;
}
