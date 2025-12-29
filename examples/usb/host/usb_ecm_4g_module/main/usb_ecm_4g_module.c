/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/
#include <stdio.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "dhcpserver/dhcpserver_options.h"
#include "esp_http_client.h"
#include "esp_console.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "iperf_cmd.h"
#include "iot_usbh_ecm.h"
#include "app_wifi.h"
#include "iot_eth.h"
#include "iot_eth_netif_glue.h"
#include "iot_usbh_cdc.h"
#include "at_3gpp_ts_27_007.h"
#include "network_test.h"
#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
#include "modem_http_config.h"
#endif

static const char *TAG = "ECM_4G_MODULE";

static EventGroupHandle_t s_event_group;
static iot_eth_driver_t *s_ecm_eth_driver = NULL;
static esp_netif_t *s_ecm_netif = NULL;
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
    usbh_cdc_port_handle_t _port = usb_ecm_get_cdc_port_handle(s_ecm_eth_driver);
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

#ifdef CONFIG_EXAMPLE_USB_ECM_LIGHT_SLEEP_TEST

#define GPIO_MODULE_RST_NUM     CONFIG_EXAMPLE_MODULE_RST_GPIO_NUM
#define GPIO_WAKEUP_NUM         CONFIG_EXAMPLE_MODULE_WAKEUP_GPIO_NUM
#define GPIO_WAKEUP_LEVEL       CONFIG_EXAMPLE_MODULE_WAKEUP_LEVEL

#ifdef CONFIG_EXAMPLE_ENABLE_AT_CMD

#include "../priv_include/modem_at_internal.h"

static esp_err_t at_cmd_power_down(at_handle_t ath, void *param, void *result)
{
    // This command is used to power down the modem,
    // you can modify it according to your module's AT command set
    ESP_LOGW(TAG, "Modem QC is powering down...");
    return at_send_command_response_ok(ath, "AT+QPOWD");
}
#endif

static esp_err_t register_gpio_wakeup(void)
{
    /* Initialize GPIO */
    gpio_config_t config = {
        .pin_bit_mask = BIT64(GPIO_WAKEUP_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "Initialize GPIO%d failed", GPIO_WAKEUP_NUM);

    /* Enable wake up from GPIO */
    ESP_RETURN_ON_ERROR(gpio_wakeup_enable(GPIO_WAKEUP_NUM, GPIO_WAKEUP_LEVEL == 0
                                           ? GPIO_INTR_LOW_LEVEL
                                           : GPIO_INTR_HIGH_LEVEL),
                        TAG, "Enable gpio wakeup failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(), TAG, "Configure gpio as wakeup source failed");

    /* Make sure the GPIO is inactive and it won't trigger wakeup immediately */
    while (gpio_get_level(GPIO_WAKEUP_NUM) == GPIO_WAKEUP_LEVEL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "gpio wakeup source is ready");

    return ESP_OK;
}

static void module_rst_set(bool on)
{
    gpio_set_level(GPIO_MODULE_RST_NUM, !on ? 1 : 0);
}

static void module_power_init()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(GPIO_MODULE_RST_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    module_rst_set(true);
}

static void sleep_timer_cb(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "Sleep timer triggered, entering light sleep...");
    register_gpio_wakeup();
    at_cmd_power_down(g_at_ctx.at_handle, NULL, NULL); // Power down the modem via AT command
    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait usb host to settle
#ifdef CONFIG_IDF_TARGET_ESP32P4
    esp_sleep_pd_config(ESP_PD_DOMAIN_CNNT, ESP_PD_OPTION_ON);
#endif

    ESP_LOGW(TAG, "Entering deep sleep");
    vTaskDelay(pdMS_TO_TICKS(10));
    esp_light_sleep_start();
    ESP_LOGW(TAG, "Wake up from light sleep");

    vTaskDelay(pdMS_TO_TICKS(100));
    module_rst_set(false); // Reset the module after wake up
    vTaskDelay(pdMS_TO_TICKS(1500));
    module_rst_set(true);
}
#endif

#if CONFIG_EXAMPLE_USB_ECM_IPERF
static void iperf_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    repl_config.prompt = "iperf>";
    repl_config.max_history_len = 1;
    repl_config.task_priority = 24;
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    /* Register commands */
    ESP_ERROR_CHECK(app_register_iperf_commands());

    printf("\n ==================================================\n");
    printf(" |       Steps to test throughput            |\n");
    printf(" |                                                |\n");
    printf(" |  1. Print 'help' to gain overview of commands  |\n");
    printf(" |  2. Run iperf to test UDP/TCP RX/TX throughput |\n");
    printf(" |                                                |\n");
    printf(" =================================================\n\n");

    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
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
#ifndef CONFIG_EXAMPLE_USB_ECM_IPERF
            stop_ping_timer();
#endif
            break;
        default:
            ESP_LOGI(TAG, "IOT_ETH_EVENT_UNKNOWN");
            break;
        }
    } else if (event_base == IP_EVENT) {
        ESP_LOGI(TAG, "GOT_IP");
        xEventGroupSetBits(s_event_group, EVENT_GOT_IP_BIT);
#ifndef CONFIG_EXAMPLE_USB_ECM_IPERF
        start_ping_timer();
#endif
    }
}

#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK

static bool usb_host_enum_filter_cb(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue)
{
    if (dev_desc->bNumConfigurations > 1) {
        *bConfigurationValue = 2;
    } else {
        *bConfigurationValue = 1;
    }
    ESP_LOGI(TAG, "USB device configuration value set to %d", *bConfigurationValue);
    // Return true to enumerate the USB device
    return true;
}

static void usb_lib_task(void *arg)
{
    // Install USB Host driver. Should only be called once in entire application
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .enum_filter_cb = usb_host_enum_filter_cb,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    //Signalize the usbh_cdc_driver_install, the USB host library has been installed
    xTaskNotifyGive(arg);

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (ESP_OK == usb_host_device_free_all()) {
                ESP_LOGI(TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "Wait for the FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "Get FLAGS_ALL_FREE");
            has_clients = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices, uninstall USB Host library");

    // Clean up USB Host
    vTaskDelay(100); // Short delay to allow clients clean-up
    usb_host_uninstall();
    ESP_LOGD(TAG, "USB Host library is uninstalled");
    vTaskDelete(NULL);
}
#endif

static void install_ecm(uint16_t idVendor, uint16_t idProduct, const char *netif_name)
{
    esp_err_t ret = ESP_OK;
    iot_eth_handle_t eth_handle = NULL;
    iot_eth_netif_glue_handle_t glue = NULL;

    usb_device_match_id_t *dev_match_id = calloc(2, sizeof(usb_device_match_id_t));
    dev_match_id[0].match_flags = USB_DEVICE_ID_MATCH_VID_PID;
    dev_match_id[0].idVendor = idVendor;
    dev_match_id[0].idProduct = idProduct;
    memset(&dev_match_id[1], 0, sizeof(usb_device_match_id_t)); // end of list
    iot_usbh_ecm_config_t ecm_cfg = {
        .match_id_list = dev_match_id,
    };

    ret = iot_eth_new_usb_ecm(&ecm_cfg, &s_ecm_eth_driver);
    if (ret != ESP_OK || s_ecm_eth_driver == NULL) {
        ESP_LOGE(TAG, "Failed to create USB ECM driver");
        return;
    }

    iot_eth_config_t eth_cfg = {
        .driver = s_ecm_eth_driver,
        .stack_input = NULL,
    };
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB ECM driver");
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
    s_ecm_netif = esp_netif_new(&netif_cfg);
    if (s_ecm_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create network interface");
        return;
    }

    glue = iot_eth_new_netif_glue(eth_handle);
    if (glue == NULL) {
        ESP_LOGE(TAG, "Failed to create netif glue");
        return;
    }
    esp_netif_attach(s_ecm_netif, glue);
    iot_eth_start(eth_handle);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize default TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_event_group != NULL,, TAG, "Failed to create event group");
    esp_event_handler_register(IOT_ETH_EVENT, ESP_EVENT_ANY_ID, iot_event_handle, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, iot_event_handle, NULL);

#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
    BaseType_t task_created = xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 5, NULL, 0);
    ESP_RETURN_ON_FALSE(task_created == pdPASS,, TAG, "xTaskCreatePinnedToCore failed");
    // Wait unit the USB host library is installed
    uint32_t notify_value = ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));
    if (notify_value == 0) {
        ESP_LOGE(TAG, "USB host library not installed");
        return;
    }
#endif
    // install usbh cdc driver
    usbh_cdc_driver_config_t config = {
        .task_stack_size = 1024 * 4,
        .task_priority = configMAX_PRIORITIES - 1,
        .task_coreid = 0,
#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
        .skip_init_usb_host_driver = true,
#else
        .skip_init_usb_host_driver = false,
#endif
    };
    ESP_ERROR_CHECK(usbh_cdc_driver_install(&config));

#ifdef CONFIG_EXAMPLE_USB_ECM_LIGHT_SLEEP_TEST
    module_power_init();
#endif
    install_ecm(USB_DEVICE_VENDOR_ANY, USB_DEVICE_PRODUCT_ANY, "USB ECM0");
    xEventGroupWaitBits(s_event_group, EVENT_GOT_IP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

#if CONFIG_EXAMPLE_USB_ECM_IPERF
    iperf_init();
    vTaskSuspend(NULL);
#endif

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
    modem_http_get_nvs_wifi_config(&s_modem_wifi_config);
    modem_http_init(&s_modem_wifi_config);
#endif
    app_wifi_main(&s_modem_wifi_config);
    esp_netif_set_default_netif(s_ecm_netif);

#ifdef CONFIG_EXAMPLE_USB_ECM_LIGHT_SLEEP_TEST
    TimerHandle_t sleep_timer = xTimerCreate("sleep_timer", pdMS_TO_TICKS(20000), pdTRUE, NULL, sleep_timer_cb);
    ESP_RETURN_VOID_ON_FALSE(sleep_timer != NULL, TAG, "Failed to create FreeRTOS timer");
    xTimerStart(sleep_timer, 0);
#endif

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
#if CONFIG_EXAMPLE_USB_ECM_DOWNLOAD_SPEED_TEST
            test_download_speed("http://mirrors.ustc.edu.cn/ros/ubuntu/db/references.db");
#endif
        }
    }
}
