/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi_types.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem.h"
#include "esp_modem_recov_helper.h"
#include "esp_modem_dce.h"
#include "esp_modem_dce_common_commands.h"
#include "usbh_modem_board.h"

static const char *TAG = "modem_board";
ESP_EVENT_DEFINE_BASE(MODEM_BOARD_EVENT);

#define MODEM_CHECK_GOTO(a, str, goto_tag, ...)                                       \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);     \
            goto goto_tag;                                                            \
        }

#define MODEM_CHECK(a, str, return_tag, ...)                                            \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);     \
            return return_tag;                                                            \
        }                                                                             \

#define MODEM_POWER_GPIO                    CONFIG_MODEM_POWER_GPIO
#define MODEM_RESET_GPIO                    CONFIG_MODEM_RESET_GPIO
#define MODEM_POWER_GPIO_INACTIVE_LEVEL     CONFIG_MODEM_POWER_GPIO_INACTIVE_LEVEL
#define MODEM_RESET_GPIO_INACTIVE_LEVEL     CONFIG_MODEM_RESET_GPIO_INACTIVE_LEVEL
#define MODEM_POWER_GPIO_ACTIVE_MS          500
#define MODEM_POWER_GPIO_INACTIVE_MS        8000
#define MODEM_RESET_GPIO_ACTIVE_MS          200
#define MODEM_RESET_GPIO_INACTIVE_MS        5000
#define MODEM_NET_RECONNECT_DELAY_S         5
#define MODEM_SIM_PIN_PWD                   CONFIG_MODEM_SIM_PIN_PWD

/* user event */
static const int MODEM_DESTROY_BIT                = BIT0;    /* detroy modem daemon task, trigger by user, clear by daemon task */
static const int PPP_NET_MODE_ON_BIT              = BIT1;    /* dte usb reconnect event, trigger by user, clear by daemon task */
static const int PPP_NET_MODE_OFF_BIT             = BIT2;    /* dte usb disconnect event, trigger by user, clear by hardware */
static const int PPP_NET_AUTO_SUSPEND_USER_BIT    = BIT3;    /* suspend ppp net auto reconnect, trigger by user, clear by user */
/* net event */
static const int PPP_NET_CONNECT_BIT              = BIT4;    /* ppp net got ip, trigger by lwip, clear by lwip or daemon task */
static const int PPP_NET_DISCONNECT_BIT           = BIT5;    /* ppp net loss ip, trigger by lwip, clear by lwip */
/* usb event */
static const int DTE_USB_DISCONNECT_BIT           = BIT6;    /* dte usb disconnect event, trigger by hardware, clear by daemon task */
static const int DTE_USB_RECONNECT_BIT            = BIT7;    /* dte usb reconnect event, trigger by hardware or user, clear by daemon task */
/* daemon task internal event bit */
static const int PPP_NET_RECONNECTING_BIT         = BIT8;    /* ppp net reconnecting, trigger by daemon task, clear by daemon task */

static esp_modem_dce_t *s_dce = NULL;
static EventGroupHandle_t s_modem_evt_hdl = NULL;
static esp_ip_addr_t s_dns_ip_main = ESP_IP4ADDR_INIT(8, 8, 8, 8);
static esp_ip_addr_t s_dns_ip_backup = ESP_IP4ADDR_INIT(114, 114, 114, 114);

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

typedef enum {
    STAGE_DTE_LOSS = -1,    /* dte loss, restoring to command state */
    STAGE_WAITING = 0,      /* in command state, waiting ppp on event */
    STAGE_SYNC = 1,         /* trying sync using AT modem */
    STAGE_STOP_PPP = 2,     /* restoring to command state */
    STAGE_CHECK_SIM = 3,    /* check sim state */
    STAGE_CHECK_SIGNAL = 4, /* check net signal strength */
    STAGE_CHECK_REGIST = 5, /* check is net registered to operator */
    STAGE_START_PPP = 6,    /* trying dial-up */
    STAGE_WAIT_IP = 7,      /* waiting ip */
    STAGE_RUNNING = 8,      /* perfect, enjoy network */
} _modem_stage_t;

#define MODEM_STAGE_CODE2STR(code) {code, #code}

typedef struct {
    int code;
    const char *msg;
} _modem_stage_msg_t;

static const _modem_stage_msg_t modem_stage_msg_table[] = {
    MODEM_STAGE_CODE2STR(STAGE_DTE_LOSS),
    MODEM_STAGE_CODE2STR(STAGE_WAITING),
    MODEM_STAGE_CODE2STR(STAGE_SYNC),
    {STAGE_STOP_PPP, "STAGE_SYNC/STOP_PPP"},
    MODEM_STAGE_CODE2STR(STAGE_CHECK_SIM),
    MODEM_STAGE_CODE2STR(STAGE_CHECK_SIGNAL),
    MODEM_STAGE_CODE2STR(STAGE_CHECK_REGIST),
    MODEM_STAGE_CODE2STR(STAGE_START_PPP),
    MODEM_STAGE_CODE2STR(STAGE_WAIT_IP),
    MODEM_STAGE_CODE2STR(STAGE_RUNNING)
};

const char *MODEM_STAGE_STR(int code)
{
    size_t i;
    for (i = 0; i < sizeof(modem_stage_msg_table) / sizeof(modem_stage_msg_table[0]); ++i) {
        if (modem_stage_msg_table[i].code == code) {
            return modem_stage_msg_table[i].msg;
        }
    }
    return "unknown";
}

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
    if (board->reset_pin) {
        board->reset_pin->pulse(board->reset_pin);
    }
    return ESP_OK;
}

esp_err_t modem_board_force_reset(void)
{
    ESP_LOGI(TAG, "Force reset modem board....");
    gpio_config_t io_config = {
        .pin_bit_mask = BIT64(MODEM_RESET_GPIO),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io_config);
    // gpio default to inactive state
    gpio_set_level(MODEM_RESET_GPIO, MODEM_RESET_GPIO_INACTIVE_LEVEL);
    // gpio active to reset modem
    gpio_set_level(MODEM_RESET_GPIO, !MODEM_RESET_GPIO_INACTIVE_LEVEL);
    ESP_LOGI(TAG, "Resetting modem using io=%d, level=%d", MODEM_RESET_GPIO, !MODEM_RESET_GPIO_INACTIVE_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(MODEM_RESET_GPIO_ACTIVE_MS));
    gpio_set_level(MODEM_RESET_GPIO, MODEM_RESET_GPIO_INACTIVE_LEVEL);
    // waitting for modem re-init ready
    ESP_LOGI(TAG, "Waiting for modem initialize ready");
    vTaskDelay(pdMS_TO_TICKS(MODEM_RESET_GPIO_INACTIVE_MS));
    return ESP_OK;
}

static esp_err_t modem_board_power_up(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_power_up!");
    dce->handle_line = modem_board_handle_powerup;
    if (board->power_pin) {
        board->power_pin->pulse(board->power_pin);
    }
    return ESP_OK;
}

static esp_err_t modem_board_power_down(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    ESP_LOGI(TAG, "modem_board_power_down!");
    dce->handle_line = modem_board_handle_powerup;
    if (board->power_pin) {
        board->power_pin->pulse(board->power_pin);
    }
    return ESP_OK;
}

/* Functions in recover loop, will run many times to ensure work */
static esp_err_t my_recov(esp_modem_recov_resend_t *retry_cmd, esp_err_t err, int timeouts, int errors)
{
    esp_modem_dce_t *dce = retry_cmd->dce;
    ESP_LOGI(TAG, "Current timeouts: %d and errors: %d", timeouts, errors);
    if (err != ESP_OK) {
        if (timeouts + errors < 2) {
            ESP_LOGW(TAG, "Try to exit ppp mode........");
            dce->mode = ESP_MODEM_TRANSITION_MODE;
            dce->set_command_mode(dce, NULL, NULL);
        } else if (timeouts + errors <= 3) {
            // try to reset with GPIO if resend didn't help
            ESP_LOGW(TAG, "Reset modem through reset pin........");
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART, NULL, 0, 0);
            modem_board_t *board = __containerof(dce, modem_board_t, parent);
            board->reset(dce);
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART_DONE, NULL, 0, 0);
        } else {
            // otherwise power-cycle the board
            ESP_LOGW(TAG, "Reset modem through power pin........");
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART, NULL, 0, 0);
            modem_board_t *board = __containerof(dce, modem_board_t, parent);
            board->power_down(dce);
            board->power_up(dce);
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_RESTART_DONE, NULL, 0, 0);
        }
    }
    return ESP_OK;
}

static DEFINE_RETRY_CMD(re_sync_fn, re_sync, modem_board_t)

static esp_err_t modem_board_start_up(esp_modem_dce_t *dce)
{
    MODEM_CHECK_GOTO(re_sync_fn(dce, NULL, NULL) == ESP_OK, "sending sync failed", err);
    MODEM_CHECK_GOTO(dce->set_echo(dce, (void *)false, NULL) == ESP_OK, "set_echo failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;

}

static esp_err_t modem_board_deinit(esp_modem_dce_t *dce)
{
    modem_board_t *board = __containerof(dce, modem_board_t, parent);
    if (board->power_pin) {
        board->power_pin->destroy(board->power_pin);
    }
    if (board->reset_pin) {
        board->reset_pin->destroy(board->reset_pin);
    }
    esp_err_t err = esp_modem_command_list_deinit(&board->parent);
    if (err == ESP_OK) {
        free(dce);
    }
    s_dce = NULL;
    return err;
}

static esp_modem_dce_t *modem_board_create(esp_modem_dce_config_t *config)
{
    modem_board_t *board = calloc(1, sizeof(modem_board_t));
    MODEM_CHECK_GOTO(board, "failed to allocate modem_board object", err);
    MODEM_CHECK_GOTO(esp_modem_dce_init(&board->parent, config) == ESP_OK, "Failed to init modem_dce", err);
    // /* power on sequence (typical values for modem Ton=500ms, Ton-status=8s) */
    if (MODEM_POWER_GPIO) board->power_pin = esp_modem_recov_gpio_new(MODEM_POWER_GPIO, MODEM_POWER_GPIO_INACTIVE_LEVEL,
                                                                          MODEM_POWER_GPIO_ACTIVE_MS, MODEM_POWER_GPIO_INACTIVE_MS);
    // /* reset sequence (typical values for modem reser, Treset=200ms, wait 5s after reset */
    if (MODEM_RESET_GPIO) board->reset_pin = esp_modem_recov_gpio_new(MODEM_RESET_GPIO, MODEM_RESET_GPIO_INACTIVE_LEVEL,
                                                                          MODEM_RESET_GPIO_ACTIVE_MS, MODEM_RESET_GPIO_INACTIVE_MS);
    board->reset = modem_board_reset;
    board->power_up = modem_board_power_up;
    board->power_down = modem_board_power_down;
    board->re_sync = esp_modem_recov_resend_new(&board->parent, board->parent.sync, my_recov, 5, 5);
    //overwrite default function
    board->parent.deinit = modem_board_deinit;
    board->parent.start_up = modem_board_start_up;

    return &board->parent;
err:
    free(board);
    return NULL;
}

static void on_modem_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT) {
        ESP_LOGI(TAG, "IP event! %"PRIi32"", event_id);
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
            s_dns_ip_main = dns_info.ip;
            ESP_LOGI(TAG, "Main DNS: " IPSTR, IP2STR(&s_dns_ip_main.u_addr.ip4));
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            s_dns_ip_backup = dns_info.ip;
            ESP_LOGI(TAG, "Backup DNS: " IPSTR, IP2STR(&s_dns_ip_backup.u_addr.ip4));
            if (s_modem_evt_hdl) {
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_DISCONNECT_BIT);
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_CONNECT_BIT);
            }
        } else if (event_id == IP_EVENT_PPP_LOST_IP) {
            ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
            if (s_modem_evt_hdl) {
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_CONNECT_BIT);
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_DISCONNECT_BIT);
            }
        } else if (event_id == IP_EVENT_GOT_IP6) {
            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        }
    } else if (event_base == ESP_MODEM_EVENT) {
        switch (event_id) {
        default:
            ESP_LOGW(TAG, "Modem event! %"PRIi32"", event_id);
            break;
        }
    }
}

static void _usb_dte_conn_callback(void *arg)
{
    /* first conn callback, dce*/
    xEventGroupSetBits(s_modem_evt_hdl, DTE_USB_RECONNECT_BIT);
    xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
    esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_CONN, NULL, 0, 0);
}

static void _usb_dte_disconn_callback(void *arg)
{
    /* withdraw reconnect if disconn happened first */
    xEventGroupSetBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
    xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_RECONNECT_BIT);
    esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_DTE_DISCONN, NULL, 0, 0);
}

static bool _check_sim_card()
{
    int if_ready = false;
    if (modem_board_get_sim_cart_state(&if_ready) == ESP_OK) {
        if (if_ready == true) {
            ESP_LOGI(TAG, "SIM Card Ready");
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_SIMCARD_CONN, NULL, 0, 0);
            return true;
        } else {
            ESP_LOGW(TAG, "No SIM Card or PIN Wrong!");
            esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_SIMCARD_DISCONN, NULL, 0, 0);
            return false;
        }
    }
    ESP_LOGW(TAG, "Get SIM card state failed");
    return false;
}

static bool _check_signal_quality()
{
    int rssi = 0, ber = 0 ;
    if (modem_board_get_signal_quality(&rssi, &ber) == ESP_OK) {
        if (rssi != 99 && rssi > 5) {
            ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
            return true;
        } else {
            ESP_LOGW(TAG, "Low signal quality: rssi=%d, ber=%d", rssi, ber);
            return false;
        }
    }
    ESP_LOGW(TAG, "Get signal quality failed");
    return false;
}

static bool _check_network_registration()
{
    char operater_name[64] = "";
    if (modem_board_get_operator_state(operater_name, sizeof(operater_name)) == ESP_OK) {
        if (strlen(operater_name) > 0) {
            ESP_LOGI(TAG, "Network registed, Operator: %s", operater_name);
            return true;
        } else {
            // no operator name, but registed?
            ESP_LOGW(TAG, "No operator information, Network not registed ?");
            return false;
        }
    }
    ESP_LOGW(TAG, "Get operator failed, Network not registed");
    return false;
}

static bool _ppp_network_start(esp_modem_dte_t *dte)
{
    dte->dce->mode = ESP_MODEM_TRANSITION_MODE;
    if (esp_modem_start_ppp(dte) == ESP_OK) {
        return true;
    }
    return false;
}

static bool _ppp_network_stop(esp_modem_dte_t *dte)
{
    if (esp_modem_stop_ppp(dte) == ESP_OK) {
        return true;
    }
    return false;
}

static void _modem_daemon_task(void *param)
{
    modem_config_t *config = (modem_config_t *)param;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if ((config->flags & MODEM_FLAGS_INIT_NOT_FORCE_RESET) == 0) {
        modem_board_force_reset();
    }
    // init the USB DTE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.rx_buffer_size = config->rx_buffer_size; //rx ringbuffer for usb transfer
    dte_config.tx_buffer_size = config->tx_buffer_size; //tx ringbuffer for usb transfer
    dte_config.line_buffer_size = config->line_buffer_size;
    dte_config.event_task_stack_size = config->event_task_stack_size; //task to handle usb rx data
    dte_config.event_task_priority = config->event_task_priority; //task to handle usb rx data
    dte_config.conn_callback = _usb_dte_conn_callback;
    dte_config.disconn_callback = _usb_dte_disconn_callback;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_MODEM_PPP_APN);
    esp_netif_config_t ppp_netif_config = ESP_NETIF_DEFAULT_PPP();

    // Initialize esp-modem units, DTE, DCE, ppp-netif
    esp_modem_dte_t *dte = esp_modem_dte_new(&dte_config);
    assert(dte != NULL);
    esp_modem_dce_t *dce = modem_board_create(&dce_config);
    assert(dce != NULL);
    esp_netif_t *ppp_netif = esp_netif_new(&ppp_netif_config);
    assert(ppp_netif != NULL);
    /* attach driver to ppp interface, start DTE handling */
    s_dce = dce;
    ESP_ERROR_CHECK(esp_modem_default_attach(dte, dce, ppp_netif));
    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, on_modem_event, ESP_EVENT_ANY_ID, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_modem_event, NULL));
    if (config->handler) {
        ESP_ERROR_CHECK(esp_event_handler_register(MODEM_BOARD_EVENT, ESP_EVENT_ANY_ID, config->handler, config->handler_arg));
    }
    int stage_retry_times = 0;
    int retry_after_ms = 0;
    const int RETRY_TIMEOUT = CONFIG_MODEM_DIAL_RETRY_TIMES;
    _modem_stage_t modem_stage = STAGE_SYNC;
    while (true) {
        /********************************** handle external event *********************************************************/
        EventBits_t bits = xEventGroupWaitBits(s_modem_evt_hdl, (PPP_NET_MODE_ON_BIT | PPP_NET_MODE_OFF_BIT | DTE_USB_RECONNECT_BIT | DTE_USB_DISCONNECT_BIT | PPP_NET_RECONNECTING_BIT |
                                                                 PPP_NET_DISCONNECT_BIT | MODEM_DESTROY_BIT), pdFALSE, pdFALSE, portMAX_DELAY);
        ESP_LOGD(TAG, "Handling bits = %04X, stage = %d, retry = %d ", (unsigned int)bits, modem_stage, stage_retry_times);
        /* deamon task destroy */
        if (bits & MODEM_DESTROY_BIT) {
            break;
        }

        /* user trigger ppp start */
        if (bits & PPP_NET_MODE_ON_BIT) {
            if (modem_stage < STAGE_CHECK_SIM) {
                modem_stage = STAGE_SYNC;
            } else {
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
            }
            stage_retry_times = 0;
        }

        /* user trigger ppp stop */
        if (bits & PPP_NET_MODE_OFF_BIT) {
            if (modem_stage > STAGE_START_PPP) {
                modem_stage = STAGE_DTE_LOSS;
            } else {
                modem_stage = STAGE_WAITING;
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_OFF_BIT);
            }
            stage_retry_times = 0;
        }

        /* usb dis-connect will trigger network pending */
        if (bits & DTE_USB_DISCONNECT_BIT) {
            if (modem_stage > STAGE_START_PPP) {
                modem_stage = STAGE_DTE_LOSS;
            } else {
                modem_stage = STAGE_WAITING;
                xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
            }
            stage_retry_times = 0;
        }

        /* usb re-connect will trigger network re-dial after few seconds */
        if (bits & DTE_USB_RECONNECT_BIT) {
            ESP_LOGI(TAG, "DTE reconnect, reconnecting ...\n");
            /* add delay here to wait modem ready */
            for (size_t i = 0; i < MODEM_NET_RECONNECT_DELAY_S; i++) {
                /* break the reconnect if reconnect withdraw or modem destroy */
                if (((xEventGroupGetBits(s_modem_evt_hdl) & DTE_USB_RECONNECT_BIT) == 0)
                        || (xEventGroupGetBits(s_modem_evt_hdl) & MODEM_DESTROY_BIT)) {
                    break;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ESP_LOGI(TAG, "reconnect after %ds...", MODEM_NET_RECONNECT_DELAY_S - i);
                }
            }
            /* break the reconnect if reconnect withdraw or modem destroy */
            if (((xEventGroupGetBits(s_modem_evt_hdl) & DTE_USB_RECONNECT_BIT) == 0)
                    || (xEventGroupGetBits(s_modem_evt_hdl) & MODEM_DESTROY_BIT)) {
                continue;
            }

            xEventGroupClearBits(s_modem_evt_hdl, (DTE_USB_RECONNECT_BIT | PPP_NET_CONNECT_BIT));
            if ((xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_AUTO_SUSPEND_USER_BIT) == 0) {
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
            }
            modem_stage = STAGE_SYNC;
            stage_retry_times = 0;
        }
        /* net disconnect will trigger ppp stop */
        if ((bits & PPP_NET_DISCONNECT_BIT) && modem_stage == STAGE_RUNNING) {
            if ((xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_AUTO_SUSPEND_USER_BIT) == 0) {
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
            }
            modem_stage = STAGE_STOP_PPP;
            stage_retry_times = 0;
        }

        /************************************ Stage Retry logic **********************************/
        const char *stare_str = MODEM_STAGE_STR(modem_stage);
        if (stage_retry_times >= RETRY_TIMEOUT) {
            //IF stage retry timeout, retry from start
            ESP_LOGE(TAG, "Modem state %s, retry %d, timeout !", stare_str, stage_retry_times);
            ESP_LOGW(TAG, "Retry From Start !");
            if (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_RECONNECTING_BIT) {
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_RECONNECTING_BIT);
                xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
            }
            modem_stage = STAGE_SYNC;
            stare_str = MODEM_STAGE_STR(modem_stage);
            stage_retry_times = 0;
        } else if (stage_retry_times > 0) {
            ESP_LOGW(TAG, "Modem state %s, Failed, retry%d, after %dms...", stare_str, stage_retry_times, retry_after_ms);
            vTaskDelay(pdMS_TO_TICKS(retry_after_ms));
        }

        /************************************ Processing stage **********************************/
        if (modem_stage != STAGE_WAITING) {
            ESP_LOGI(TAG, "Modem state %s, Start", stare_str);
        }
        switch (modem_stage) {
        case STAGE_DTE_LOSS:
            if (_ppp_network_stop(dte) != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_WAITING;
                xEventGroupClearBits(s_modem_evt_hdl, DTE_USB_DISCONNECT_BIT);
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_OFF_BIT);
                goto _stage_succeed;
            }
            break;
        case STAGE_WAITING:
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case STAGE_SYNC:
            if (esp_modem_default_start(dte) != ESP_OK) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_WAITING;
                if (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_MODE_ON_BIT) {
                    xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
                    ESP_LOGI(TAG, "Network Auto reconnecting ...");
                    esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_NET_DISCONN, NULL, 0, 0);
                    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_RECONNECTING_BIT);
                    modem_stage = STAGE_CHECK_SIM;
                }
                goto _stage_succeed;
            }
            break;
        case STAGE_STOP_PPP:
            if (_ppp_network_stop(dte) != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_SYNC;
                goto _stage_succeed;
            }
            break;
        case STAGE_CHECK_SIM:
            if (_check_sim_card() != true) {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_CHECK_SIGNAL;
                goto _stage_succeed;
            }
            break;
        case STAGE_CHECK_SIGNAL:
            if (_check_signal_quality() != true)  {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_CHECK_REGIST;
                goto _stage_succeed;
            }
            break;
        case STAGE_CHECK_REGIST:
            if (_check_network_registration() != true)  {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_START_PPP;
                goto _stage_succeed;
            }
            break;
        case STAGE_START_PPP:
            if (_ppp_network_start(dte) != true)  {
                retry_after_ms = 3000;
                ++stage_retry_times;
            } else {
                modem_stage = STAGE_WAIT_IP;
                goto _stage_succeed;
            }
            break;
        case STAGE_WAIT_IP: {
            //waiting 60s at most
            EventBits_t con_bits = xEventGroupWaitBits(s_modem_evt_hdl, (PPP_NET_CONNECT_BIT | MODEM_DESTROY_BIT), pdFALSE, pdFALSE, pdMS_TO_TICKS(60000));
            if (con_bits & MODEM_DESTROY_BIT) {
                break;
            }
            if (con_bits & PPP_NET_CONNECT_BIT) {
                xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_RECONNECTING_BIT);
                esp_event_post(MODEM_BOARD_EVENT, MODEM_EVENT_NET_CONN, NULL, 0, 0);
                modem_stage = STAGE_RUNNING;
                goto _stage_succeed;
            } else {
                stage_retry_times = RETRY_TIMEOUT;
                ESP_LOGW(TAG, "Modem Got IP timeout, retry from start!");
            }
        }
        break;
        case STAGE_RUNNING:
            // Ping to check network
            break;
        default:
            assert(0); //no stage get in here
_stage_succeed:
            ESP_LOGI(TAG, "Modem state %s, Success!", stare_str);
            stage_retry_times = 0;
            //add delay between each stage
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }
    }
    xEventGroupClearBits(s_modem_evt_hdl, (PPP_NET_MODE_ON_BIT | PPP_NET_MODE_OFF_BIT | MODEM_DESTROY_BIT | DTE_USB_RECONNECT_BIT | PPP_NET_RECONNECTING_BIT));
    ESP_LOGI(TAG, "Modem Daemon Task Deleted!");
    vTaskDelete(NULL);
}

esp_err_t modem_board_init(modem_config_t *config)
{
    MODEM_CHECK(s_modem_evt_hdl == NULL, "Modem already initialized", ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "iot_usbh_modem, version: %d.%d.%d", IOT_USBH_MODEM_VER_MAJOR, IOT_USBH_MODEM_VER_MINOR, IOT_USBH_MODEM_VER_PATCH);
    MODEM_CHECK(config != NULL && config->line_buffer_size && config->rx_buffer_size && config->tx_buffer_size, "Buffer size can not be 0", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(config != NULL && config->event_task_stack_size, "Task stack size can not be 0", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(config != NULL && config->event_task_priority > CONFIG_USBH_TASK_BASE_PRIORITY, "Task priority must > USB", ESP_ERR_INVALID_ARG);
    s_modem_evt_hdl = xEventGroupCreate();
    assert(s_modem_evt_hdl != NULL);
    // if set not enter ppp mode, daemon task will suspend
    if (config->flags & MODEM_FLAGS_INIT_NOT_ENTER_PPP) {
        modem_board_ppp_auto_connect(false);
    }
    /* Create Modem Daemon task */
    TaskHandle_t daemon_task_handle = NULL;
    xTaskCreate(_modem_daemon_task, "modem_daemon", config->event_task_stack_size, config, config->event_task_priority, &daemon_task_handle);
    assert(daemon_task_handle != NULL);
    xTaskNotifyGive(daemon_task_handle);
    // If auto enter ppp and block until ppp got ip
    if (((config->flags & MODEM_FLAGS_INIT_NOT_ENTER_PPP) == 0) && ((config->flags & MODEM_FLAGS_INIT_NOT_BLOCK) == 0)) {
        xEventGroupWaitBits(s_modem_evt_hdl, PPP_NET_CONNECT_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t modem_board_get_dns_info(esp_netif_dns_type_t type, esp_netif_dns_info_t *dns)
{
    MODEM_CHECK(dns != NULL && type <= ESP_NETIF_DNS_BACKUP, "Invalid DNS info", ESP_ERR_INVALID_ARG);

    switch (type) {
    case ESP_NETIF_DNS_MAIN:
        dns->ip = s_dns_ip_main;
        break;
    case ESP_NETIF_DNS_BACKUP:
        dns->ip = s_dns_ip_backup;
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t modem_board_get_signal_quality(int *rssi, int *ber)
{
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    esp_modem_dce_csq_ctx_t result;
    esp_err_t err = esp_modem_dce_get_signal_quality(s_dce, NULL, &result);
    if (err == ESP_OK) {
        if (rssi) {
            *rssi = result.rssi;
        }
        if (ber) {
            *ber = result.ber;
        }
    }
    return err;
}

esp_err_t modem_board_get_sim_cart_state(int *if_ready)
{
    MODEM_CHECK(if_ready != NULL, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    *if_ready = false;
    esp_modem_dce_read_pin(s_dce, NULL, if_ready);
    if ((*if_ready) == false) {
        esp_modem_dce_set_pin(s_dce, MODEM_SIM_PIN_PWD, NULL);
    } else {
        return ESP_OK;
    }

    return esp_modem_dce_read_pin(s_dce, NULL, &if_ready);
}

esp_err_t modem_board_get_operator_state(char *buf, size_t buf_size)
{
    MODEM_CHECK(buf != NULL && buf_size != 0, "arg can not be NULL", ESP_ERR_INVALID_ARG);
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    return esp_modem_dce_get_operator_name(s_dce, (void *)buf_size, (void *)buf);
}

esp_err_t modem_board_set_apn(const char *new_apn, bool force_enable)
{
    MODEM_CHECK(s_dce != NULL, "modem not ready", ESP_ERR_INVALID_STATE);
    if (esp_modem_dce_set_apn(s_dce, new_apn) != ESP_OK) {
        ESP_LOGI(TAG, "APN not change (:%s)", new_apn);
        return ESP_ERR_INVALID_STATE;
    }
    if (force_enable) {
        if (s_modem_evt_hdl) {
            xEventGroupSetBits(s_modem_evt_hdl, DTE_USB_RECONNECT_BIT);
        }
        ESP_LOGI(TAG, "re-dial after change APN");
    }
    return ESP_OK;
}

esp_err_t modem_board_ppp_auto_connect(bool enable)
{
    MODEM_CHECK(s_modem_evt_hdl != NULL, "modem not initialized", ESP_ERR_INVALID_STATE);
    if (enable) {
        xEventGroupClearBits(s_modem_evt_hdl, PPP_NET_AUTO_SUSPEND_USER_BIT);
    } else {
        xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_AUTO_SUSPEND_USER_BIT);
    }
    ESP_LOGI(TAG, "PPP Auto Connect %s", enable ? "Enable" : "Disable");
    return ESP_OK;
}

esp_err_t modem_board_ppp_start(uint32_t timeout_ms)
{
    MODEM_CHECK(s_modem_evt_hdl != NULL, "modem not initialized", ESP_ERR_INVALID_STATE);
    if (s_modem_evt_hdl == NULL) {
        ESP_LOGW(TAG, "modem not ready");
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_ON_BIT);
    uint32_t timeout_ms_step = 10;
    uint32_t waiting_ms = 0;
    while (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_MODE_ON_BIT) {
        vTaskDelay(pdMS_TO_TICKS(timeout_ms_step));
        waiting_ms += timeout_ms_step;
        if (waiting_ms >= timeout_ms) {
            break;
        }
    }
    if (waiting_ms >= timeout_ms) {
        ESP_LOGW(TAG, "PPP start timeout");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "PPP start succeed");
    return ESP_OK;
}

esp_err_t modem_board_ppp_stop(uint32_t timeout_ms)
{
    MODEM_CHECK(s_modem_evt_hdl != NULL, "modem not initialized", ESP_ERR_INVALID_STATE);
    xEventGroupSetBits(s_modem_evt_hdl, PPP_NET_MODE_OFF_BIT);
    uint32_t timeout_ms_step = 10;
    uint32_t waiting_ms = 0;
    while (xEventGroupGetBits(s_modem_evt_hdl) & PPP_NET_MODE_OFF_BIT) {
        vTaskDelay(pdMS_TO_TICKS(timeout_ms_step));
        waiting_ms += timeout_ms_step;
        if (waiting_ms >= timeout_ms) {
            break;
        }
    }
    if (waiting_ms >= timeout_ms) {
        ESP_LOGW(TAG, "PPP stop timeout");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "PPP stop succeed");
    return ESP_OK;
}
