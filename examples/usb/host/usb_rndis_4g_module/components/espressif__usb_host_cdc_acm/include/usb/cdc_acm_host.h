/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "usb/usb_host.h"
#include "usb/usb_types_cdc.h"
#include "esp_err.h"

// Pass these to cdc_acm_host_open() to signal that you don't care about VID/PID of the opened device
#define CDC_HOST_ANY_VID (0)
#define CDC_HOST_ANY_PID (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cdc_dev_s *cdc_acm_dev_hdl_t;

/**
 * @brief CDC-ACM Device Event types to upper layer
 *
 */
typedef enum {
    CDC_ACM_HOST_ERROR,
    CDC_ACM_HOST_SERIAL_STATE,
    CDC_ACM_HOST_NETWORK_CONNECTION,
    CDC_ACM_HOST_DEVICE_DISCONNECTED
} cdc_acm_host_dev_event_t;

/**
 * @brief CDC-ACM Device Event data structure
 *
 */
typedef struct {
    cdc_acm_host_dev_event_t type;
    union {
        int error;                         //!< Error code from USB Host
        cdc_acm_uart_state_t serial_state; //!< Serial (UART) state
        bool network_connected;            //!< Network connection event
        cdc_acm_dev_hdl_t cdc_hdl;         //!< Disconnection event
    } data;
} cdc_acm_host_dev_event_data_t;

/**
 * @brief New USB device callback
 *
 * Provides already opened usb_dev, that will be closed after this callback returns.
 * This is useful for peeking device's descriptors, e.g. peeking VID/PID and loading proper driver.
 *
 * @attention This callback is called from USB Host context, so the CDC device can't be opened here.
 */
typedef void (*cdc_acm_new_dev_callback_t)(usb_device_handle_t usb_dev);

/**
 * @brief Data receive callback type
 *
 * @param[in] data     Pointer to received data
 * @param[in] data_len Length of received data in bytes
 * @param[in] user_arg User's argument passed to open function
 * @return true        Received data was processed     -> Flush RX buffer
 * @return false       Received data was NOT processed -> Append new data to the buffer
 */
typedef bool (*cdc_acm_data_callback_t)(const uint8_t *data, size_t data_len, void *user_arg);

/**
 * @brief Device event callback type
 *
 * @param[in] event    Event structure
 * @param[in] user_arg User's argument passed to open function
 */
typedef void (*cdc_acm_host_dev_callback_t)(const cdc_acm_host_dev_event_data_t *event, void *user_ctx);

/**
 * @brief Configuration structure of USB Host CDC-ACM driver
 *
 */
typedef struct {
    size_t driver_task_stack_size;         /**< Stack size of the driver's task */
    unsigned driver_task_priority;         /**< Priority of the driver's task */
    int  xCoreID;                          /**< Core affinity of the driver's task */
    cdc_acm_new_dev_callback_t new_dev_cb; /**< New USB device connected callback. Can be NULL. */
} cdc_acm_host_driver_config_t;

/**
 * @brief Configuration structure of CDC-ACM device
 *
 */
typedef struct {
    uint32_t connection_timeout_ms;       /**< Timeout for USB device connection in [ms] */
    size_t out_buffer_size;               /**< Maximum size of USB bulk out transfer, set to 0 for read-only devices */
    size_t in_buffer_size;                /**< Maximum size of USB bulk in transfer */
    cdc_acm_host_dev_callback_t event_cb; /**< Device's event callback function. Can be NULL */
    cdc_acm_data_callback_t data_cb;      /**< Device's data RX callback function. Can be NULL for write-only devices */
    void *user_arg;                       /**< User's argument that will be passed to the callbacks */
} cdc_acm_host_device_config_t;

/**
 * @brief Install CDC-ACM driver
 *
 * - USB Host Library must already be installed before calling this function (via usb_host_install())
 * - This function should be called before calling any other CDC driver functions
 *
 * @param[in] driver_config Driver configuration structure. If set to NULL, a default configuration will be used.
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_STATE: The CDC driver is already installed or USB host library is not installed
 *   - ESP_ERR_NO_MEM: Not enough memory for installing the driver
 */
esp_err_t cdc_acm_host_install(const cdc_acm_host_driver_config_t *driver_config);

/**
 * @brief Uninstall CDC-ACM driver
 *
 * - Users must ensure that all CDC devices must be closed via cdc_acm_host_close() before calling this function
 *
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_STATE: The CDC driver is not installed or not all CDC devices are closed
 *   - ESP_ERR_NOT_FINISHED: The CDC driver failed to uninstall completely
 */
esp_err_t cdc_acm_host_uninstall(void);

/**
 * @brief Register new USB device callback
 *
 * The callback will be called for every new USB device, not just CDC-ACM class.
 *
 * @param[in] new_dev_cb New device callback function
 * @return
 *   - ESP_OK: Success
 */
esp_err_t cdc_acm_host_register_new_dev_callback(cdc_acm_new_dev_callback_t new_dev_cb);

/**
 * @brief Open CDC-ACM device
 *
 * The driver first looks for CDC compliant descriptor, if it is not found the driver checks if the interface has 2 Bulk endpoints that can be used for data
 *
 * Use CDC_HOST_ANY_* macros to signal that you don't care about the device's VID and PID. In this case, first USB device will be opened.
 * It is recommended to use this feature if only one device can ever be in the system (there is no USB HUB connected).
 *
 * @param[in] vid           Device's Vendor ID, set to CDC_HOST_ANY_VID for any
 * @param[in] pid           Device's Product ID, set to CDC_HOST_ANY_PID for any
 * @param[in] interface_idx Index of device's interface used for CDC-ACM communication
 * @param[in] dev_config    Configuration structure of the device
 * @param[out] cdc_hdl_ret  CDC device handle
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_STATE: The CDC driver is not installed
 *   - ESP_ERR_INVALID_ARG: dev_config or cdc_hdl_ret is NULL
 *   - ESP_ERR_NO_MEM: Not enough memory for opening the device
 *   - ESP_ERR_NOT_FOUND: USB device with specified VID/PID is not connected or does not have specified interface
 */
esp_err_t cdc_acm_host_open(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_acm_host_device_config_t *dev_config, cdc_acm_dev_hdl_t *cdc_hdl_ret);

// This function is deprecated, please use cdc_acm_host_open()
static inline esp_err_t cdc_acm_host_open_vendor_specific(uint16_t vid, uint16_t pid, uint8_t interface_num, const cdc_acm_host_device_config_t *dev_config, cdc_acm_dev_hdl_t *cdc_hdl_ret)
{
    return cdc_acm_host_open(vid, pid, interface_num, dev_config, cdc_hdl_ret);
}

/**
 * @brief Close CDC device and release its resources
 *
 * @note All in-flight transfers will be prematurely canceled.
 * @param[in] cdc_hdl CDC handle obtained from cdc_acm_host_open()
 * @return
 *   - ESP_OK: Success - device closed
 *   - ESP_ERR_INVALID_STATE: cdc_hdl is NULL or the CDC driver is not installed
 */
esp_err_t cdc_acm_host_close(cdc_acm_dev_hdl_t cdc_hdl);

/**
 * @brief Transmit data - blocking mode
 *
 * @param cdc_hdl CDC handle obtained from cdc_acm_host_open()
 * @param[in] data       Data to be sent
 * @param[in] data_len   Data length
 * @param[in] timeout_ms Timeout in [ms]
 * @return esp_err_t
 */
esp_err_t cdc_acm_host_data_tx_blocking(cdc_acm_dev_hdl_t cdc_hdl, const uint8_t *data, size_t data_len, uint32_t timeout_ms);

/**
 * @brief SetLineCoding function
 *
 * @see Chapter 6.3.10, USB CDC-PSTN specification rev. 1.2
 *
 * @param     cdc_hdl     CDC handle obtained from cdc_acm_host_open()
 * @param[in] line_coding Line Coding structure
 * @return esp_err_t
 */
esp_err_t cdc_acm_host_line_coding_set(cdc_acm_dev_hdl_t cdc_hdl, const cdc_acm_line_coding_t *line_coding);

/**
 * @brief GetLineCoding function
 *
 * @see Chapter 6.3.11, USB CDC-PSTN specification rev. 1.2
 *
 * @param      cdc_hdl     CDC handle obtained from cdc_acm_host_open()
 * @param[out] line_coding Line Coding structure to be filled
 * @return esp_err_t
 */
esp_err_t cdc_acm_host_line_coding_get(cdc_acm_dev_hdl_t cdc_hdl, cdc_acm_line_coding_t *line_coding);

/**
 * @brief SetControlLineState function
 *
 * @see Chapter 6.3.12, USB CDC-PSTN specification rev. 1.2
 *
 * @param     cdc_hdl CDC handle obtained from cdc_acm_host_open()
 * @param[in] dtr     Indicates to DCE if DTE is present or not. This signal corresponds to V.24 signal 108/2 and RS-232 signal Data Terminal Ready.
 * @param[in] rts     Carrier control for half duplex modems. This signal corresponds to V.24 signal 105 and RS-232 signal Request To Send.
 * @return esp_err_t
 */
esp_err_t cdc_acm_host_set_control_line_state(cdc_acm_dev_hdl_t cdc_hdl, bool dtr, bool rts);

/**
 * @brief SendBreak function
 *
 * This function will block until the duration_ms has passed.
 *
 * @see Chapter 6.3.13, USB CDC-PSTN specification rev. 1.2
 *
 * @param     cdc_hdl     CDC handle obtained from cdc_acm_host_open()
 * @param[in] duration_ms Duration of the Break signal in [ms]
 * @return esp_err_t
 */
esp_err_t cdc_acm_host_send_break(cdc_acm_dev_hdl_t cdc_hdl, uint16_t duration_ms);

/**
 * @brief Print device's descriptors
 *
 * Device and full Configuration descriptors are printed in human readable format to stdout.
 *
 * @param cdc_hdl CDC handle obtained from cdc_acm_host_open()
 */
void cdc_acm_host_desc_print(cdc_acm_dev_hdl_t cdc_hdl);

/**
 * @brief Get protocols defined in USB-CDC interface descriptors
 *
 * @param cdc_hdl   CDC handle obtained from cdc_acm_host_open()
 * @param[out] comm Communication protocol
 * @param[out] data Data protocol
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid device
 */
esp_err_t cdc_acm_host_protocols_get(cdc_acm_dev_hdl_t cdc_hdl, cdc_comm_protocol_t *comm, cdc_data_protocol_t *data);

/**
 * @brief Get CDC functional descriptor
 *
 * @param cdc_hdl       CDC handle obtained from cdc_acm_host_open()
 * @param[in] desc_type Type of functional descriptor
 * @param[out] desc_out Pointer to the required descriptor
 * @return
 *   - ESP_OK: Success
 *   - ESP_ERR_INVALID_ARG: Invalid device or descriptor type
 *   - ESP_ERR_NOT_FOUND: The required descriptor is not present in the device
 */
esp_err_t cdc_acm_host_cdc_desc_get(cdc_acm_dev_hdl_t cdc_hdl, cdc_desc_subtype_t desc_type, const usb_standard_desc_t **desc_out);

/**
 * @brief Send command to CTRL endpoint
 *
 * Sends Control transfer as described in USB specification chapter 9.
 * This function can be used by device drivers that use custom/vendor specific commands.
 * These commands can either extend or replace commands defined in USB CDC-PSTN specification rev. 1.2.
 *
 * @param        cdc_hdl       CDC handle obtained from cdc_acm_host_open()
 * @param[in]    bmRequestType Field of USB control request
 * @param[in]    bRequest      Field of USB control request
 * @param[in]    wValue        Field of USB control request
 * @param[in]    wIndex        Field of USB control request
 * @param[in]    wLength       Field of USB control request
 * @param[inout] data          Field of USB control request
 * @return esp_err_t
 */
esp_err_t cdc_acm_host_send_custom_request(cdc_acm_dev_hdl_t cdc_hdl, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t *data);

#ifdef __cplusplus
}
class CdcAcmDevice {
public:
    // Operators
    CdcAcmDevice() : cdc_hdl(NULL) {};
    virtual ~CdcAcmDevice()
    {
        // Close CDC-ACM device, if it wasn't explicitly closed
        if (this->cdc_hdl != NULL) {
            this->close();
        }
    }

    inline esp_err_t tx_blocking(uint8_t *data, size_t len, uint32_t timeout_ms = 100)
    {
        return cdc_acm_host_data_tx_blocking(this->cdc_hdl, data, len, timeout_ms);
    }

    inline esp_err_t open(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_acm_host_device_config_t *dev_config)
    {
        return cdc_acm_host_open(vid, pid, interface_idx, dev_config, &this->cdc_hdl);
    }

    inline esp_err_t open_vendor_specific(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_acm_host_device_config_t *dev_config)
    {
        return cdc_acm_host_open(vid, pid, interface_idx, dev_config, &this->cdc_hdl);
    }

    inline esp_err_t close()
    {
        const esp_err_t err = cdc_acm_host_close(this->cdc_hdl);
        if (err == ESP_OK) {
            this->cdc_hdl = NULL;
        }
        return err;
    }

    virtual inline esp_err_t line_coding_get(cdc_acm_line_coding_t *line_coding) const
    {
        return cdc_acm_host_line_coding_get(this->cdc_hdl, line_coding);
    }

    virtual inline esp_err_t line_coding_set(cdc_acm_line_coding_t *line_coding)
    {
        return cdc_acm_host_line_coding_set(this->cdc_hdl, line_coding);
    }

    virtual inline esp_err_t set_control_line_state(bool dtr, bool rts)
    {
        return cdc_acm_host_set_control_line_state(this->cdc_hdl, dtr, rts);
    }

    virtual inline esp_err_t send_break(uint16_t duration_ms)
    {
        return cdc_acm_host_send_break(this->cdc_hdl, duration_ms);
    }

    inline esp_err_t send_custom_request(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t *data)
    {
        return cdc_acm_host_send_custom_request(this->cdc_hdl, bmRequestType, bRequest, wValue, wIndex, wLength, data);
    }

private:
    CdcAcmDevice &operator= (const CdcAcmDevice &Copy);
    bool operator== (const CdcAcmDevice &param) const;
    bool operator!= (const CdcAcmDevice &param) const;
    cdc_acm_dev_hdl_t cdc_hdl;
};
#endif
