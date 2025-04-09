/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"

#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "cdc_host_descriptor_parsing.h"
#include "cdc_host_types.h"

static const char *TAG = "cdc_acm";

// Control transfer constants
#define CDC_ACM_CTRL_TRANSFER_SIZE (512)   // All standard CTRL requests and responses fit in this size
#define CDC_ACM_CTRL_TIMEOUT_MS    (5000) // Every CDC device should be able to respond to CTRL transfer in 5 seconds

// CDC-ACM spinlock
static portMUX_TYPE cdc_acm_lock = portMUX_INITIALIZER_UNLOCKED;
#define CDC_ACM_ENTER_CRITICAL()   portENTER_CRITICAL(&cdc_acm_lock)
#define CDC_ACM_EXIT_CRITICAL()    portEXIT_CRITICAL(&cdc_acm_lock)

// CDC-ACM events
#define CDC_ACM_TEARDOWN          BIT0
#define CDC_ACM_TEARDOWN_COMPLETE BIT1

// CDC-ACM check macros
#define CDC_ACM_CHECK(cond, ret_val) ({                                     \
            if (!(cond)) {                                                  \
                return (ret_val);                                           \
            }                                                               \
})

#define CDC_ACM_CHECK_FROM_CRIT(cond, ret_val) ({                           \
            if (!(cond)) {                                                  \
                CDC_ACM_EXIT_CRITICAL();                                    \
                return ret_val;                                             \
            }                                                               \
})

// CDC-ACM driver object
typedef struct {
    usb_host_client_handle_t cdc_acm_client_hdl;        /*!< USB Host handle reused for all CDC-ACM devices in the system */
    SemaphoreHandle_t open_close_mutex;
    EventGroupHandle_t event_group;
    cdc_acm_new_dev_callback_t new_dev_cb;
    SLIST_HEAD(list_dev, cdc_dev_s) cdc_devices_list;   /*!< List of open pseudo devices */
} cdc_acm_obj_t;

static cdc_acm_obj_t *p_cdc_acm_obj = NULL;

/**
 * @brief Default CDC-ACM driver configuration
 *
 * This configuration is used when user passes NULL to config pointer during device open.
 */
static const cdc_acm_host_driver_config_t cdc_acm_driver_config_default = {
    .driver_task_stack_size = 4096,
    .driver_task_priority = 10,
    .xCoreID = 0,
    .new_dev_cb = NULL,
};

/**
 * @brief Notification received callback
 *
 * Notification (interrupt) IN transfer is submitted at the end of this function to ensure periodic poll of IN endpoint.
 *
 * @param[in] transfer Transfer that triggered the callback
 */
static void notif_xfer_cb(usb_transfer_t *transfer);

/**
 * @brief Data received callback
 *
 * Data (bulk) IN transfer is submitted at the end of this function to ensure continuous poll of IN endpoint.
 *
 * @param[in] transfer Transfer that triggered the callback
 */
static void in_xfer_cb(usb_transfer_t *transfer);

/**
 * @brief Data send callback
 *
 * Reused for bulk OUT and CTRL transfers
 *
 * @param[in] transfer Transfer that triggered the callback
 */
static void out_xfer_cb(usb_transfer_t *transfer);

/**
 * @brief USB Host Client event callback
 *
 * Handling of USB device connection/disconnection to/from root HUB.
 *
 * @param[in] event_msg Event message type
 * @param[in] arg Caller's argument (not used in this driver)
 */
static void usb_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg);

/**
 * @brief Send CDC specific request
 *
 * Helper function that will send CDC specific request to default endpoint.
 * Both IN and OUT requests are sent through this API, depending on the in_transfer parameter.
 *
 * @see  Chapter 6.2, USB CDC specification rev. 1.2
 * @note CDC specific requests are only supported by devices that have dedicated management element.
 *
 * @param[in] cdc_dev Pointer to CDC device
 * @param[in] in_transfer Direction of data phase. true: IN, false: OUT
 * @param[in] request CDC request code
 * @param[inout] data Pointer to data buffer. Input for OUT transfers, output for IN transfers.
 * @param[in] data_len Length of data buffer
 * @param[in] value Value to be set in bValue of Setup packet
 * @return esp_err_t
 */
static esp_err_t send_cdc_request(cdc_dev_t *cdc_dev, bool in_transfer, cdc_request_code_t request, uint8_t *data, uint16_t data_len, uint16_t value);

/**
 * @brief Reset IN transfer
 *
 * In in_xfer_cb() we can modify IN transfer parameters, this function resets the transfer to its defaults
 *
 * @param[in] cdc_dev Pointer to CDC device
 */
static void cdc_acm_reset_in_transfer(cdc_dev_t *cdc_dev)
{
    assert(cdc_dev->data.in_xfer);
    usb_transfer_t *transfer = cdc_dev->data.in_xfer;
    uint8_t **ptr = (uint8_t **)(&(transfer->data_buffer));
    *ptr = cdc_dev->data.in_data_buffer_base;
    transfer->num_bytes = transfer->data_buffer_size;
    // This is a hotfix for IDF changes, where 'transfer->data_buffer_size' does not contain actual buffer length,
    // but *allocated* buffer length, which can be larger if CONFIG_HEAP_POISONING_COMPREHENSIVE is enabled
    transfer->num_bytes -= transfer->data_buffer_size % cdc_dev->data.in_mps;
}

/**
 * @brief CDC-ACM driver handling task
 *
 * USB host client registration and deregistration is handled here.
 *
 * @param[in] arg User's argument. Handle of a task that started this task.
 */
static void cdc_acm_client_task(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    cdc_acm_obj_t *cdc_acm_obj = p_cdc_acm_obj; // Make local copy of the driver's handle
    assert(cdc_acm_obj->cdc_acm_client_hdl);

    // Start handling client's events
    while (1) {
        usb_host_client_handle_events(cdc_acm_obj->cdc_acm_client_hdl, portMAX_DELAY);
        EventBits_t events = xEventGroupGetBits(cdc_acm_obj->event_group);
        if (events & CDC_ACM_TEARDOWN) {
            break;
        }
    }

    ESP_LOGD(TAG, "Deregistering client");
    ESP_ERROR_CHECK(usb_host_client_deregister(cdc_acm_obj->cdc_acm_client_hdl));
    xEventGroupSetBits(cdc_acm_obj->event_group, CDC_ACM_TEARDOWN_COMPLETE);
    vTaskDelete(NULL);
}

/**
 * @brief Cancel transfer and reset endpoint
 *
 * This function will cancel ongoing transfer a reset its endpoint to ready state.
 *
 * @param[in] dev_hdl USB device handle
 * @param[in] transfer Transfer to be cancelled
 * @return esp_err_t
 */
static esp_err_t cdc_acm_reset_transfer_endpoint(usb_device_handle_t dev_hdl, usb_transfer_t *transfer)
{
    assert(dev_hdl);
    assert(transfer);

    ESP_RETURN_ON_ERROR(usb_host_endpoint_halt(dev_hdl, transfer->bEndpointAddress), TAG,);
    ESP_RETURN_ON_ERROR(usb_host_endpoint_flush(dev_hdl, transfer->bEndpointAddress), TAG,);
    usb_host_endpoint_clear(dev_hdl, transfer->bEndpointAddress);
    return ESP_OK;
}

/**
 * @brief Start CDC device
 *
 * After this call, USB host peripheral will continuously poll IN endpoints.
 *
 * @param cdc_dev
 * @param[in] event_cb  Device event callback
 * @param[in] in_cb     Data received callback
 * @param[in] user_arg  Optional user's argument, that will be passed to the callbacks
 * @return esp_err_t
 */
static esp_err_t cdc_acm_start(cdc_dev_t *cdc_dev, cdc_acm_host_dev_callback_t event_cb, cdc_acm_data_callback_t in_cb, void *user_arg)
{
    esp_err_t ret = ESP_OK;
    assert(cdc_dev);

    CDC_ACM_ENTER_CRITICAL();
    cdc_dev->notif.cb = event_cb;
    cdc_dev->data.in_cb = in_cb;
    cdc_dev->cb_arg = user_arg;
    CDC_ACM_EXIT_CRITICAL();

    // Claim data interface and start polling its IN endpoint
    ESP_GOTO_ON_ERROR(
        usb_host_interface_claim(
            p_cdc_acm_obj->cdc_acm_client_hdl,
            cdc_dev->dev_hdl,
            cdc_dev->data.intf_desc->bInterfaceNumber,
            cdc_dev->data.intf_desc->bAlternateSetting),
        err, TAG, "Could not claim interface");
    if (cdc_dev->data.in_xfer) {
        ESP_LOGD(TAG, "Submitting poll for BULK IN transfer");
        ESP_ERROR_CHECK(usb_host_transfer_submit(cdc_dev->data.in_xfer));
    }

    // If notification are supported, claim its interface and start polling its IN endpoint
    if (cdc_dev->notif.xfer) {
        if (cdc_dev->notif.intf_desc != cdc_dev->data.intf_desc) {
            ESP_GOTO_ON_ERROR(
                usb_host_interface_claim(
                    p_cdc_acm_obj->cdc_acm_client_hdl,
                    cdc_dev->dev_hdl,
                    cdc_dev->notif.intf_desc->bInterfaceNumber,
                    cdc_dev->notif.intf_desc->bAlternateSetting),
                err, TAG, "Could not claim interface");
        }
        ESP_LOGD(TAG, "Submitting poll for INTR IN transfer");
        ESP_ERROR_CHECK(usb_host_transfer_submit(cdc_dev->notif.xfer));
    }

    // Everything OK, add the device into list and return
    CDC_ACM_ENTER_CRITICAL();
    SLIST_INSERT_HEAD(&p_cdc_acm_obj->cdc_devices_list, cdc_dev, list_entry);
    CDC_ACM_EXIT_CRITICAL();
    return ret;

err:
    usb_host_interface_release(p_cdc_acm_obj->cdc_acm_client_hdl, cdc_dev->dev_hdl, cdc_dev->data.intf_desc->bInterfaceNumber);
    if (cdc_dev->notif.xfer && (cdc_dev->notif.intf_desc != cdc_dev->data.intf_desc)) {
        usb_host_interface_release(p_cdc_acm_obj->cdc_acm_client_hdl, cdc_dev->dev_hdl, cdc_dev->notif.intf_desc->bInterfaceNumber);
    }
    return ret;
}

static void cdc_acm_transfers_free(cdc_dev_t *cdc_dev);
/**
 * @brief Helper function that releases resources claimed by CDC device
 *
 * Close underlying USB device, free device driver memory
 *
 * @note All interfaces claimed by this device must be release before calling this function
 * @param cdc_dev CDC device handle to be removed
 */
static void cdc_acm_device_remove(cdc_dev_t *cdc_dev)
{
    assert(cdc_dev);
    cdc_acm_transfers_free(cdc_dev);
    free(cdc_dev->cdc_func_desc);
    // We don't check the error code of usb_host_device_close, as the close might fail, if someone else is still using the device (not all interfaces are released)
    usb_host_device_close(p_cdc_acm_obj->cdc_acm_client_hdl, cdc_dev->dev_hdl); // Gracefully continue on error
    free(cdc_dev);
}

/**
 * @brief Open USB device with requested VID/PID
 *
 * This function has two regular return paths:
 * 1. USB device with matching VID/PID is already opened by this driver: allocate new CDC device on top of the already opened USB device.
 * 2. USB device with matching VID/PID is NOT opened by this driver yet: poll USB connected devices until it is found.
 *
 * @note This function will block for timeout_ms, if the device is not enumerated at the moment of calling this function.
 * @param[in] vid Vendor ID
 * @param[in] pid Product ID
 * @param[in] timeout_ms Connection timeout [ms]
 * @param[out] dev CDC-ACM device
 * @return esp_err_t
 */
static esp_err_t cdc_acm_find_and_open_usb_device(uint16_t vid, uint16_t pid, int timeout_ms, cdc_dev_t **dev)
{
    assert(p_cdc_acm_obj);
    assert(dev);

    *dev = calloc(1, sizeof(cdc_dev_t));
    if (*dev == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // First, check list of already opened CDC devices
    ESP_LOGD(TAG, "Checking list of opened USB devices");
    cdc_dev_t *cdc_dev;
    SLIST_FOREACH(cdc_dev, &p_cdc_acm_obj->cdc_devices_list, list_entry) {
        const usb_device_desc_t *device_desc;
        ESP_ERROR_CHECK(usb_host_get_device_descriptor(cdc_dev->dev_hdl, &device_desc));
        if ((vid == device_desc->idVendor || vid == CDC_HOST_ANY_VID) &&
                (pid == device_desc->idProduct || pid == CDC_HOST_ANY_PID)) {
            // Return path 1:
            (*dev)->dev_hdl = cdc_dev->dev_hdl;
            return ESP_OK;
        }
    }

    // Second, poll connected devices until new device is connected or timeout
    TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    TimeOut_t connection_timeout;
    vTaskSetTimeOutState(&connection_timeout);

    do {
        ESP_LOGD(TAG, "Checking list of connected USB devices");
        uint8_t dev_addr_list[10];
        int num_of_devices;
        ESP_ERROR_CHECK(usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_of_devices));

        // Go through device address list and find the one we are looking for
        for (int i = 0; i < num_of_devices; i++) {
            usb_device_handle_t current_device;
            // Open USB device
            if (usb_host_device_open(p_cdc_acm_obj->cdc_acm_client_hdl, dev_addr_list[i], &current_device) != ESP_OK) {
                continue; // In case we failed to open this device, continue with next one in the list
            }
            assert(current_device);
            const usb_device_desc_t *device_desc;
            ESP_ERROR_CHECK(usb_host_get_device_descriptor(current_device, &device_desc));
            if ((vid == device_desc->idVendor || vid == CDC_HOST_ANY_VID) &&
                    (pid == device_desc->idProduct || pid == CDC_HOST_ANY_PID)) {
                // Return path 2:
                (*dev)->dev_hdl = current_device;
                return ESP_OK;
            }
            usb_host_device_close(p_cdc_acm_obj->cdc_acm_client_hdl, current_device);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    } while (xTaskCheckForTimeOut(&connection_timeout, &timeout_ticks) == pdFALSE);

    // Timeout was reached, clean-up
    free(*dev);
    *dev = NULL;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t cdc_acm_host_install(const cdc_acm_host_driver_config_t *driver_config)
{
    CDC_ACM_CHECK(!p_cdc_acm_obj, ESP_ERR_INVALID_STATE);

    // Check driver configuration, use default if NULL is passed
    if (driver_config == NULL) {
        driver_config = &cdc_acm_driver_config_default;
    }

    // Allocate all we need for this driver
    esp_err_t ret;
    cdc_acm_obj_t *cdc_acm_obj = heap_caps_calloc(1, sizeof(cdc_acm_obj_t), MALLOC_CAP_DEFAULT);
    EventGroupHandle_t event_group = xEventGroupCreate();
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    TaskHandle_t driver_task_h = NULL;
    xTaskCreatePinnedToCore(
        cdc_acm_client_task, "USB-CDC", driver_config->driver_task_stack_size, NULL,
        driver_config->driver_task_priority, &driver_task_h, driver_config->xCoreID);

    if (cdc_acm_obj == NULL || driver_task_h == NULL || event_group == NULL || mutex == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto err;
    }

    // Register USB Host client
    usb_host_client_handle_t usb_client = NULL;
    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 3,
        .async.client_event_callback = usb_event_cb,
        .async.callback_arg = NULL
    };
    ESP_GOTO_ON_ERROR(usb_host_client_register(&client_config, &usb_client), err, TAG, "Failed to register USB host client");

    // Initialize CDC-ACM driver structure
    SLIST_INIT(&(cdc_acm_obj->cdc_devices_list));
    cdc_acm_obj->event_group = event_group;
    cdc_acm_obj->open_close_mutex = mutex;
    cdc_acm_obj->cdc_acm_client_hdl = usb_client;
    cdc_acm_obj->new_dev_cb = driver_config->new_dev_cb;

    // Between 1st call of this function and following section, another task might try to install this driver:
    // Make sure that there is only one instance of this driver in the system
    CDC_ACM_ENTER_CRITICAL();
    if (p_cdc_acm_obj) {
        // Already created
        ret = ESP_ERR_INVALID_STATE;
        CDC_ACM_EXIT_CRITICAL();
        goto client_err;
    } else {
        p_cdc_acm_obj = cdc_acm_obj;
    }
    CDC_ACM_EXIT_CRITICAL();

    // Everything OK: Start CDC-Driver task and return
    xTaskNotifyGive(driver_task_h);
    return ESP_OK;

client_err:
    usb_host_client_deregister(usb_client);
err: // Clean-up
    free(cdc_acm_obj);
    if (event_group) {
        vEventGroupDelete(event_group);
    }
    if (driver_task_h) {
        vTaskDelete(driver_task_h);
    }
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
    return ret;
}

esp_err_t cdc_acm_host_uninstall()
{
    esp_err_t ret;

    CDC_ACM_ENTER_CRITICAL();
    CDC_ACM_CHECK_FROM_CRIT(p_cdc_acm_obj, ESP_ERR_INVALID_STATE);
    cdc_acm_obj_t *cdc_acm_obj = p_cdc_acm_obj; // Save Driver's handle to temporary handle
    CDC_ACM_EXIT_CRITICAL();

    xSemaphoreTake(p_cdc_acm_obj->open_close_mutex, portMAX_DELAY); // Wait for all open/close calls to finish

    CDC_ACM_ENTER_CRITICAL();
    if (SLIST_EMPTY(&p_cdc_acm_obj->cdc_devices_list)) { // Check that device list is empty (all devices closed)
        p_cdc_acm_obj = NULL; // NULL static driver pointer: No open/close calls form this point
    } else {
        ret = ESP_ERR_INVALID_STATE;
        CDC_ACM_EXIT_CRITICAL();
        goto unblock;
    }
    CDC_ACM_EXIT_CRITICAL();

    // Signal to CDC task to stop, unblock it and wait for its deletion
    xEventGroupSetBits(cdc_acm_obj->event_group, CDC_ACM_TEARDOWN);
    usb_host_client_unblock(cdc_acm_obj->cdc_acm_client_hdl);
    ESP_GOTO_ON_FALSE(
        xEventGroupWaitBits(cdc_acm_obj->event_group, CDC_ACM_TEARDOWN_COMPLETE, pdFALSE, pdFALSE, pdMS_TO_TICKS(100)),
        ESP_ERR_NOT_FINISHED, unblock, TAG,);

    // Free remaining resources and return
    vEventGroupDelete(cdc_acm_obj->event_group);
    xSemaphoreGive(cdc_acm_obj->open_close_mutex);
    vSemaphoreDelete(cdc_acm_obj->open_close_mutex);
    free(cdc_acm_obj);
    return ESP_OK;

unblock:
    xSemaphoreGive(cdc_acm_obj->open_close_mutex);
    return ret;
}

esp_err_t cdc_acm_host_register_new_dev_callback(cdc_acm_new_dev_callback_t new_dev_cb)
{
    CDC_ACM_ENTER_CRITICAL();
    p_cdc_acm_obj->new_dev_cb = new_dev_cb;
    CDC_ACM_EXIT_CRITICAL();
    return ESP_OK;
}

/**
 * @brief Free USB transfers used by this device
 *
 * @note There can be no transfers in flight, at the moment of calling this function.
 * @param[in] cdc_dev Pointer to CDC device
 */
static void cdc_acm_transfers_free(cdc_dev_t *cdc_dev)
{
    assert(cdc_dev);
    if (cdc_dev->notif.xfer != NULL) {
        usb_host_transfer_free(cdc_dev->notif.xfer);
    }
    if (cdc_dev->data.in_xfer != NULL) {
        cdc_acm_reset_in_transfer(cdc_dev);
        usb_host_transfer_free(cdc_dev->data.in_xfer);
    }
    if (cdc_dev->data.out_xfer != NULL) {
        if (cdc_dev->data.out_xfer->context != NULL) {
            vSemaphoreDelete((SemaphoreHandle_t)cdc_dev->data.out_xfer->context);
        }
        if (cdc_dev->data.out_mux != NULL) {
            vSemaphoreDelete(cdc_dev->data.out_mux);
        }
        usb_host_transfer_free(cdc_dev->data.out_xfer);
    }
    if (cdc_dev->ctrl_transfer != NULL) {
        if (cdc_dev->ctrl_transfer->context != NULL) {
            vSemaphoreDelete((SemaphoreHandle_t)cdc_dev->ctrl_transfer->context);
        }
        if (cdc_dev->ctrl_mux != NULL) {
            vSemaphoreDelete(cdc_dev->ctrl_mux);
        }
        usb_host_transfer_free(cdc_dev->ctrl_transfer);
    }
}

/**
 * @brief Allocate CDC transfers
 *
 * @param[in] cdc_dev       Pointer to CDC device
 * @param[in] notif_ep_desc Pointer to notification EP descriptor
 * @param[in] in_ep_desc-   Pointer to data IN EP descriptor
 * @param[in] in_buf_len    Length of data IN buffer
 * @param[in] out_ep_desc   Pointer to data OUT EP descriptor
 * @param[in] out_buf_len   Length of data OUT buffer
 * @return
 *     - ESP_OK:            Success
 *     - ESP_ERR_NO_MEM:    Not enough memory for transfers and semaphores allocation
 *     - ESP_ERR_NOT_FOUND: IN or OUT endpoints were not found in the selected interface
 */
static esp_err_t cdc_acm_transfers_allocate(cdc_dev_t *cdc_dev, const usb_ep_desc_t *notif_ep_desc, const usb_ep_desc_t *in_ep_desc, size_t in_buf_len, const usb_ep_desc_t *out_ep_desc, size_t out_buf_len)
{
    assert(in_ep_desc);
    assert(out_ep_desc);
    esp_err_t ret;

    // 1. Setup notification transfer if it is supported
    if (notif_ep_desc) {
        ESP_GOTO_ON_ERROR(
            usb_host_transfer_alloc(USB_EP_DESC_GET_MPS(notif_ep_desc), 0, &cdc_dev->notif.xfer),
            err, TAG,);
        cdc_dev->notif.xfer->device_handle = cdc_dev->dev_hdl;
        cdc_dev->notif.xfer->bEndpointAddress = notif_ep_desc->bEndpointAddress;
        cdc_dev->notif.xfer->callback = notif_xfer_cb;
        cdc_dev->notif.xfer->context = cdc_dev;
        cdc_dev->notif.xfer->num_bytes = USB_EP_DESC_GET_MPS(notif_ep_desc);
    }

    // 2. Setup control transfer
    ESP_GOTO_ON_ERROR(
        usb_host_transfer_alloc(CDC_ACM_CTRL_TRANSFER_SIZE, 0, &cdc_dev->ctrl_transfer),
        err, TAG,);
    cdc_dev->ctrl_transfer->timeout_ms = 1000;
    cdc_dev->ctrl_transfer->bEndpointAddress = 0;
    cdc_dev->ctrl_transfer->device_handle = cdc_dev->dev_hdl;
    cdc_dev->ctrl_transfer->callback = out_xfer_cb;
    cdc_dev->ctrl_transfer->context = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(cdc_dev->ctrl_transfer->context, ESP_ERR_NO_MEM, err, TAG,);
    cdc_dev->ctrl_mux = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(cdc_dev->ctrl_mux, ESP_ERR_NO_MEM, err, TAG,);

    // 3. Setup IN data transfer (if it is required (in_buf_len > 0))
    if (in_buf_len != 0) {
        ESP_GOTO_ON_ERROR(
            usb_host_transfer_alloc(in_buf_len, 0, &cdc_dev->data.in_xfer),
            err, TAG,
        );
        assert(cdc_dev->data.in_xfer);
        cdc_dev->data.in_xfer->callback = in_xfer_cb;
        cdc_dev->data.in_xfer->num_bytes = in_buf_len;
        cdc_dev->data.in_xfer->bEndpointAddress = in_ep_desc->bEndpointAddress;
        cdc_dev->data.in_xfer->device_handle = cdc_dev->dev_hdl;
        cdc_dev->data.in_xfer->context = cdc_dev;
        cdc_dev->data.in_mps = USB_EP_DESC_GET_MPS(in_ep_desc);
        cdc_dev->data.in_data_buffer_base = cdc_dev->data.in_xfer->data_buffer;
    }

    // 4. Setup OUT bulk transfer (if it is required (out_buf_len > 0))
    if (out_buf_len != 0) {
        ESP_GOTO_ON_ERROR(
            usb_host_transfer_alloc(out_buf_len, 0, &cdc_dev->data.out_xfer),
            err, TAG,
        );
        assert(cdc_dev->data.out_xfer);
        cdc_dev->data.out_xfer->device_handle = cdc_dev->dev_hdl;
        cdc_dev->data.out_xfer->context = xSemaphoreCreateBinary();
        ESP_GOTO_ON_FALSE(cdc_dev->data.out_xfer->context, ESP_ERR_NO_MEM, err, TAG,);
        cdc_dev->data.out_mux = xSemaphoreCreateMutex();
        ESP_GOTO_ON_FALSE(cdc_dev->data.out_mux, ESP_ERR_NO_MEM, err, TAG,);
        cdc_dev->data.out_xfer->bEndpointAddress = out_ep_desc->bEndpointAddress;
        cdc_dev->data.out_xfer->callback = out_xfer_cb;
    }
    return ESP_OK;

err:
    cdc_acm_transfers_free(cdc_dev);
    return ret;
}

esp_err_t cdc_acm_host_open(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_acm_host_device_config_t *dev_config, cdc_acm_dev_hdl_t *cdc_hdl_ret)
{
    esp_err_t ret;
    CDC_ACM_CHECK(p_cdc_acm_obj, ESP_ERR_INVALID_STATE);
    CDC_ACM_CHECK(dev_config, ESP_ERR_INVALID_ARG);
    CDC_ACM_CHECK(cdc_hdl_ret, ESP_ERR_INVALID_ARG);

    xSemaphoreTake(p_cdc_acm_obj->open_close_mutex, portMAX_DELAY);
    // Find underlying USB device
    cdc_dev_t *cdc_dev;
    ret =  cdc_acm_find_and_open_usb_device(vid, pid, dev_config->connection_timeout_ms, &cdc_dev);
    if (ESP_OK != ret) {
        goto exit;
    }

    // Get Device and Configuration descriptors
    const usb_config_desc_t *config_desc;
    const usb_device_desc_t *device_desc;
    ESP_ERROR_CHECK(usb_host_get_device_descriptor(cdc_dev->dev_hdl, &device_desc));
    ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(cdc_dev->dev_hdl, &config_desc));

    // Parse the required interface descriptor
    cdc_parsed_info_t cdc_info;
    ESP_GOTO_ON_ERROR(
        cdc_parse_interface_descriptor(device_desc, config_desc, interface_idx, &cdc_info),
        err, TAG, "Could not open required interface as CDC");

    // Save all members of cdc_dev
    cdc_dev->data.intf_desc = cdc_info.data_intf;
    cdc_dev->data_protocol = (cdc_data_protocol_t)cdc_dev->data.intf_desc->bInterfaceProtocol;
    cdc_dev->notif.intf_desc = cdc_info.notif_intf;
    if (cdc_info.notif_intf) {
        cdc_dev->comm_protocol = (cdc_comm_protocol_t)cdc_dev->notif.intf_desc->bInterfaceProtocol;
    }
    cdc_dev->cdc_func_desc = cdc_info.func;
    cdc_dev->cdc_func_desc_cnt = cdc_info.func_cnt;

    // The following line is here for backward compatibility with v1.0.*
    // where fixed size of IN buffer (equal to IN Maximum Packet Size) was used
    const size_t in_buf_size = (dev_config->data_cb && (dev_config->in_buffer_size == 0)) ? USB_EP_DESC_GET_MPS(cdc_info.in_ep) : dev_config->in_buffer_size;

    // Allocate USB transfers, claim CDC interfaces and return CDC-ACM handle
    ESP_GOTO_ON_ERROR(
        cdc_acm_transfers_allocate(cdc_dev, cdc_info.notif_ep, cdc_info.in_ep, in_buf_size, cdc_info.out_ep, dev_config->out_buffer_size),
        err, TAG,);
    ESP_GOTO_ON_ERROR(cdc_acm_start(cdc_dev, dev_config->event_cb, dev_config->data_cb, dev_config->user_arg), err, TAG,);
    *cdc_hdl_ret = (cdc_acm_dev_hdl_t)cdc_dev;
    xSemaphoreGive(p_cdc_acm_obj->open_close_mutex);
    return ESP_OK;

err:
    cdc_acm_device_remove(cdc_dev);
exit:
    xSemaphoreGive(p_cdc_acm_obj->open_close_mutex);
    *cdc_hdl_ret = NULL;
    return ret;
}

esp_err_t cdc_acm_host_close(cdc_acm_dev_hdl_t cdc_hdl)
{
    CDC_ACM_CHECK(p_cdc_acm_obj, ESP_ERR_INVALID_STATE);
    CDC_ACM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);

    xSemaphoreTake(p_cdc_acm_obj->open_close_mutex, portMAX_DELAY);

    // Make sure that the device is in the devices list (that it is not already closed)
    cdc_dev_t *cdc_dev;
    bool device_found = false;
    CDC_ACM_ENTER_CRITICAL();
    SLIST_FOREACH(cdc_dev, &p_cdc_acm_obj->cdc_devices_list, list_entry) {
        if (cdc_dev == (cdc_dev_t *)cdc_hdl) {
            device_found = true;
            break;
        }
    }

    // Device was not found in the cdc_devices_list; it was already closed, return OK
    if (!device_found) {
        CDC_ACM_EXIT_CRITICAL();
        xSemaphoreGive(p_cdc_acm_obj->open_close_mutex);
        return ESP_OK;
    }

    // No user callbacks from this point
    cdc_dev->notif.cb = NULL;
    cdc_dev->data.in_cb = NULL;
    CDC_ACM_EXIT_CRITICAL();

    // Cancel polling of BULK IN and INTERRUPT IN
    if (cdc_dev->data.in_xfer) {
        ESP_ERROR_CHECK(cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->data.in_xfer));
    }
    if (cdc_dev->notif.xfer != NULL) {
        ESP_ERROR_CHECK(cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->notif.xfer));
    }

    // Release all interfaces
    ESP_ERROR_CHECK(usb_host_interface_release(p_cdc_acm_obj->cdc_acm_client_hdl, cdc_dev->dev_hdl, cdc_dev->data.intf_desc->bInterfaceNumber));
    if ((cdc_dev->notif.intf_desc != NULL) && (cdc_dev->notif.intf_desc != cdc_dev->data.intf_desc)) {
        ESP_ERROR_CHECK(usb_host_interface_release(p_cdc_acm_obj->cdc_acm_client_hdl, cdc_dev->dev_hdl, cdc_dev->notif.intf_desc->bInterfaceNumber));
    }

    CDC_ACM_ENTER_CRITICAL();
    SLIST_REMOVE(&p_cdc_acm_obj->cdc_devices_list, cdc_dev, cdc_dev_s, list_entry);
    CDC_ACM_EXIT_CRITICAL();

    cdc_acm_device_remove(cdc_dev);
    xSemaphoreGive(p_cdc_acm_obj->open_close_mutex);
    return ESP_OK;
}

void cdc_acm_host_desc_print(cdc_acm_dev_hdl_t cdc_hdl)
{
    assert(cdc_hdl);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;

    const usb_device_desc_t *device_desc;
    const usb_config_desc_t *config_desc;
    ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_get_device_descriptor(cdc_dev->dev_hdl, &device_desc));
    ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_get_active_config_descriptor(cdc_dev->dev_hdl, &config_desc));
    usb_print_device_descriptor(device_desc);
    usb_print_config_descriptor(config_desc, cdc_print_desc);
}

/**
 * @brief Check finished transfer status
 *
 * Return to on transfer completed OK.
 * Cancel the transfer and issue user's callback in case of an error.
 *
 * @param[in] transfer Transfer to be checked
 * @return true Transfer completed
 * @return false Transfer NOT completed
 */
static bool cdc_acm_is_transfer_completed(usb_transfer_t *transfer)
{
    cdc_dev_t *cdc_dev = (cdc_dev_t *)transfer->context;
    bool completed = false;

    switch (transfer->status) {
    case USB_TRANSFER_STATUS_COMPLETED:
        completed = true;
        break;
    case USB_TRANSFER_STATUS_NO_DEVICE: // User is notified about device disconnection from usb_event_cb
    case USB_TRANSFER_STATUS_CANCELED:
        break;
    case USB_TRANSFER_STATUS_ERROR:
    case USB_TRANSFER_STATUS_TIMED_OUT:
    case USB_TRANSFER_STATUS_STALL:
    case USB_TRANSFER_STATUS_OVERFLOW:
    case USB_TRANSFER_STATUS_SKIPPED:
    default:
        // Transfer was not completed or cancelled by user. Inform user about this
        if (cdc_dev->notif.cb) {
            const cdc_acm_host_dev_event_data_t error_event = {
                .type = CDC_ACM_HOST_ERROR,
                .data.error = (int) transfer->status
            };
            cdc_dev->notif.cb(&error_event, cdc_dev->cb_arg);
        }
    }
    return completed;
}

static void in_xfer_cb(usb_transfer_t *transfer)
{
    ESP_LOGD(TAG, "in xfer cb");
    cdc_dev_t *cdc_dev = (cdc_dev_t *)transfer->context;

    if (!cdc_acm_is_transfer_completed(transfer)) {
        return;
    }

    if (cdc_dev->data.in_cb) {
        const bool data_processed = cdc_dev->data.in_cb(transfer->data_buffer, transfer->actual_num_bytes, cdc_dev->cb_arg);

        // Information for developers:
        // In order to save RAM and CPU time, the application can indicate that the received data was not processed and that the application expects more data.
        // In this case, the next received data must be appended to the existing buffer.
        // Since the data_buffer in usb_transfer_t is a constant pointer, we must cast away to const qualifier.
        if (!data_processed) {
#if !SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
            // In case the received data was not processed, the next RX data must be appended to current buffer
            uint8_t **ptr = (uint8_t **)(&(transfer->data_buffer));
            *ptr += transfer->actual_num_bytes;

            // Calculate remaining space in the buffer. Attention: pointer arithmetic!
            size_t space_left = transfer->data_buffer_size - (transfer->data_buffer - cdc_dev->data.in_data_buffer_base);
            uint16_t mps = cdc_dev->data.in_mps;
            transfer->num_bytes = (space_left / mps) * mps; // Round down to MPS for next transfer

            if (transfer->num_bytes == 0) {
                // The IN buffer cannot accept more data, inform the user and reset the buffer
                ESP_LOGW(TAG, "IN buffer overflow");
                cdc_dev->serial_state.bOverRun = true;
                if (cdc_dev->notif.cb) {
                    const cdc_acm_host_dev_event_data_t serial_state_event = {
                        .type = CDC_ACM_HOST_SERIAL_STATE,
                        .data.serial_state = cdc_dev->serial_state
                    };
                    cdc_dev->notif.cb(&serial_state_event, cdc_dev->cb_arg);
                }

                cdc_acm_reset_in_transfer(cdc_dev);
                cdc_dev->serial_state.bOverRun = false;
            }
#else
            // For targets that must sync internal memory through L1CACHE, we cannot change the data_buffer
            // because it would lead to unaligned cache sync, which is not allowed
            ESP_LOGW(TAG, "RX buffer append is not yet supported on ESP32-P4!");
#endif
        } else {
            cdc_acm_reset_in_transfer(cdc_dev);
        }
    }

    ESP_LOGD(TAG, "Submitting poll for BULK IN transfer");
    usb_host_transfer_submit(cdc_dev->data.in_xfer);
}

static void notif_xfer_cb(usb_transfer_t *transfer)
{
    ESP_LOGD(TAG, "notif xfer cb");
    cdc_dev_t *cdc_dev = (cdc_dev_t *)transfer->context;

    if (cdc_acm_is_transfer_completed(transfer)) {
        cdc_notification_t *notif = (cdc_notification_t *)transfer->data_buffer;
        switch (notif->bNotificationCode) {
        case USB_CDC_NOTIF_NETWORK_CONNECTION: {
            if (cdc_dev->notif.cb) {
                const cdc_acm_host_dev_event_data_t net_conn_event = {
                    .type = CDC_ACM_HOST_NETWORK_CONNECTION,
                    .data.network_connected = (bool) notif->wValue
                };
                cdc_dev->notif.cb(&net_conn_event, cdc_dev->cb_arg);
            }
            break;
        }
        case USB_CDC_NOTIF_SERIAL_STATE: {
            cdc_dev->serial_state.val = *((uint16_t *)notif->Data);
            if (cdc_dev->notif.cb) {
                const cdc_acm_host_dev_event_data_t serial_state_event = {
                    .type = CDC_ACM_HOST_SERIAL_STATE,
                    .data.serial_state = cdc_dev->serial_state
                };
                cdc_dev->notif.cb(&serial_state_event, cdc_dev->cb_arg);
            }
            break;
        }
        case USB_CDC_NOTIF_RESPONSE_AVAILABLE: // Encapsulated commands not implemented - fallthrough
        default:
            ESP_LOGW(TAG, "Unsupported notification type 0x%02X", notif->bNotificationCode);
            ESP_LOG_BUFFER_HEX(TAG, transfer->data_buffer, transfer->actual_num_bytes);
            break;
        }

        // Start polling for new data again
        ESP_LOGD(TAG, "Submitting poll for INTR IN transfer");
        usb_host_transfer_submit(cdc_dev->notif.xfer);
    }
}

static void out_xfer_cb(usb_transfer_t *transfer)
{
    ESP_LOGD(TAG, "out/ctrl xfer cb");
    assert(transfer->context);
    xSemaphoreGive((SemaphoreHandle_t)transfer->context);
}

static void usb_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        // Guard p_cdc_acm_obj->new_dev_cb from concurrent access
        ESP_LOGD(TAG, "New device connected");
        CDC_ACM_ENTER_CRITICAL();
        cdc_acm_new_dev_callback_t _new_dev_cb = p_cdc_acm_obj->new_dev_cb;
        CDC_ACM_EXIT_CRITICAL();

        if (_new_dev_cb) {
            usb_device_handle_t new_dev;
            if (usb_host_device_open(p_cdc_acm_obj->cdc_acm_client_hdl, event_msg->new_dev.address, &new_dev) != ESP_OK) {
                break;
            }
            assert(new_dev);
            _new_dev_cb(new_dev);
            usb_host_device_close(p_cdc_acm_obj->cdc_acm_client_hdl, new_dev);
        }

        break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE: {
        ESP_LOGD(TAG, "Device suddenly disconnected");
        // Find CDC pseudo-devices associated with this USB device and close them
        cdc_dev_t *cdc_dev;
        cdc_dev_t *tcdc_dev;
        // We are using 'SAFE' version of 'SLIST_FOREACH' which enables user to close the disconnected device in the callback
        SLIST_FOREACH_SAFE(cdc_dev, &p_cdc_acm_obj->cdc_devices_list, list_entry, tcdc_dev) {
            if (cdc_dev->dev_hdl == event_msg->dev_gone.dev_hdl && cdc_dev->notif.cb) {
                // The suddenly disconnected device was opened by this driver: inform user about this
                const cdc_acm_host_dev_event_data_t disconn_event = {
                    .type = CDC_ACM_HOST_DEVICE_DISCONNECTED,
                    .data.cdc_hdl = (cdc_acm_dev_hdl_t) cdc_dev,
                };
                cdc_dev->notif.cb(&disconn_event, cdc_dev->cb_arg);
            }
        }
        break;
    }
    default:
        assert(false);
        break;
    }
}

esp_err_t cdc_acm_host_data_tx_blocking(cdc_acm_dev_hdl_t cdc_hdl, const uint8_t *data, size_t data_len, uint32_t timeout_ms)
{
    esp_err_t ret;
    CDC_ACM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    CDC_ACM_CHECK(data && (data_len > 0), ESP_ERR_INVALID_ARG);
    CDC_ACM_CHECK(cdc_dev->data.out_xfer, ESP_ERR_NOT_SUPPORTED); // Device was opened as read-only.
    CDC_ACM_CHECK(data_len <= cdc_dev->data.out_xfer->data_buffer_size, ESP_ERR_INVALID_SIZE);

    // Take OUT mutex and fill the OUT transfer
    BaseType_t taken = xSemaphoreTake(cdc_dev->data.out_mux, pdMS_TO_TICKS(timeout_ms));
    if (taken != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGD(TAG, "Submitting BULK OUT transfer");
    SemaphoreHandle_t transfer_finished_semaphore = (SemaphoreHandle_t)cdc_dev->data.out_xfer->context;
    xSemaphoreTake(transfer_finished_semaphore, 0); // Make sure the semaphore is taken before we submit new transfer

    memcpy(cdc_dev->data.out_xfer->data_buffer, data, data_len);
    cdc_dev->data.out_xfer->num_bytes = data_len;
    cdc_dev->data.out_xfer->timeout_ms = timeout_ms;
    ESP_GOTO_ON_ERROR(usb_host_transfer_submit(cdc_dev->data.out_xfer), unblock, TAG,);

    // Wait for OUT transfer completion
    taken = xSemaphoreTake(transfer_finished_semaphore, pdMS_TO_TICKS(timeout_ms));
    if (!taken) {
        cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->data.out_xfer); // Resetting the endpoint will cause all in-progress transfers to complete
        ESP_LOGW(TAG, "TX transfer timeout");
        ret = ESP_ERR_TIMEOUT;
        goto unblock;
    }

    ESP_GOTO_ON_FALSE(cdc_dev->data.out_xfer->status == USB_TRANSFER_STATUS_COMPLETED, ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Bulk OUT transfer error");
    ESP_GOTO_ON_FALSE(cdc_dev->data.out_xfer->actual_num_bytes == data_len, ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Incorrect number of bytes transferred");
    ret = ESP_OK;

unblock:
    xSemaphoreGive(cdc_dev->data.out_mux);
    return ret;
}

esp_err_t cdc_acm_host_line_coding_get(cdc_acm_dev_hdl_t cdc_hdl, cdc_acm_line_coding_t *line_coding)
{
    CDC_ACM_CHECK(line_coding, ESP_ERR_INVALID_ARG);

    ESP_RETURN_ON_ERROR(
        send_cdc_request((cdc_dev_t *)cdc_hdl, true, USB_CDC_REQ_GET_LINE_CODING, (uint8_t *)line_coding, sizeof(cdc_acm_line_coding_t), 0),
        TAG,);
    ESP_LOGD(TAG, "Line Get: Rate: %"PRIu32", Stop bits: %d, Parity: %d, Databits: %d", line_coding->dwDTERate,
             line_coding->bCharFormat, line_coding->bParityType, line_coding->bDataBits);
    return ESP_OK;
}

esp_err_t cdc_acm_host_line_coding_set(cdc_acm_dev_hdl_t cdc_hdl, const cdc_acm_line_coding_t *line_coding)
{
    CDC_ACM_CHECK(line_coding, ESP_ERR_INVALID_ARG);

    ESP_RETURN_ON_ERROR(
        send_cdc_request((cdc_dev_t *)cdc_hdl, false, USB_CDC_REQ_SET_LINE_CODING, (uint8_t *)line_coding, sizeof(cdc_acm_line_coding_t), 0),
        TAG,);
    ESP_LOGD(TAG, "Line Set: Rate: %"PRIu32", Stop bits: %d, Parity: %d, Databits: %d", line_coding->dwDTERate,
             line_coding->bCharFormat, line_coding->bParityType, line_coding->bDataBits);
    return ESP_OK;
}

esp_err_t cdc_acm_host_set_control_line_state(cdc_acm_dev_hdl_t cdc_hdl, bool dtr, bool rts)
{
    const uint16_t ctrl_bitmap = (uint16_t)dtr | ((uint16_t)rts << 1);

    ESP_RETURN_ON_ERROR(
        send_cdc_request((cdc_dev_t *)cdc_hdl, false, USB_CDC_REQ_SET_CONTROL_LINE_STATE, NULL, 0, ctrl_bitmap),
        TAG,);
    ESP_LOGD(TAG, "Control Line Set: DTR: %d, RTS: %d", dtr, rts);
    return ESP_OK;
}

esp_err_t cdc_acm_host_send_break(cdc_acm_dev_hdl_t cdc_hdl, uint16_t duration_ms)
{
    ESP_RETURN_ON_ERROR(
        send_cdc_request((cdc_dev_t *)cdc_hdl, false, USB_CDC_REQ_SEND_BREAK, NULL, 0, duration_ms),
        TAG,);

    // Block until break is deasserted
    vTaskDelay(pdMS_TO_TICKS(duration_ms + 1));
    return ESP_OK;
}

esp_err_t cdc_acm_host_send_custom_request(cdc_acm_dev_hdl_t cdc_hdl, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t *data)
{
    CDC_ACM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    if (wLength > 0) {
        CDC_ACM_CHECK(data, ESP_ERR_INVALID_ARG);
    }
    CDC_ACM_CHECK(cdc_dev->ctrl_transfer->data_buffer_size >= wLength, ESP_ERR_INVALID_SIZE);

    esp_err_t ret;

    // Take Mutex and fill the CTRL request
    BaseType_t taken = xSemaphoreTake(cdc_dev->ctrl_mux, pdMS_TO_TICKS(CDC_ACM_CTRL_TIMEOUT_MS));
    if (!taken) {
        return ESP_ERR_TIMEOUT;
    }
    usb_setup_packet_t *req = (usb_setup_packet_t *)(cdc_dev->ctrl_transfer->data_buffer);
    uint8_t *start_of_data = (uint8_t *)req + sizeof(usb_setup_packet_t);
    req->bmRequestType = bmRequestType;
    req->bRequest = bRequest;
    req->wValue = wValue;
    req->wIndex = wIndex;
    req->wLength = wLength;

    // For IN transfers we must transfer data ownership to CDC driver
    const bool in_transfer = bmRequestType & USB_BM_REQUEST_TYPE_DIR_IN;
    if (!in_transfer) {
        memcpy(start_of_data, data, wLength);
    }

    cdc_dev->ctrl_transfer->num_bytes = wLength + sizeof(usb_setup_packet_t);
    ESP_GOTO_ON_ERROR(
        usb_host_transfer_submit_control(p_cdc_acm_obj->cdc_acm_client_hdl, cdc_dev->ctrl_transfer),
        unblock, TAG, "CTRL transfer failed");

    taken = xSemaphoreTake((SemaphoreHandle_t)cdc_dev->ctrl_transfer->context, pdMS_TO_TICKS(CDC_ACM_CTRL_TIMEOUT_MS));
    if (!taken) {
        // Transfer was not finished, error in USB LIB. Reset the endpoint
        cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->ctrl_transfer);
        ret = ESP_ERR_TIMEOUT;
        goto unblock;
    }

    ESP_GOTO_ON_FALSE(cdc_dev->ctrl_transfer->status == USB_TRANSFER_STATUS_COMPLETED, ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Control transfer error");
    // ESP_GOTO_ON_FALSE(cdc_dev->ctrl_transfer->actual_num_bytes == cdc_dev->ctrl_transfer->num_bytes, ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Incorrect number of bytes transferred");

    // For OUT transfers, we must transfer data ownership to user
    if (in_transfer) {
        memcpy(data, start_of_data, wLength);
    }
    ret = ESP_OK;

unblock:
    xSemaphoreGive(cdc_dev->ctrl_mux);
    return ret;
}

static esp_err_t send_cdc_request(cdc_dev_t *cdc_dev, bool in_transfer, cdc_request_code_t request, uint8_t *data, uint16_t data_len, uint16_t value)
{
    CDC_ACM_CHECK(cdc_dev, ESP_ERR_INVALID_ARG);
    CDC_ACM_CHECK(cdc_dev->notif.intf_desc, ESP_ERR_NOT_SUPPORTED);

    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    if (in_transfer) {
        req_type |= USB_BM_REQUEST_TYPE_DIR_IN;
    } else {
        req_type |= USB_BM_REQUEST_TYPE_DIR_OUT;
    }
    return cdc_acm_host_send_custom_request((cdc_acm_dev_hdl_t) cdc_dev, req_type, request, value, cdc_dev->notif.intf_desc->bInterfaceNumber, data_len, data);
}

esp_err_t cdc_acm_host_protocols_get(cdc_acm_dev_hdl_t cdc_hdl, cdc_comm_protocol_t *comm, cdc_data_protocol_t *data)
{
    CDC_ACM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;

    if (comm != NULL) {
        *comm = cdc_dev->comm_protocol;
    }
    if (data != NULL) {
        *data = cdc_dev->data_protocol;
    }
    return ESP_OK;
}

esp_err_t cdc_acm_host_cdc_desc_get(cdc_acm_dev_hdl_t cdc_hdl, cdc_desc_subtype_t desc_type, const usb_standard_desc_t **desc_out)
{
    CDC_ACM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    CDC_ACM_CHECK(desc_type < USB_CDC_DESC_SUBTYPE_MAX, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    *desc_out = NULL;

    for (int i = 0; i < cdc_dev->cdc_func_desc_cnt; i++) {
        const cdc_header_desc_t *_desc = (const cdc_header_desc_t *)((*(cdc_dev->cdc_func_desc))[i]);
        if (_desc->bDescriptorSubtype == desc_type) {
            ret = ESP_OK;
            *desc_out = (const usb_standard_desc_t *)_desc;
            break;
        }
    }
    return ret;
}
