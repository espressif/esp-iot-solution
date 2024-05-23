/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"
#include "cdc.h"
#include "sdkconfig.h"

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

// CDC-ACM spinlock
static portMUX_TYPE cdc_acm_lock = portMUX_INITIALIZER_UNLOCKED;
#define CDC_ACM_ENTER_CRITICAL()   portENTER_CRITICAL(&cdc_acm_lock)
#define CDC_ACM_EXIT_CRITICAL()    portEXIT_CRITICAL(&cdc_acm_lock)

typedef struct {
    tusb_cdcacm_callback_t callback_rx;
    tusb_cdcacm_callback_t callback_rx_wanted_char;
    tusb_cdcacm_callback_t callback_line_state_changed;
    tusb_cdcacm_callback_t callback_line_coding_changed;
} esp_tusb_cdcacm_t; /*!< CDC_ACM object */

static const char *TAG = "tusb_cdc_acm";

static inline esp_tusb_cdcacm_t *get_acm(tinyusb_cdcacm_itf_t itf)
{
    esp_tusb_cdc_t *cdc_inst = tinyusb_cdc_get_intf(itf);
    if (cdc_inst == NULL) {
        return (esp_tusb_cdcacm_t *)NULL;
    }
    return (esp_tusb_cdcacm_t *)(cdc_inst->subclass_obj);
}

/* TinyUSB callbacks
   ********************************************************************* */

/* Invoked by cdc interface when line state changed e.g connected/disconnected */
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    if (dtr && rts) { // connected
        if (acm != NULL) {
            ESP_LOGV(TAG, "Host connected to CDC no.%d.", itf);
        } else {
            ESP_LOGW(TAG, "Host is connected to CDC no.%d, but it is not initialized. Initialize it using `tinyusb_cdc_init`.", itf);
            return;
        }
    } else { // disconnected
        if (acm != NULL) {
            ESP_LOGV(TAG, "Serial device is ready to connect to CDC no.%d", itf);
        } else {
            return;
        }
    }
    if (acm) {
        CDC_ACM_ENTER_CRITICAL();
        tusb_cdcacm_callback_t cb = acm->callback_line_state_changed;
        CDC_ACM_EXIT_CRITICAL();
        if (cb) {
            cdcacm_event_t event = {
                .type = CDC_EVENT_LINE_STATE_CHANGED,
                .line_state_changed_data = {
                    .dtr = dtr,
                    .rts = rts
                }
            };
            cb(itf, &event);
        }
    }
}

/* Invoked when CDC interface received data from host */
void tud_cdc_rx_cb(uint8_t itf)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    if (acm) {
        CDC_ACM_ENTER_CRITICAL();
        tusb_cdcacm_callback_t cb = acm->callback_rx;
        CDC_ACM_EXIT_CRITICAL();
        if (cb) {
            cdcacm_event_t event = {
                .type = CDC_EVENT_RX
            };
            cb(itf, &event);
        }
    }
}

// Invoked when line coding is change via SET_LINE_CODING
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    if (acm) {
        CDC_ACM_ENTER_CRITICAL();
        tusb_cdcacm_callback_t cb = acm->callback_line_coding_changed;
        CDC_ACM_EXIT_CRITICAL();
        if (cb) {
            cdcacm_event_t event = {
                .type = CDC_EVENT_LINE_CODING_CHANGED,
                .line_coding_changed_data = {
                    .p_line_coding = p_line_coding,
                }
            };
            cb(itf, &event);
        }
    }
}

// Invoked when received `wanted_char`
void tud_cdc_rx_wanted_cb(uint8_t itf, char wanted_char)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    if (acm) {
        CDC_ACM_ENTER_CRITICAL();
        tusb_cdcacm_callback_t cb = acm->callback_rx_wanted_char;
        CDC_ACM_EXIT_CRITICAL();
        if (cb) {
            cdcacm_event_t event = {
                .type = CDC_EVENT_RX_WANTED_CHAR,
                .rx_wanted_char_data = {
                    .wanted_char = wanted_char,
                }
            };
            cb(itf, &event);
        }
    }
}

esp_err_t tinyusb_cdcacm_register_callback(tinyusb_cdcacm_itf_t itf,
                                           cdcacm_event_type_t event_type,
                                           tusb_cdcacm_callback_t callback)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    if (acm) {
        switch (event_type) {
        case CDC_EVENT_RX:
            CDC_ACM_ENTER_CRITICAL();
            acm->callback_rx = callback;
            CDC_ACM_EXIT_CRITICAL();
            return ESP_OK;
        case CDC_EVENT_RX_WANTED_CHAR:
            CDC_ACM_ENTER_CRITICAL();
            acm->callback_rx_wanted_char = callback;
            CDC_ACM_EXIT_CRITICAL();
            return ESP_OK;
        case CDC_EVENT_LINE_STATE_CHANGED:
            CDC_ACM_ENTER_CRITICAL();
            acm->callback_line_state_changed = callback;
            CDC_ACM_EXIT_CRITICAL();
            return ESP_OK;
        case CDC_EVENT_LINE_CODING_CHANGED:
            CDC_ACM_ENTER_CRITICAL();
            acm->callback_line_coding_changed = callback;
            CDC_ACM_EXIT_CRITICAL();
            return ESP_OK;
        default:
            ESP_LOGE(TAG, "Wrong event type");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGE(TAG, "CDC-ACM is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t tinyusb_cdcacm_unregister_callback(tinyusb_cdcacm_itf_t itf,
                                             cdcacm_event_type_t event_type)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    if (!acm) {
        ESP_LOGE(TAG, "Interface is not initialized. Use `tinyusb_cdc_init` for initialization");
        return ESP_ERR_INVALID_STATE;
    }
    switch (event_type) {
    case CDC_EVENT_RX:
        CDC_ACM_ENTER_CRITICAL();
        acm->callback_rx = NULL;
        CDC_ACM_EXIT_CRITICAL();
        return ESP_OK;
    case CDC_EVENT_RX_WANTED_CHAR:
        CDC_ACM_ENTER_CRITICAL();
        acm->callback_rx_wanted_char = NULL;
        CDC_ACM_EXIT_CRITICAL();
        return ESP_OK;
    case CDC_EVENT_LINE_STATE_CHANGED:
        CDC_ACM_ENTER_CRITICAL();
        acm->callback_line_state_changed = NULL;
        CDC_ACM_EXIT_CRITICAL();
        return ESP_OK;
    case CDC_EVENT_LINE_CODING_CHANGED:
        CDC_ACM_ENTER_CRITICAL();
        acm->callback_line_coding_changed = NULL;
        CDC_ACM_EXIT_CRITICAL();
        return ESP_OK;
    default:
        ESP_LOGE(TAG, "Wrong event type");
        return ESP_ERR_INVALID_ARG;
    }
}

/*********************************************************************** TinyUSB callbacks*/
/* CDC-ACM
   ********************************************************************* */

esp_err_t tinyusb_cdcacm_read(tinyusb_cdcacm_itf_t itf, uint8_t *out_buf, size_t out_buf_sz, size_t *rx_data_size)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    ESP_RETURN_ON_FALSE(acm, ESP_ERR_INVALID_STATE, TAG, "Interface is not initialized. Use `tinyusb_cdc_init` for initialization");

    if (tud_cdc_n_available(itf) == 0) {
        *rx_data_size = 0;
    } else {
        *rx_data_size = tud_cdc_n_read(itf, out_buf, out_buf_sz);
    }
    return ESP_OK;
}

size_t tinyusb_cdcacm_write_queue_char(tinyusb_cdcacm_itf_t itf, char ch)
{
    if (!get_acm(itf)) { // non-initialized
        return 0;
    }
    return tud_cdc_n_write_char(itf, ch);
}

size_t tinyusb_cdcacm_write_queue(tinyusb_cdcacm_itf_t itf, const uint8_t *in_buf, size_t in_size)
{
    if (!get_acm(itf)) { // non-initialized
        return 0;
    }
    const uint32_t size_available = tud_cdc_n_write_available(itf);
    return tud_cdc_n_write(itf, in_buf, MIN(in_size, size_available));
}

static uint32_t tud_cdc_n_write_occupied(tinyusb_cdcacm_itf_t itf)
{
    return CFG_TUD_CDC_TX_BUFSIZE - tud_cdc_n_write_available(itf);
}

esp_err_t tinyusb_cdcacm_write_flush(tinyusb_cdcacm_itf_t itf, uint32_t timeout_ticks)
{
    if (!get_acm(itf)) { // non-initialized
        return ESP_FAIL;
    }

    if (!timeout_ticks) { // if no timeout - nonblocking mode
        // It might take some time until TinyUSB flushes the endpoint
        // Since this call is non-blocking, we don't wait for flush finished,
        // We only inform the user by returning ESP_ERR_NOT_FINISHED
        tud_cdc_n_write_flush(itf);
        if (tud_cdc_n_write_occupied(itf)) {
            return ESP_ERR_NOT_FINISHED;
        }
    } else { // trying during the timeout
        uint32_t ticks_start = xTaskGetTickCount();
        uint32_t ticks_now = ticks_start;
        while (1) { // loop until success or until the time runs out
            ticks_now = xTaskGetTickCount();
            tud_cdc_n_write_flush(itf);
            if (tud_cdc_n_write_occupied(itf) == 0) {
                break; // All data flushed
            }
            if ((ticks_now - ticks_start) > timeout_ticks) {   // Time is up
                ESP_LOGW(TAG, "Flush failed");
                return ESP_ERR_TIMEOUT;
            }
            vTaskDelay(1);
        }
    }
    return ESP_OK;
}

static esp_err_t alloc_obj(tinyusb_cdcacm_itf_t itf)
{
    esp_tusb_cdc_t *cdc_inst = tinyusb_cdc_get_intf(itf);
    if (cdc_inst == NULL) {
        return ESP_FAIL;
    }
    cdc_inst->subclass_obj = calloc(1, sizeof(esp_tusb_cdcacm_t));
    if (cdc_inst->subclass_obj == NULL) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t tusb_cdc_acm_init(const tinyusb_config_cdcacm_t *cfg)
{
    esp_err_t ret = ESP_OK;
    int itf = (int)cfg->cdc_port;
    /* Creating a CDC object */
    const tinyusb_config_cdc_t cdc_cfg = {
        .usb_dev = cfg->usb_dev,
        .cdc_class = TUSB_CLASS_CDC,
        .cdc_subclass.comm_subclass = CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL
    };

    ESP_RETURN_ON_ERROR(tinyusb_cdc_init(itf, &cdc_cfg), TAG, "tinyusb_cdc_init failed");
    ESP_GOTO_ON_ERROR(alloc_obj(itf), fail, TAG, "alloc_obj failed");

    /* Callbacks setting up*/
    if (cfg->callback_rx) {
        tinyusb_cdcacm_register_callback(itf, CDC_EVENT_RX, cfg->callback_rx);
    }
    if (cfg->callback_rx_wanted_char) {
        tinyusb_cdcacm_register_callback(itf, CDC_EVENT_RX_WANTED_CHAR, cfg->callback_rx_wanted_char);
    }
    if (cfg->callback_line_state_changed) {
        tinyusb_cdcacm_register_callback(itf, CDC_EVENT_LINE_STATE_CHANGED, cfg->callback_line_state_changed);
    }
    if (cfg->callback_line_coding_changed) {
        tinyusb_cdcacm_register_callback(itf, CDC_EVENT_LINE_CODING_CHANGED, cfg->callback_line_coding_changed);
    }

    return ESP_OK;
fail:
    tinyusb_cdc_deinit(itf);
    return ret;
}

esp_err_t tusb_cdc_acm_deinit(int itf)
{
    return tinyusb_cdc_deinit(itf);
}

bool tusb_cdc_acm_initialized(tinyusb_cdcacm_itf_t itf)
{
    esp_tusb_cdcacm_t *acm = get_acm(itf);
    if (acm) {
        return true;
    } else {
        return false;
    }
}
/*********************************************************************** CDC-ACM*/
