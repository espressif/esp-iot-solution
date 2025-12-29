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
#include "../lwip/esp_netif_lwip_internal.h"

const static char *TAG = "iot_eth.netif_glue";

typedef struct iot_eth_netif_glue_t iot_eth_netif_glue_t;

struct iot_eth_netif_glue_t {
    esp_netif_driver_base_t base;
    iot_eth_handle_t eth;
    esp_event_handler_instance_t eth_event_handler_instance;
    esp_event_handler_instance_t ip_got_ip_handler_instance;
    esp_event_handler_instance_t ppp_got_ip_handler_instance;
#ifdef CONFIG_LWIP_IPV6
    esp_event_handler_instance_t ip_got_ip6_handler_instance;
#endif
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

static void eth_l2_free(void *h, void *buffer)
{
    free(buffer);
}

static esp_err_t iot_eth_post_attach(esp_netif_t *esp_netif, void *args)
{
    iot_eth_netif_glue_t *glue = (iot_eth_netif_glue_t *)args;
    glue->base.netif = esp_netif;

    iot_eth_update_input_path(glue->eth, eth_input_to_netif, esp_netif);

    // set driver related config to esp-netif
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  glue->eth,
        .transmit = iot_eth_transmit,
    };
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    bool is_ppp = ESP_NETIF_IS_POINT2POINT_TYPE(esp_netif, PPP_LWIP_NETIF);
#else
    bool is_ppp = _IS_NETIF_POINT2POINT_TYPE(esp_netif, PPP_LWIP_NETIF);
#endif
    if (!is_ppp) {
        ESP_LOGD(TAG, "non-ppp netif");
        driver_ifconfig.driver_free_rx_buffer = eth_l2_free;
    }
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    ESP_LOGD(TAG, "ethernet attached to netif(%p)", esp_netif);
    return ESP_OK;
}

static void eth_action_got_ip(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    iot_eth_netif_glue_t *netif_glue = handler_args;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    esp_netif_t *netif = event->esp_netif;
    if (netif_glue->base.netif != netif) {
        return;
    }

    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "netif \"%s\" Got IP Address", esp_netif_get_desc(netif));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));

    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
    ESP_LOGI(TAG, "Main DNS: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
    ESP_LOGI(TAG, "Backup DNS: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

#ifdef CONFIG_LWIP_IPV6
static const char *example_ipv6_addr_types_to_str[6] = {
    "ESP_IP6_ADDR_IS_UNKNOWN",
    "ESP_IP6_ADDR_IS_GLOBAL",
    "ESP_IP6_ADDR_IS_LINK_LOCAL",
    "ESP_IP6_ADDR_IS_SITE_LOCAL",
    "ESP_IP6_ADDR_IS_UNIQUE_LOCAL",
    "ESP_IP6_ADDR_IS_IPV4_MAPPED_IPV6"
};

static void eth_action_got_ipv6(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    iot_eth_netif_glue_t *netif_glue = handler_args;
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    esp_netif_t *netif = event->esp_netif;
    if (netif_glue->base.netif != netif) {
        return;
    }

    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
    ESP_LOGI(TAG, "netif \"%s\" Got IPv6 address: " IPV6STR ", type: %s",
             esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip),
             example_ipv6_addr_types_to_str[ipv6_type]);
}
#endif

static void eth_action_handle(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    iot_eth_handle_t eth_handle = *(iot_eth_handle_t *)event_data;
    iot_eth_netif_glue_t *netif_glue = handler_args;

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
            uint8_t mac[6] = {0};
            esp_err_t mac_ok = iot_eth_get_addr(netif_glue->eth, mac);
            if (mac_ok == ESP_OK) {
                ESP_LOGI(TAG, "Set MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                esp_netif_set_mac(netif_glue->base.netif, mac);
            }

            // TODO: get speed, then use esp_netif_set_link_speed
            esp_netif_action_connected(netif_glue->base.netif, base, event_id, event_data);
#ifdef CONFIG_LWIP_IPV6
            esp_netif_create_ip6_linklocal(netif_glue->base.netif);
#endif
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
    esp_err_t ret = ESP_OK;

    if (eth_netif_glue->eth_event_handler_instance) {
        ret = esp_event_handler_instance_unregister(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, eth_netif_glue->eth_event_handler_instance);
        ESP_RETURN_ON_ERROR(ret, TAG, "Fail to unregister eth_action_handle (%s)", esp_err_to_name(ret));
        eth_netif_glue->eth_event_handler_instance = NULL;
    }

    if (eth_netif_glue->ip_got_ip_handler_instance) {
        ret = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_netif_glue->ip_got_ip_handler_instance);
        ESP_RETURN_ON_ERROR(ret, TAG, "Fail to unregister eth_action_got_ip (%s)", esp_err_to_name(ret));
        eth_netif_glue->ip_got_ip_handler_instance = NULL;
    }

#ifdef CONFIG_LWIP_IPV6
    if (eth_netif_glue->ip_got_ip6_handler_instance) {
        ret = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_GOT_IP6, eth_netif_glue->ip_got_ip6_handler_instance);
        ESP_RETURN_ON_ERROR(ret, TAG, "Fail to unregister eth_action_got_ip (%s)", esp_err_to_name(ret));
        eth_netif_glue->ip_got_ip6_handler_instance = NULL;
    }
#endif

    if (eth_netif_glue->ppp_got_ip_handler_instance) {
        ret = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_PPP_GOT_IP, eth_netif_glue->ppp_got_ip_handler_instance);
        ESP_RETURN_ON_ERROR(ret, TAG, "Fail to unregister eth_action_got_ip (%s)", esp_err_to_name(ret));
        eth_netif_glue->ppp_got_ip_handler_instance = NULL;
    }

    return ret;
}

static esp_err_t iot_eth_register_glue_event(iot_eth_netif_glue_handle_t eth_netif_glue)
{
    ESP_RETURN_ON_FALSE(eth_netif_glue, ESP_ERR_INVALID_ARG, TAG, "eth_netif_glue handle can't be null");

    // Initialize handler instances to NULL
    eth_netif_glue->eth_event_handler_instance = NULL;
    eth_netif_glue->ip_got_ip_handler_instance = NULL;
    eth_netif_glue->ppp_got_ip_handler_instance = NULL;

    esp_err_t ret = ESP_OK;
    ret = esp_event_handler_instance_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, eth_action_handle, eth_netif_glue, &eth_netif_glue->eth_event_handler_instance);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Fail to register eth_action_handle (%s)", esp_err_to_name(ret));

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_action_got_ip, eth_netif_glue, &eth_netif_glue->ip_got_ip_handler_instance);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Fail to register eth_action_got_ip for ETH_GOT_IP (%s)", esp_err_to_name(ret));

#ifdef CONFIG_LWIP_IPV6
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_GOT_IP6, eth_action_got_ipv6, eth_netif_glue, &eth_netif_glue->ip_got_ip6_handler_instance);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Fail to register eth_action_got_ip for ETH_GOT_IP6 (%s)", esp_err_to_name(ret));
#endif

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, eth_action_got_ip, eth_netif_glue, &eth_netif_glue->ppp_got_ip_handler_instance);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Fail to register eth_action_got_ip for PPP_GOT_IP (%s)", esp_err_to_name(ret));
    return ESP_OK;
err:
    // Clean up already registered handlers
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
