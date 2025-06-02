/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "iot_eth.h"
#include "iot_eth_types.h"

static const char *TAG = "iot_eth";

ESP_EVENT_DEFINE_BASE(IOT_ETH_EVENT);

typedef struct {
    iot_eth_driver_t *driver;
    iot_eth_mediator_t mediator;
    _Atomic iot_eth_link_t link;
    uint8_t mac[6];
    void *user_data;
    static_input_cb_t stack_input;
} iot_eth_t;

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

esp_err_t iot_eth_transmit(iot_eth_handle_t handle, void *data, size_t len)
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

esp_err_t iot_eth_on_stage_changed(iot_eth_mediator_t *mediator, iot_eth_stage_t stage, void *arg)
{
    iot_eth_t *eth = __containerof(mediator, iot_eth_t, mediator);
    switch (stage) {
    case IOT_ETH_STAGE_LL_INIT:
        ESP_LOGD(TAG, "IOT_ETH_STAGE_LL_INIT");
        break;
    case IOT_ETH_STAGE_LL_DEINIT:
        ESP_LOGD(TAG, "IOT_ETH_STAGE_LL_DEINIT");
        break;
    case IOT_ETH_STAGE_GET_MAC:
        eth->driver->get_addr(eth->driver, eth->mac);
        ESP_LOGI(TAG, "%s: MAC address: %02X:%02X:%02X:%02X:%02X:%02X", eth->driver->name, eth->mac[0], eth->mac[1], eth->mac[2], eth->mac[3], eth->mac[4], eth->mac[5]);
        break;
    case IOT_ETH_STAGE_LINK:
        iot_eth_link_t *link = (iot_eth_link_t *)arg;
        atomic_store(&eth->link, *link);
        if (*link == IOT_ETH_LINK_UP) {
            ESP_RETURN_ON_ERROR(esp_event_post(IOT_ETH_EVENT, IOT_ETH_EVENT_CONNECTED, &eth, sizeof(iot_eth_t *), 0), TAG, "send ETHERNET_EVENT_CONNECTED event failed");
        } else {
            ESP_RETURN_ON_ERROR(esp_event_post(IOT_ETH_EVENT, IOT_ETH_EVENT_DISCONNECTED, &eth, sizeof(iot_eth_t *), 0), TAG, "send ETHERNET_EVENT_DISCONNECTED event failed");
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

    esp_err_t ret = ESP_OK;
    iot_eth_t *eth = calloc(1, sizeof(iot_eth_t));
    ESP_RETURN_ON_FALSE(eth != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for iot_eth_t");
    eth->driver = config->driver;
    eth->stack_input = config->stack_input;
    eth->mediator.stack_input = iot_eth_stack_input;
    eth->mediator.on_stage_changed = iot_eth_on_stage_changed;
    eth->driver->set_mediator(eth->driver, &eth->mediator);
    ret = eth->driver->init(eth->driver);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to init driver");
    *handle = (iot_eth_handle_t)eth;
    return ESP_OK;
err:
    free(eth);
    return ret;
}

esp_err_t iot_eth_uninstall(iot_eth_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    esp_err_t ret = eth->driver->deinit(eth->driver);
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to deinit driver");

    free(eth);
    return ESP_OK;
err:
    return ret;
}

esp_err_t iot_eth_start(iot_eth_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    esp_err_t ret = eth->driver->start(eth->driver);
    if (ret == ESP_OK) {
        ESP_RETURN_ON_ERROR(esp_event_post(IOT_ETH_EVENT, IOT_ETH_EVENT_START, &eth, sizeof(iot_eth_t *), 0), TAG, "send IOT_ETH_EVENT_START event failed");
    }
    return ret;
}

esp_err_t iot_eth_stop(iot_eth_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    esp_err_t ret = eth->driver->stop(eth->driver);
    if (ret == ESP_OK) {
        ESP_RETURN_ON_ERROR(esp_event_post(IOT_ETH_EVENT, IOT_ETH_EVENT_STOP, &eth, sizeof(iot_eth_t *), 0), TAG, "send IOT_ETH_EVENT_STOP event failed");
    }
    return ret;
}

esp_err_t iot_eth_get_addr(iot_eth_handle_t handle, uint8_t *mac)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    return eth->driver->get_addr(eth->driver, mac);
}

esp_err_t iot_eth_update_input_path(iot_eth_handle_t handle, static_input_cb_t stack_input, void *user_data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    iot_eth_t *eth = (iot_eth_t *)handle;
    eth->stack_input = stack_input;
    eth->user_data = user_data;
    return ESP_OK;
}
