/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "iot_eth.h"
#include "iot_eth_types.h"

static const char *TAG = "iot_eth";

typedef struct {
    iot_eth_driver_t *driver;
    iot_eth_mediator_t mediator;
    esp_netif_t *netif;
    _Atomic iot_eth_link_t link;
    uint8_t mac[6];
    void *user_data;
    esp_err_t (*stack_input)(iot_eth_handle_t handle, uint8_t *data, size_t len, void *user_data);
    esp_err_t (*on_lowlevel_init_done)(iot_eth_handle_t handle);
    esp_err_t (*on_lowlevel_deinit)(iot_eth_handle_t handle);
} iot_eth_t;

static esp_err_t iot_eth_stack_input_default(iot_eth_handle_t handle, uint8_t *data, size_t len, void *user_data)
{
    iot_eth_t *eth = (iot_eth_t *)handle;
    return esp_netif_receive(eth->netif, data, len, NULL);
}

esp_err_t iot_eth_stack_input(iot_eth_mediator_t *mediator, uint8_t *data, size_t len)
{
    iot_eth_t *eth = __containerof(mediator, iot_eth_t, mediator);
    if (eth->stack_input != NULL) {
        return eth->stack_input(eth, data, len, eth->user_data);
    }
    // if no stack input, free the data
    free(data);
    return ESP_OK;
}

static esp_err_t iot_eth_transmit_default(void *handle, void *data, size_t len)
{
    return iot_eth_transmit(handle, data, len);
}

esp_err_t iot_eth_transmit(iot_eth_handle_t handle, uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "buf can't be NULL");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "buf len can't be zero");
    iot_eth_t *eth = (iot_eth_t *)handle;
    esp_err_t ret = ESP_OK;
    if (atomic_load(&eth->link) != IOT_ETH_LINK_UP) {
        ret = ESP_ERR_INVALID_STATE;
        goto err;
    }
    ret = eth->driver->transmit(eth->driver, data, len);
err:
    return ret;
}

static void iot_eth_free_rx_buffer(void *h, void *buffer)
{
    if (buffer) {
        free(buffer);
    }
}

static void iot_eth_got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

esp_err_t iot_eth_on_stage_changed(iot_eth_mediator_t *mediator, iot_eth_stage_t stage, void *arg)
{
    iot_eth_t *eth = __containerof(mediator, iot_eth_t, mediator);
    switch (stage) {
    case IOT_ETH_STAGE_LLINIT:
        if (eth->on_lowlevel_init_done != NULL) {
            return eth->on_lowlevel_init_done(eth);
        }
        break;
    case IOT_ETH_STAGE_DEINIT:
        if (eth->on_lowlevel_deinit != NULL) {
            return eth->on_lowlevel_deinit(eth);
        }
        break;
    case IOT_ETH_STAGE_GET_MAC:
        eth->driver->get_addr(eth->driver, eth->mac);
        ESP_LOGI(TAG, "%s: MAC address: %02X:%02X:%02X:%02X:%02X:%02X", eth->driver->name, eth->mac[0], eth->mac[1], eth->mac[2], eth->mac[3], eth->mac[4], eth->mac[5]);
        if (eth->netif) {
            esp_netif_set_mac(eth->netif, eth->mac);
        }
        break;
    case IOT_ETH_STAGE_LINK:
        iot_eth_link_t *link = (iot_eth_link_t *)arg;
        atomic_store(&eth->link, *link);
        if (*link == IOT_ETH_LINK_UP) {
            if (eth->netif) {
                esp_netif_action_start(eth->netif, NULL, 0, NULL);
                esp_netif_action_connected(eth->netif, NULL, 0, NULL);
                ESP_LOGI(TAG, "%s: Link up", eth->driver->name);
            }
        } else {
            if (eth->netif) {
                esp_netif_action_disconnected(eth->netif, NULL, 0, NULL);
                esp_netif_action_stop(eth->netif, NULL, 0, NULL);
                ESP_LOGI(TAG, "%s: Link down", eth->driver->name);
            }
        }
        break;
    case IOT_ETH_STATE_PAUSE:
        break;
    }
    return ESP_OK;
}

esp_err_t iot_eth_install(iot_eth_config_t *config, iot_eth_handle_t *handle)
{
    ESP_LOGI(TAG, "IoT ETH Version: %d.%d.%d", IOT_ETH_VER_MAJOR, IOT_ETH_VER_MINOR, IOT_ETH_VER_PATCH);
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    // Init TCP/IP network interface (should be called only once in application)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // Already initialized is OK
        ESP_LOGE(TAG, "Failed to initialize TCP/IP stack");
        return ret;
    }

    // Create default event loop that runs in background if not already created
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // Already initialized is OK
        ESP_LOGE(TAG, "Failed to create event loop");
        return ret;
    }

    iot_eth_t *eth = calloc(1, sizeof(iot_eth_t));
    ESP_RETURN_ON_FALSE(eth != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for iot_eth_t");
    eth->driver = config->driver;
    // default stack input to esp_netif_receive(TCP/IP stack)
    if (config->stack_input == NULL) {
        ret = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &iot_eth_got_ip_event_handler, NULL);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to register event handler");

        eth->stack_input = iot_eth_stack_input_default;
        // Prepare network interface for the driver
        esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
        eth->netif = esp_netif_new(&netif_cfg);
        ESP_RETURN_ON_FALSE(eth->netif != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create netif");
        esp_netif_driver_ifconfig_t driver_ifconfig = {
            .handle = eth,
            .transmit = iot_eth_transmit_default,
            .driver_free_rx_buffer = iot_eth_free_rx_buffer,
        };

        ret = esp_netif_set_driver_config(eth->netif, &driver_ifconfig);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to set driver config");
        esp_netif_set_hostname(eth->netif, eth->driver->name);
    } else {
        eth->stack_input = config->stack_input;
    }
    eth->on_lowlevel_init_done = config->on_lowlevel_init_done;
    eth->on_lowlevel_deinit = config->on_lowlevel_deinit;
    eth->mediator.stack_input = iot_eth_stack_input;
    eth->mediator.on_stage_changed = iot_eth_on_stage_changed;
    eth->driver->set_mediator(eth->driver, &eth->mediator);
    ret = eth->driver->init(eth->driver);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to init driver");
    *handle = (iot_eth_handle_t)eth;
    return ESP_OK;
err:
    if (eth->netif != NULL) {
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &iot_eth_got_ip_event_handler);
        esp_netif_destroy(eth->netif);
        eth->netif = NULL;
    }
    free(eth);
    return ret;
}

esp_err_t iot_eth_uninstall(iot_eth_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    esp_err_t ret = eth->driver->deinit(eth->driver);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to deinit driver");

    if (eth->netif != NULL) {
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &iot_eth_got_ip_event_handler);
        esp_netif_destroy(eth->netif);
        eth->netif = NULL;
    }
    free(eth);
    return ESP_OK;
err:
    return ret;
}

esp_err_t iot_eth_start(iot_eth_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    return eth->driver->start(eth->driver);
}

esp_err_t iot_eth_stop(iot_eth_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    return eth->driver->stop(eth->driver);
}

esp_err_t iot_eth_get_addr(iot_eth_handle_t handle, uint8_t *mac)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    return eth->driver->get_addr(eth->driver, mac);
}
