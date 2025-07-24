/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "dhcpserver/dhcpserver_options.h"
#include "ping/ping_sock.h"
#include "iot_usbh_rndis.h"
#include "app_wifi.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"
#include "iot_usbh_cdc.h"
#include "at_3gpp_ts_27_007.h"
#include "network_test.h"
#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
#include "modem_http_config.h"
#endif

static const char *TAG = "RNDIS_4G_MODULE";

static EventGroupHandle_t s_event_group;
static iot_eth_driver_t *s_rndis_eth_driver = NULL;
static esp_netif_t *s_rndis_netif = NULL;
static modem_wifi_config_t s_modem_wifi_config = MODEM_WIFI_DEFAULT_CONFIG();

#define EVENT_GOT_IP_BIT (BIT0)
#define EVENT_AT_READY_BIT (BIT1)

#if CONFIG_EXAMPLE_ENABLE_AT_CMD
typedef struct {
    usbh_cdc_port_handle_t cdc_port;      /*!< CDC port handle */
    at_handle_t at_handle;                /*!< AT command parser handle */
} at_ctx_t;

at_ctx_t g_at_ctx = {0};

static esp_err_t _at_send_cmd(const char *command, size_t length, void *usr_data)
{
    at_ctx_t *at_ctx = (at_ctx_t *)usr_data;
    return usbh_cdc_write_bytes(at_ctx->cdc_port, (const uint8_t *)command, length, pdMS_TO_TICKS(500));
}

static void _at_port_closed_cb(usbh_cdc_port_handle_t cdc_port_handle, void *arg)
{
    at_ctx_t *at_ctx = (at_ctx_t *)arg;
    ESP_LOGI(TAG, "AT port closed");
    at_ctx->cdc_port = NULL;

    if (at_ctx->at_handle) {
        modem_at_stop(at_ctx->at_handle);
        modem_at_parser_destroy(at_ctx->at_handle);
        at_ctx->at_handle = NULL;
    }
}

static void _at_recv_data_cb(usbh_cdc_port_handle_t cdc_port_handle, void *arg)
{
    at_ctx_t *at_ctx = (at_ctx_t *)arg;
    size_t length = 0;
    usbh_cdc_get_rx_buffer_size(cdc_port_handle, &length);
    char *buffer;
    size_t buffer_remain;
    modem_at_get_response_buffer(at_ctx->at_handle, &buffer, &buffer_remain);
    if (buffer_remain < length) {
        length = buffer_remain;
        ESP_LOGE(TAG, "data size is too big, truncated to %d", length);
    }
    usbh_cdc_read_bytes(cdc_port_handle, (uint8_t *)buffer, &length, 0);
    // Parse the AT command response
    modem_at_write_response_done(at_ctx->at_handle, length);
}

static esp_err_t at_init()
{
    ESP_LOGI(TAG, "AT init");
    // Open a CDC port for AT command
    usbh_cdc_port_handle_t _port = usb_rndis_get_cdc_port_handle(s_rndis_eth_driver);
    usb_device_handle_t _dev_hdl = NULL;
    ESP_ERROR_CHECK(usbh_cdc_get_dev_handle(_port, &_dev_hdl));
    usb_device_info_t device_info;
    ESP_ERROR_CHECK(usb_host_device_info(_dev_hdl, &device_info));
    usbh_cdc_port_config_t cdc_port_config = {
        .dev_addr = device_info.dev_addr,
        .itf_num = CONFIG_EXAMPLE_AT_INTERFACE_NUM,
        .in_transfer_buffer_size = 512,
        .out_transfer_buffer_size = 512,
        .cbs = {
            .notif_cb = NULL,
            .recv_data = _at_recv_data_cb,
            .closed = _at_port_closed_cb,
            .user_data = &g_at_ctx,
        },
    };
    ESP_RETURN_ON_ERROR(usbh_cdc_port_open(&cdc_port_config, &g_at_ctx.cdc_port), TAG, "Failed to open CDC port");

    // init the AT command parser
    modem_at_config_t at_config = {
        .send_buffer_length = 256,
        .recv_buffer_length = 256,
        .io = {
            .send_cmd = _at_send_cmd,
            .usr_data = &g_at_ctx,
        }
    };
    g_at_ctx.at_handle = modem_at_parser_create(&at_config);
    ESP_ERROR_CHECK(g_at_ctx.at_handle != NULL ? ESP_OK : ESP_FAIL);

    return modem_at_start(g_at_ctx.at_handle);
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
#if CONFIG_EXAMPLE_ENABLE_AT_CMD
            if (at_init() == ESP_OK) {
                xEventGroupSetBits(s_event_group, EVENT_AT_READY_BIT);
            }
#endif
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
        ESP_LOGI(TAG, "GOT_IP");
        xEventGroupSetBits(s_event_group, EVENT_GOT_IP_BIT);
        start_ping_timer();
    }
}

static void install_rndis(uint16_t idVendor, uint16_t idProduct, const char *netif_name)
{
    esp_err_t ret = ESP_OK;
    iot_eth_handle_t eth_handle = NULL;
    iot_eth_netif_glue_handle_t glue = NULL;

    usb_device_match_id_t *dev_match_id = calloc(2, sizeof(usb_device_match_id_t));
    dev_match_id[0].match_flags = USB_DEVICE_ID_MATCH_VID_PID;
    dev_match_id[0].idVendor = idVendor;
    dev_match_id[0].idProduct = idProduct;
    memset(&dev_match_id[1], 0, sizeof(usb_device_match_id_t)); // end of list
    iot_usbh_rndis_config_t rndis_cfg = {
        .match_id_list = dev_match_id,
    };

    ret = iot_eth_new_usb_rndis(&rndis_cfg, &s_rndis_eth_driver);
    if (ret != ESP_OK || s_rndis_eth_driver == NULL) {
        ESP_LOGE(TAG, "Failed to create USB RNDIS driver");
        return;
    }

    iot_eth_config_t eth_cfg = {
        .driver = s_rndis_eth_driver,
        .stack_input = NULL,
    };
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB RNDIS driver");
        return;
    }

    esp_netif_inherent_config_t _inherent_eth_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    _inherent_eth_config.if_key = netif_name;
    _inherent_eth_config.if_desc = netif_name;
    esp_netif_config_t netif_cfg = {
        .base = &_inherent_eth_config,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };
    s_rndis_netif = esp_netif_new(&netif_cfg);
    if (s_rndis_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create network interface");
        return;
    }

    glue = iot_eth_new_netif_glue(eth_handle);
    if (glue == NULL) {
        ESP_LOGE(TAG, "Failed to create netif glue");
        return;
    }
    esp_netif_attach(s_rndis_netif, glue);
    iot_eth_start(eth_handle);
}

void app_main(void)
{
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
    esp_event_handler_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, iot_event_handle, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, iot_event_handle, NULL);

    // install usbh cdc driver
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = configMAX_PRIORITIES - 1,
        .task_coreid = 0,
        .skip_init_usb_host_driver = false,
    };
    ESP_ERROR_CHECK(usbh_cdc_driver_install(&config));

    install_rndis(USB_DEVICE_VENDOR_ANY, USB_DEVICE_PRODUCT_ANY, "USB RNDIS0");
    xEventGroupWaitBits(s_event_group, EVENT_GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
    modem_http_get_nvs_wifi_config(&s_modem_wifi_config);
    modem_http_init(&s_modem_wifi_config);
#endif
    app_wifi_main(&s_modem_wifi_config);
    esp_netif_set_default_netif(s_rndis_netif);

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_event_group, EVENT_AT_READY_BIT | EVENT_GOT_IP_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
#if CONFIG_EXAMPLE_ENABLE_AT_CMD
        if (bits & EVENT_AT_READY_BIT) {
            at_cmd_set_echo(g_at_ctx.at_handle, true);

            esp_modem_at_csq_t result;
            esp_err_t err = at_cmd_get_signal_quality(g_at_ctx.at_handle, &result);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Signal quality rssi: %d", result.rssi);
            }

            // Print modem information
            char str[128] = {0};
            at_cmd_get_manufacturer_id(g_at_ctx.at_handle, str, sizeof(str));
            ESP_LOGI(TAG, "Modem manufacturer ID: %s", str);
            str[0] = '\0'; // clear the string buffer
            at_cmd_get_module_id(g_at_ctx.at_handle, str, sizeof(str));
            ESP_LOGI(TAG, "Modem module ID: %s", str);
            str[0] = '\0'; // clear the string buffer
            at_cmd_get_revision_id(g_at_ctx.at_handle, str, sizeof(str));
            ESP_LOGI(TAG, "Modem revision ID: %s", str);
            str[0] = '\0'; // clear the string buffer
            at_cmd_get_pdp_context(g_at_ctx.at_handle, str, sizeof(str));
            ESP_LOGI(TAG, "Modem PDP context: %s", str);
        }
#endif
        if (bits & EVENT_GOT_IP_BIT) {
            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a bit for DNS to be ready
            test_query_public_ip(); // Query public IP via HTTP
#if CONFIG_EXAMPLE_USB_RNDIS_DOWNLOAD_SPEED_TEST
            test_download_speed("http://mirrors.ustc.edu.cn/ros/ubuntu/db/references.db");
#endif
        }
    }
}
