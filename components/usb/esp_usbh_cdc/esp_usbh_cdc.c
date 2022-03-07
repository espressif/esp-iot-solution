// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdio.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "hal/usb_hal.h"
#include "hal/usbh_ll.h"
#include "hcd.h"
#include "usb/usb_types_stack.h"
#include "usb_private.h"
#include "usb/usb_helpers.h"
#include "esp_private/usb_phy.h"
#include "esp_usbh_cdc.h"

#define CDC_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define CDC_CHECK_GOTO(a, str, lable) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto lable; \
    }

static const char *TAG = "USB_HCDC";

#define USB_PORT_NUM                         1        //Default port number
#define USB_DEVICE_ADDR                      1        //Default CDC device address
#define USB_ENUM_CONFIG_INDEX                0        //Index of the first configuration of the device
#define USB_ENUM_SHORT_DESC_REQ_LEN          8        //Number of bytes to request when getting a short descriptor (just enough to get bMaxPacketSize0 or wTotalLength)
#define USB_DEVICE_CONFIG                    1        //Default CDC device configuration
#define USB_EP_CTRL_DEFAULT_MPS              64       //Default MPS(max payload size) of Endpoint 0
#define CDC_EVENT_QUEUE_LEN                  16
#define DATA_EVENT_QUEUE_LEN                 32
#define CTRL_TRANSFER_DATA_MAX_BYTES         CONFIG_CTRL_TRANSFER_DATA_MAX_BYTES      //Just assume that will only IN/OUT 256 bytes in ctrl pipe
#define TIMEOUT_USB_RINGBUF_MS               200       //Timeout for Ring Buffer push
#define TIMEOUT_USB_CTRL_XFER_MS             5000      //Timeout for USB control transfer

#define USB_TASK_BASE_PRIORITY CONFIG_USB_TASK_BASE_PRIORITY

/**
 * @brief Task for USB I/O request and payload processing,
 * can not be blocked, higher task priority is suggested.
 *
 */
#define USB_PROC_TASK_NAME "usb_proc"
#define USB_PROC_TASK_PRIORITY (USB_TASK_BASE_PRIORITY+2)
#define USB_PROC_TASK_STACK_SIZE 3072
#define USB_PROC_TASK_CORE CONFIG_USB_TASK_CORE_ID

#define CDC_DATA_TASK_NAME "cdc-data"
#define CDC_DATA_TASK_PRIORITY (USB_TASK_BASE_PRIORITY+1)
#define CDC_DATA_TASK_STACK_SIZE 3072
#define CDC_DATA_TASK_CORE CONFIG_USB_TASK_CORE_ID

#define BULK_OUT_URB_NUM CONFIG_CDC_BULK_OUT_URB_NUM
#define BULK_IN_URB_NUM CONFIG_CDC_BULK_IN_URB_NUM
#define BUFFER_SIZE_BULK_OUT CONFIG_CDC_BULK_OUT_URB_BUFFER_SIZE
#define BUFFER_SIZE_BULK_IN CONFIG_CDC_BULK_IN_URB_BUFFER_SIZE
#define USB_TASK_KILL_BIT             BIT1
#define CDC_DATA_TASK_KILL_BIT        BIT4
#define CDC_DEVICE_READY_BIT          BIT19

#ifdef CONFIG_CDC_USE_TRACE_FACILITY
volatile static int s_ringbuf_in_size = 0;
volatile static int s_ringbuf_out_size = 0;
volatile static int s_bulkbuf_out_max = 0;
volatile static int s_bulkbuf_in_max = 0;
volatile static int s_ringbuf_out_max = 0;
volatile static int s_ringbuf_in_max = 0;
#endif

static EventGroupHandle_t s_usb_event_group = NULL;
static RingbufHandle_t s_in_ringbuf_handle = NULL;
static RingbufHandle_t s_out_ringbuf_handle = NULL;
static SemaphoreHandle_t s_usb_read_mux = NULL;
static SemaphoreHandle_t s_usb_write_mux = NULL;
static TaskHandle_t s_usb_processing_task_hdl = NULL;

static portMUX_TYPE s_in_ringbuf_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_out_ringbuf_mux = portMUX_INITIALIZER_UNLOCKED;
volatile static int s_in_buffered_data_len = 0;
volatile static int s_out_buffered_data_len = 0;

typedef enum {
    PORT_EVENT,
    PIPE_EVENT,
} cdc_event_type_t;

typedef struct {
    cdc_event_type_t _type;
    union {
        hcd_port_handle_t port_hdl;
        hcd_pipe_handle_t pipe_handle;
    } _handle;
    union {
        hcd_port_event_t port_event;
        hcd_pipe_event_t pipe_event;
    } _event;
} cdc_event_msg_t;

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

/*------------------------------------------------ Task Code ----------------------------------------------------*/

static bool _cdc_driver_is_init(void)
{
    if (s_usb_event_group == NULL) {
        return false;
    }

    return true;
}

static size_t get_usb_out_ringbuf_len(void)
{
    portENTER_CRITICAL(&s_out_ringbuf_mux);
    size_t  len = s_out_buffered_data_len;
    portEXIT_CRITICAL(&s_out_ringbuf_mux);
    return len;
}

static esp_err_t usb_out_ringbuf_push(const uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    int res = xRingbufferSend(s_out_ringbuf_handle, buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "The out buffer is too small, the data has been lost %u", write_bytes);
        return ESP_FAIL;
    }

    portENTER_CRITICAL(&s_out_ringbuf_mux);
    s_out_buffered_data_len += write_bytes;
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    s_ringbuf_out_max = s_out_buffered_data_len > s_ringbuf_out_max ? s_out_buffered_data_len : s_ringbuf_out_max;
#endif
    portEXIT_CRITICAL(&s_out_ringbuf_mux);
    return ESP_OK;
}

static esp_err_t usb_out_ringbuf_pop(uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(s_out_ringbuf_handle, read_bytes, ticks_to_wait, req_bytes);

    if (buf_rcv) {
        memcpy(buf, buf_rcv, *read_bytes);
        vRingbufferReturnItem(s_out_ringbuf_handle, (void *)(buf_rcv));
        portENTER_CRITICAL(&s_out_ringbuf_mux);
        s_out_buffered_data_len -= *read_bytes;
        portEXIT_CRITICAL(&s_out_ringbuf_mux);
        return ESP_OK;
    } else {
        return ESP_ERR_NO_MEM;
    }
}

static esp_err_t usb_in_ringbuf_push(const uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    int res = xRingbufferSend(s_in_ringbuf_handle, buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "The in buffer is too small, the data has been lost");
        return ESP_FAIL;
    }

    portENTER_CRITICAL(&s_in_ringbuf_mux);
    s_in_buffered_data_len += write_bytes;
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    s_ringbuf_in_max = s_in_buffered_data_len > s_ringbuf_in_max ? s_in_buffered_data_len : s_ringbuf_in_max;
#endif
    portEXIT_CRITICAL(&s_in_ringbuf_mux);
    return ESP_OK;
}

static esp_err_t usb_in_ringbuf_pop(uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(s_in_ringbuf_handle, read_bytes, ticks_to_wait, req_bytes);

    if (buf_rcv) {
        memcpy(buf, buf_rcv, *read_bytes);
        vRingbufferReturnItem(s_in_ringbuf_handle, (void *)(buf_rcv));
        portENTER_CRITICAL(&s_in_ringbuf_mux);
        s_in_buffered_data_len -= *read_bytes;
        portEXIT_CRITICAL(&s_in_ringbuf_mux);
        return ESP_OK;
    } else {
        return ESP_ERR_NO_MEM;
    }
}

typedef enum {
    CDC_DEVICE_STATE_CHECK,
    CDC_DEVICE_STATE_NOT_ATTACHED,
    CDC_DEVICE_STATE_ATTACHED,
    CDC_DEVICE_STATE_POWERED,
    CDC_DEVICE_STATE_DEFAULT,
    CDC_DEVICE_STATE_ADDRESS,
    CDC_DEVICE_STATE_CONFIGURED,
    CDC_DEVICE_STATE_SUSPENDED,
} _device_state_t;

static _device_state_t _update_device_state(_device_state_t state)
{
    static _device_state_t device_state = CDC_DEVICE_STATE_NOT_ATTACHED;
    if (state != CDC_DEVICE_STATE_CHECK) {
        device_state = state;
    }
    return device_state;
}

static bool _if_device_ready()
{
    if(!s_usb_event_group) return false;
    return xEventGroupGetBits(s_usb_event_group) & CDC_DEVICE_READY_BIT;
}

static hcd_port_event_t _usb_port_event_dflt_process(hcd_port_handle_t port_hdl, hcd_port_event_t event)
{
    (void)event;
    assert(port_hdl != NULL);
    hcd_port_event_t actual_evt = hcd_port_handle_event(port_hdl);

    switch (actual_evt) {
        case HCD_PORT_EVENT_CONNECTION:
            ESP_LOGI(TAG, "line %u HCD_PORT_EVENT_CONNECTION", __LINE__);
            break;

        case HCD_PORT_EVENT_DISCONNECTION:
            ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_DISCONNECTION", __LINE__);
            break;

        case HCD_PORT_EVENT_ERROR:
            ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_ERROR", __LINE__);
            break;

        case HCD_PORT_EVENT_OVERCURRENT:
            ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_OVERCURRENT", __LINE__);
            break;

        case HCD_PORT_EVENT_NONE:
            ESP_LOGD(TAG, "line %u HCD_PORT_EVENT_NONE", __LINE__);
            break;

        default:
            ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, actual_evt);
            break;
    }

    return actual_evt;
}


static void _default_pipe_event_dflt_process(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event);

static esp_err_t _usb_port_event_wait(hcd_port_handle_t expected_port_hdl,
                                      hcd_port_event_t expected_event, TickType_t xTicksToWait)
{
    //Wait for port callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_port_get_context(expected_port_hdl);
    assert(queue_hdl != NULL);
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    BaseType_t queue_ret = errQUEUE_EMPTY;
    cdc_event_msg_t evt_msg;

    do {
        queue_ret = xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            break;
        }

        if (PIPE_EVENT == evt_msg._type) {
            _default_pipe_event_dflt_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
            ret = ESP_ERR_NOT_FOUND;
        } else {
            if (expected_port_hdl == evt_msg._handle.port_hdl && expected_event == evt_msg._event.port_event) {
                _usb_port_event_dflt_process(expected_port_hdl, expected_event);
                ret = ESP_OK;
            } else {
                ESP_LOGD(TAG, "Got unexpected port_handle and event");
                _usb_port_event_dflt_process(evt_msg._handle.port_hdl, evt_msg._event.port_event);
                ret = ESP_ERR_INVALID_RESPONSE;
            }
        }
    } while (ret == ESP_ERR_NOT_FOUND);

    return ret;
}

static bool _usb_pipe_callback(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event, void *user_arg, bool in_isr)
{
    QueueHandle_t usb_event_queue = (QueueHandle_t)user_arg;
    cdc_event_msg_t event_msg = {
        ._type = PIPE_EVENT,
        ._handle.pipe_handle = pipe_handle,
        ._event.pipe_event = pipe_event,
    };

    if (in_isr) {
        BaseType_t xTaskWoken = pdFALSE;
        xQueueSendFromISR(usb_event_queue, &event_msg, &xTaskWoken);
        return (xTaskWoken == pdTRUE);
    } else {
        xQueueSend(usb_event_queue, &event_msg, portMAX_DELAY);
        return false;
    }
}

/************************************************************* USB Port API ***********************************************************/

static usb_phy_handle_t s_phy_handle = NULL;

/**
 * @brief Initialize USB controler and USB port
 * 
 * @param context context can be get from port handle
 * @param callback  usb port event callback
 * @param callback_arg callback args
 * @return hcd_port_handle_t port handle if initialize succeed, null if failed
 */
static hcd_port_handle_t _usb_port_init(void *context, hcd_port_callback_t callback, void *callback_arg)
{
    CDC_CHECK(context != NULL && callback != NULL && callback_arg != NULL, "invalid args", NULL);
    esp_err_t ret = ESP_OK;
    hcd_port_handle_t port_hdl = NULL;

    /* Router internal USB PHY to usb-otg instead of usb-serial-jtag (if it has) */
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_HOST,
        .otg_speed = USB_PHY_SPEED_UNDEFINED,   //In Host mode, the speed is determined by the connected device
        .gpio_conf = NULL,
    };
    ret = usb_new_phy(&phy_config, &s_phy_handle);
    CDC_CHECK(ESP_OK == ret, "USB PHY init failed", NULL);
    /* Initialize USB Peripheral */
    hcd_config_t hcd_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL2,
    };
    ret = hcd_install(&hcd_config);
    CDC_CHECK_GOTO(ESP_OK == ret, "HCD Install failed", hcd_init_err);

    //TODO: create a usb port task to handle event
    /* Initialize USB Port */
    hcd_port_config_t port_cfg = {
        .fifo_bias = HCD_PORT_FIFO_BIAS_BALANCED,
        .callback = callback,
        .callback_arg = callback_arg,
        .context = context,
    };
    ret = hcd_port_init(USB_PORT_NUM, &port_cfg, &port_hdl);
    CDC_CHECK_GOTO(ESP_OK == ret, "HCD Port init failed", port_init_err);

    ESP_LOGI(TAG, "USB Port=%d init succeed", USB_PORT_NUM);
    return port_hdl;

port_init_err:
    hcd_uninstall();
hcd_init_err:
    usb_del_phy(s_phy_handle);
    return NULL;
}

static esp_err_t _usb_port_reset(hcd_port_handle_t port_hdl)
{
    CDC_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_command(port_hdl, HCD_PORT_CMD_RESET);
    CDC_CHECK(ESP_OK == ret, "Port reset failed", ESP_FAIL);
    ESP_LOGI(TAG, "Port reset succeed");
    return ret;
}

static esp_err_t _usb_port_disable(hcd_port_handle_t port_hdl)
{
    CDC_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_command(port_hdl, HCD_PORT_CMD_DISABLE);
    CDC_CHECK(ESP_OK == ret, "Port disable failed", ESP_FAIL);
    ESP_LOGI(TAG, "Port disable succeed");
    return ret;
}

static esp_err_t _usb_port_power(hcd_port_handle_t port_hdl, bool on)
{
    CDC_CHECK(port_hdl != NULL, "invalid port handle", ESP_ERR_INVALID_ARG);
    esp_err_t ret;
    if(on) ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_ON);
    else ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_OFF);
    CDC_CHECK(ESP_OK == ret, "port speed get failed", ESP_FAIL);
    ESP_LOGI(TAG, "Port power: %s", on?"ON":"OFF");
    return ret;
}

static esp_err_t _usb_port_get_speed(hcd_port_handle_t port_hdl, usb_speed_t *port_speed)
{
    CDC_CHECK(port_hdl != NULL && port_speed != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_get_speed(port_hdl, port_speed);
    CDC_CHECK(ESP_OK == ret, "port get speed failed", ESP_FAIL);
    ESP_LOGI(TAG, "Port speed = %d", *port_speed);
    return ret;
}


/**
 * @brief Port deinit
 * 
 * @param port_hdl port handle to deinit
 * @return esp_err_t 
 */
static esp_err_t _usb_port_deinit(hcd_port_handle_t port_hdl)
{
    esp_err_t ret;
    ret = _usb_port_disable(port_hdl);

    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "port disable failed");
    }
    //TODO: should be handle in usb port task
    _usb_port_event_dflt_process(port_hdl, 0);
    ret = _usb_port_power(port_hdl, false);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "Port power off failed");
    }
    //TODO: should be handle in usb port task
    _usb_port_event_wait(port_hdl, HCD_PORT_EVENT_DISCONNECTION, 200 / portTICK_PERIOD_MS);
    ret = hcd_port_deinit(port_hdl);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "port deinit failed");
    }

    ret = hcd_uninstall();
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "hcd uninstall failed");
    }
    ret = usb_del_phy(s_phy_handle);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "phy delete failed");
    }
    ESP_LOGI(TAG, "USB Port=%d deinit succeed", USB_PORT_NUM);
    return ret;
}

/************************************************** PIPE API **************************************************/

static hcd_pipe_handle_t _usb_pipe_init(hcd_port_handle_t port_hdl, usb_ep_desc_t *ep_desc, uint8_t dev_addr,
                                        usb_speed_t dev_speed, void *context, void *callback_arg)
{
    CDC_CHECK(port_hdl != NULL, "invalid args", NULL);
    hcd_pipe_config_t pipe_cfg = {
        .callback = _usb_pipe_callback,
        .callback_arg = callback_arg,
        .context = context,
        .ep_desc = ep_desc,    //NULL EP descriptor to create a default pipe
        .dev_addr = dev_addr,
        .dev_speed = dev_speed,
    };

    esp_err_t ret = ESP_OK;
    hcd_pipe_handle_t pipe_handle = NULL;
    ret = hcd_pipe_alloc(port_hdl, &pipe_cfg, &pipe_handle);
    CDC_CHECK(ESP_OK == ret, "pipe alloc failed", NULL);
    return pipe_handle;
}

static esp_err_t _default_pipe_event_wait_until(hcd_pipe_handle_t expected_pipe_hdl,
        hcd_pipe_event_t expected_event, TickType_t xTicksToWait);

static esp_err_t _usb_pipe_deinit(hcd_pipe_handle_t pipe_hdl, size_t urb_num)
{
    CDC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ESP_LOGI(TAG, "pipe state = %d", hcd_pipe_get_state(pipe_hdl));

    esp_err_t ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p halt failed", pipe_hdl);
    }

    ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_FLUSH);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p flush failed", pipe_hdl);
    }
    _default_pipe_event_wait_until(pipe_hdl, HCD_PIPE_EVENT_URB_DONE, 200 / portTICK_PERIOD_MS);

    for (size_t i = 0; i < urb_num; i++) {
        urb_t *urb = hcd_urb_dequeue(pipe_hdl);
        ESP_LOGD(TAG, "urb dequeue handle = %p", urb);
    }

    return hcd_pipe_free(pipe_hdl);
}

static esp_err_t _usb_pipe_flush(hcd_pipe_handle_t pipe_hdl, size_t urb_num)
{
    CDC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ESP_LOGI(TAG, "pipe flushing: state = %d", hcd_pipe_get_state(pipe_hdl));

    esp_err_t ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p halt failed", pipe_hdl);
    }

    ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_FLUSH);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p flush failed", pipe_hdl);
    }
    _default_pipe_event_wait_until(pipe_hdl, HCD_PIPE_EVENT_URB_DONE, 200 / portTICK_PERIOD_MS);

    for (size_t i = 0; i < urb_num; i++) {
        urb_t *urb = hcd_urb_dequeue(pipe_hdl);
        ESP_LOGD(TAG, "urb dequeue handle = %p", urb);
    }

    ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_CLEAR);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p clear failed", pipe_hdl);
    }

    return ESP_OK;
}

/*------------------------------------------------ USB URB Code ----------------------------------------------------*/
static urb_t *_usb_urb_alloc(int num_isoc_packets, size_t packet_data_buffer_size, void *context)
{
    //Allocate list of URBS
    urb_t *urb = heap_caps_calloc(1, sizeof(urb_t) + (num_isoc_packets * sizeof(usb_isoc_packet_desc_t)), MALLOC_CAP_DEFAULT);

    if (NULL == urb) {
        ESP_LOGE(TAG, "urb alloc failed");
        return NULL;
    }

    //Allocate data buffer for each URB and assign them
    uint8_t *data_buffer = NULL;

    if (num_isoc_packets) {
        /* ISOC urb */
        data_buffer = heap_caps_calloc(num_isoc_packets, packet_data_buffer_size, MALLOC_CAP_DMA);
    } else {
        /* no ISOC urb */
        data_buffer = heap_caps_calloc(1, packet_data_buffer_size, MALLOC_CAP_DMA);
    }

    if (NULL == data_buffer) {
        ESP_LOGE(TAG, "urb data_buffer alloc failed");
        return NULL;
    }

    //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
    transfer_dummy->data_buffer = data_buffer;
    transfer_dummy->num_isoc_packets = num_isoc_packets;
    transfer_dummy->context = context;

    ESP_LOGD(TAG, "urb alloced");
    return urb;
}

void _usb_urb_free(urb_t *urb)
{
    //Free data buffers of each URB
    heap_caps_free(urb->transfer.data_buffer);
    //Free the URB list
    heap_caps_free(urb);
    ESP_LOGD(TAG, "urb free");
}

static void _default_pipe_event_dflt_process(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event)
{
    assert(pipe_handle != NULL);

    switch (pipe_event) {
        case HCD_PIPE_EVENT_URB_DONE:
            ESP_LOGD(TAG, "line %u Pipe: default HCD_PIPE_EVENT_URB_DONE", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_XFER:
            ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_XFER", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
            ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_OVERFLOW:
            ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_OVERFLOW", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_STALL:
            ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_ERROR_STALL", __LINE__);
            break;

        case HCD_PIPE_EVENT_NONE:
            ESP_LOGD(TAG, "line %u Pipe: default HCD_PIPE_EVENT_NONE", __LINE__);
            break;

        default:
            ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, pipe_event);
            break;
    }
}

static esp_err_t _default_pipe_event_wait_until(hcd_pipe_handle_t expected_pipe_hdl,
        hcd_pipe_event_t expected_event, TickType_t xTicksToWait)
{
    //Wait for pipe callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_pipe_get_context(expected_pipe_hdl);
    cdc_event_msg_t evt_msg;
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    BaseType_t queue_ret = errQUEUE_EMPTY;

    do {
        queue_ret = xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            break;
        }

        if (PORT_EVENT == evt_msg._type) {
            _usb_port_event_dflt_process(evt_msg._handle.port_hdl, evt_msg._event.port_event);
            ret = ESP_ERR_NOT_FOUND;
        } else {
            if (expected_event == evt_msg._event.pipe_event) {
                _default_pipe_event_dflt_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
                ret = ESP_OK;
            } else {
                ESP_LOGD(TAG, "Got unexpected pipe_handle and event");
                _default_pipe_event_dflt_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
                ret = ESP_ERR_INVALID_RESPONSE;
            }
        }
    } while (ret == ESP_ERR_NOT_FOUND);

    return ret;
}

/*------------------------------------------------ USB Control Process Code ----------------------------------------------------*/

#ifdef CONFIG_CDC_GET_DEVICE_DESC
static esp_err_t _usb_get_dev_desc(hcd_pipe_handle_t pipe_handle, usb_device_desc_t *device_desc)
{
    CDC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    CDC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
    urb_t *urb_done = NULL;
    ESP_LOGI(TAG, "get device desc");
    USB_SETUP_PACKET_INIT_GET_DEVICE_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(sizeof(usb_device_desc_t), USB_EP_CTRL_DEFAULT_MPS);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    CDC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    CDC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    CDC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    CDC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    CDC_CHECK_GOTO((urb_done->transfer.actual_num_bytes <= sizeof(usb_setup_packet_t) + sizeof(usb_device_desc_t)), "urb status: data overflow", free_urb_);
    ESP_LOGI(TAG, "get device desc, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);
    usb_device_desc_t *dev_desc = (usb_device_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    if (device_desc != NULL ) *device_desc = *dev_desc;
    usb_print_device_descriptor(dev_desc);
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}
#endif

#ifdef CONFIG_CDC_GET_CONFIG_DESC
static esp_err_t _usb_get_config_desc(hcd_pipe_handle_t pipe_handle, usb_config_desc_t **config_desc)
{
    (void)config_desc;
    CDC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    CDC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
    urb_t *urb_done = NULL;
    ESP_LOGI(TAG, "get short config desc");
    USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, USB_ENUM_CONFIG_INDEX, USB_ENUM_SHORT_DESC_REQ_LEN);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(sizeof(usb_config_desc_t), USB_EP_CTRL_DEFAULT_MPS);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    CDC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    CDC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    CDC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    CDC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    CDC_CHECK_GOTO((urb_done->transfer.actual_num_bytes <= sizeof(usb_setup_packet_t) + sizeof(usb_config_desc_t)), "urb status: data overflow", free_urb_);
    ESP_LOGI(TAG, "get config desc, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);
    usb_config_desc_t *cfg_desc = (usb_config_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    uint16_t full_config_length = cfg_desc->wTotalLength;
    if (cfg_desc->wTotalLength > CTRL_TRANSFER_DATA_MAX_BYTES) {
        ESP_LOGE(TAG, "Configuration descriptor larger than control transfer max length");
        goto free_urb_;
    }
    ESP_LOGI(TAG, "get full config desc");
    USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, USB_ENUM_CONFIG_INDEX, full_config_length);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(full_config_length, USB_EP_CTRL_DEFAULT_MPS);
    ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    CDC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    CDC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    CDC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    CDC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    CDC_CHECK_GOTO((urb_done->transfer.actual_num_bytes <= sizeof(usb_setup_packet_t) + full_config_length), "urb status: data overflow", free_urb_);
    ESP_LOGI(TAG, "get full config desc, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);
    cfg_desc = (usb_config_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    usb_print_config_descriptor(cfg_desc, NULL);
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}
#endif

static esp_err_t _usb_set_device_addr(hcd_pipe_handle_t pipe_handle, uint8_t dev_addr)
{
    CDC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    CDC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    //STD: Set ADDR
    USB_SETUP_PACKET_INIT_SET_ADDR((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, dev_addr);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t); //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Addr = %u", dev_addr);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    CDC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    CDC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    CDC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    CDC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ret = hcd_pipe_update_dev_addr(pipe_handle, dev_addr);
    CDC_CHECK_GOTO(ESP_OK == ret, "default pipe update address failed", free_urb_);
    ret = hcd_pipe_update_mps(pipe_handle, USB_EP_CTRL_DEFAULT_MPS);
    CDC_CHECK_GOTO(ESP_OK == ret, "default pipe update MPS failed", free_urb_);
    ESP_LOGI(TAG, "Set Device Addr Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _usb_set_device_config(hcd_pipe_handle_t pipe_handle, uint16_t configuration)
{
    CDC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    CDC_CHECK(configuration != 0, "configuration can't be 0", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    CDC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    USB_SETUP_PACKET_INIT_SET_CONFIG((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, configuration);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t); //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Configuration = %u", configuration);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    CDC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    CDC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    CDC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    CDC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Configuration Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

#ifdef CONFIG_CDC_SEND_DTE_ACTIVE
static esp_err_t _usb_set_device_line_state(hcd_pipe_handle_t pipe_handle, bool dtr, bool rts)
{
    CDC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    CDC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    USB_CTRL_REQ_CDC_SET_LINE_STATE((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, 0, dtr, rts);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t); //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Line State: dtr %d, rts %d", dtr, rts);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    CDC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    CDC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    CDC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    CDC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Line State Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}
#endif

typedef struct {
    hcd_port_handle_t port_hdl;
    usb_speed_t dev_speed;
    uint8_t dev_addr;
    usb_ep_desc_t bulk_in_ep_desc;
    usb_ep_desc_t bulk_out_ep_desc;
    EventGroupHandle_t event_group_hdl;
    usbh_cdc_cb_t rx_callback;       /*!< packet receive callback, should not block */
    void *rx_callback_arg;           /*!< packet receive callback args */
} _cdc_data_task_args_t;

static inline void _processing_out_pipe(hcd_pipe_handle_t pipe_hdl, bool if_dequeue)
{
    static size_t pendding_urb_num = 0;
    static urb_t *pendding_urb[BULK_OUT_URB_NUM] = {NULL};
    esp_err_t ret = ESP_FAIL;

    if (!pendding_urb_num && !if_dequeue) {
        return;
    }

    if (if_dequeue) {
        urb_t *done_urb = hcd_urb_dequeue(pipe_hdl);

        if (done_urb->transfer.status != USB_TRANSFER_STATUS_COMPLETED) {
            /* retry if transfer not completed */
            hcd_urb_enqueue(pipe_hdl, done_urb);
            return;
        }

        ESP_LOGV(TAG, "ST actual len = %d", done_urb->transfer.actual_num_bytes);
        /* add done urb to pendding urb list */
        for (size_t i = 0; i < BULK_OUT_URB_NUM; i++) {
            if (pendding_urb[i] == NULL) {
                pendding_urb[i] = done_urb;
                ++pendding_urb_num;
                assert(pendding_urb_num <= BULK_OUT_URB_NUM);
                break;
            }
        }
    }

    /* check if we have buffered data need to send */
    if (get_usb_out_ringbuf_len() == 0) return;

    /* fetch a pendding urb from list */
    urb_t *next_urb = NULL;
    size_t next_index = 0;
    for (; next_index < BULK_OUT_URB_NUM; next_index++) {
        if (pendding_urb[next_index] != NULL) {
            next_urb = pendding_urb[next_index];
            break;
        }
    }
    assert(next_urb != NULL);

    size_t num_bytes_to_send = 0;
    ret = usb_out_ringbuf_pop(next_urb->transfer.data_buffer, BUFFER_SIZE_BULK_OUT, &num_bytes_to_send, 0);
    if (ret != ESP_OK || num_bytes_to_send == 0) {
        return;
    }
    next_urb->transfer.num_bytes = num_bytes_to_send;
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    s_bulkbuf_out_max = num_bytes_to_send > s_bulkbuf_out_max ? num_bytes_to_send : s_bulkbuf_out_max;
#endif
    hcd_urb_enqueue(pipe_hdl, next_urb);
    pendding_urb[next_index] = NULL;
    --pendding_urb_num;
    assert(pendding_urb_num <= BULK_OUT_URB_NUM);
    ESP_LOGV(TAG, "ST %d: %.*s", next_urb->transfer.num_bytes, next_urb->transfer.num_bytes, next_urb->transfer.data_buffer);
}

static void inline _processing_in_pipe(hcd_pipe_handle_t pipe_hdl, EventGroupHandle_t event_group_hdl, usbh_cdc_cb_t rx_cb, void *rx_cb_arg)
{
    urb_t *done_urb = hcd_urb_dequeue(pipe_hdl);
    ESP_LOGV(TAG, "RCV actual %d: %.*s", done_urb->transfer.actual_num_bytes, done_urb->transfer.actual_num_bytes, done_urb->transfer.data_buffer);

    if (done_urb->transfer.actual_num_bytes > 0) {
        esp_err_t ret = usb_in_ringbuf_push(done_urb->transfer.data_buffer, done_urb->transfer.actual_num_bytes, pdMS_TO_TICKS(TIMEOUT_USB_RINGBUF_MS));
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
        s_bulkbuf_in_max = done_urb->transfer.actual_num_bytes > s_bulkbuf_in_max ? done_urb->transfer.actual_num_bytes : s_bulkbuf_in_max;
#endif

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "in ringbuf too small, skip a usb payload");
        }

        if (rx_cb) {
            rx_cb(rx_cb_arg);
        }
    }

    hcd_urb_enqueue(pipe_hdl, done_urb);
}

static void _cdc_data_task(void *arg)
{
    assert(arg != NULL);
    _cdc_data_task_args_t *task_args = (_cdc_data_task_args_t *)(arg);
    hcd_pipe_handle_t pipe_hdl_in = NULL;
    hcd_pipe_handle_t pipe_hdl_out = NULL;
    QueueHandle_t data_queue_hdl = NULL;
    EventGroupHandle_t event_group_hdl = task_args->event_group_hdl;
    usbh_cdc_cb_t rx_cb = task_args->rx_callback;
    void *rx_cb_arg = task_args->rx_callback_arg;
    size_t num_bytes_to_send = 0;
    urb_t *urb_in[BULK_IN_URB_NUM] = {NULL};
    urb_t *urb_out[BULK_OUT_URB_NUM] = {NULL};

    cdc_event_msg_t evt_msg = {};
    data_queue_hdl = xQueueCreate(DATA_EVENT_QUEUE_LEN, sizeof(cdc_event_msg_t));
    assert(NULL != data_queue_hdl);

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Creating bulk in pipe");
    pipe_hdl_in = _usb_pipe_init(task_args->port_hdl, &task_args->bulk_in_ep_desc, task_args->dev_addr,
                                 task_args->dev_speed, (void *)data_queue_hdl, (void *)data_queue_hdl);
    assert(pipe_hdl_in != NULL);

    ESP_LOGI(TAG, "Creating bulk out pipe");
    pipe_hdl_out = _usb_pipe_init(task_args->port_hdl, &task_args->bulk_out_ep_desc, task_args->dev_addr,
                                  task_args->dev_speed, (void *)data_queue_hdl, (void *)data_queue_hdl);
    assert(pipe_hdl_out != NULL);

    for (size_t i = 0; i < BULK_IN_URB_NUM; i++) {
        urb_t *urb = heap_caps_calloc(1, sizeof(urb_t), MALLOC_CAP_INTERNAL);
        assert(NULL != urb);
        urb_in[i] = urb;
        uint8_t *data_buffer = heap_caps_malloc(BUFFER_SIZE_BULK_IN, MALLOC_CAP_DMA);
        assert(NULL != data_buffer);
        //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
        usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
        transfer_dummy->data_buffer = data_buffer;
        transfer_dummy->num_bytes = BUFFER_SIZE_BULK_IN;
        hcd_urb_enqueue(pipe_hdl_in, urb);
    }

    for (size_t i = 0; i < BULK_OUT_URB_NUM; i++) {
        urb_t *urb = heap_caps_calloc(1, sizeof(urb_t), MALLOC_CAP_INTERNAL);
        assert(NULL != urb);
        urb_out[i] = urb;
        uint8_t *data_buffer = heap_caps_malloc(BUFFER_SIZE_BULK_OUT, MALLOC_CAP_DMA);
        assert(NULL != data_buffer);
        //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
        usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
        transfer_dummy->data_buffer = data_buffer;
        urb->transfer.num_bytes = num_bytes_to_send;
        hcd_urb_enqueue(pipe_hdl_out, urb);
        ESP_LOGV(TAG, "ST %d: %.*s", urb->transfer.num_bytes, urb->transfer.num_bytes, urb->transfer.data_buffer);
    }
    xEventGroupSetBits(event_group_hdl, CDC_DEVICE_READY_BIT);

    while (!(xEventGroupGetBits(event_group_hdl) & CDC_DATA_TASK_KILL_BIT)) {
        if (xQueueReceive(data_queue_hdl, &evt_msg, 1) != pdTRUE) {
            /* check if ringbuf has date to send */
            _processing_out_pipe(pipe_hdl_out, false);
            continue;
        }
        switch (evt_msg._event.pipe_event) {
            case HCD_PIPE_EVENT_URB_DONE:
                if (evt_msg._handle.pipe_handle == pipe_hdl_out) {
                    _processing_out_pipe(pipe_hdl_out, true);
                } else if (evt_msg._handle.pipe_handle == pipe_hdl_in) {
                    _processing_in_pipe(pipe_hdl_in, event_group_hdl, rx_cb, rx_cb_arg);
                } else {
                    ESP_LOGE(TAG, "invalid pipe handle");
                    assert(0);
                }

                break;

            case HCD_PIPE_EVENT_ERROR_XFER:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_XFER", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_OVERFLOW:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_OVERFLOW", __LINE__);
                break;

            case HCD_PIPE_EVENT_ERROR_STALL:
                ESP_LOGW(TAG, "line %u Pipe: bulk_out HCD_PIPE_EVENT_ERROR_STALL", __LINE__);
                break;

            case HCD_PIPE_EVENT_NONE:
                break;

            default:
                ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, evt_msg._event.pipe_event);
                break;
        }
    }
    xEventGroupClearBits(event_group_hdl, CDC_DEVICE_READY_BIT);
    esp_err_t ret = _usb_pipe_deinit(pipe_hdl_in, BULK_IN_URB_NUM);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "in pipe delete failed");
    }

    ret = _usb_pipe_deinit(pipe_hdl_out, BULK_OUT_URB_NUM);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "out pipe delete failed");
    }

    for (size_t i = 0; i < BULK_IN_URB_NUM; i++) {
        free(urb_in[i]->transfer.data_buffer);
        free(urb_in[i]);
    }

    for (size_t i = 0; i < BULK_OUT_URB_NUM; i++) {
        free(urb_out[i]->transfer.data_buffer);
        free(urb_out[i]);
    }

    xEventGroupClearBits(event_group_hdl, CDC_DATA_TASK_KILL_BIT);
    vQueueDelete(data_queue_hdl);
    ESP_LOGI(TAG, "CDC task deleted");
    vTaskDelete(NULL);
}

static bool _usb_port_callback(hcd_port_handle_t port_hdl, hcd_port_event_t port_event, void *user_arg, bool in_isr)
{
    QueueHandle_t usb_event_queue = (QueueHandle_t)user_arg;
    assert(in_isr);    //Current HCD implementation should never call a port callback in a task context
    cdc_event_msg_t event_msg = {
        ._type = PORT_EVENT,
        ._handle.port_hdl = port_hdl,
        ._event.port_event = port_event,
    };
    BaseType_t xTaskWoken = pdFALSE;
    xQueueSendFromISR(usb_event_queue, &event_msg, &xTaskWoken);
    return (xTaskWoken == pdTRUE);
}

static void _usb_processing_task(void *arg)
{
    CDC_CHECK_GOTO(arg != NULL, "Task arg can't be NULL", delete_task_);
    usbh_cdc_config_t *cdc_config = (usbh_cdc_config_t *)arg;

    esp_err_t ret = ESP_OK;
    usb_speed_t port_speed = 0;
    hcd_port_handle_t port_hdl = NULL;
    hcd_port_event_t port_actual_evt;
    hcd_pipe_handle_t pipe_hdl_dflt = NULL;
    QueueHandle_t cdc_queue_hdl = NULL;
    cdc_event_msg_t evt_msg = {};
    TaskHandle_t cdc_data_task_hdl = NULL;
    usbh_cdc_cb_t conn_callback = cdc_config->conn_callback;
    void *conn_callback_arg = cdc_config->conn_callback_arg;
    usbh_cdc_cb_t disconn_callback = cdc_config->disconn_callback;
    void *disconn_callback_arg = cdc_config->disconn_callback_arg;
    _cdc_data_task_args_t cdc_task_args = {
        .dev_addr = USB_DEVICE_ADDR,
        .bulk_in_ep_desc = *(cdc_config->bulk_in_ep),
        .bulk_out_ep_desc = *(cdc_config->bulk_out_ep),
        .event_group_hdl = s_usb_event_group,
        .rx_callback = cdc_config->rx_callback,
        .rx_callback_arg = cdc_config->rx_callback_arg,
    };

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    cdc_queue_hdl = xQueueCreate(CDC_EVENT_QUEUE_LEN, sizeof(cdc_event_msg_t));
    assert(NULL != cdc_queue_hdl);
    port_hdl = _usb_port_init((void *)cdc_queue_hdl, _usb_port_callback, (void *)cdc_queue_hdl);
    CDC_CHECK_GOTO(port_hdl != NULL, "USB Port init failed", delete_cdc_queue_);
    cdc_task_args.port_hdl = port_hdl;

    while (1) {

        ESP_LOGI(TAG, "Waitting USB Connection");
        if (hcd_port_get_state(port_hdl) == HCD_PORT_STATE_NOT_POWERED) {
            ret = _usb_port_power(port_hdl, true);
            CDC_CHECK_GOTO(ESP_OK == ret, "Port Set POWER_ON failed", usb_driver_reset_);
        }

        while (!(xEventGroupGetBits(s_usb_event_group) & USB_TASK_KILL_BIT)) {

            if (xQueueReceive(cdc_queue_hdl, &evt_msg, 10 / portTICK_PERIOD_MS) != pdTRUE) {
                continue;
            }

            switch (evt_msg._type) {
                case PORT_EVENT:
                    port_actual_evt = _usb_port_event_dflt_process(port_hdl, evt_msg._event.port_event);
                    switch (port_actual_evt) {
                        case HCD_PORT_EVENT_CONNECTION:
                            // Reset port and get speed
                            ESP_LOGI(TAG, "Resetting Port");
                            ret = _usb_port_reset(port_hdl);
                            CDC_CHECK_GOTO(ESP_OK == ret, "Port Reset failed", usb_driver_reset_);
                            _update_device_state(CDC_DEVICE_STATE_DEFAULT);
                            ESP_LOGI(TAG, "Getting Port Speed");
                            ret = _usb_port_get_speed(port_hdl, &port_speed);
                            CDC_CHECK_GOTO(ESP_OK == ret, "USB Port get speed failed", usb_driver_reset_);
                            ESP_LOGI(TAG, "USB Speed: %s-speed", port_speed?"full":"low");
                            cdc_task_args.dev_speed = port_speed;

                            if (!pipe_hdl_dflt) pipe_hdl_dflt = _usb_pipe_init(port_hdl, NULL, 0, port_speed,
                                                                    (void *)cdc_queue_hdl, (void *)cdc_queue_hdl);

                            CDC_CHECK_GOTO(pipe_hdl_dflt != NULL, "default pipe create failed", usb_driver_reset_);

                            ret = _usb_set_device_addr(pipe_hdl_dflt, USB_DEVICE_ADDR);
                            CDC_CHECK_GOTO(ESP_OK == ret, "Set device address failed", usb_driver_reset_);
#ifdef CONFIG_CDC_GET_DEVICE_DESC
                            ret = _usb_get_dev_desc(pipe_hdl_dflt, NULL);
                            CDC_CHECK_GOTO(ESP_OK == ret, "Get device descriptor failed", usb_driver_reset_);
#endif
#ifdef CONFIG_CDC_GET_CONFIG_DESC
                            ret = _usb_get_config_desc(pipe_hdl_dflt, NULL);
                            CDC_CHECK_GOTO(ESP_OK == ret, "Get config descriptor failed", usb_driver_reset_);
#endif
                            _update_device_state(CDC_DEVICE_STATE_ADDRESS);
                            ret = _usb_set_device_config(pipe_hdl_dflt, USB_DEVICE_CONFIG);
                            CDC_CHECK_GOTO(ESP_OK == ret, "Set device configuration failed", usb_driver_reset_);
                            _update_device_state(CDC_DEVICE_STATE_CONFIGURED);
#ifdef CONFIG_CDC_SEND_DTE_ACTIVE
                            ret = _usb_set_device_line_state(pipe_hdl_dflt, true, false);
                            CDC_CHECK_GOTO(ESP_OK == ret, "Set device line state failed", usb_driver_reset_);
#endif
                            xTaskCreatePinnedToCore(_cdc_data_task, CDC_DATA_TASK_NAME, CDC_DATA_TASK_STACK_SIZE, (void *)(&cdc_task_args),
                                                    CDC_DATA_TASK_PRIORITY, &cdc_data_task_hdl, CDC_DATA_TASK_CORE);
                            CDC_CHECK_GOTO(cdc_data_task_hdl != NULL, "cdc task create failed", usb_driver_reset_);
                            xTaskNotifyGive(cdc_data_task_hdl);
                            if(conn_callback) conn_callback(conn_callback_arg);
                            break;

                        case HCD_PORT_EVENT_DISCONNECTION:
                            _update_device_state(CDC_DEVICE_STATE_NOT_ATTACHED);
                            ESP_LOGI(TAG, "hcd port state = %d", hcd_port_get_state(port_hdl));
                            if(disconn_callback) disconn_callback(disconn_callback_arg);
                            goto usb_driver_reset_;
                            break;
                        case HCD_PORT_EVENT_ERROR:
                            _update_device_state(CDC_DEVICE_STATE_NOT_ATTACHED);
                            ESP_LOGI(TAG, "hcd port state = %d", hcd_port_get_state(port_hdl));
                            if(disconn_callback) disconn_callback(disconn_callback_arg);
                            goto usb_driver_reset_;
                            break;
                        default:
                            break;
                    }

                    break;

                case PIPE_EVENT:
                    _default_pipe_event_dflt_process(pipe_hdl_dflt, evt_msg._event.pipe_event);
                    break;

                default:
                    break;
            }
        }

/* Reset USB to init state */
usb_driver_reset_:
        ESP_LOGW(TAG, "Resetting USB Driver");

        if (cdc_data_task_hdl) {
            xEventGroupSetBits(s_usb_event_group, CDC_DATA_TASK_KILL_BIT);
            ESP_LOGW(TAG, "Waitting for CDC task delete");
            while (xEventGroupGetBits(s_usb_event_group) & CDC_DATA_TASK_KILL_BIT) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            cdc_data_task_hdl = NULL;
        }

        if (pipe_hdl_dflt) {
            ESP_LOGW(TAG, "Resetting default pipe");
            _usb_pipe_deinit(pipe_hdl_dflt, 1);
            pipe_hdl_dflt = NULL;
        }

        ESP_LOGI(TAG, "hcd recovering");
        hcd_port_recover(port_hdl);
        vTaskDelay(50 / portTICK_PERIOD_MS);

        if (xEventGroupGetBits(s_usb_event_group) & USB_TASK_KILL_BIT) {
            break;
        }
        
    }

/* if set USB_TASK_KILL_BIT, all resource will be released */
    ESP_LOGW(TAG, "Deleting USB Driver");
    if (port_hdl) {
        _usb_port_deinit(port_hdl);
        port_hdl = NULL;
    }
delete_cdc_queue_:
    if (cdc_queue_hdl) {
        vQueueDelete(cdc_queue_hdl);
    }
delete_task_:
    ESP_LOGW(TAG, "usb processing task deleted");
    ESP_LOGW(TAG, "usb processing task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    xEventGroupClearBits(s_usb_event_group, USB_TASK_KILL_BIT);
    vTaskDelete(NULL);
}


esp_err_t usbh_cdc_driver_install(const usbh_cdc_config_t *config)
{
    CDC_CHECK(config != NULL, "config can't be NULL", ESP_ERR_INVALID_ARG);
    CDC_CHECK(config->rx_buffer_size != 0 && config->rx_buffer_size != 0, "buffer size can't be 0", ESP_ERR_INVALID_ARG);
    CDC_CHECK(config->bulk_in_ep != NULL || config->bulk_in_ep_addr & 0x80, "bulk_in_ep or bulk_in_ep_addr invalid", ESP_ERR_INVALID_ARG);
    CDC_CHECK(config->bulk_out_ep != NULL || !(config->bulk_out_ep_addr & 0x80), "bulk_out_ep or bulk_out_ep_addr invalid", ESP_ERR_INVALID_ARG);

    if (_cdc_driver_is_init()) {
        ESP_LOGW(TAG, "USB Driver has inited");
        return ESP_ERR_INVALID_STATE;
    }

    usb_ep_desc_t bulk_in_ep = DEFAULT_BULK_EP_DESC();
    usb_ep_desc_t bulk_out_ep = DEFAULT_BULK_EP_DESC();
    usbh_cdc_config_t config_dummy = *config;
    if (config->bulk_in_ep == NULL) {
        /* using default ep_desc with user's ep_addr */
        bulk_in_ep.bEndpointAddress = config->bulk_in_ep_addr;
    } else {
        /* using user's ep_desc */
        bulk_in_ep = *config->bulk_in_ep;
    }
    config_dummy.bulk_in_ep = &bulk_in_ep;

    if (config->bulk_out_ep == NULL) {
        /* using default ep_desc with user's ep_addr */
        bulk_out_ep.bEndpointAddress = config->bulk_out_ep_addr;
    } else {
        /* using user's ep_desc */
        bulk_out_ep = *config->bulk_out_ep;
    }
    config_dummy.bulk_out_ep = &bulk_out_ep;

#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    s_ringbuf_in_size = config_dummy.rx_buffer_size;
    s_ringbuf_out_size = config_dummy.tx_buffer_size;
#endif

    s_usb_event_group = xEventGroupCreate();
    CDC_CHECK(s_usb_event_group != NULL, "Create event group failed", ESP_FAIL);
    s_in_ringbuf_handle = xRingbufferCreate(config_dummy.rx_buffer_size, RINGBUF_TYPE_BYTEBUF);
    CDC_CHECK_GOTO(s_in_ringbuf_handle != NULL, "Create in ringbuffer failed", delete_resource_);
    s_out_ringbuf_handle = xRingbufferCreate(config_dummy.tx_buffer_size, RINGBUF_TYPE_BYTEBUF);
    CDC_CHECK_GOTO(s_out_ringbuf_handle != NULL, "Create out ringbuffer failed", delete_resource_);
    s_usb_read_mux = xSemaphoreCreateMutex();
    CDC_CHECK_GOTO(s_usb_read_mux != NULL, "Create read mutex failed", delete_resource_);
    s_usb_write_mux = xSemaphoreCreateMutex();
    CDC_CHECK_GOTO(s_usb_write_mux != NULL, "Create write mutex failed", delete_resource_);

    BaseType_t ret = xTaskCreatePinnedToCore(_usb_processing_task, USB_PROC_TASK_NAME, USB_PROC_TASK_STACK_SIZE, (void *)&config_dummy,
                     USB_PROC_TASK_PRIORITY, &s_usb_processing_task_hdl, USB_PROC_TASK_CORE);
    CDC_CHECK_GOTO(ret == pdPASS, "Create usb task failed", delete_resource_);

    vTaskDelay(50 / portTICK_PERIOD_MS);
    xTaskNotifyGive(s_usb_processing_task_hdl);

    ESP_LOGI(TAG, "usb driver install succeed");
    return ESP_OK;

delete_resource_:
    if(s_usb_write_mux) vSemaphoreDelete(s_usb_write_mux);
    if(s_usb_read_mux) vSemaphoreDelete(s_usb_read_mux);
    if(s_out_ringbuf_handle) vRingbufferDelete(s_out_ringbuf_handle);
    if(s_in_ringbuf_handle) vRingbufferDelete(s_in_ringbuf_handle);
    if(s_usb_event_group) vEventGroupDelete(s_usb_event_group);
    s_usb_event_group = NULL;
    s_in_ringbuf_handle = NULL;
    s_out_ringbuf_handle = NULL;
    s_usb_read_mux = NULL;
    s_usb_write_mux = NULL;
    vTaskDelete(NULL);
    return ESP_FAIL;
}

esp_err_t usbh_cdc_driver_delete(void)
{
    if (!_cdc_driver_is_init()) {
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupSetBits(s_usb_event_group, USB_TASK_KILL_BIT);

    ESP_LOGW(TAG, "Waitting for USB Driver Delete");
    while (xEventGroupGetBits(s_usb_event_group) & USB_TASK_KILL_BIT) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    vRingbufferDelete(s_in_ringbuf_handle);
    s_in_ringbuf_handle = NULL;
    vRingbufferDelete(s_out_ringbuf_handle);
    s_out_ringbuf_handle = NULL;
    vSemaphoreDelete(s_usb_read_mux);
    s_usb_read_mux = NULL;
    vSemaphoreDelete(s_usb_write_mux);
    s_usb_write_mux = NULL;
    vEventGroupDelete(s_usb_event_group);
    s_usb_event_group = NULL;
    ESP_LOGW(TAG, "USB Driver Deleted!");
    return ESP_OK;
}

esp_err_t usbh_cdc_wait_connect(TickType_t ticks_to_wait)
{
    if (!_cdc_driver_is_init()) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Waitting Device Connection");
    TickType_t ticks_counter = 0;
    const size_t ticks_step = 5;
    while (!(xEventGroupGetBits(s_usb_event_group) & CDC_DEVICE_READY_BIT)) {
        vTaskDelay(ticks_step);
        ticks_counter += ticks_step;
        if (ticks_counter >= ticks_to_wait) break;
    }

    if (!(xEventGroupGetBits(s_usb_event_group) & CDC_DEVICE_READY_BIT)) {
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "Device Connected");
    return ESP_OK;
}

esp_err_t usbh_cdc_get_buffered_data_len(size_t *size)
{
    CDC_CHECK(size != NULL, "arg can't be NULL", ESP_ERR_INVALID_ARG);

    if (_cdc_driver_is_init() == false) {
        *size = 0;
        ESP_LOGD(TAG, "CDC Driver not installed");
        return ESP_ERR_INVALID_STATE;
    }

    if (_if_device_ready() == false) {
        *size = 0;
        ESP_LOGV(TAG, "Device not connected or not ready");
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_in_ringbuf_mux);
    *size = s_in_buffered_data_len;
    portEXIT_CRITICAL(&s_in_ringbuf_mux);
    return ESP_OK;
}

int usbh_cdc_read_bytes(uint8_t *buf, size_t length, TickType_t ticks_to_wait)
{
    CDC_CHECK(buf != NULL, "invalid args", -1);
    size_t read_sz = 0;
    int rx_data_size = 0;

    if (_cdc_driver_is_init() == false) {
        ESP_LOGD(TAG, "CDC Driver not installed");
        return -1;
    }

    if (_if_device_ready() == false) {
        ESP_LOGV(TAG, "Device not connected or not ready");
        return -1;
    }

    xSemaphoreTake(s_usb_read_mux, portMAX_DELAY);
    esp_err_t res = usb_in_ringbuf_pop(buf, length, &read_sz, ticks_to_wait);

    if (res != ESP_OK) {
        xSemaphoreGive(s_usb_read_mux);
        ESP_LOGD(TAG, "Read ringbuffer failed");
        return -1;
    }

    rx_data_size = read_sz;

    /* Buffer's data can be wrapped, at that situations we should make another retrievement */
    if (usb_in_ringbuf_pop(buf + read_sz, length - read_sz, &read_sz, 0) == ESP_OK) {
        rx_data_size += read_sz;
    }

    xSemaphoreGive(s_usb_read_mux);
    return rx_data_size;
}

int usbh_cdc_write_bytes(const uint8_t *buf, size_t length)
{
    CDC_CHECK(buf != NULL, "invalid args", -1);
    int tx_data_size = 0;

    if (_cdc_driver_is_init() == false) {
        ESP_LOGD(TAG, "CDC Driver not installed");
        return -1;
    }

    if (_if_device_ready() == false) {
        ESP_LOGV(TAG, "Device not connected or not ready");
        return -1;
    }

    xSemaphoreTake(s_usb_write_mux, portMAX_DELAY);
    esp_err_t ret = usb_out_ringbuf_push(buf, length, pdMS_TO_TICKS(TIMEOUT_USB_RINGBUF_MS));

    if (ret != ESP_OK) {
        xSemaphoreGive(s_usb_write_mux);
        ESP_LOGD(TAG, "Write ringbuffer failed");
        return -1;
    }

    tx_data_size = length;
    xSemaphoreGive(s_usb_write_mux);
    return tx_data_size;
}

#ifdef CONFIG_CDC_USE_TRACE_FACILITY
void usbh_cdc_print_buffer_msg(void)
{
    ESP_LOGI(TAG, "USBH CDC Transfer Buffer Dump:");
    ESP_LOGI(TAG, "usb transfer Buffer size, out = %d, in = %d", BUFFER_SIZE_BULK_OUT, BUFFER_SIZE_BULK_IN);
    ESP_LOGI(TAG, "usb transfer Max packet size, out = %d, in = %d\n", s_bulkbuf_out_max, s_bulkbuf_in_max);
    ESP_LOGI(TAG, "USBH CDC Ringbuffer Dump:");
    ESP_LOGI(TAG, "usb ringbuffer size, out = %d, in = %d", s_ringbuf_out_size, s_ringbuf_in_size);
    ESP_LOGI(TAG, "usb ringbuffer load, out = %d, in = %d", s_out_buffered_data_len, s_in_buffered_data_len);
    ESP_LOGI(TAG, "usb ringbuffer High water mark, out = %d, in = %d", s_ringbuf_out_max, s_ringbuf_in_max);
}
#endif