/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "usb/usb_types_stack.h"

typedef void* usbh_port_handle_t;
typedef void* usbh_pipe_handle_t;
typedef void* iot_usbh_urb_handle_t;

typedef enum {
    PORT_EVENT_NONE,            /*!< No event has occurred. Or the previous event is no longer valid */
    PORT_EVENT_CONNECTION,      /*!< A device has been connected to the port */
    PORT_EVENT_DISCONNECTION,   /*!< A device disconnection has been detected */
    PORT_EVENT_ERROR,           /*!< A port error has been detected. Port is now PORT_STATE_RECOVERY  */
    PORT_EVENT_OVERCURRENT,     /*!< Overcurrent detected on the port. Port is now PORT_STATE_RECOVERY */
} usbh_port_event_t;

typedef enum {
    PIPE_EVENT_NONE,                    /*!< The pipe has no events (used to indicate no events when polling) */
    PIPE_EVENT_URB_DONE,                /*!< The pipe has completed an URB. The URB can be dequeued */
    PIPE_EVENT_ERROR_XFER,              /*!< Excessive (three consecutive) transaction errors (e.g., no ACK, bad CRC etc) */
    PIPE_EVENT_ERROR_URB_NOT_AVAIL,     /*!< URB was not available */
    PIPE_EVENT_ERROR_OVERFLOW,          /*!< Received more data than requested. Usually a Packet babble error (i.e., an IN packet has exceeded the endpoint's MPS) */
    PIPE_EVENT_ERROR_STALL,             /*!< Pipe received a STALL response received */
} usbh_pipe_event_t;

typedef enum {
    FIFO_BIAS_BALANCED,                 /*!< Balanced FIFO sizing for RX, Non-periodic TX, and periodic TX */
    FIFO_BIAS_PREFER_RX,                /*!< Bias towards a large RX FIFO */
    FIFO_BIAS_PREFER_TX,                /*!< Bias towards periodic TX FIFO */
} usbh_usb_fifo_bias_t;

typedef enum {
    PORT_EVENT,
    PIPE_EVENT,
} usbh_event_type_t;

typedef struct {
    usbh_event_type_t _type;
    union {
        usbh_port_handle_t port_hdl;
        usbh_pipe_handle_t pipe_handle;
    } _handle;
    union {
        usbh_port_event_t port_event;
        usbh_pipe_event_t pipe_event;
    } _event;
} usbh_event_msg_t;

typedef esp_err_t(*usbh_cb_t)(usbh_port_handle_t, void *); /*!< USB connect/disconnect callback type */
typedef esp_err_t(*usbh_evt_cb_t)(usbh_event_msg_t *, void *);  /*!< USB pipe event callback type */

#define DEFAULT_USBH_PORT_CONFIG() \
{\
    .port_num = 1,\
    .context = NULL,\
    .fifo_bias = FIFO_BIAS_BALANCED,\
    .intr_flags = 0,\
    .queue_length = 16,\
    .conn_callback = NULL,\
    .disconn_callback = NULL,\
    .event_callback = NULL,\
    .conn_callback_arg = NULL,\
    .disconn_callback_arg = NULL,\
    .event_callback_arg = NULL,\
}

typedef struct {
    int port_num;                                           /*!< USB Port number */
    void *context;                                          /*!< USB Port context */
    usbh_usb_fifo_bias_t fifo_bias;                         /*!< HCD port internal FIFO biasing */
    int intr_flags;                                         /*!< Interrupt flags for HCD interrupt */
    int queue_length;                                       /*!< USB default event queue length*/
    usbh_cb_t conn_callback;                                /*!< USB connect callback, set NULL if not use */
    usbh_cb_t disconn_callback;                             /*!< USB disconnect callback, set NULL if not use */
    usbh_evt_cb_t event_callback;                           /*!< USB pipe event callback, set NULL if not use */
    void *conn_callback_arg;                                /*!< USB connect callback arg, set NULL if not use  */
    void *disconn_callback_arg;                             /*!< USB disconnect callback arg, set NULL if not use */
    void *event_callback_arg;                               /*!< USB pipe callback arg, set NULL if not use  */
} usbh_port_config_t;

/**
 * @brief Initialize USB port and default pipe
 *
 * @param config port configurations
 * @return port handle if initialize succeed, NULL if failed
 */
usbh_port_handle_t iot_usbh_port_init(usbh_port_config_t *config);

/**
 * @brief Deinitialize the USB port.
 *
 * @param port_hdl port handle return by iot_usbh_port_init
 * @return esp_err_t
 */
esp_err_t iot_usbh_port_deinit(usbh_port_handle_t port_hdl);

/**
 * It returns the context pointer that was passed to the port when it was created
 * 
 * @param port_hdl port handle return by iot_usbh_port_init
 * 
 * @return The context of the port.
 */
void* iot_usbh_port_get_context(usbh_port_handle_t port_hdl);

/**
 * Starts the USB host port, Only call after device initialized, USB connection will be handled
 *
 * @param port_hdl the port handle returned by iot_usbh_port_init.
 *
 * @return esp_err_t
 */
esp_err_t iot_usbh_port_start(usbh_port_handle_t port_hdl);

/**
 * Stops the USB port, USB event will not be handled
 *
 * @param port_hdl the port handle returned by iot_usbh_port_init.
 *
 * @return esp_err_t
 */
esp_err_t iot_usbh_port_stop(usbh_port_handle_t port_hdl);

/**
 * It allocates a pipe instance, initializes it, and returns a handle to it
 *
 * @param port_hdl the handle of the USB port
 * @param ep_desc the endpoint descriptor of the pipe. If it is NULL, the pipe is the default pipe.
 * @param queue_hdl the queue handle to receive the pipe callback. If it is NULL, the queue handle of the port will be used.
 * @param context The context parameter is passed to the HCD layer. It is used to store the context of the pipe.
 *
 * @return pointer to a pipe instance.
 */
usbh_pipe_handle_t iot_usbh_pipe_init(usbh_port_handle_t port_hdl, const usb_ep_desc_t *ep_desc, QueueHandle_t queue_hdl, void *context);

/**
 * It deinitialize a pipe, call iot_usbh_pipe_flush first before deinit
 *
 * @param pipe_hdl The handle of the pipe to be deinitialize.
 *
 * @return esp_err_t
 */
esp_err_t iot_usbh_pipe_deinit(usbh_pipe_handle_t pipe_hdl);

/**
 * It returns the context pointer that was passed to the pipe when it was created
 * 
 * @param pipe_hdl the pipe handle returned by iot_usbh_pipe_init()
 * 
 * @return The context of the pipe.
 */
void* iot_usbh_pipe_get_context(usbh_pipe_handle_t pipe_hdl);

/**
 * It flushes the pipe, waits for the pipe to be idle, and then clears the pipe
 *
 * @param pipe_hdl the pipe handle returned by iot_usbh_pipe_init
 * @param urb_num The number of URBs to be flushed.
 *
 * @return esp_err_t
 */
esp_err_t iot_usbh_pipe_flush(usbh_pipe_handle_t pipe_hdl, size_t urb_num);

/**
 * It allocates a URB and its underlying transfer structure, and then initializes the transfer
 * structure
 *
 * @param num_isoc_packets Number of isochronous packets in the URB.
 * @param packet_data_buffer_size The size of the data buffer for each packet.
 * @param context This is a pointer to a context structure that will be passed back to the callback
 * function.
 *
 * @return pointer to a urb instance
 */
iot_usbh_urb_handle_t iot_usbh_urb_alloc(int num_isoc_packets, size_t packet_data_buffer_size, void *context);

/**
 * It frees the memory allocated for the URB
 *
 * @param urb_hdl The URB to be freed.
 * @return esp_err_t
 */
esp_err_t iot_usbh_urb_free(iot_usbh_urb_handle_t urb_hdl);

/**
 * It returns a pointer to the buffer that was allocated for the URB
 * 
 * @param urb_hdl the handle of the URB
 * @param buf_size the size of the buffer that the URB has allocated for the transfer.
 * @param num_isoc number of isochronous packets in the URB.
 * 
 * @return The buffer pointer is being returned.
 */
void* iot_usbh_urb_buffer_claim(iot_usbh_urb_handle_t urb_hdl, size_t *buf_size, size_t *num_isoc);

/**
 * It takes a pipe handle and a URB, and passes them to the HCD
 *
 * @param pipe_hdl The pipe handle to handle the urb transfer
 * @param urb_hdl the URB to be enqueued
 *
 * @return esp_err_t
 */
esp_err_t iot_usbh_urb_enqueue(usbh_pipe_handle_t pipe_hdl, iot_usbh_urb_handle_t urb_hdl, size_t xfer_size);

/**
 * It takes a pipe handle, and returns the transferred URB in the queue of the pipe
 * 
 * @param pipe_hdl the pipe handle to dequeue
 * @param xfered_size the number of bytes transferred
 * @param status The status of the transfer.
 * 
 * @return A pointer to the URB that was dequeued.
 */
iot_usbh_urb_handle_t iot_usbh_urb_dequeue(usbh_pipe_handle_t pipe_hdl, size_t *xfered_size, usb_transfer_status_t *status);

/**
 * It waits for the URB to be done, then returns the URB
 * 
 * @param port_hdl the port to handle the urb
 * @param urb_hdl the URB handle to be used for the transfer.
 * @param xfer_size the size of the data to be transferred.
 * @param xfered_size the number of bytes transferred
 * @param status The status of the transfer.
 * 
 * @return A pointer to the URB that was just completed.
 */

iot_usbh_urb_handle_t iot_usbh_urb_ctrl_xfer(usbh_port_handle_t port_hdl, iot_usbh_urb_handle_t urb_hdl, size_t xfer_size, size_t *xfered_size, usb_transfer_status_t *status);
