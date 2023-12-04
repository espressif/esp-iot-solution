/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_bit_defs.h"
#include "esp_log.h"
#include "usb/usb_types_stack.h"
#include "usb/usb_helpers.h"
#include "iot_usbh.h"
#include "iot_usbh_cdc.h"

#define ERR_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define ERR_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

static const char *TAG = "USB_HCDC";

#define DATA_EVENT_QUEUE_LEN           32                                   /*! queue length of cdc event */
#define TIMEOUT_USB_RINGBUF_MS         200                                  /*! Timeout for ring buffer operate */
#define TIMEOUT_USB_DRIVER_MS          6000                                 /*! Timeout for driver operate */
#define TIMEOUT_USB_DRIVER_CHECK_MS    10                                   /*! Timeout check step */
#define CDC_DATA_TASK_NAME             "cdc-data"                           /*! cdc task name */
#define USBH_TASK_BASE_PRIORITY        CONFIG_USBH_TASK_BASE_PRIORITY       /*! usbh task priority */
#define CDC_DATA_TASK_PRIORITY         USBH_TASK_BASE_PRIORITY              /*! cdc task priority, higher to handle event quick */
#define CDC_DATA_TASK_STACK_SIZE       4096                                 /*! cdc task stack size */
#define CDC_DATA_TASK_CORE             CONFIG_USBH_TASK_CORE_ID             /*! cdc task core id */
#define BULK_OUT_URB_NUM               CONFIG_CDC_BULK_OUT_URB_NUM          /*! number of out urb enqueued simultaneously */
#define BULK_IN_URB_NUM                CONFIG_CDC_BULK_IN_URB_NUM           /*! number of in urb enqueued simultaneously */
#define BUFFER_SIZE_BULK_OUT           CONFIG_CDC_BULK_OUT_URB_BUFFER_SIZE  /*! out ringbuffer size */
#define BUFFER_SIZE_BULK_IN            CONFIG_CDC_BULK_IN_URB_BUFFER_SIZE   /*! in ringbuffer size */
#define CDC_DATA_TASK_KILL_BIT         BIT4                                 /*! event bit to kill cdc task */
#define CDC_DATA_TASK_RESET_BIT        BIT5                                 /*! event bit to reset cdc task */
#define CDC_DEVICE_READY_BIT           BIT6                                 /*! event bit to indicate if cdc device is ready */

typedef enum {
    CDC_STATE_NONE,
    CDC_STATE_INSTALLED,
    CDC_STATE_CONNECTED,
    CDC_STATE_READY,
} _cdc_state_t;

typedef struct {
    int itf_num;                                                /*!< interface number enabled */
    _cdc_state_t state;                                         /*!< the driver state */
    usbh_port_handle_t port_hdl;                                /*!< usbh port handle */
    EventGroupHandle_t event_group_hdl;                         /*!< event group to sync cdc event */
    TaskHandle_t task_hdl;                                      /*!< cdc task handle */
    bool itf_ready[CDC_INTERFACE_NUM_MAX];                      /*!< if interface is ready */
    usb_ep_desc_t bulk_in_ep_desc[CDC_INTERFACE_NUM_MAX];       /*!< in endpoint descriptor of each interface */
    usb_ep_desc_t bulk_out_ep_desc[CDC_INTERFACE_NUM_MAX];      /*!< out endpoint descriptor of each interface */
    RingbufHandle_t in_ringbuf_handle[CDC_INTERFACE_NUM_MAX];   /*!< in ringbuffer handle of corresponding interface */
    RingbufHandle_t out_ringbuf_handle[CDC_INTERFACE_NUM_MAX];  /*!< if interface is ready */
    usbh_cdc_cb_t rx_callback[CDC_INTERFACE_NUM_MAX];           /*!< packet receive callback, should not block */
    void *rx_callback_arg[CDC_INTERFACE_NUM_MAX];               /*!< packet receive callback args */
    usbh_cdc_cb_t conn_callback;                                /*!< USB connect callback, set NULL if not use */
    usbh_cdc_cb_t disconn_callback;                             /*!< USB disconnect callback, set NULL if not use */
    void *conn_callback_arg;                                    /*!< USB connect callback arg, set NULL if not use  */
    void *disconn_callback_arg;                                 /*!< USB disconnect callback arg, set NULL if not use */
} _class_cdc_t;

static _class_cdc_t s_cdc_instance = {};

/**
 * @brief Set control line state bRequest=0x22
 */
#define USB_CTRL_REQ_CDC_SET_LINE_STATE(ctrl_req_ptr, itf, dtr, rts) ({  \
        (ctrl_req_ptr)->bmRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = 0x22;    \
        (ctrl_req_ptr)->wValue = (rts ? 2 : 0) | (dtr ? 1 : 0); \
        (ctrl_req_ptr)->wIndex =  itf;    \
        (ctrl_req_ptr)->wLength = 0;   \
    })

/**
 * @brief Default endpoint descriptor
 */
#define DEFAULT_BULK_EP_DESC()  { \
    .bLength = sizeof(usb_ep_desc_t), \
    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT, \
    .bEndpointAddress = 0x01, \
    .bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK, \
    .wMaxPacketSize = 64, \
    .bInterval = 0, \
    }

#define DEFAULT_USBH_CDC_CONFIGS() { \
    0, \

/*--------------------------------- CDC Buffer Handle Code --------------------------------------*/
static size_t _get_usb_out_ringbuf_len(int itf_num)
{
    size_t uxItemsWaiting = 0;
    vRingbufferGetInfo(s_cdc_instance.out_ringbuf_handle[itf_num], NULL, NULL, NULL, NULL, &uxItemsWaiting);
    return uxItemsWaiting;
}

static esp_err_t _usb_out_ringbuf_push(int itf_num, const uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    int res = xRingbufferSend(s_cdc_instance.out_ringbuf_handle[itf_num], buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "The out buffer is too small, the data has been lost %u", write_bytes);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t _usb_out_ringbuf_pop(int itf_num, uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(s_cdc_instance.out_ringbuf_handle[itf_num], read_bytes, ticks_to_wait, req_bytes);

    if (buf_rcv) {
        memcpy(buf, buf_rcv, *read_bytes);
        vRingbufferReturnItem(s_cdc_instance.out_ringbuf_handle[itf_num], (void *)(buf_rcv));
        return ESP_OK;
    } else {
        return ESP_ERR_NO_MEM;
    }
}

static esp_err_t _usb_in_ringbuf_push(int itf_num, const uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    int res = xRingbufferSend(s_cdc_instance.in_ringbuf_handle[itf_num], buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "The in buffer is too small, the data has been lost");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t _usb_in_ringbuf_pop(int itf_num, uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(s_cdc_instance.in_ringbuf_handle[itf_num], read_bytes, ticks_to_wait, req_bytes);

    if (buf_rcv) {
        memcpy(buf, buf_rcv, *read_bytes);
        vRingbufferReturnItem(s_cdc_instance.in_ringbuf_handle[itf_num], (void *)(buf_rcv));
        return ESP_OK;
    } else {
        return ESP_ERR_NO_MEM;
    }
}

static bool _cdc_driver_is_init(void)
{
    return (s_cdc_instance.state == CDC_STATE_NONE) ? false : true;
}

/* if cdc task and cdc pipe is ready to transmit data */
static bool _cdc_driver_is_ready()
{
    if (!s_cdc_instance.event_group_hdl) {
        return false;
    }
    return xEventGroupGetBits(s_cdc_instance.event_group_hdl) & CDC_DEVICE_READY_BIT;
}

static inline void _processing_out_pipe(int itf_num, usbh_pipe_handle_t pipe_hdl, bool if_dequeue, bool reset)
{
    static size_t pending_urb_num[CDC_INTERFACE_NUM_MAX] = {0};
    static iot_usbh_urb_handle_t pending_urb[CDC_INTERFACE_NUM_MAX][BULK_OUT_URB_NUM] = {NULL};
    esp_err_t ret = ESP_FAIL;

    if (reset) {
        for (size_t i = 0; i < CDC_INTERFACE_NUM_MAX; i++) {
            pending_urb_num[i] = 0;
            for (size_t j = 0; j < BULK_OUT_URB_NUM; j++) {
                pending_urb[i][j] = NULL;
            }
        }
        return;
    }

    if (!pending_urb_num[itf_num] && !if_dequeue) {
        return;
    }

    if (if_dequeue) {
        size_t num_bytes = 0;
        usb_transfer_status_t status = USB_TRANSFER_STATUS_ERROR;
        iot_usbh_urb_handle_t done_urb = iot_usbh_urb_dequeue(pipe_hdl, &num_bytes, &status);

        if (status != USB_TRANSFER_STATUS_COMPLETED) {
            /* retry if transfer not completed */
            iot_usbh_urb_enqueue(pipe_hdl, done_urb, -1);
            return;
        }

        ESP_LOGV(TAG, "ST actual len = %u", num_bytes);
        /* add done urb to pending urb list */
        for (size_t i = 0; i < BULK_OUT_URB_NUM; i++) {
            if (pending_urb[itf_num][i] == NULL) {
                pending_urb[itf_num][i] = done_urb;
                ++pending_urb_num[itf_num];
                assert(pending_urb_num[itf_num] <= BULK_OUT_URB_NUM);
                break;
            }
        }
    }

    /* check if we have buffered data need to send */
    if (_get_usb_out_ringbuf_len(itf_num) == 0) {
        return;
    }

    /* fetch a pending urb from list */
    iot_usbh_urb_handle_t next_urb = NULL;
    size_t next_index = 0;
    for (; next_index < BULK_OUT_URB_NUM; next_index++) {
        if (pending_urb[itf_num][next_index] != NULL) {
            next_urb = pending_urb[itf_num][next_index];
            break;
        }
    }
    assert(next_urb != NULL);

    size_t num_bytes_to_send = 0;
    size_t buffer_size = 0;
    uint8_t *buffer = iot_usbh_urb_buffer_claim(next_urb, &buffer_size, NULL);
    ret = _usb_out_ringbuf_pop(itf_num, buffer, buffer_size, &num_bytes_to_send, 0);
    if (ret != ESP_OK || num_bytes_to_send == 0) {
        return;
    }
    iot_usbh_urb_enqueue(pipe_hdl, next_urb, num_bytes_to_send);
    pending_urb[itf_num][next_index] = NULL;
    --pending_urb_num[itf_num];
    assert(pending_urb_num[itf_num] <= BULK_OUT_URB_NUM);
    ESP_LOGV(TAG, "ITF%d ST %d: %.*s", itf_num, num_bytes_to_send, num_bytes_to_send, buffer);
}

static void inline _processing_in_pipe(int itf_num, usbh_pipe_handle_t pipe_hdl, usbh_cdc_cb_t rx_cb, void *rx_cb_arg)
{
    size_t num_bytes = 0;
    iot_usbh_urb_handle_t done_urb = iot_usbh_urb_dequeue(pipe_hdl, &num_bytes, NULL);
    if (num_bytes > 0) {
        uint8_t *data_buffer = iot_usbh_urb_buffer_claim(done_urb, NULL, NULL);
        ESP_LOGV(TAG, "ITF%d RCV actual %d: %.*s", itf_num, num_bytes, num_bytes, data_buffer);
        esp_err_t ret = _usb_in_ringbuf_push(itf_num, data_buffer, num_bytes, pdMS_TO_TICKS(TIMEOUT_USB_RINGBUF_MS));

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "in ringbuf too small, skip a usb payload");
        }

        if (rx_cb) {
            rx_cb(rx_cb_arg);
        }
    }

    iot_usbh_urb_enqueue(pipe_hdl, done_urb, -1);
}

typedef struct {
    uint8_t pipe_itf;
    uint8_t pipe_addr;
} _pipe_context;

static void _cdc_data_task(void *arg)
{
    ESP_LOGI(TAG, "CDC task start");
    usbh_pipe_handle_t pipe_hdl_in[CDC_INTERFACE_NUM_MAX] = {NULL};
    usbh_pipe_handle_t pipe_hdl_out[CDC_INTERFACE_NUM_MAX] = {NULL};
    iot_usbh_urb_handle_t urb_in[CDC_INTERFACE_NUM_MAX][BULK_IN_URB_NUM] = {NULL};
    iot_usbh_urb_handle_t urb_out[CDC_INTERFACE_NUM_MAX][BULK_OUT_URB_NUM] = {NULL};
    _pipe_context pipe_ctx_in[CDC_INTERFACE_NUM_MAX] = {0};
    _pipe_context pipe_ctx_out[CDC_INTERFACE_NUM_MAX] = {0};
    _pipe_context *pipe_ctx = NULL;
    usbh_cdc_cb_t rx_cb[CDC_INTERFACE_NUM_MAX] = {NULL};
    void *rx_cb_arg[CDC_INTERFACE_NUM_MAX] = {NULL};
    EventGroupHandle_t event_group_hdl = s_cdc_instance.event_group_hdl;
    int itf_num = s_cdc_instance.itf_num;

    for (size_t i = 0; i < itf_num; i++) {
        rx_cb[i] = s_cdc_instance.rx_callback[i];
        rx_cb_arg[i] = s_cdc_instance.rx_callback_arg[i];
        pipe_ctx_in[i].pipe_itf = i;
        pipe_ctx_out[i].pipe_itf = i;
        pipe_ctx_in[i].pipe_addr = s_cdc_instance.bulk_in_ep_desc[i].bEndpointAddress;
        pipe_ctx_out[i].pipe_addr = s_cdc_instance.bulk_out_ep_desc[i].bEndpointAddress;
        for (size_t j = 0; j < BULK_IN_URB_NUM; j++) {
            iot_usbh_urb_handle_t urb = iot_usbh_urb_alloc(0, BUFFER_SIZE_BULK_IN, NULL);
            assert(NULL != urb);
            urb_in[i][j] = urb;
        }
        for (size_t j = 0; j < BULK_OUT_URB_NUM; j++) {
            iot_usbh_urb_handle_t urb = iot_usbh_urb_alloc(0, BUFFER_SIZE_BULK_OUT, NULL);
            assert(NULL != urb);
            urb_out[i][j] = urb;
        }
    }

    usbh_event_msg_t evt_msg = {};
    QueueHandle_t evt_queue_hdl = xQueueCreate(DATA_EVENT_QUEUE_LEN, sizeof(usbh_event_msg_t));
    assert(NULL != evt_queue_hdl);
    // Waiting for usbh ready
    while (!(xEventGroupGetBits(event_group_hdl) & (CDC_DATA_TASK_KILL_BIT | CDC_DATA_TASK_RESET_BIT))) {
        if (ulTaskNotifyTake(pdTRUE, 1) == 0) {
            continue;
        }

        s_cdc_instance.state = CDC_STATE_CONNECTED;
        // Create pipe when device connect
        xQueueReset(evt_queue_hdl);
        for (uint8_t i = 0; i < itf_num; i++) {
            ESP_LOGD(TAG, "Creating bulk in pipe itf = %d, ep = 0x%02X, ", pipe_ctx_in[i].pipe_itf, pipe_ctx_in[i].pipe_addr);
            pipe_hdl_in[i] = iot_usbh_pipe_init(s_cdc_instance.port_hdl, &s_cdc_instance.bulk_in_ep_desc[i], (void *)evt_queue_hdl, (void *)(&pipe_ctx_in[i]));
            assert(pipe_hdl_in[i] != NULL);
            ESP_LOGD(TAG, "Creating bulk out pipe itf = %d, ep = 0x%02X", pipe_ctx_out[i].pipe_itf, pipe_ctx_out[i].pipe_addr);
            pipe_hdl_out[i] = iot_usbh_pipe_init(s_cdc_instance.port_hdl, &s_cdc_instance.bulk_out_ep_desc[i], (void *)evt_queue_hdl, (void *)(&pipe_ctx_out[i]));
            assert(pipe_hdl_out[i] != NULL);
            for (size_t j = 0; j < BULK_IN_URB_NUM; j++) {
                iot_usbh_urb_enqueue(pipe_hdl_in[i], urb_in[i][j], BUFFER_SIZE_BULK_IN);
            }
            for (size_t j = 0; j < BULK_OUT_URB_NUM; j++) {
                iot_usbh_urb_enqueue(pipe_hdl_out[i], urb_out[i][j], 0);
            }
            s_cdc_instance.itf_ready[i] = true;
        }
        //reset static members after usb reconnect
        _processing_out_pipe(0, NULL, false, true);
        xEventGroupSetBits(event_group_hdl, CDC_DEVICE_READY_BIT);
        s_cdc_instance.state = CDC_STATE_READY;

        while (!(xEventGroupGetBits(event_group_hdl) & (CDC_DATA_TASK_KILL_BIT | CDC_DATA_TASK_RESET_BIT))) {
            if (xQueueReceive(evt_queue_hdl, &evt_msg, 1) != pdTRUE) {
                /* check if ringbuf has data wait to send */
                for (size_t i = 0; i < itf_num; i++) {
                    _processing_out_pipe(i, pipe_hdl_out[i], false, false);
                }
                continue;
            }
            switch (evt_msg._type) {
            case PORT_EVENT:
                break;
            case PIPE_EVENT:
                pipe_ctx = (_pipe_context *)iot_usbh_pipe_get_context(evt_msg._handle.pipe_handle);
                switch (evt_msg._event.pipe_event) {
                case PIPE_EVENT_URB_DONE:
                    if (pipe_ctx->pipe_addr & 0x80) {
                        _processing_in_pipe(pipe_ctx->pipe_itf, evt_msg._handle.pipe_handle, rx_cb[pipe_ctx->pipe_itf], rx_cb_arg[pipe_ctx->pipe_itf]);
                    } else {
                        _processing_out_pipe(pipe_ctx->pipe_itf, evt_msg._handle.pipe_handle, true, false);
                    }
                    break;

                case PIPE_EVENT_ERROR_XFER:
                    ESP_LOGW(TAG, "Itf:%u ep: 0x%02X PIPE_EVENT_ERROR_XFER", pipe_ctx->pipe_itf, pipe_ctx->pipe_addr);
                    break;

                case PIPE_EVENT_ERROR_URB_NOT_AVAIL:
                    ESP_LOGW(TAG, "Itf:%u ep: 0x%02X PIPE_EVENT_ERROR_URB_NOT_AVAIL", pipe_ctx->pipe_itf, pipe_ctx->pipe_addr);
                    break;

                case PIPE_EVENT_ERROR_OVERFLOW:
                    ESP_LOGW(TAG, "Itf:%u ep: 0x%02X PIPE_EVENT_ERROR_OVERFLOW", pipe_ctx->pipe_itf, pipe_ctx->pipe_addr);
                    break;

                case PIPE_EVENT_ERROR_STALL:
                    ESP_LOGW(TAG, "Itf:%u ep: 0x%02X PIPE_EVENT_ERROR_STALL", pipe_ctx->pipe_itf, pipe_ctx->pipe_addr);
                    break;

                case PIPE_EVENT_NONE:
                    break;

                default:
                    ESP_LOGE(TAG, "Itf:%u ep: 0x%02X invalid HCD_PORT_EVENT", pipe_ctx->pipe_itf, pipe_ctx->pipe_addr);
                    break;
                }
            }
        }
        // reset state to installed
        ESP_LOGI(TAG, "CDC task Reset");
        xEventGroupClearBits(event_group_hdl, CDC_DEVICE_READY_BIT);
        for (size_t i = 0; i < itf_num; i++) {
            esp_err_t ret = iot_usbh_pipe_flush(pipe_hdl_in[i], BULK_IN_URB_NUM);
            if (ESP_OK != ret) {
                ESP_LOGW(TAG, "in pipe flush failed");
            }
            ret = iot_usbh_pipe_deinit(pipe_hdl_in[i]);
            if (ESP_OK != ret) {
                ESP_LOGE(TAG, "in pipe delete failed");
            }
            ret = iot_usbh_pipe_flush(pipe_hdl_out[i], BULK_OUT_URB_NUM);
            if (ESP_OK != ret) {
                ESP_LOGW(TAG, "out pipe flush failed");
            }
            ret = iot_usbh_pipe_deinit(pipe_hdl_out[i]);
            if (ESP_OK != ret) {
                ESP_LOGE(TAG, "out pipe delete failed");
            }

            s_cdc_instance.itf_ready[i] = false;
            pipe_hdl_in[i] = NULL;
            pipe_hdl_out[i] = NULL;
            usbh_cdc_flush_rx_buffer(i);
            usbh_cdc_flush_tx_buffer(i);
        }
        xEventGroupClearBits(event_group_hdl, CDC_DATA_TASK_RESET_BIT);
        s_cdc_instance.state = CDC_STATE_INSTALLED;
    } //handle hotplug

    for (size_t i = 0; i < itf_num; i++) {
        for (size_t j = 0; j < BULK_IN_URB_NUM; j++) {
            iot_usbh_urb_free(urb_in[i][j]);
            urb_in[i][j] = NULL;
        }

        for (size_t j = 0; j < BULK_OUT_URB_NUM; j++) {
            iot_usbh_urb_free(urb_out[i][j]);
            urb_out[i][j] = NULL;
        }
    }
    xEventGroupClearBits(event_group_hdl, CDC_DATA_TASK_KILL_BIT | CDC_DEVICE_READY_BIT);
    s_cdc_instance.state = CDC_STATE_INSTALLED;
    vQueueDelete(evt_queue_hdl);
    ESP_LOGI(TAG, "CDC task deleted");
    vTaskDelete(NULL);
}

#ifdef CONFIG_CDC_SEND_DTE_ACTIVE
static esp_err_t _usb_set_device_line_state(usbh_port_handle_t port_hdl, bool dtr, bool rts)
{
    ESP_LOGI(TAG, "Set Device Line State: dtr %d, rts %d", dtr, rts);
    iot_usbh_urb_handle_t urb_ctrl = iot_usbh_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    ERR_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
    USB_CTRL_REQ_CDC_SET_LINE_STATE((usb_setup_packet_t *)iot_usbh_urb_buffer_claim(urb_ctrl, NULL, NULL), 0, dtr, rts);
    size_t num_bytes = 0;
    usb_transfer_status_t status = USB_TRANSFER_STATUS_ERROR;
    iot_usbh_urb_handle_t urb_ctrl_done = iot_usbh_urb_ctrl_xfer(port_hdl, urb_ctrl, sizeof(usb_setup_packet_t), &num_bytes, &status);
    ERR_CHECK_GOTO(urb_ctrl == urb_ctrl_done, "urb status: not same", free_urb_);
    ERR_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Line State Done");
    iot_usbh_urb_free(urb_ctrl);
    return ESP_OK;

free_urb_:
    ESP_LOGE(TAG, "Set Device Line State Failed");
    iot_usbh_urb_free(urb_ctrl);
    return ESP_FAIL;
}
#endif

static esp_err_t _usbh_cdc_disconnect_handler(usbh_port_handle_t port_hdl, void *arg)
{
    if (xEventGroupGetBits(s_cdc_instance.event_group_hdl) & CDC_DEVICE_READY_BIT) {
        xEventGroupSetBits(s_cdc_instance.event_group_hdl, CDC_DATA_TASK_RESET_BIT);
        int counter = TIMEOUT_USB_DRIVER_MS;
        while (xEventGroupGetBits(s_cdc_instance.event_group_hdl) & CDC_DATA_TASK_RESET_BIT) {
            vTaskDelay(pdMS_TO_TICKS(TIMEOUT_USB_DRIVER_CHECK_MS));
            counter -= TIMEOUT_USB_DRIVER_CHECK_MS;
            if (counter <= 0) {
                ESP_LOGE(TAG, "%s timeout", __func__);
            }
        }
        xEventGroupClearBits(s_cdc_instance.event_group_hdl, CDC_DATA_TASK_RESET_BIT);
    }

    if (s_cdc_instance.disconn_callback) {
        s_cdc_instance.disconn_callback(s_cdc_instance.disconn_callback_arg);
    }
    return ESP_OK;
}

static esp_err_t _usbh_cdc_connect_handler(usbh_port_handle_t port_hdl, void *arg)
{
    esp_err_t ret = ESP_OK;
#ifdef CONFIG_CDC_SEND_DTE_ACTIVE
    // will block to wait urb success
    ret = _usb_set_device_line_state(port_hdl, true, false);
#endif
    if (ret == ESP_OK) {
        // only run callback after enum stage success
        xTaskNotifyGive(s_cdc_instance.task_hdl);
        if (s_cdc_instance.conn_callback) {
            s_cdc_instance.conn_callback(s_cdc_instance.conn_callback_arg);
        }
    }
    return ret;
}

esp_err_t usbh_cdc_driver_install(const usbh_cdc_config_t *config)
{
    if (_cdc_driver_is_init()) {
        ESP_LOGW(TAG, "cdc driver has installed");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "iot_usbh_cdc, version: %d.%d.%d", IOT_USBH_CDC_VER_MAJOR, IOT_USBH_CDC_VER_MINOR, IOT_USBH_CDC_VER_PATCH);
    ERR_CHECK(config != NULL, "config can't be NULL", ESP_ERR_INVALID_ARG);
    int itf_num = (config->itf_num == 0) ? 1 : config->itf_num;

    // check endpoint of each enabled interface
    for (size_t i = 0; i < itf_num; i++) {
        ERR_CHECK(config->rx_buffer_sizes[i] != 0 && config->tx_buffer_sizes[i] != 0, "buffer size can't be 0", ESP_ERR_INVALID_ARG);
        ERR_CHECK(config->bulk_in_eps[i] != NULL || config->bulk_in_ep_addrs[i] & 0x80, "bulk_in_ep or bulk_in_ep_addr invalid", ESP_ERR_INVALID_ARG);
        ERR_CHECK(config->bulk_out_eps[i] != NULL || !(config->bulk_out_ep_addrs[i] & 0x80), "bulk_out_ep or bulk_out_ep_addr invalid", ESP_ERR_INVALID_ARG);
    }
    for (size_t i = 0; i < itf_num; i++) {
        if (config->bulk_in_eps[i] == NULL) {
            /* using default ep_desc with user's ep_addr */
            s_cdc_instance.bulk_in_ep_desc[i] = (usb_ep_desc_t)DEFAULT_BULK_EP_DESC();
            s_cdc_instance.bulk_in_ep_desc[i].bEndpointAddress = config->bulk_in_ep_addrs[i];
        } else {
            /* using user's ep_desc */
            s_cdc_instance.bulk_in_ep_desc[i] = *config->bulk_in_eps[i];
        }

        if (config->bulk_out_eps[i] == NULL) {
            /* using default ep_desc with user's ep_addr */
            s_cdc_instance.bulk_out_ep_desc[i] = (usb_ep_desc_t)DEFAULT_BULK_EP_DESC();
            s_cdc_instance.bulk_out_ep_desc[i].bEndpointAddress = config->bulk_out_ep_addrs[i];
        } else {
            /* using user's ep_desc */
            s_cdc_instance.bulk_out_ep_desc[i] = *config->bulk_out_eps[i];
        }
    }
    s_cdc_instance.itf_num = itf_num;
    s_cdc_instance.event_group_hdl = xEventGroupCreate();
    ERR_CHECK(s_cdc_instance.event_group_hdl != NULL, "Create event group failed", ESP_FAIL);

    for (size_t i = 0; i < itf_num; i++) {
        s_cdc_instance.in_ringbuf_handle[i] = xRingbufferCreate(config->rx_buffer_sizes[i], RINGBUF_TYPE_BYTEBUF);
        ERR_CHECK_GOTO(s_cdc_instance.in_ringbuf_handle[i] != NULL, "Create in ringbuffer failed", delete_resource_);
        s_cdc_instance.out_ringbuf_handle[i] = xRingbufferCreate(config->tx_buffer_sizes[i], RINGBUF_TYPE_BYTEBUF);
        ERR_CHECK_GOTO(s_cdc_instance.out_ringbuf_handle[i] != NULL, "Create out ringbuffer failed", delete_resource_);
    }
    usbh_port_config_t port_config = DEFAULT_USBH_PORT_CONFIG();
    s_cdc_instance.conn_callback = config->conn_callback;
    s_cdc_instance.conn_callback_arg = config->conn_callback_arg;
    s_cdc_instance.disconn_callback = config->disconn_callback;
    s_cdc_instance.disconn_callback_arg = config->disconn_callback_arg;
    port_config.conn_callback = _usbh_cdc_connect_handler;
    port_config.disconn_callback = _usbh_cdc_disconnect_handler;
    s_cdc_instance.port_hdl = iot_usbh_port_init(&port_config);
    ERR_CHECK_GOTO(s_cdc_instance.port_hdl != NULL, "usb port init failed", delete_resource_);

    xTaskCreatePinnedToCore(_cdc_data_task, CDC_DATA_TASK_NAME, CDC_DATA_TASK_STACK_SIZE, NULL,
                            CDC_DATA_TASK_PRIORITY, &s_cdc_instance.task_hdl, CDC_DATA_TASK_CORE);
    ERR_CHECK_GOTO(s_cdc_instance.task_hdl != NULL, "Create usb task failed", delete_resource_);
    s_cdc_instance.state = CDC_STATE_INSTALLED;
    iot_usbh_port_start(s_cdc_instance.port_hdl);
    ESP_LOGI(TAG, "usbh cdc driver install succeed");
    return ESP_OK;

delete_resource_:
    if (s_cdc_instance.port_hdl) {
        iot_usbh_port_deinit(s_cdc_instance.port_hdl);
        s_cdc_instance.port_hdl = NULL;
    }
    for (size_t i = 0; i < itf_num; i++) {
        if (s_cdc_instance.out_ringbuf_handle[i]) {
            vRingbufferDelete(s_cdc_instance.out_ringbuf_handle[i]);
        }
        if (s_cdc_instance.in_ringbuf_handle[i]) {
            vRingbufferDelete(s_cdc_instance.in_ringbuf_handle[i]);
        }
        s_cdc_instance.in_ringbuf_handle[i] = NULL;
        s_cdc_instance.out_ringbuf_handle[i] = NULL;
    }
    if (s_cdc_instance.event_group_hdl) {
        vEventGroupDelete(s_cdc_instance.event_group_hdl);
        s_cdc_instance.event_group_hdl = NULL;
    }
    return ESP_FAIL;
}

esp_err_t usbh_cdc_driver_delete(void)
{
    if (!_cdc_driver_is_init()) {
        ESP_LOGW(TAG, "cdc driver not installed");
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupSetBits(s_cdc_instance.event_group_hdl, CDC_DATA_TASK_KILL_BIT);
    ESP_LOGI(TAG, "Waiting for CDC Driver Delete");
    int counter = TIMEOUT_USB_DRIVER_MS;
    while (xEventGroupGetBits(s_cdc_instance.event_group_hdl) & CDC_DATA_TASK_KILL_BIT) {
        vTaskDelay(pdMS_TO_TICKS(TIMEOUT_USB_DRIVER_CHECK_MS));
        counter -= TIMEOUT_USB_DRIVER_CHECK_MS;
        if (counter <= 0) {
            ESP_LOGW(TAG, "Waiting for CDC Driver Delete, timeout");
            break;
        }
    }

    s_cdc_instance.state = CDC_STATE_NONE;
    if (s_cdc_instance.port_hdl) {
        iot_usbh_port_deinit(s_cdc_instance.port_hdl);
        s_cdc_instance.port_hdl = NULL;
    }

    for (size_t i = 0; i < s_cdc_instance.itf_num; i++) {
        if (s_cdc_instance.out_ringbuf_handle[i]) {
            vRingbufferDelete(s_cdc_instance.out_ringbuf_handle[i]);
        }
        if (s_cdc_instance.in_ringbuf_handle[i]) {
            vRingbufferDelete(s_cdc_instance.in_ringbuf_handle[i]);
        }
    }

    vEventGroupDelete(s_cdc_instance.event_group_hdl);
    memset(&s_cdc_instance, 0, sizeof(_class_cdc_t));;
    ESP_LOGI(TAG, "CDC Driver Deleted!");
    return ESP_OK;
}

esp_err_t usbh_cdc_wait_connect(TickType_t ticks_to_wait)
{
    if (!_cdc_driver_is_init()) {
        ESP_LOGW(TAG, "cdc driver not installed");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Waiting CDC Device Connection");
    EventBits_t bits = xEventGroupWaitBits(s_cdc_instance.event_group_hdl, CDC_DEVICE_READY_BIT, false, false, ticks_to_wait);
    if ((bits & CDC_DEVICE_READY_BIT) == 0) {
        ESP_LOGW(TAG, "Waiting Device Connection, timeout");
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "CDC Device Connected");
    return ESP_OK;
}

esp_err_t usbh_cdc_itf_get_buffered_data_len(uint8_t itf, size_t *size)
{
    ERR_CHECK(size != NULL && itf < CDC_INTERFACE_NUM_MAX, "arg can't be NULL", ESP_ERR_INVALID_ARG);

    if (_cdc_driver_is_init() == false) {
        *size = 0;
        return ESP_ERR_INVALID_STATE;
    }

    if (usbh_cdc_get_itf_state(itf) == false) {
        *size = 0;
        return ESP_ERR_INVALID_STATE;
    }
    vRingbufferGetInfo(s_cdc_instance.in_ringbuf_handle[itf], NULL, NULL, NULL, NULL, size);
    return ESP_OK;
}

esp_err_t usbh_cdc_get_buffered_data_len(size_t *size)
{
    return usbh_cdc_itf_get_buffered_data_len(0, size);
}

int usbh_cdc_itf_read_bytes(uint8_t itf, uint8_t *buf, size_t length, TickType_t ticks_to_wait)
{
    ERR_CHECK(buf != NULL && itf < CDC_INTERFACE_NUM_MAX, "invalid args", -1);
    size_t read_sz = 0;
    int rx_data_size = 0;

    if (usbh_cdc_get_itf_state(itf) == false) {
        ESP_LOGV(TAG, "%s: Device not connected or itf%u not ready", __func__, itf);
        return 0;
    }

    esp_err_t res = _usb_in_ringbuf_pop(itf, buf, length, &read_sz, ticks_to_wait);

    if (res != ESP_OK) {
        ESP_LOGD(TAG, "Read ringbuffer failed");
        return 0;
    }

    rx_data_size = read_sz;

    /* Buffer's data can be wrapped, at that situations we should make another retirement */
    if (_usb_in_ringbuf_pop(itf, buf + read_sz, length - read_sz, &read_sz, 0) == ESP_OK) {
        rx_data_size += read_sz;
    }
    ESP_LOGD(TAG, "cdc read itf: %u, buf = %p, len = %u, read = %d", itf, buf, length, rx_data_size);
    return rx_data_size;
}

int usbh_cdc_read_bytes(uint8_t *buf, size_t length, TickType_t ticks_to_wait)
{
    return usbh_cdc_itf_read_bytes(0, buf, length, ticks_to_wait);
}

int usbh_cdc_itf_write_bytes(uint8_t itf, const uint8_t *buf, size_t length)
{
    ESP_LOGD(TAG, "cdc write itf: %u, buf = %p, len = %u", itf, buf, length);
    ERR_CHECK(buf != NULL && itf < CDC_INTERFACE_NUM_MAX, "invalid args", -1);
    int tx_data_size = 0;

    if (usbh_cdc_get_itf_state(itf) == false) {
        ESP_LOGV(TAG, "%s: Device not connected or itf%u not ready", __func__, itf);
        return 0;
    }

    esp_err_t ret = _usb_out_ringbuf_push(itf, buf, length, pdMS_TO_TICKS(TIMEOUT_USB_RINGBUF_MS));

    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Write ringbuffer failed");
        return 0;
    }

    tx_data_size = length;
    return tx_data_size;
}

int usbh_cdc_write_bytes(const uint8_t *buf, size_t length)
{
    return usbh_cdc_itf_write_bytes(0, buf, length);
}

int usbh_cdc_get_itf_state(uint8_t itf)
{
    ERR_CHECK(itf < CDC_INTERFACE_NUM_MAX, "invalid args: itf", 0);
    if (_cdc_driver_is_ready() && s_cdc_instance.itf_ready[itf]) {
        return true;
    }
    return false;
}

esp_err_t usbh_cdc_flush_rx_buffer(uint8_t itf)
{
    ERR_CHECK(itf < CDC_INTERFACE_NUM_MAX, "invalid args: itf", ESP_ERR_INVALID_ARG);
    if (s_cdc_instance.in_ringbuf_handle[itf] == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t read_bytes = 0;
    size_t uxItemsWaiting = 0;
    vRingbufferGetInfo(s_cdc_instance.in_ringbuf_handle[itf], NULL, NULL, NULL, NULL, &uxItemsWaiting);
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(s_cdc_instance.in_ringbuf_handle[itf], &read_bytes, 1, uxItemsWaiting);

    if (buf_rcv) {
        vRingbufferReturnItem(s_cdc_instance.in_ringbuf_handle[itf], (void *)(buf_rcv));
    }
    ESP_LOGI(TAG, "rx%u flush -%u = %u", itf, read_bytes, uxItemsWaiting);
    return ESP_OK;
}

esp_err_t usbh_cdc_flush_tx_buffer(uint8_t itf)
{
    ERR_CHECK(itf < CDC_INTERFACE_NUM_MAX, "invalid args: itf", ESP_ERR_INVALID_ARG);
    if (s_cdc_instance.out_ringbuf_handle[itf] == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t read_bytes = 0;
    size_t uxItemsWaiting = 0;
    vRingbufferGetInfo(s_cdc_instance.out_ringbuf_handle[itf], NULL, NULL, NULL, NULL, &uxItemsWaiting);
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(s_cdc_instance.out_ringbuf_handle[itf], &read_bytes, 1, uxItemsWaiting);

    if (buf_rcv) {
        vRingbufferReturnItem(s_cdc_instance.out_ringbuf_handle[itf], (void *)(buf_rcv));
    }
    ESP_LOGI(TAG, "tx%u flush -%u = %u", itf, read_bytes, uxItemsWaiting);
    return ESP_OK;
}
