/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "usbh_rndis.h"
#include "app_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_event.h"
#include "dhcpserver/dhcpserver_options.h"
#include "ping/ping_sock.h"

static const char *TAG = "4g_module";
static esp_netif_t *s_netif = NULL;
esp_netif_t *ap_netif = NULL;
static modem_wifi_config_t s_modem_wifi_config = MODEM_WIFI_DEFAULT_CONFIG();

void driver_free_rx_buffer(void *h, void* buffer)
{
    // assert(h == s_netif);
    assert(buffer != NULL);
    printf("!!! free %p\n", buffer);
    free(buffer - 44);
}

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

/** Event handler for Ethernet events */
static void eth_on_got_ip(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    // if (!example_is_our_netif(EXAMPLE_NETIF_DESC_ETH, event->esp_netif)) {
    //     return;
    // }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    ESP_LOGI(TAG, "Main DNS server : " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    esp_netif_ip_info_t ip_info = {};
    esp_netif_get_ip_info(event->esp_netif, &ip_info);
    ESP_LOGI(TAG, "Assigned IP address:"IPSTR ",", IP2STR(&ip_info.ip));
}

typedef struct {
    esp_netif_driver_base_t base;
    void *h;
} app_rndis_netif_driver_t;

esp_err_t app_rndis_attach_start(esp_netif_t * esp_netif, void *args)
{
    ESP_LOGI(TAG, "app_rndis_attach_start");
    app_rndis_netif_driver_t *driver = args;
    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = (void *)1,                // not using an instance, USB-NCM is a static singleton (must be != NULL)
        .transmit = usbh_rndis_eth_output,  // point to static Tx function
        .driver_free_rx_buffer = driver_free_rx_buffer,    // point to Free Rx buffer function
    };
    driver->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_cfg));
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    usbh_rndis_init();

    usbh_rndis_create();

    usbh_rndis_open();

    ap_netif = modem_wifi_ap_init();
    assert(ap_netif != NULL);
    ESP_ERROR_CHECK(modem_wifi_set(&s_modem_wifi_config));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();

    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = (void *)1,                // not using an instance, USB-NCM is a static singleton (must be != NULL)
        .transmit = usbh_rndis_eth_output,  // point to static Tx function
        .driver_free_rx_buffer = driver_free_rx_buffer,    // point to Free Rx buffer function
    };

    esp_netif_config_t spi_config = {
        .base = &esp_netif_config,
        .driver = &driver_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    // 2) Use static config for driver's config pointing only to static transmit and free functions
    s_netif = esp_netif_new(&spi_config);

    // uint8_t *mac = usbh_rndis_get_mac();
    // uint8_t mac[6];
    // esp_wifi_get_mac(WIFI_IF_STA, mac);
    uint8_t mac[6] = {  0x01, 0x01, 0x5E, 0x01, 0x01, 0x01 };
    esp_netif_set_mac(s_netif, mac);

    app_rndis_netif_driver_t *driver = calloc(1, sizeof(app_rndis_netif_driver_t));
    driver->base.post_attach = app_rndis_attach_start;
    driver->base.netif = s_netif;
    driver->h = s_netif;

    esp_netif_attach(s_netif, driver);
    esp_netif_action_start(s_netif, 0, 0, 0);
    esp_netif_action_connected(s_netif, NULL, 0, NULL);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_on_got_ip, NULL));

    xTaskCreate(usbh_rndis_rx_thread, "usb_lib_task", 4096, s_netif, 5, NULL);

    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    char *ping_addr_s = NULL;
    ping_addr_s = "8.8.8.8";
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ipaddr_aton(ping_addr_s, &target_addr);
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = 2000;
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

    while (1) {
        // esp_ping_start(ping);
        usbh_rndis_keepalive();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    // TODO: keep alive
}
