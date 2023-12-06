/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_modem.h"
#include "esp_modem_dce.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_modem_internal.h"
#include "esp_modem_dte_internal.h"
#include "esp_modem_netif.h"

static const char *TAG = "esp-modem";

ESP_EVENT_DEFINE_BASE(ESP_MODEM_EVENT);

esp_err_t esp_modem_set_event_handler(esp_modem_dte_t *dte, esp_event_handler_t handler, int32_t event_id, void *handler_args)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    return esp_event_handler_register_with(esp_dte->event_loop_hdl, ESP_MODEM_EVENT, event_id, handler, handler_args);
}

esp_err_t esp_modem_remove_event_handler(esp_modem_dte_t *dte, esp_event_handler_t handler)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    return esp_event_handler_unregister_with(esp_dte->event_loop_hdl, ESP_MODEM_EVENT, ESP_EVENT_ANY_ID, handler);
}

esp_err_t esp_modem_post_event(esp_modem_dte_t *dte, int32_t event_id, void* event_data, size_t event_data_size, TickType_t ticks_to_wait)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    return esp_event_post_to(esp_dte->event_loop_hdl, ESP_MODEM_EVENT, event_id, event_data, event_data_size, ticks_to_wait);
}

esp_err_t esp_modem_start_ppp(esp_modem_dte_t *dte)
{
    esp_modem_dce_t *dce = dte->dce;
    ESP_MODEM_ERR_CHECK(dce, "DTE has not yet bind with DCE", err);
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);

    /* Enter PPP mode */
    ESP_MODEM_ERR_CHECK(dte->change_mode(dte, ESP_MODEM_PPP_MODE) == ESP_OK, "enter ppp mode failed", err);

    /* post PPP mode started event */
    esp_event_post_to(esp_dte->event_loop_hdl, ESP_MODEM_EVENT, ESP_MODEM_EVENT_PPP_START, NULL, 0, 0);
    return ESP_OK;
err:
    return ESP_FAIL;
}

esp_err_t esp_modem_stop_ppp(esp_modem_dte_t *dte)
{
    esp_modem_dce_t *dce = dte->dce;
    ESP_MODEM_ERR_CHECK(dce, "DTE has not yet bind with DCE", err);
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);

    /* post PPP mode stopped event */
    esp_event_post_to(esp_dte->event_loop_hdl, ESP_MODEM_EVENT, ESP_MODEM_EVENT_PPP_STOP, NULL, 0, 0);

    /* wait for the PPP mode to exit gracefully */
    ESP_LOGW(TAG, "Waiting exit the PPP mode gracefully");
    EventBits_t bits = xEventGroupWaitBits(esp_dte->process_group, ESP_MODEM_STOP_PPP_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(20000));
    if (!(bits & ESP_MODEM_STOP_PPP_BIT)) {
        ESP_LOGW(TAG, "Failed to exit the PPP mode gracefully");
    }
    /* Enter command mode, failed if 1.dte disconnect 2.already in command mode*/
    esp_err_t ret = ESP_OK;
    if (esp_dte->conn_state) {
        ret = dte->change_mode(dte, ESP_MODEM_COMMAND_MODE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "enter command mode failed");
        }
    }
    return ESP_OK;
err:
    return ESP_FAIL;
}

esp_err_t esp_modem_notify_ppp_netif_closed(esp_modem_dte_t *dte)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    EventBits_t bits = xEventGroupSetBits(esp_dte->process_group, ESP_MODEM_STOP_PPP_BIT);
    return bits & ESP_MODEM_STOP_BIT ? ESP_FAIL : ESP_OK; // set error if the group indicated MODEM_STOP condition
}

esp_err_t esp_modem_notify_initialized(esp_modem_dte_t *dte)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    EventBits_t bits = xEventGroupSetBits(esp_dte->process_group, ESP_MODEM_START_BIT);
    return bits & ESP_MODEM_START_BIT ? ESP_OK : ESP_FAIL;  // START bit should be set (since it's not auto-cleared)
    // report error otherwise
}

esp_err_t esp_modem_default_destroy(esp_modem_dte_t *dte)
{
    ESP_MODEM_ERR_CHECK(dte, "Cannot destroy NULL dte", err_params);
    esp_modem_netif_driver_t *netif_adapter = dte->netif_adapter;
    esp_modem_dce_t *dce = dte->dce;
    ESP_MODEM_ERR_CHECK(dce && netif_adapter, "Cannot destroy dce or netif_adapter", err_params);
    ESP_MODEM_ERR_CHECK(esp_modem_netif_clear_default_handlers(netif_adapter) == ESP_OK,
                        "modem_netif failed to clread handlers", err);
    esp_modem_netif_destroy(netif_adapter);
    dte->netif_adapter = NULL;
    ESP_MODEM_ERR_CHECK(dce->deinit(dce) == ESP_OK, "failed to deinit dce", err);
    dte->dce = NULL;
    ESP_MODEM_ERR_CHECK(dte->deinit(dte) == ESP_OK, "failed to deinit ", err);
    return ESP_OK;
err:
    return ESP_FAIL;
err_params:
    return ESP_ERR_INVALID_ARG;
}

esp_err_t esp_modem_default_start(esp_modem_dte_t *dte)
{
    ESP_MODEM_ERR_CHECK(dte, "failed to start zero DTE", err_params);
    esp_modem_dce_t *dce = dte->dce;
    ESP_MODEM_ERR_CHECK(dce, "failed to start zero DCE", err_params);

    return dce->start_up(dce);

err_params:
    return ESP_ERR_INVALID_ARG;
}

esp_err_t esp_modem_default_attach(esp_modem_dte_t *dte, esp_modem_dce_t *dce, esp_netif_t* ppp_netif)
{
    /* Bind DTE with DCE */
    dce->dte = dte;
    dte->dce = dce;

    /* Init and bind DTE with the PPP netif adapter */
    esp_modem_netif_driver_t *modem_netif_adapter = esp_modem_netif_new(dte);
    ESP_MODEM_ERR_CHECK(esp_modem_netif_set_default_handlers(modem_netif_adapter, ppp_netif) == ESP_OK,
                        "modem_netif failed to set handlers", err);
    ESP_MODEM_ERR_CHECK(esp_netif_attach(ppp_netif, modem_netif_adapter) == ESP_OK,
                        "attach netif to modem adapter failed", err);
    ESP_MODEM_ERR_CHECK(esp_modem_notify_initialized(dte) == ESP_OK, "DTE init notification failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

esp_err_t esp_modem_dce_init(esp_modem_dce_t *dce, esp_modem_dce_config_t *config)
{
    esp_err_t err = ESP_OK;
    /* init the default DCE first */
    ESP_MODEM_ERR_CHECK(dce && config, "failed to init with zero dce or configuration", err_params);
    ESP_MODEM_ERR_CHECK(esp_modem_dce_default_init(dce, config) == ESP_OK, "dce default init has failed", err);
    if (config->populate_command_list) {
        ESP_MODEM_ERR_CHECK(esp_modem_set_default_command_list(dce) == ESP_OK, "esp_modem_dce_set_default_commands failed", err);
    }
    ESP_LOGI(TAG, "--------- Modem PreDefined Info ------------------");
    ESP_LOGI(TAG, "Model: %s", CONFIG_MODEM_TARGET_NAME);
    ESP_LOGI(TAG, "Modem itf: IN Addr:0x%02X, OUT Addr:0x%02X", CONFIG_MODEM_USB_IN_EP_ADDR, CONFIG_MODEM_USB_OUT_EP_ADDR);
#ifdef CONFIG_MODEM_SUPPORT_SECONDARY_AT_PORT
    ESP_LOGI(TAG, "Secondary AT itf: IN Addr:0x%02X, OUT Addr:0x%02X", CONFIG_MODEM_USB_IN2_EP_ADDR, CONFIG_MODEM_USB_OUT2_EP_ADDR);
#endif
    ESP_LOGI(TAG, "----------------------------------------------------");
    switch (config->device) {
    //TODO: add modem specified command or workflow
    case ESP_MODEM_DEVICE_UNSPECIFIED:
    default:
        break;
    }
    ESP_MODEM_ERR_CHECK(err == ESP_OK, "dce specific initialization has failed for %d type device", err, config->device);
    return ESP_OK;
err:
    return ESP_FAIL;
err_params:
    return ESP_ERR_INVALID_ARG;
}
