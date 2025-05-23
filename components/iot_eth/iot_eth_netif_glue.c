/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"

const static char *TAG = "iot_eth.netif_glue";

typedef struct iot_eth_netif_glue_t iot_eth_netif_glue_t;

struct iot_eth_netif_glue_t {
    esp_netif_driver_base_t base;
    iot_eth_handle_t eth;
};

static esp_err_t eth_input_to_netif(iot_eth_handle_t eth_handle, uint8_t *buffer, size_t length, void *user_data)
{
#if CONFIG_ESP_NETIF_L2_TAP
    esp_err_t ret = ESP_OK;
    ret = esp_vfs_l2tap_eth_filter_frame(eth_handle, buffer, (size_t *)&length, info);
    if (length == 0) {
        return ret;
    }
#endif
    return esp_netif_receive((esp_netif_t *)user_data, buffer, length, NULL);
}

static void eth_l2_free(void *h, void* buffer)
{
    free(buffer);
}

static esp_err_t iot_eth_post_attach(esp_netif_t *esp_netif, void *args)
{
    uint8_t mac[6];
    iot_eth_netif_glue_t *glue = (iot_eth_netif_glue_t *)args;
    glue->base.netif = esp_netif;
    iot_eth_get_addr(glue->eth, mac);

    iot_eth_update_input_path(glue->eth, eth_input_to_netif, esp_netif);

    // set driver related config to esp-netif
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  glue->eth,
        .transmit = iot_eth_transmit,
        .driver_free_rx_buffer = eth_l2_free,
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);

    esp_netif_set_mac(esp_netif, mac);
    ESP_LOGI(TAG, "ethernet attached to netif");
    return ESP_OK;
}

static void eth_action_got_ip(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

static void eth_action_handle(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    iot_eth_handle_t eth_handle = *(iot_eth_handle_t *)event_data;
    iot_eth_netif_glue_t *netif_glue = handler_args;
    ESP_LOGI(TAG, "eth_action_handle: %p, %p, %" PRIi32 ", %p, %p", netif_glue, base, event_id, event_data, *(iot_eth_handle_t *)event_data);

    switch (event_id) {
    case IOT_ETH_EVENT_START:
        if (netif_glue->eth == eth_handle) {
            esp_netif_action_start(netif_glue->base.netif, base, event_id, event_data);
        }
        break;
    case IOT_ETH_EVENT_STOP:
        if (netif_glue->eth == eth_handle) {
            esp_netif_action_stop(netif_glue->base.netif, base, event_id, event_data);
        }
        break;
    case IOT_ETH_EVENT_CONNECTED:
        if (netif_glue->eth == eth_handle) {
            uint8_t mac[6];
            iot_eth_get_addr(netif_glue->eth, mac);
            esp_netif_set_mac(netif_glue->base.netif, mac);

            // TODO: get speed, then use esp_netif_set_link_speed
            esp_netif_action_connected(netif_glue->base.netif, base, event_id, event_data);
        }
        break;
    case IOT_ETH_EVENT_DISCONNECTED:
        if (netif_glue->eth == eth_handle) {
            esp_netif_action_disconnected(netif_glue->base.netif, base, event_id, event_data);
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown event id: %" PRIi32, event_id);
        break;
    }
}

static esp_err_t iot_eth_unregister_glue_event(iot_eth_netif_glue_handle_t eth_netif_glue)
{
    esp_err_t ret = esp_event_handler_unregister(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, eth_action_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to unregister eth_action_handle");
        return ret;
    }
    ret = esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_action_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to unregister eth_action_got_ip");
    }
    return ret;
}

static esp_err_t iot_eth_register_glue_event(iot_eth_netif_glue_handle_t eth_netif_glue)
{
    ESP_RETURN_ON_FALSE(eth_netif_glue, ESP_ERR_INVALID_ARG, TAG, "eth_netif_glue handle can't be null");

    esp_err_t ret = esp_event_handler_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, eth_action_handle, eth_netif_glue);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_action_got_ip, eth_netif_glue);
    if (ret != ESP_OK) {
        goto fail;
    }

    return ESP_OK;

fail:
    iot_eth_unregister_glue_event(eth_netif_glue);
    return ret;
}

esp_err_t iot_eth_del_netif_glue(iot_eth_netif_glue_handle_t eth_netif_glue)
{
    iot_eth_unregister_glue_event(eth_netif_glue);
    free(eth_netif_glue);
    return ESP_OK;
}

iot_eth_netif_glue_handle_t iot_eth_new_netif_glue(iot_eth_handle_t eth_hdl)
{
    iot_eth_netif_glue_t *glue = (iot_eth_netif_glue_t *)calloc(1, sizeof(iot_eth_netif_glue_t));
    if (!glue) {
        ESP_LOGE(TAG, "Failed to allocate memory for netif glue");
        return NULL;
    }
    glue->eth = eth_hdl;
    glue->base.post_attach = iot_eth_post_attach;

    if (iot_eth_register_glue_event(glue) != ESP_OK) {
        iot_eth_del_netif_glue(glue);
        return NULL;
    }
    return glue;
}
