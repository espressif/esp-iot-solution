/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include "esp_netif.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_modem_dte.h"
#include "esp_log.h"
#include "esp_netif_ppp.h"

static const char *TAG = "esp-modem-netif";
#define PPP_CODE2STR(code) {code, #code}

typedef struct {
    int code;
    const char *msg;
} _ppp_evt_msg_t;

static const _ppp_evt_msg_t ppp_evt_msg_table[] = {
    PPP_CODE2STR(NETIF_PPP_ERRORNONE),
    PPP_CODE2STR(NETIF_PPP_ERRORPARAM),
    PPP_CODE2STR(NETIF_PPP_ERROROPEN),
    PPP_CODE2STR(NETIF_PPP_ERRORDEVICE),
    PPP_CODE2STR(NETIF_PPP_ERRORALLOC),
    PPP_CODE2STR(NETIF_PPP_ERRORUSER),
    PPP_CODE2STR(NETIF_PPP_ERRORCONNECT),
    PPP_CODE2STR(NETIF_PPP_ERRORAUTHFAIL),
    PPP_CODE2STR(NETIF_PPP_ERRORPROTOCOL),
    PPP_CODE2STR(NETIF_PPP_ERRORPEERDEAD),
    PPP_CODE2STR(NETIF_PPP_ERRORIDLETIMEOUT),
    PPP_CODE2STR(NETIF_PPP_ERRORCONNECTTIME),
    PPP_CODE2STR(NETIF_PPP_ERRORLOOPBACK),
    PPP_CODE2STR(NETIF_PPP_PHASE_DEAD),
    PPP_CODE2STR(NETIF_PPP_PHASE_MASTER),
    PPP_CODE2STR(NETIF_PPP_PHASE_HOLDOFF),
    PPP_CODE2STR(NETIF_PPP_PHASE_INITIALIZE),
    PPP_CODE2STR(NETIF_PPP_PHASE_SERIALCONN),
    PPP_CODE2STR(NETIF_PPP_PHASE_DORMANT),
    PPP_CODE2STR(NETIF_PPP_PHASE_ESTABLISH),
    PPP_CODE2STR(NETIF_PPP_PHASE_AUTHENTICATE),
    PPP_CODE2STR(NETIF_PPP_PHASE_CALLBACK),
    PPP_CODE2STR(NETIF_PPP_PHASE_NETWORK),
    PPP_CODE2STR(NETIF_PPP_PHASE_RUNNING),
    PPP_CODE2STR(NETIF_PPP_PHASE_TERMINATE),
    PPP_CODE2STR(NETIF_PPP_PHASE_DISCONNECT),
    PPP_CODE2STR(NETIF_PPP_CONNECT_FAILED),
};

/**
 * @brief ESP32 Modem handle to be used as netif IO object
 */
struct esp_modem_netif_driver_s {
    esp_netif_driver_base_t base;           /*!< base structure reserved as esp-netif driver */
    esp_modem_dte_t        *dte;            /*!< ptr to the esp_modem objects (DTE) */
};

const char *esp_modem_netif_event_to_name(int code)
{
    size_t i;
    for (i = 0; i < sizeof(ppp_evt_msg_table) / sizeof(ppp_evt_msg_table[0]); ++i) {
        if (ppp_evt_msg_table[i].code == code) {
            return ppp_evt_msg_table[i].msg;
        }
    }
    return "unknown ppp event";
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    esp_modem_dte_t *dte = arg;
    if (event_id < NETIF_PP_PHASE_OFFSET) {
        ESP_LOGI(TAG, "PPP state changed event %"PRIi32": (%s)", event_id, esp_modem_netif_event_to_name(event_id));
        // only notify the modem on state/error events, ignoring phase transitions
        esp_modem_notify_ppp_netif_closed(dte);
    }
}

/**
 * @brief Transmit function called from esp_netif to output network stack data
 *
 * Note: This API has to conform to esp-netif transmit prototype
 *
 * @param h Opaque pointer representing esp-netif driver, esp_dte in this case of esp_modem
 * @param data data buffer
 * @param length length of data to send
 *
 * @return ESP_OK on success
 */
static esp_err_t esp_modem_dte_transmit(void *h, void *buffer, size_t len)
{
    esp_modem_dte_t *dte = h;
    if (dte->send_data(dte, (const char *)buffer, len) > 0) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief Post attach adapter for esp-modem
 *
 * Used to exchange internal callbacks, context between esp-netif nad modem-netif
 *
 * @param esp_netif handle to esp-netif object
 * @param args pointer to modem-netif driver
 *
 * @return ESP_OK on success, modem-start error code if starting failed
 */
static esp_err_t esp_modem_post_attach_init(esp_netif_t * esp_netif, void * args)
{
    esp_modem_netif_driver_t *driver = args;
    esp_modem_dte_t *dte = driver->dte;
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = NULL,
        .transmit = esp_modem_dte_transmit,
        .handle = dte
    };
    driver->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    // // check if PPP error events are enabled, if not, do enable the error occurred/state changed
    // // to notify the modem layer when switching modes
    esp_netif_ppp_config_t ppp_config = {
        .ppp_error_event_enabled = true,
        .ppp_phase_event_enabled = true,
    };
    esp_netif_ppp_set_params(esp_netif, &ppp_config);

    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, dte));
    return ESP_OK;
}

/**
 * @brief Data path callback from esp-modem to pass data to esp-netif
 *
 * @param buffer data pointer
 * @param len data length
 * @param context context data used for esp-modem-netif handle
 *
 * @return ESP_OK on success
 */
static esp_err_t modem_netif_receive_cb(void *buffer, size_t len, void *context)
{
    esp_modem_netif_driver_t *driver = context;
    esp_netif_receive(driver->base.netif, buffer, len, NULL);
    return ESP_OK;
}

esp_modem_netif_driver_t *esp_modem_netif_new(esp_modem_dte_t *dte)
{
    esp_modem_netif_driver_t *driver = esp_modem_netif_setup(dte);
    return driver;
}

esp_modem_netif_driver_t *esp_modem_netif_setup(esp_modem_dte_t *dte)
{
    esp_modem_netif_driver_t *driver =  calloc(1, sizeof(esp_modem_netif_driver_t));
    if (driver == NULL) {
        ESP_LOGE(TAG, "Cannot allocate esp_modem_netif_driver_t");
        goto drv_create_failed;
    }
    esp_err_t err = esp_modem_set_rx_cb(dte, modem_netif_receive_cb, driver);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_rx_cb failed with: %d", err);
        goto drv_create_failed;
    }

    driver->base.post_attach = esp_modem_post_attach_init;
    driver->dte = dte;
    return driver;

drv_create_failed:
    return NULL;
}

void esp_modem_netif_destroy(esp_modem_netif_driver_t *driver)
{
    esp_netif_destroy(driver->base.netif);
    return esp_modem_netif_teardown(driver);
}

void esp_modem_netif_teardown(esp_modem_netif_driver_t *driver)
{
    free(driver);
}

esp_err_t esp_modem_netif_clear_default_handlers(esp_modem_netif_driver_t *h)
{
    esp_modem_netif_driver_t *driver = h;
    esp_err_t ret;
    ret = esp_modem_remove_event_handler(driver->dte, esp_netif_action_start);
    if (ret != ESP_OK) {
        goto clear_event_failed;
    }
    ret = esp_modem_remove_event_handler(driver->dte, esp_netif_action_stop);
    if (ret != ESP_OK) {
        goto clear_event_failed;
    }
    return ESP_OK;

clear_event_failed:
    ESP_LOGE(TAG, "Failed to unregister event handlers");
    return ESP_FAIL;

}

esp_err_t esp_modem_netif_set_default_handlers(esp_modem_netif_driver_t *h, esp_netif_t * esp_netif)
{
    esp_modem_netif_driver_t *driver = h;
    esp_err_t ret;
    ret = esp_modem_set_event_handler(driver->dte, esp_netif_action_start, ESP_MODEM_EVENT_PPP_START, esp_netif);
    if (ret != ESP_OK) {
        goto set_event_failed;
    }
    ret = esp_modem_set_event_handler(driver->dte, esp_netif_action_stop, ESP_MODEM_EVENT_PPP_STOP, esp_netif);
    if (ret != ESP_OK) {
        goto set_event_failed;
    }
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected, esp_netif);
    if (ret != ESP_OK) {
        goto set_event_failed;
    }
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected, esp_netif);
    if (ret != ESP_OK) {
        goto set_event_failed;
    }
    return ESP_OK;

set_event_failed:
    ESP_LOGE(TAG, "Failed to register event handlers");
    esp_modem_netif_clear_default_handlers(driver);
    return ESP_FAIL;
}
