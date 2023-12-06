/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_modem.h"
#include "esp_modem_dce.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_modem_internal.h"
#include "esp_modem_dte_internal.h"
#include "iot_usbh_cdc.h"

#define ESP_MODEM_EVENT_QUEUE_SIZE (16)

#define MIN_PATTERN_INTERVAL (9)
#define MIN_POST_IDLE (0)
#define MIN_PRE_IDLE (0)

/**
 * @brief Macro defined for error checking
 *
 */
static const char *TAG = "esp-modem-dte";

/**
 * @brief Returns true if the supplied string contains only CR or LF
 *
 * @param str string to check
 * @param len length of string
 */
static inline bool is_only_cr_lf(const char *str, uint32_t len)
{
    for (int i = 0; i < len; ++i) {
        if (str[i] != '\r' && str[i] != '\n') {
            return false;
        }
    }
    return true;
}

esp_err_t esp_modem_set_rx_cb(esp_modem_dte_t *dte, esp_modem_on_receive receive_cb, void *receive_cb_ctx)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    esp_dte->receive_cb_ctx = receive_cb_ctx;
    esp_dte->receive_cb = receive_cb;
    return ESP_OK;
}

/**
 * @brief Handle one line in DTE
 *
 * @param esp_dte ESP modem DTE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t esp_dte_handle_line(esp_modem_dte_internal_t *esp_dte)
{
    esp_err_t err = ESP_FAIL;
    esp_modem_dce_t *dce = esp_dte->parent.dce;
    ESP_MODEM_ERR_CHECK(dce, "DTE has not yet bind with DCE", err);
    const char *line = (const char *)(esp_dte->buffer);
    size_t len = strlen(line);
    /* Skip pure "\r\n" lines */
    if (len > 2 && !is_only_cr_lf(line, len)) {
        if (dce->handle_line == NULL) {
            /* Received an asynchronous line, but no handler waiting this this */
            ESP_LOGD(TAG, "No handler for line: %s", line);
            err = ESP_OK; /* Not an error, just propagate the line to user handler */
            goto post_event_unknown;
        }
        ESP_MODEM_ERR_CHECK(dce->handle_line(dce, line) == ESP_OK, "handle line failed", err);
    }
    return ESP_OK;
post_event_unknown:
    /* Send ESP_MODEM_EVENT_UNKNOWN signal to event loop */
    esp_event_post_to(esp_dte->event_loop_hdl, ESP_MODEM_EVENT, ESP_MODEM_EVENT_UNKNOWN,
                      (void *)line, strlen(line) + 1, pdMS_TO_TICKS(100));
err:
    return err;
}

static void esp_handle_usb_data(esp_modem_dte_internal_t *esp_dte)
{
    size_t length = 0;
    usbh_cdc_get_buffered_data_len(&length);

    if (esp_dte->parent.dce->mode != ESP_MODEM_PPP_MODE && length) {

        // Read the data and process it using `handle_line` logic
        // to avoid reading two line at the same time
        length = MIN(esp_dte->line_buffer_size - 1, length);
        length = usbh_cdc_read_bytes(esp_dte->buffer, length, pdMS_TO_TICKS(10));
        esp_dte->buffer[length] = '\0';
        if (strchr((char *)esp_dte->buffer, '\n') == NULL) {
            size_t max = esp_dte->line_buffer_size - 1;
            size_t bytes;
            // if pattern not found in the data,
            // continue reading as long as the modem is in MODEM_STATE_PROCESSING, checking for the pattern
            while (length < max && esp_dte->buffer[length - 1] != '\n' &&
                    esp_dte->parent.dce->state == ESP_MODEM_STATE_PROCESSING) {
                bytes = usbh_cdc_read_bytes(esp_dte->buffer + length, 1, pdMS_TO_TICKS(10));
                length += bytes;
                ESP_LOGV("esp-modem: debug_data", "Continuous read in non-data mode: length: %d char: %x", length, esp_dte->buffer[length - 1]);
            }
            esp_dte->buffer[length] = '\0';
        }
        ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data", esp_dte->buffer, length, ESP_LOG_DEBUG);
        if (esp_dte->parent.dce->handle_line) {
            /* Send new line to handle if handler registered */
            esp_dte_handle_line(esp_dte);
        }
        return;
    }
    length = MIN(esp_dte->data_buffer_size, length);
    length = usbh_cdc_read_bytes(esp_dte->data_buffer, length, pdMS_TO_TICKS(10));
    /* pass the input data to configured callback */
    if (length) {
        ESP_LOG_BUFFER_HEXDUMP("esp-modem-dte: ppp_input", esp_dte->data_buffer, length, ESP_LOG_VERBOSE);
        esp_dte->receive_cb(esp_dte->data_buffer, length, esp_dte->receive_cb_ctx);
    }
}

static void esp_handle_usb2_data(esp_modem_dte_internal_t *esp_dte)
{
    size_t length = 0;
    usbh_cdc_itf_get_buffered_data_len(1, &length);

    // Only handle interface1 data during interface0 in ppp mode
    if (esp_dte->parent.dce->mode == ESP_MODEM_PPP_MODE && length) {

        // Read the data and process it using `handle_line` logic
        // to avoid reading two line at the same time
        length = MIN(esp_dte->line_buffer_size - 1, length);
        length = usbh_cdc_itf_read_bytes(1, esp_dte->buffer, length, pdMS_TO_TICKS(10));
        esp_dte->buffer[length] = '\0';
        if (strchr((char *)esp_dte->buffer, '\n') == NULL) {
            size_t max = esp_dte->line_buffer_size - 1;
            size_t bytes;
            // if pattern not found in the data,
            // continue reading as long as the modem is in MODEM_STATE_PROCESSING, checking for the pattern
            while (length < max && esp_dte->buffer[length - 1] != '\n' &&
                    esp_dte->parent.dce->state == ESP_MODEM_STATE_PROCESSING) {
                bytes = usbh_cdc_itf_read_bytes(1, esp_dte->buffer + length, 1, pdMS_TO_TICKS(10));
                length += bytes;
                ESP_LOGV("esp-modem: debug_data2", "Continuous read in non-data mode: length: %d char: %x", length, esp_dte->buffer[length - 1]);
            }
            esp_dte->buffer[length] = '\0';
        }
        ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data2", esp_dte->buffer, length, ESP_LOG_DEBUG);
        if (esp_dte->parent.dce->handle_line) {
            /* Send new line to handle if handler registered */
            esp_dte_handle_line(esp_dte);
        }
        return;
    }
    length = MIN(esp_dte->data_buffer_size, length);
    uint8_t *temp_buffer = (uint8_t *)calloc(1, length);
    length = usbh_cdc_itf_read_bytes(1, temp_buffer, length, pdMS_TO_TICKS(10));
    /* pass the input data to configured callback */
    if (length) {
        ESP_LOGI(TAG, "Intf2 not handle date, just dump:");
        ESP_LOG_BUFFER_HEXDUMP("esp-modem-dte: inf2", temp_buffer, length, ESP_LOG_INFO);
    }
    free(temp_buffer);
}

static void _usb_recv_date_cb(void *arg)
{
    TaskHandle_t *p_usb_event_hdl = (TaskHandle_t *)arg;
    if (*p_usb_event_hdl == NULL) {
        return;
    }
    xTaskNotifyGive(*p_usb_event_hdl);
}

static void _usb_conn_callback(void *arg)
{
    esp_modem_dte_internal_t *esp_dte = (esp_modem_dte_internal_t *)arg;
    esp_dte->conn_state = 1;
    if (esp_dte->conn_callback) {
        esp_dte->conn_callback(NULL);
    }
}

static void _usb_disconn_callback(void *arg)
{
    esp_modem_dte_internal_t *esp_dte = (esp_modem_dte_internal_t *)arg;
    esp_dte->conn_state = 0;
    if (esp_dte->disconn_callback) {
        esp_dte->disconn_callback(NULL);
    }
}

/**
 * @brief USB Event Task Entry
 *
 * @param param task parameter
 */
static void _usb_data_recv_task(void *param)
{
    esp_modem_dte_internal_t *esp_dte = (esp_modem_dte_internal_t *)param;
    EventBits_t bits = xEventGroupWaitBits(esp_dte->process_group, (ESP_MODEM_START_BIT | ESP_MODEM_STOP_BIT), pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & ESP_MODEM_STOP_BIT) {
        vTaskDelete(NULL);
    }
    usbh_cdc_flush_rx_buffer(0);
    usbh_cdc_flush_rx_buffer(1);
    while (xEventGroupGetBits(esp_dte->process_group) & ESP_MODEM_START_BIT) {
        /* Drive the event loop */
        esp_event_loop_run(esp_dte->event_loop_hdl, 0);//no block
        size_t length = 0, length2 = 0;
        usbh_cdc_itf_get_buffered_data_len(0, &length);
        if (length > 0) {
            esp_handle_usb_data(esp_dte);
        }
        if (usbh_cdc_get_itf_state(1)) {
            usbh_cdc_itf_get_buffered_data_len(1, &length2);
            if (length2 > 0) {
                esp_handle_usb2_data(esp_dte);
            }
        }
        if (!(length || length2)) {
            ulTaskNotifyTake(true, 1);//yield to other task, but unblock as soon as possiable
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Send command to DCE
 *
 * @param dte Modem DTE object
 * @param command command string
 * @param timeout timeout value, unit: ms
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t esp_modem_dte_send_cmd(esp_modem_dte_t *dte, const char *command, uint32_t timeout)
{
    esp_err_t ret = ESP_FAIL;
    esp_modem_dce_t *dce = dte->dce;
    ESP_MODEM_ERR_CHECK(dce, "DTE has not yet bind with DCE", err);
    ESP_MODEM_ERR_CHECK(command, "command is NULL", err);
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    /* Calculate timeout clock tick */
    /* Reset runtime information */
    dce->state = ESP_MODEM_STATE_PROCESSING;
    /* Send command via UART */
    if (dce->mode == ESP_MODEM_PPP_MODE && usbh_cdc_get_itf_state(1) && strcmp(command, "+++")) {
        /* if interface 0 in ppp mode while interface 1 exist, and command not ppp exist "+++"
         * using interface 1 for command send*/
        usbh_cdc_itf_write_bytes(1, (const uint8_t*)command, strlen(command));
    } else if (dce->mode != ESP_MODEM_PPP_MODE || strcmp(command, "+++") == 0) {
        /* send command in no PPP mode, or send +++ in PPP mode */
        usbh_cdc_itf_write_bytes(0, (const uint8_t*)command, strlen(command));
    } else {
        ESP_LOGW(TAG, "Not support sending command in ppp mode, please exit ppp mode first");
        return ESP_FAIL;
    }

    /* Check timeout */
    EventBits_t bits = xEventGroupWaitBits(esp_dte->process_group, (ESP_MODEM_COMMAND_BIT | ESP_MODEM_STOP_BIT), pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout));
    ESP_MODEM_ERR_CHECK(bits & ESP_MODEM_COMMAND_BIT, "process command timeout", err);
    ret = ESP_OK;
err:
    dce->handle_line = NULL;
    return ret;
}

/**
 * @brief Send data to DCE
 *
 * @param dte Modem DTE object
 * @param data data buffer
 * @param length length of data to send
 * @return int actual length of data that has been send out
 */
static int esp_modem_dte_send_data(esp_modem_dte_t *dte, const char *data, uint32_t length)
{
    ESP_MODEM_ERR_CHECK(data, "data is NULL", err);
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    if (esp_dte->parent.dce->mode == ESP_MODEM_TRANSITION_MODE) {
        ESP_LOGD(TAG, "Not sending data in transition mode");
        return -1;
    }
    ESP_LOG_BUFFER_HEXDUMP("esp-modem-dte: ppp_output", data, length, ESP_LOG_VERBOSE);

    return usbh_cdc_write_bytes((const uint8_t*)data, length);
err:
    return -1;
}

/**
 * @brief Send data and wait for prompt from DCE
 *
 * @param dte Modem DTE object
 * @param data data buffer
 * @param length length of data to send
 * @param prompt pointer of specific prompt
 * @param timeout timeout value (unit: ms)
 * @return esp_err_t
 *      ESP_OK on success
 *      ESP_FAIL on error
 */
static esp_err_t esp_modem_dte_send_wait(esp_modem_dte_t *dte, const char *data, uint32_t length,
                                         const char *prompt, uint32_t timeout)
{
    ESP_MODEM_ERR_CHECK(data, "data is NULL", err_param);
    ESP_MODEM_ERR_CHECK(prompt, "prompt is NULL", err_param);
    ESP_MODEM_ERR_CHECK(usbh_cdc_write_bytes((const uint8_t*)data, length) >= 0, "uart write bytes failed", err_param);
    uint32_t len = strlen(prompt);
    uint8_t *buffer = calloc(len + 1, sizeof(uint8_t));
    int ret = usbh_cdc_read_bytes(buffer, len, pdMS_TO_TICKS(timeout));
    ESP_MODEM_ERR_CHECK(ret >= len, "wait prompt [%s] timeout", err, prompt);
    ESP_MODEM_ERR_CHECK(!strncmp(prompt, (const char *)buffer, len), "get wrong prompt: %s", err, buffer);
    free(buffer);
    return ESP_OK;
err:
    free(buffer);
err_param:
    return ESP_FAIL;
}

/**
 * @brief Change Modem's working mode
 *
 * @param dte Modem DTE object
 * @param new_mode new working mode
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t esp_modem_dte_change_mode(esp_modem_dte_t *dte, esp_modem_mode_t new_mode)
{
    esp_modem_dce_t *dce = dte->dce;
    ESP_MODEM_ERR_CHECK(dce, "DTE has not yet bind with DCE", err);
    //esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    ESP_MODEM_ERR_CHECK(dce->mode != new_mode, "already in mode: %d", err, new_mode);
    esp_modem_mode_t current_mode = dce->mode;
    ESP_MODEM_ERR_CHECK(current_mode != new_mode, "already in mode: %d", err, new_mode);
    dce->mode = ESP_MODEM_TRANSITION_MODE;  // mode switching will be finished in set_working_mode() on success
    // (or restored on failure)
    switch (new_mode) {
    case ESP_MODEM_PPP_MODE:
        ESP_MODEM_ERR_CHECK(dce->set_working_mode(dce, new_mode) == ESP_OK, "set new working mode:%d failed", err_restore_mode, new_mode);
        break;
    case ESP_MODEM_COMMAND_MODE:
        ESP_MODEM_ERR_CHECK(dce->set_working_mode(dce, new_mode) == ESP_OK, "set new working mode:%d failed", err_restore_mode, new_mode);
        break;
    default:
        break;
    }
    return ESP_OK;
err_restore_mode:
    dce->mode = current_mode;
err:
    return ESP_FAIL;
}

static esp_err_t esp_modem_dte_process_cmd_done(esp_modem_dte_t *dte)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    EventBits_t bits = xEventGroupSetBits(esp_dte->process_group, ESP_MODEM_COMMAND_BIT);
    return bits & ESP_MODEM_STOP_BIT ? ESP_FAIL : ESP_OK; // report error if the group indicated MODEM_STOP condition
}

/**
 * @brief Deinitialize a Modem DTE object
 *
 * @param dte Modem DTE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t esp_modem_dte_deinit(esp_modem_dte_t *dte)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    /* Clear the start bit */
    xEventGroupClearBits(esp_dte->process_group, ESP_MODEM_START_BIT);
    /* Delete UART event task */
    vTaskDelete(esp_dte->uart_event_task_hdl);
    /* Delete semaphore */
    vEventGroupDelete(esp_dte->process_group);
    /* Delete event loop */
    esp_event_loop_delete(esp_dte->event_loop_hdl);
    /* Delete send cmd lock */
    vSemaphoreDelete(esp_dte->parent.send_cmd_lock);
    /* Uninstall UART Driver */
    usbh_cdc_driver_delete();
    /* Free memory */
    free(esp_dte->buffer);
    free(esp_dte->data_buffer);
    if (dte->dce) {
        dte->dce->dte = NULL;
    }
    free(esp_dte);
    return ESP_OK;
}

/**
 * @brief Create and init Modem DTE object
 *
 */
esp_modem_dte_t *esp_modem_dte_new(const esp_modem_dte_config_t *config)
{
    esp_err_t ret;
    /* malloc memory for esp_dte object */
    esp_modem_dte_internal_t *esp_dte = calloc(1, sizeof(esp_modem_dte_internal_t));
    ESP_MODEM_ERR_CHECK(esp_dte, "calloc esp_dte failed", err_dte_mem);
    /* malloc memory to storing lines from modem dce */
    esp_dte->line_buffer_size = config->line_buffer_size;
    esp_dte->buffer = calloc(1, config->line_buffer_size);
    ESP_MODEM_ERR_CHECK(esp_dte->buffer, "calloc line memory failed", err_buf_mem);
    esp_dte->data_buffer_size = config->line_buffer_size;
    esp_dte->data_buffer = calloc(1, esp_dte->data_buffer_size);
    ESP_MODEM_ERR_CHECK(esp_dte->data_buffer, "calloc data memory failed", err_buf_mem);
    esp_dte->parent.send_cmd_lock = xSemaphoreCreateMutex();
    ESP_MODEM_ERR_CHECK(esp_dte->parent.send_cmd_lock, "create send cmd lock failed", err_buf_mem);
    /* Bind methods */
    esp_dte->parent.send_cmd = esp_modem_dte_send_cmd;
    esp_dte->parent.send_data = esp_modem_dte_send_data;
    esp_dte->parent.send_wait = esp_modem_dte_send_wait;
    esp_dte->parent.change_mode = esp_modem_dte_change_mode;
    esp_dte->parent.process_cmd_done = esp_modem_dte_process_cmd_done;
    esp_dte->parent.deinit = esp_modem_dte_deinit;
    esp_dte->conn_callback = config->conn_callback;
    esp_dte->disconn_callback = config->disconn_callback;
    /* Create Event loop */
    esp_event_loop_args_t loop_args = {
        .queue_size = ESP_MODEM_EVENT_QUEUE_SIZE,
        .task_name = NULL,
    };
    ESP_MODEM_ERR_CHECK(esp_event_loop_create(&loop_args, &esp_dte->event_loop_hdl) == ESP_OK, "create event loop failed", err_eloop);
    /* Create semaphore */
    esp_dte->process_group = xEventGroupCreate();
    ESP_MODEM_ERR_CHECK(esp_dte->process_group, "create process semaphore failed", err_sem);

    usbh_cdc_config_t cdc_config = {
        .bulk_in_ep_addr = CONFIG_MODEM_USB_IN_EP_ADDR,
        .bulk_out_ep_addr = CONFIG_MODEM_USB_OUT_EP_ADDR,
        .rx_buffer_size = config->rx_buffer_size,
        .tx_buffer_size = config->tx_buffer_size,
        .rx_callback = _usb_recv_date_cb,
        .rx_callback_arg = &esp_dte->uart_event_task_hdl,
        .conn_callback = _usb_conn_callback,
        .conn_callback_arg = esp_dte,
        .disconn_callback = _usb_disconn_callback,
        .disconn_callback_arg = esp_dte,
    };

#ifdef CONFIG_MODEM_SUPPORT_SECONDARY_AT_PORT
    cdc_config.itf_num = 2,
    cdc_config.bulk_in_ep_addrs[1] = CONFIG_MODEM_USB_IN2_EP_ADDR;
    cdc_config.bulk_out_ep_addrs[1] = CONFIG_MODEM_USB_OUT2_EP_ADDR;
    cdc_config.rx_buffer_sizes[1] = config->rx_buffer_size;
    cdc_config.tx_buffer_sizes[1] = config->tx_buffer_size;
    ESP_LOGI(TAG, "Enable second AT port");
#endif

    ret = usbh_cdc_driver_install(&cdc_config);
    ESP_MODEM_ERR_CHECK(ret == ESP_OK, "usb driver install failed", err_usb_config);
    ret = usbh_cdc_wait_connect(portMAX_DELAY);
    ESP_MODEM_ERR_CHECK(ret == ESP_OK, "usb connect timeout", err_usb_config);
    /* Create USB Event task */
    BaseType_t base_ret = xTaskCreate(_usb_data_recv_task,             //Task Entry
                                      "usb_data_recv",              //Task Name
                                      config->event_task_stack_size,           //Task Stack Size(Bytes)
                                      esp_dte,                           //Task Parameter
                                      config->event_task_priority,             //Task Priority, must higher than USB Task
                                      & (esp_dte->uart_event_task_hdl)  //Task Handler
                                     );
    ESP_MODEM_ERR_CHECK(base_ret == pdTRUE, "create uart event task failed", err_tsk_create);

    return &(esp_dte->parent);
    /* Error handling */
err_tsk_create:
    usbh_cdc_driver_delete();
err_usb_config:
    vEventGroupDelete(esp_dte->process_group);
err_sem:
    esp_event_loop_delete(esp_dte->event_loop_hdl);
err_eloop:
    vSemaphoreDelete(esp_dte->parent.send_cmd_lock);
err_buf_mem:
    free(esp_dte->data_buffer);
    free(esp_dte->buffer);
    free(esp_dte);
err_dte_mem:
    return NULL;
}

esp_err_t esp_modem_dte_set_params(esp_modem_dte_t *dte, const esp_modem_dte_config_t *config)
{
    esp_modem_dte_internal_t *esp_dte = __containerof(dte, esp_modem_dte_internal_t, parent);
    return uart_set_baudrate(esp_dte->uart_port, config->baud_rate);
}
