// Copyright 2016-2021 Espressif Systems (Shanghai) PTE LTD
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "soc/gpio_pins.h"
#include "soc/gpio_sig_map.h"
#include "esp_rom_gpio.h"
#include "hal/usbh_ll.h"
#include "hcd.h"
#include "usb_private.h"
#include "esp_log.h"
#include "uvc_stream.h"
#include "uvc_debug.h"
#include "sdkconfig.h"

#define UVC_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define UVC_CHECK_GOTO(a, str, lable) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto lable; \
    }

const char *TAG = "UVC_STREAM";

/**
 * @brief General params for UVC device
 *
 */
#define USB_PORT_NUM                         1        //Default port number
#define USB_DEVICE_ADDR                      1        //Default UVC device address
#define USB_EP_CTRL_DEFAULT_MPS              64       //Default MPS(max payload size) of Endpoint 0
#define USB_EP_ISOC_MAX_MPS                  512      //Max MPS ESP32-S2/S3 can handle
#define CTRL_TRANSFER_DATA_MAX_BYTES         256      //Max data length assumed in control transfer
#define NUM_ISOC_STREAM_URBS                 4        //Number of isochronous stream URBS created for continuous enqueue
#define NUM_PACKETS_PER_URB_URB              5        //Number of packets in each isochronous stream URB
#define UVC_EVENT_QUEUE_LEN                  30       //UVC event queue length 
#define FRAME_MAX_INTERVAL                   2000000  //Specified in 100 ns units, General max frame interval (5 FPS)
#define FRAME_MIN_INTERVAL                   333333   //General min frame interval (30 FPS)

/**
 * @brief Task for USB I/O request and payload processing,
 * can not be blocked, higher task priority is suggested.
 *
 */
#define USB_PROC_TASK_NAME "usb_proc"
#define USB_PROC_TASK_PRIORITY 5
#define USB_PROC_TASK_STACK_SIZE (1024 * 3)
#define USB_PROC_TASK_CORE 0

/**
 * @brief Task for sample processing, run user callback when new frame ready.
 *
 */
#define SAMPLE_PROC_TASK_NAME "sample_proc"
#define SAMPLE_PROC_TASK_PRIORITY 2
#define SAMPLE_PROC_TASK_STACK_SIZE (1024 * 3)
#define SAMPLE_PROC_TASK_CORE 0

/**
 * @brief Events bit map
 *
 */
#define USB_STREAM_INIT_DONE         BIT1       //set if uvc stream inited
#define USB_STREAM_RUNNING           BIT2       //set if uvc streaming
#define USB_STREAM_SUSPEND_DONE      BIT3       //set if uvc streaming successfully suspend
#define USB_STREAM_RESUME_DONE       BIT4       //set if uvc streaming successfully resume
#define USB_STREAM_STOP_DONE         BIT5       //set if uvc streaming successfully stop
#define SAMPLE_PROC_RUNNING          BIT6       //set if sample processing
#define SAMPLE_PROC_STOP_DONE        BIT7       //set if sample processing successfully stop

/**************************************************Share Handles***********************************************/

static TaskHandle_t s_usb_processing_task_hdl = NULL;
static TaskHandle_t s_samlpe_processing_task_hdl = NULL;

/********************************************** Helper Functions **********************************************/
/**
 * @brief
 * bmRequestType UVC_SET_CUR ? 0x21 : 0xA1
 * bRequest uvc_req_code
 * wValue uvc_vs_ctrl_selector<<8
 * wIndex zero or interface
 * wLength len
 * Data param
 */
#define USB_CTRL_UVC_COMMIT_REQ(ctrl_req_ptr) ({  \
        (ctrl_req_ptr)->bRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = UVC_SET_CUR;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_COMMIT_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

#define USB_CTRL_UVC_PROBE_SET_REQ(ctrl_req_ptr) ({  \
        (ctrl_req_ptr)->bRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = UVC_SET_CUR;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_PROBE_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

#define USB_CTRL_UVC_PROBE_GET_REQ(ctrl_req_ptr) ({  \
        (ctrl_req_ptr)->bRequestType = 0xA1;   \
        (ctrl_req_ptr)->bRequest = UVC_GET_CUR;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_PROBE_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

#define USB_CTRL_UVC_PROBE_GET_GENERAL_REQ(ctrl_req_ptr, req) ({  \
        (ctrl_req_ptr)->bRequestType = 0xA1;   \
        (ctrl_req_ptr)->bRequest = req;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_PROBE_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

/**
 * @brief fill buffer with user ctrl configs
 *
 * @param buf user buffer
 * @param len buffer size
 * @param ctrl user ctrl configs
 * @return ** void
 */
void _uvc_stream_ctrl_to_buf(uint8_t *buf, size_t len, uvc_stream_ctrl_t *ctrl)
{
    assert(len >= 26);
    memset(buf, 0, len);

    /* prepare for a SET transfer */
    SHORT_TO_SW(ctrl->bmHint, buf);
    buf[2] = ctrl->bFormatIndex;
    buf[3] = ctrl->bFrameIndex;
    INT_TO_DW(ctrl->dwFrameInterval, buf + 4);
    SHORT_TO_SW(ctrl->wKeyFrameRate, buf + 8);
    SHORT_TO_SW(ctrl->wPFrameRate, buf + 10);
    SHORT_TO_SW(ctrl->wCompQuality, buf + 12);
    SHORT_TO_SW(ctrl->wCompWindowSize, buf + 14);
    SHORT_TO_SW(ctrl->wDelay, buf + 16);
    INT_TO_DW(ctrl->dwMaxVideoFrameSize, buf + 18);
    INT_TO_DW(ctrl->dwMaxPayloadTransferSize, buf + 22);
}

/**
 * @brief parse buffer data to ctrl configs
 *
 * @param buf buffer for parse
 * @param len buffer size
 * @param ctrl ctrl struct to save configs
 * @return ** void
 */
void _buf_to_uvc_stream_ctrl(uint8_t *buf, size_t len, uvc_stream_ctrl_t *ctrl)
{
    assert(len >= 26);

    /* prepare for a SET transfer */
    ctrl->bmHint = SW_TO_SHORT(buf);
    ctrl->bFormatIndex = buf[2];
    ctrl->bFrameIndex = buf[3];
    ctrl->dwFrameInterval = DW_TO_INT(buf + 4);
    ctrl->wKeyFrameRate = SW_TO_SHORT(buf + 8);
    ctrl->wPFrameRate = SW_TO_SHORT(buf + 10);
    ctrl->wCompQuality = SW_TO_SHORT(buf + 12);
    ctrl->wCompWindowSize = SW_TO_SHORT(buf + 14);
    ctrl->wDelay = SW_TO_SHORT(buf + 16);
    ctrl->dwMaxVideoFrameSize = DW_TO_INT(buf + 18);
    ctrl->dwMaxPayloadTransferSize = DW_TO_INT(buf + 22);
}

/** @brief Print the values in a stream control block
 * @ingroup diag
 *
 * @param devh UVC device
 * @param stream Output stream (stderr if NULL)
 */
void _uvc_stream_ctrl_printf(FILE *stream, uvc_stream_ctrl_t *ctrl)
{
    if (stream == NULL) {
        stream = stderr;
    }

    fprintf(stream, "bmHint: %04x\n", ctrl->bmHint);
    fprintf(stream, "bFormatIndex: %d\n", ctrl->bFormatIndex);
    fprintf(stream, "bFrameIndex: %d\n", ctrl->bFrameIndex);
    fprintf(stream, "dwFrameInterval: %u\n", ctrl->dwFrameInterval);
    fprintf(stream, "wKeyFrameRate: %d\n", ctrl->wKeyFrameRate);
    fprintf(stream, "wPFrameRate: %d\n", ctrl->wPFrameRate);
    fprintf(stream, "wCompQuality: %d\n", ctrl->wCompQuality);
    fprintf(stream, "wCompWindowSize: %d\n", ctrl->wCompWindowSize);
    fprintf(stream, "wDelay: %d\n", ctrl->wDelay);
    fprintf(stream, "dwMaxVideoFrameSize: %u\n", ctrl->dwMaxVideoFrameSize);
    fprintf(stream, "dwMaxPayloadTransferSize: %u\n", ctrl->dwMaxPayloadTransferSize);
    fprintf(stream, "dwClockFrequency: %u\n", ctrl->dwClockFrequency);
    fprintf(stream, "bmFramingInfo: %u\n", ctrl->bmFramingInfo);
    fprintf(stream, "bPreferredVersion: %u\n", ctrl->bPreferredVersion);
    fprintf(stream, "bMinVersion: %u\n", ctrl->bMinVersion);
    fprintf(stream, "bMaxVersion: %u\n", ctrl->bMaxVersion);
    fprintf(stream, "bInterfaceNumber: %d\n", ctrl->bInterfaceNumber);
}

//320*240@15 fps param for demo camera
const static uvc_stream_ctrl_t s_uvc_stream_ctrl_default = {
    .bmHint = 1,
    .bFormatIndex = 2,
    .bFrameIndex = 3,
    .dwFrameInterval = 666666,
    .wKeyFrameRate = 0,
    .wPFrameRate = 0,
    .wCompQuality = 0,
    .wCompWindowSize = 0,
    .wDelay = 0,
    .dwMaxVideoFrameSize = 35000,
    .dwMaxPayloadTransferSize = 512,
};

/*************************************** Internal Types ********************************************/
/**
 * @brief Internal UVC device defination
 *
 */
typedef struct _uvc_device {
    usb_speed_t dev_speed;
    uint16_t configuration;
    uint8_t dev_addr;
    uint8_t isoc_ep_addr;
    uint32_t isoc_ep_mps;
    uvc_stream_ctrl_t ctrl_set;
    uvc_stream_ctrl_t ctrl_probed;
    uint32_t xfer_buffer_size;
    uint8_t *xfer_buffer_a;
    uint8_t *xfer_buffer_b;
    uint32_t frame_buffer_size;
    uint8_t *frame_buffer;
    uint16_t frame_width;
    uint16_t frame_height;
    uint16_t interface;
    uint16_t interface_alt;
    QueueHandle_t queue_hdl;
    EventGroupHandle_t event_group;
    uvc_frame_callback_t *user_cb;
    void *user_ptr;
    bool active;
} _uvc_device_t;

/**
 * @brief Singleton Pattern, Only one camera supported
 *
 */
static _uvc_device_t s_uvc_dev = {0};

/**
 * @brief Stream information
 *
 */
typedef struct _uvc_stream_handle {
    /** if true, stream is running (streaming video to host) */
    uvc_device_handle_t devh;
    uint8_t running;
    /** Current control block */
    struct uvc_stream_ctrl cur_ctrl;
    uint8_t fid;
    uint32_t seq, hold_seq;
    uint32_t pts, hold_pts;
    uint32_t last_scr, hold_last_scr;
    size_t got_bytes, hold_bytes;
    uint8_t *outbuf, *holdbuf;
    uvc_frame_callback_t *user_cb;
    void *user_ptr;
    xSemaphoreHandle cb_mutex;
    TaskHandle_t taskh;
    struct uvc_frame frame;
    enum uvc_frame_format frame_format;
    struct timespec capture_time_finished;
} _uvc_stream_handle_t;

typedef enum {
    USER_EVENT,
    PORT_EVENT,
    PIPE_EVENT,
} _uvc_event_type_t;

typedef enum {
    STREAM_SUSPEND,
    STREAM_RESUME,
    STREAM_STOP,
} _uvc_user_cmd_t;

typedef struct {
    _uvc_event_type_t _type;
    union {
        hcd_port_handle_t port_hdl;
        hcd_pipe_handle_t pipe_handle;
    } _handle;
    union {
        _uvc_user_cmd_t user_cmd;
        hcd_port_event_t port_event;
        hcd_pipe_event_t pipe_event;
    } _event;
} _uvc_event_msg_t;

static inline void _uvc_process_payload(_uvc_stream_handle_t *strmh, uint8_t *payload, size_t payload_len);

/*------------------------------------------------ USB Port Code ----------------------------------------------------*/

static bool _usb_port_callback(hcd_port_handle_t port_hdl, hcd_port_event_t port_event, void *user_arg, bool in_isr)
{
    QueueHandle_t usb_event_queue = (QueueHandle_t)user_arg;
    assert(in_isr);    //Current HCD implementation should never call a port callback in a task context
    _uvc_event_msg_t event_msg = {
        ._type = PORT_EVENT,
        ._handle.port_hdl = port_hdl,
        ._event.port_event = port_event,
    };
    BaseType_t xTaskWoken = pdFALSE;
    xQueueSendFromISR(usb_event_queue, &event_msg, &xTaskWoken);
    return (xTaskWoken == pdTRUE);
}

static void _usb_port_event_process(hcd_port_handle_t port_hdl, hcd_port_event_t event)
{
    (void)event;
    assert(port_hdl != NULL);
    hcd_port_event_t actual_evt = hcd_port_handle_event(port_hdl);

    switch (actual_evt) {
        case HCD_PORT_EVENT_CONNECTION:
            //Reset newly connected device
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

        case HCD_PORT_EVENT_SUDDEN_DISCONN:
            ESP_LOGW(TAG, "line %u HCD_PORT_EVENT_SUDDEN_DISCONN", __LINE__);
            break;

        case HCD_PORT_EVENT_NONE:
            break;

        default:
            ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, actual_evt);
            break;
    }
}

static esp_err_t _usb_port_event_wait(hcd_port_handle_t expected_port_hdl,
                                      hcd_port_event_t expected_event, TickType_t xTicksToWait)
{
    //Wait for port callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_port_get_context(expected_port_hdl);
    assert(queue_hdl != NULL);
    _uvc_event_msg_t evt_msg;
    xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);

    //Check the contents of that event message
    if (PORT_EVENT != evt_msg._type || expected_port_hdl != evt_msg._handle.port_hdl
            || expected_event != evt_msg._event.port_event) {
        ESP_LOGE(TAG, "unexpected event: type=%d, port_hanle=%p, event=%d", evt_msg._type, evt_msg._handle.port_hdl, evt_msg._event.port_event);
        ESP_LOGI(TAG, "but expected: type=%d, port_hanle=%p, event=%d", PORT_EVENT, expected_port_hdl, expected_event);
        return ESP_FAIL;
    }

    return ESP_OK;
}


static void _hcd_force_conn_state(bool connected, TickType_t delay_ticks)
{
    vTaskDelay(delay_ticks);
    usb_wrap_dev_t *wrap = &USB_WRAP;
    if (connected) {
        //Swap back to internal PHY that is connected to a device
        wrap->otg_conf.phy_sel = 0;
    } else {
        //Set external PHY input signals to fixed voltage levels mimicking a disconnected state
        esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ZERO_INPUT, USB_EXTPHY_VP_IDX, false);
        esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ZERO_INPUT, USB_EXTPHY_VM_IDX, false);
        esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ONE_INPUT, USB_EXTPHY_RCV_IDX, false);
        //Swap to the external PHY
        wrap->otg_conf.phy_sel = 1;
    }
}

static hcd_port_handle_t _usb_port_init(void *context, void *callback_arg)
{
    UVC_CHECK(context != NULL && callback_arg != NULL, "invalid args", NULL);
    esp_err_t ret = ESP_OK;
    hcd_port_handle_t port_hdl = NULL;

    hcd_config_t hcd_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL2,
    };
    ret = hcd_install(&hcd_config);
    UVC_CHECK(ESP_OK == ret, "HCD Install failed", NULL);

    hcd_port_config_t port_cfg = {
        .callback = _usb_port_callback,
        .callback_arg = callback_arg,
        .context = context,
    };

    ret = hcd_port_init(USB_PORT_NUM, &port_cfg, &port_hdl);
    UVC_CHECK_GOTO(ESP_OK == ret, "HCD Port init failed", port_init_err);

    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_ON);
    UVC_CHECK_GOTO(ESP_OK == ret, "Port Set POWER_ON failed", port_power_err);
    ret = _usb_port_event_wait(port_hdl, HCD_PORT_EVENT_CONNECTION, portMAX_DELAY);
    _usb_port_event_process(port_hdl, HCD_PORT_EVENT_CONNECTION);
    UVC_CHECK_GOTO(ESP_OK == ret, "Port POWER_ON failed", port_power_err);

    // Reset port and get speed
    ESP_LOGI(TAG, "Resetting Port");
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_RESET);
    UVC_CHECK_GOTO(ESP_OK == ret, "Port Set RESET failed", port_reset_err);
    //Adjust FIFO sizes to rx MAX
    ret = hcd_port_set_fifo_bias(port_hdl, HCD_PORT_FIFO_BIAS_RX);
    UVC_CHECK(ESP_OK == ret, "Port Set fifo failed", NULL);
    ESP_LOGI(TAG, "Port=%p init succeed", port_hdl);
    return port_hdl;

port_reset_err:
    hcd_port_command(port_hdl, HCD_PORT_CMD_DISABLE);
    _hcd_force_conn_state(false, pdMS_TO_TICKS(100));
    _usb_port_event_wait(port_hdl, HCD_PORT_EVENT_DISCONNECTION, portMAX_DELAY);
    _usb_port_event_process(port_hdl, HCD_PORT_EVENT_DISCONNECTION);
    hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_OFF);
port_power_err:
    hcd_port_deinit(port_hdl);
port_init_err:
    hcd_uninstall();
    return NULL;
}

static esp_err_t _usb_port_deinit(hcd_port_handle_t port_hdl)
{
    esp_err_t ret;
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_DISABLE);
    UVC_CHECK(ESP_OK == ret, "port disable failed", ESP_FAIL);
    ESP_LOGW(TAG, "Waiting for USB Hardware disconnection\n");
    _hcd_force_conn_state(false, pdMS_TO_TICKS(100));
    ret = _usb_port_event_wait(port_hdl, HCD_PORT_EVENT_DISCONNECTION, portMAX_DELAY);
    _usb_port_event_process(port_hdl, HCD_PORT_EVENT_DISCONNECTION);
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_OFF);
    UVC_CHECK(ESP_OK == ret, "Port POWER_OFF failed", ESP_FAIL);
    ret = hcd_port_deinit(port_hdl);
    ESP_ERROR_CHECK(ret);
    UVC_CHECK(ESP_OK == ret, "port deinit failed", ESP_FAIL);
    ret = hcd_uninstall();
    UVC_CHECK(ESP_OK == ret, "hcd uninstall failed", ESP_FAIL);
    return ret;
}

static esp_err_t _usb_port_get_speed(hcd_port_handle_t port_hdl, usb_speed_t *port_speed)
{
    UVC_CHECK(port_hdl != NULL && port_speed != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    esp_err_t ret = hcd_port_get_speed(port_hdl, port_speed);
    UVC_CHECK(ESP_OK == ret, "port speed get failed", ESP_FAIL);
    ESP_LOGI(TAG, "Port speed = %d", *port_speed);
    return ret;
}

/*------------------------------------------------ USB URB Code ----------------------------------------------------*/

urb_t *_usb_urb_alloc(int num_isoc_packets, size_t packet_data_buffer_size, void *context)
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

/*------------------------------------------------ USB Pipe Code ----------------------------------------------------*/

static bool _usb_pipe_callback(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event, void *user_arg, bool in_isr)
{
    QueueHandle_t usb_event_queue = (QueueHandle_t)user_arg;
    _uvc_event_msg_t event_msg = {
        ._type = PIPE_EVENT,
        ._handle.pipe_handle = pipe_handle,
        ._event.pipe_event = pipe_event,
    };

    if (in_isr) {
        BaseType_t xTaskWoken = pdFALSE;
        xQueueSendFromISR(usb_event_queue, &event_msg, &xTaskWoken);
        TRIGGER_PIPE_EVENT();
        return (xTaskWoken == pdTRUE);
    } else {
        xQueueSend(usb_event_queue, &event_msg, portMAX_DELAY);
        return false;
    }
}

static void _default_pipe_event_process(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event)
{
    assert(pipe_handle != NULL);

    switch (pipe_event) {
        case HCD_PIPE_EVENT_URB_DONE:
            break;

        case HCD_PIPE_EVENT_INVALID:
            ESP_LOGW(TAG, "line %u Pipe: default HCD_PIPE_EVENT_INVALID", __LINE__);
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
            break;

        default:
            ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, pipe_event);
            break;
    }
}

static void _stream_pipe_event_process(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event, bool if_enqueue)
{
    assert(pipe_handle != NULL);
    bool enqueue_flag = if_enqueue;

    switch (pipe_event) {
        case HCD_PIPE_EVENT_URB_DONE: {
            urb_t *urb_done = hcd_urb_dequeue(pipe_handle);

            if (urb_done == NULL) {
                enqueue_flag = false;
                break;
            }

            TRIGGER_URB_DEQUEUE();
            _uvc_stream_handle_t *strmh = (_uvc_stream_handle_t *)(urb_done->transfer.context);
            size_t index = 0;

            for (size_t i = 0; i < urb_done->transfer.num_isoc_packets; i++) {
                if (urb_done->transfer.isoc_packet_desc[i].status != USB_TRANSFER_STATUS_COMPLETED) {
                    ESP_LOGV(TAG, "line:%u bad iso transct status %d", __LINE__, urb_done->transfer.isoc_packet_desc[i].status);
                    continue;
                }

                uint8_t *simplebuffer = urb_done->transfer.data_buffer + (index * s_uvc_dev.isoc_ep_mps);
                ++index;
                _uvc_process_payload(strmh, simplebuffer, (urb_done->transfer.isoc_packet_desc[i].actual_num_bytes));
            }

            if (enqueue_flag) {
                esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_done);

                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "line:%u URB ENQUEUE FAILED %s", __LINE__, esp_err_to_name(ret));
                }

                TRIGGER_URB_ENQUEUE();
            }

            ESP_LOGV(TAG, "line %u Pipe: iso HCD_PIPE_EVENT_URB_DONE", __LINE__);
            break;
        }

        case HCD_PIPE_EVENT_INVALID:
            ESP_LOGW(TAG, "line %u Pipe: iso HCD_PIPE_EVENT_INVALID", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_XFER:
            ESP_LOGW(TAG, "line %u Pipe: iso HCD_PIPE_EVENT_ERROR_XFER", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
            ESP_LOGW(TAG, "line %u Pipe: iso HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_OVERFLOW:
            ESP_LOGW(TAG, "line %u Pipe: iso HCD_PIPE_EVENT_ERROR_OVERFLOW", __LINE__);
            break;

        case HCD_PIPE_EVENT_ERROR_STALL:
            ESP_LOGW(TAG, "line %u Pipe: iso HCD_PIPE_EVENT_ERROR_STALL", __LINE__);
            break;

        case HCD_PIPE_EVENT_NONE:
            ESP_LOGW(TAG, "line %u Pipe: iso HCD_PIPE_EVENT_NONE", __LINE__);
            break;

        default:
            ESP_LOGE(TAG, "line %u invalid HCD_PORT_EVENT%d", __LINE__, pipe_event);
            break;
    }
}

static hcd_pipe_handle_t _usb_pipe_init(hcd_port_handle_t port_hdl, usb_desc_ep_t *ep_desc, uint8_t dev_addr, usb_speed_t dev_speed, void *context, void *callback_arg)
{
    UVC_CHECK(port_hdl != NULL, "invalid args", NULL);
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
    UVC_CHECK(ESP_OK == ret, "pipe alloc failed", NULL);
    return pipe_handle;
}

static esp_err_t _usb_pipe_deinit(hcd_pipe_handle_t pipe_hdl)
{
    UVC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    return hcd_pipe_free(pipe_hdl);
}

static esp_err_t _default_pipe_event_wait_until(hcd_pipe_handle_t expected_pipe_hdl,
        hcd_pipe_event_t expected_event, TickType_t xTicksToWait)
{
    //Wait for pipe callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_pipe_get_context(expected_pipe_hdl);
    _uvc_event_msg_t evt_msg;
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    BaseType_t queue_ret = errQUEUE_EMPTY;

    do {
        queue_ret = xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            break;
        }

        if (PORT_EVENT == evt_msg._type) {
            _usb_port_event_process(evt_msg._handle.port_hdl, evt_msg._event.port_event);
            ret = ESP_ERR_NOT_FOUND;
        } else {
            if (expected_pipe_hdl != evt_msg._handle.pipe_handle) {
                _stream_pipe_event_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event, true);
                ESP_LOGW(TAG, "Got unexpected pipe:%p", evt_msg._handle.pipe_handle);
                ret = ESP_ERR_NOT_FOUND;
            } else if (expected_event != evt_msg._event.pipe_event) {
                _default_pipe_event_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
                ret = ESP_ERR_INVALID_RESPONSE;
            } else {
                ESP_LOGD(TAG, "Got expected pipe_handle and event");
                ret = ESP_OK;
            }
        }
    } while (ret == ESP_ERR_NOT_FOUND);

    return ret;
}

static esp_err_t _stream_pipe_event_wait_until_num(hcd_pipe_handle_t expected_pipe_hdl,
        hcd_pipe_event_t expected_event, int event_num, TickType_t xTicksToWait, bool if_dequeue)
{
    //Wait for pipe callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_pipe_get_context(expected_pipe_hdl);
    _uvc_event_msg_t evt_msg;
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    BaseType_t queue_ret = errQUEUE_EMPTY;
    int rest_num = event_num;

    do {
        queue_ret = xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            break;
        }

        if (PORT_EVENT == evt_msg._type) {
            _usb_port_event_process(evt_msg._handle.port_hdl, evt_msg._event.port_event);
            ret = ESP_ERR_NOT_FOUND;
        } else {
            if (expected_pipe_hdl != evt_msg._handle.pipe_handle) {
                _default_pipe_event_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event);
                ESP_LOGW(TAG, "Got unexpected pipe:%p", evt_msg._handle.pipe_handle);
                ret = ESP_ERR_NOT_FOUND;
            } else if (expected_event != evt_msg._event.pipe_event) {
                _stream_pipe_event_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event, !if_dequeue);//check dequeue status
                ret = ESP_ERR_INVALID_RESPONSE;
            } else {
                rest_num--;
                _stream_pipe_event_process(evt_msg._handle.pipe_handle, evt_msg._event.pipe_event, !if_dequeue);//check dequeue status
                ESP_LOGD(TAG, "Got expected pipe_handle and event");
                ret = ESP_OK;
            }
        }
    } while (rest_num > 0);

    if (rest_num == 0) {
        /* code */
        ESP_LOGI(TAG, "%s succeed", __func__);
        ret = ESP_OK;
    }

    return ret;
}

/*------------------------------------------------ USB Control Process Code ----------------------------------------------------*/

static esp_err_t _usb_set_device_addr(hcd_pipe_handle_t pipe_handle, uint8_t dev_addr)
{
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_ctrl_req_t), NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    //STD: Set ADDR
    USB_CTRL_REQ_INIT_SET_ADDR((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer, dev_addr);
    urb_ctrl->transfer.num_bytes = 0; //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Addr = %u", dev_addr);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, portMAX_DELAY);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", free_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ret = hcd_pipe_update_dev_addr(pipe_handle, dev_addr);
    UVC_CHECK_GOTO(ESP_OK == ret, "default pipe update address failed", free_urb_);
    ret = hcd_pipe_update_mps(pipe_handle, USB_EP_CTRL_DEFAULT_MPS);
    UVC_CHECK_GOTO(ESP_OK == ret, "default pipe update MPS failed", free_urb_);
    ESP_LOGI(TAG, "Set Device Addr Done");

free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _usb_set_device_config(hcd_pipe_handle_t pipe_handle, uint16_t configuration)
{
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(configuration != 0, "configuration can't be 0", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_ctrl_req_t), NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    USB_CTRL_REQ_INIT_SET_CONFIG((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer, configuration);
    urb_ctrl->transfer.num_bytes = 0; //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Configuration = %u", configuration);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, portMAX_DELAY);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", free_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Configuration Done");

free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _usb_set_device_interface(hcd_pipe_handle_t pipe_handle, uint16_t interface, uint16_t interface_alt)
{
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(interface != 0, "interface can't be 0", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_ctrl_req_t), NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    USB_CTRL_REQ_INIT_SET_INTERFACE((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer, interface, interface_alt);
    urb_ctrl->transfer.num_bytes = 0;
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Interface = %u, Alt = %u", interface, interface_alt);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, portMAX_DELAY);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", free_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Interface Done");

free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _uvc_vs_commit_control(hcd_pipe_handle_t pipe_handle, uvc_stream_ctrl_t *ctrl_set, uvc_stream_ctrl_t *ctrl_probed)
{
    assert(ctrl_set != NULL);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_ctrl_req_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    urb_t *urb_done = NULL;

    ESP_LOGI(TAG, "SET_CUR Probe");
    USB_CTRL_UVC_PROBE_SET_REQ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer);
    _uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_ctrl_req_t)), ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_set);
    urb_ctrl->transfer.num_bytes = ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer)->wLength;
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, portMAX_DELAY);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", free_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "SET_CUR Probe Done");

    ESP_LOGI(TAG, "GET_CUR Probe");
    USB_CTRL_UVC_PROBE_GET_REQ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer);
    urb_ctrl->transfer.num_bytes = USB_EP_CTRL_DEFAULT_MPS; //IN should be integer multiple of MPS
    ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, portMAX_DELAY);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", free_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    UVC_CHECK_GOTO((urb_done->transfer.actual_num_bytes >= ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer)->wLength), "urb status: data overflow", free_urb_);
    _buf_to_uvc_stream_ctrl((urb_done->transfer.data_buffer + sizeof(usb_ctrl_req_t)), ((usb_ctrl_req_t *)urb_done->transfer.data_buffer)->wLength, ctrl_probed);
    _uvc_stream_ctrl_printf(stdout, ctrl_probed);
    ESP_LOGI(TAG, "GET_CUR Probe Done, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);

    ESP_LOGI(TAG, "SET_CUR COMMIT");
    USB_CTRL_UVC_COMMIT_REQ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer);
    _uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_ctrl_req_t)), ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_probed);
    urb_ctrl->transfer.num_bytes = ((usb_ctrl_req_t *)urb_ctrl->transfer.data_buffer)->wLength;
    ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, portMAX_DELAY);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", free_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "SET_CUR COMMIT Done");

free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

/***************************************************LibUVC Implements****************************************/
/**
 * @brief Swap the working buffer with the presented buffer and notify consumers
 */
static inline void _uvc_swap_buffers(_uvc_stream_handle_t *strmh)
{
    uint8_t *tmp_buf;

    xSemaphoreTake(strmh->cb_mutex, portMAX_DELAY);
    /* swap the buffers */
    tmp_buf = strmh->holdbuf;
    strmh->hold_bytes = strmh->got_bytes;
    strmh->holdbuf = strmh->outbuf;
    strmh->outbuf = tmp_buf;
    strmh->hold_last_scr = strmh->last_scr;
    strmh->hold_pts = strmh->pts;
    strmh->hold_seq = strmh->seq;

    xTaskNotifyGive(s_samlpe_processing_task_hdl);
    xSemaphoreGive(strmh->cb_mutex);

    strmh->seq++;
    strmh->got_bytes = 0;
    strmh->last_scr = 0;
    strmh->pts = 0;
}

/**
 * @brief Clean buffer without swap
 */
static inline void _uvc_drop_buffers(_uvc_stream_handle_t *strmh)
{
    strmh->got_bytes = 0;
    strmh->last_scr = 0;
    strmh->pts = 0;
}

/**
 * @brief Populate the fields of a frame to be handed to user code
 * must be called with stream cb lock held!
 */
void _uvc_populate_frame(_uvc_stream_handle_t *strmh)
{
    if (strmh->hold_bytes > s_uvc_dev.frame_buffer_size) {
        ESP_LOGW(TAG, "Frame Buffer Overflow, framesize = %u", strmh->hold_bytes);
        return;
    }

    uvc_frame_t *frame = &strmh->frame;
    frame->frame_format = strmh->frame_format;
    frame->width = s_uvc_dev.frame_width;
    frame->height = s_uvc_dev.frame_height;

    frame->step = 0;
    frame->sequence = strmh->hold_seq;
    frame->capture_time_finished = strmh->capture_time_finished;
    frame->data_bytes = strmh->hold_bytes;
    memcpy(frame->data, strmh->holdbuf, frame->data_bytes);
}

/**
 * @brief Process each payload of isoc transfer
 *
 * @param strmh stream handle
 * @param payload payload buffer
 * @param payload_len payload buffer length
 */
static inline void _uvc_process_payload(_uvc_stream_handle_t *strmh, uint8_t *payload, size_t payload_len)
{
    size_t header_len;
    uint8_t header_info;
    size_t data_len;

    /* ignore empty payload transfers */
    if (payload_len == 0) {
        return;
    }

    /********************* processing header *******************/
    header_len = payload[0];

    if (header_len != 12 || header_len > payload_len) {
        ESP_LOGW(TAG, "bogus packet: actual_len=%zd, header_len=%zd\n", payload_len, header_len);
        return;
    }

    data_len = payload_len - header_len;
    /* checking the end-of-header */
    size_t variable_offset = 2;
    header_info = payload[1];

    /* ERR bit defined in Stream Header*/
    if (header_info & 0x40) {
        ESP_LOGW(TAG, "bad packet: error bit set");
        return;
    }

    if (strmh->fid != (header_info & 1) && strmh->got_bytes != 0) {
        /* The frame ID bit was flipped, but we have image data sitting
            around from prior transfers. This means the camera didn't send
            an EOF for the last transfer of the previous frame. */
        ESP_LOGW(TAG, "SWAP NO EOF %d", strmh->got_bytes);
        /* clean whole no-flipped buffer */
        _uvc_drop_buffers(strmh);
    }

    strmh->fid = header_info & 1;

    if (header_info & (1 << 2)) {
        strmh->pts = DW_TO_INT(payload + variable_offset);
        variable_offset += 4;
    }

    if (header_info & (1 << 3)) {
        /** @todo read the SOF token counter */
        strmh->last_scr = DW_TO_INT(payload + variable_offset);
        variable_offset += 6;
    }

    ESP_LOGV(TAG, "len=%u info=%u, pts = %u , last_scr = %u", header_len, header_info, strmh->pts, strmh->last_scr);

    /********************* processing data *****************/
    if (data_len >= 1) {
        if (strmh->got_bytes + data_len > s_uvc_dev.xfer_buffer_size) {
            /* This means transfer buffer Not enough for whole frame, just drop whole buffer here.
               Please increase buffer size to handle big frame*/
            ESP_LOGW(TAG, "Transfer buffer overflow, gotdata=%u B", strmh->got_bytes + data_len);
            _uvc_drop_buffers(strmh);
            return;
        } else {
            memcpy(strmh->outbuf + strmh->got_bytes, payload + header_len, data_len);
        }

        strmh->got_bytes += data_len;

        if (header_info & (1 << 1)) {
            /* The EOF bit is set, so publish the complete frame */
            _uvc_swap_buffers(strmh);
        }
    }
}

/**
 * @brief open a video stream (only one can be created)
 *
 * @param devh device handle
 * @param strmhp return stream handle if suceed
 * @param config ctrl configs
 * @return uvc_error_t
 */
uvc_error_t uvc_stream_open_ctrl(uvc_device_handle_t *devh, _uvc_stream_handle_t **strmhp, uvc_stream_ctrl_t *ctrl)
{
    /* TODO:Chosen frame and format descriptors, Hardcode here*/
    _uvc_stream_handle_t *strmh = NULL;
    uvc_error_t ret;
    strmh = calloc(1, sizeof(*strmh));

    if (!strmh) {
        ret = UVC_ERROR_NO_MEM;
        goto fail_;
    }

    strmh->devh = *devh;
    strmh->frame.library_owns_data = 1;
    strmh->cur_ctrl = *ctrl;
    strmh->running = 0;
    /** @todo take only what we need */
    strmh->outbuf = s_uvc_dev.xfer_buffer_a;
    strmh->holdbuf = s_uvc_dev.xfer_buffer_b;
    strmh->frame.data = s_uvc_dev.frame_buffer;

    strmh->cb_mutex = xSemaphoreCreateMutex();

    if (strmh->cb_mutex == NULL) {
        ESP_LOGE(TAG, "line-%u Mutex create faild", __LINE__);
        ret = UVC_ERROR_NO_MEM;
        goto fail_;
    }

    *strmhp = strmh;
    return UVC_SUCCESS;

fail_:

    if (strmh) {
        free(strmh);
    }

    return ret;
}

static void _sample_processing_task(void *arg);

/** Begin streaming video from the stream into the callback function.
 * @ingroup streaming
 *
 * @param strmh UVC stream
 * @param cb   User callback function. See {uvc_frame_callback_t} for restrictions.
 * @param flags Stream setup flags, currently undefined. Set this to zero. The lower bit
 * is reserved for backward compatibility.
 */
uvc_error_t uvc_stream_start(_uvc_stream_handle_t *strmh, uvc_frame_callback_t *cb, void *user_ptr, uint8_t flags)
{
    if (strmh->running) {
        ESP_LOGW(TAG, "line:%u UVC_ERROR_BUSY", __LINE__);
        return UVC_ERROR_BUSY;
    }

    strmh->running = 1;
    strmh->seq = 1;
    strmh->fid = 0;
    strmh->pts = 0;
    strmh->last_scr = 0;
    strmh->frame_format = UVC_FRAME_FORMAT_MJPEG;
    strmh->user_cb = cb;
    strmh->user_ptr = user_ptr;

    if (cb) {
        xTaskCreatePinnedToCore(_sample_processing_task, SAMPLE_PROC_TASK_NAME, SAMPLE_PROC_TASK_STACK_SIZE, (void *)strmh,
                                SAMPLE_PROC_TASK_PRIORITY, &s_samlpe_processing_task_hdl, SAMPLE_PROC_TASK_CORE);
        ESP_LOGI(TAG, "Sample processing task started");
        strmh->taskh = s_samlpe_processing_task_hdl;
    }

    return UVC_SUCCESS;
}

/** @brief Stop stream.
 * @ingroup streaming
 *
 * Stops stream, ends threads and cancels pollers
 *
 * @param devh UVC device
 */
uvc_error_t uvc_stream_stop(_uvc_stream_handle_t *strmh)
{
    if (strmh->running == 0) {
        ESP_LOGW(TAG, "line:%u UVC_ERROR_BUSY", __LINE__);
        return UVC_ERROR_INVALID_PARAM;
    }
    strmh->running = 0;
    _uvc_device_t *device_handle = (_uvc_device_t *)(strmh->devh);
    xTaskNotifyGive(strmh->taskh);
    xSemaphoreGive(strmh->cb_mutex);
    xEventGroupWaitBits(device_handle->event_group, SAMPLE_PROC_STOP_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "Sample processing task stoped");
    return UVC_SUCCESS;
}

/** @brief Close stream.
 * @ingroup streaming
 *
 * Closes stream, frees handle and all streaming resources.
 *
 * @param strmh UVC stream handle
 */
void uvc_stream_close(_uvc_stream_handle_t *strmh)
{
    vSemaphoreDelete(strmh->cb_mutex);
    free(strmh);
    return;
}

/****************************************************** Task Code ******************************************************/

static void _usb_processing_task(void *arg)
{
    UVC_CHECK_GOTO(arg != NULL, "Task arg can't be NULL", delete_task_);
    _uvc_device_t *uvc_dev = calloc(1, sizeof(_uvc_device_t));
    UVC_CHECK_GOTO(uvc_dev != NULL, "calloc device failed", delete_task_);
    memcpy(uvc_dev, arg, sizeof(_uvc_device_t));

    esp_err_t ret = ESP_OK;
    usb_speed_t port_speed = 0;
    hcd_port_handle_t port_hdl = NULL;
    _uvc_stream_handle_t *stream_hdl = NULL;
    _uvc_event_msg_t evt_msg = {};
    bool keep_running = true;

    //Waitting for camera ready
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    port_hdl = _usb_port_init((void *)uvc_dev->queue_hdl, (void *)uvc_dev->queue_hdl);
    UVC_CHECK_GOTO(port_hdl != NULL, "USB Port init failed", delete_dev_);

    ret = _usb_port_get_speed(port_hdl, &port_speed);
    UVC_CHECK_GOTO(ESP_OK == ret, "USB Port get speed failed", delete_dev_);

    // Initialize default pipe
    hcd_pipe_handle_t dflt_pipe_hdl = _usb_pipe_init(port_hdl, NULL, 0, port_speed,
                                      (void *)uvc_dev->queue_hdl, (void *)uvc_dev->queue_hdl);
    UVC_CHECK_GOTO(dflt_pipe_hdl != NULL, "default pipe create failed", deinit_usb_port_);

    usb_desc_ep_t isoc_ep_desc = {
        .bLength = USB_DESC_EP_SIZE,
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
        .bEndpointAddress = uvc_dev->isoc_ep_addr,
        .bmAttributes = USB_BM_ATTRIBUTES_XFER_ISOC,
        .wMaxPacketSize = uvc_dev->isoc_ep_mps,
        .bInterval = 1,
    };
    hcd_pipe_handle_t stream_pipe_hdl = _usb_pipe_init(port_hdl, &isoc_ep_desc, uvc_dev->dev_addr,
                                        port_speed, (void *)uvc_dev->queue_hdl, (void *)uvc_dev->queue_hdl);
    UVC_CHECK_GOTO(stream_pipe_hdl != NULL, "stream pipe create failed", deinit_dflt_pipe_);

    ret = _usb_set_device_addr(dflt_pipe_hdl, uvc_dev->dev_addr);
    UVC_CHECK_GOTO(ESP_OK == ret, "Set device address failed", deinit_stream_pipe_);
    ret = _usb_set_device_config(dflt_pipe_hdl, uvc_dev->configuration);
    UVC_CHECK_GOTO(ESP_OK == ret, "Set device configuration failed", deinit_stream_pipe_);
    ret = _uvc_vs_commit_control(dflt_pipe_hdl, &uvc_dev->ctrl_set, &uvc_dev->ctrl_probed);
    UVC_CHECK_GOTO(ESP_OK == ret, "UVC negotiate failed", deinit_stream_pipe_);
    ret = _usb_set_device_interface(dflt_pipe_hdl, uvc_dev->interface, uvc_dev->interface_alt);
    UVC_CHECK_GOTO(ESP_OK == ret, "Set device interface failed", deinit_stream_pipe_);

    uvc_stream_open_ctrl((uvc_device_handle_t *)(&uvc_dev), &stream_hdl, &uvc_dev->ctrl_probed);
    UVC_CHECK_GOTO(stream_hdl != NULL, "UVC open stream failed", deinit_stream_pipe_);

    uvc_stream_start(stream_hdl, uvc_dev->user_cb, uvc_dev->user_ptr, 0);
    //Enum done,start the periodic pipe task
    vTaskDelay(pdMS_TO_TICKS(50));

    urb_t *stream_urbs[NUM_ISOC_STREAM_URBS] = {NULL};

    for (int i = 0; i < NUM_ISOC_STREAM_URBS; i++) {
        stream_urbs[i] = _usb_urb_alloc(NUM_PACKETS_PER_URB_URB, uvc_dev->isoc_ep_mps, stream_hdl);
        assert(stream_urbs[i] != NULL);
        for (size_t j = 0; j < NUM_PACKETS_PER_URB_URB; j++) {
            //We need to initialize each individual isoc packet descriptor of the URB
            stream_urbs[i]->transfer.isoc_packet_desc[j].num_bytes = uvc_dev->isoc_ep_mps;
        }
    }

    ESP_LOGI(TAG, "Camera Start Streaming");

    for (int i = 0; i < NUM_ISOC_STREAM_URBS; i++) {
        ret = hcd_urb_enqueue(stream_pipe_hdl, stream_urbs[i]);
        assert(ESP_OK == ret);
    }

    xEventGroupSetBits(uvc_dev->event_group, USB_STREAM_INIT_DONE | USB_STREAM_RUNNING);
    xEventGroupClearBits(uvc_dev->event_group, USB_STREAM_SUSPEND_DONE | USB_STREAM_RESUME_DONE);

    while (keep_running) {
        if (xQueueReceive(uvc_dev->queue_hdl, &evt_msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (evt_msg._type) {
            case PORT_EVENT:
                _usb_port_event_process(port_hdl, evt_msg._event.port_event);
                break;

            case PIPE_EVENT:
                if (evt_msg._handle.pipe_handle == dflt_pipe_hdl) {
                    _default_pipe_event_process(dflt_pipe_hdl, evt_msg._event.pipe_event);
                } else if (evt_msg._handle.pipe_handle == stream_pipe_hdl) {
                    _stream_pipe_event_process(stream_pipe_hdl, evt_msg._event.pipe_event, true);
                }
                break;

            case USER_EVENT:
                if (evt_msg._event.user_cmd == STREAM_SUSPEND) {
                    _stream_pipe_event_wait_until_num(stream_pipe_hdl, HCD_PIPE_EVENT_URB_DONE, NUM_ISOC_STREAM_URBS, portMAX_DELAY, true);
                    _usb_set_device_interface(dflt_pipe_hdl, uvc_dev->interface, 0);
                    ESP_LOGD(TAG, "Stream suspended!");
                    xEventGroupSetBits(uvc_dev->event_group, USB_STREAM_SUSPEND_DONE);
                    xEventGroupClearBits(uvc_dev->event_group, USB_STREAM_RUNNING);
                    break;
                } else if (evt_msg._event.user_cmd == STREAM_RESUME) {
                    ret = _uvc_vs_commit_control(dflt_pipe_hdl, &uvc_dev->ctrl_set, &uvc_dev->ctrl_probed);
                    assert(ESP_OK == ret);
                    _usb_set_device_interface(dflt_pipe_hdl, uvc_dev->interface, uvc_dev->interface_alt);

                    for (int i = 0; i < NUM_ISOC_STREAM_URBS; i++) {
                        ret = hcd_urb_enqueue(stream_pipe_hdl, stream_urbs[i]);
                        assert(ESP_OK == ret);
                    }

                    xEventGroupSetBits(uvc_dev->event_group, USB_STREAM_RESUME_DONE | USB_STREAM_RUNNING);
                    ESP_LOGD(TAG, "Streaming resumed");
                    break;
                } else if (evt_msg._event.user_cmd == STREAM_STOP) {
                    keep_running = false;
                    xEventGroupClearBits(uvc_dev->event_group, USB_STREAM_INIT_DONE | USB_STREAM_RUNNING);
                    break;
                }

                ESP_LOGW(TAG, "Undefined User Event");
                break;

            default:
                break;
        }
    }

    for (int i = 0; i < NUM_ISOC_STREAM_URBS; i++) {
        _usb_urb_free(stream_urbs[i]);
    }

    uvc_stream_stop(stream_hdl);
    uvc_stream_close(stream_hdl);
deinit_stream_pipe_:
    _usb_pipe_deinit(stream_pipe_hdl);
deinit_dflt_pipe_:
    _usb_pipe_deinit(dflt_pipe_hdl);
deinit_usb_port_:
    _usb_port_deinit(port_hdl);
delete_dev_:
    xEventGroupSetBits(uvc_dev->event_group, USB_STREAM_STOP_DONE);
    free(uvc_dev);
delete_task_:
    ESP_LOGW(TAG, "_usb_processing_task deleted");
    ESP_LOGW(TAG, "_usb_processing_task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}

/*populate frame then call user callback*/
static void _sample_processing_task(void *arg)
{
    assert(arg != NULL);
    _uvc_stream_handle_t *strmh = (_uvc_stream_handle_t *)(arg);
    _uvc_device_t *device_handle = (_uvc_device_t *)(strmh->devh);
    uint32_t last_seq = 0;

    xEventGroupSetBits(device_handle->event_group, SAMPLE_PROC_RUNNING);
    xEventGroupClearBits(device_handle->event_group, SAMPLE_PROC_STOP_DONE);

    do {
        xSemaphoreTake(strmh->cb_mutex, portMAX_DELAY);

        while (strmh->running && last_seq == strmh->hold_seq) {
            //pthread_cond_wait(&strmh->cb_cond, &strmh->cb_mutex);
            xSemaphoreGive(strmh->cb_mutex);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ESP_LOGV(TAG, "GOT LENGTH %d", strmh->hold_bytes);
            xSemaphoreTake(strmh->cb_mutex, portMAX_DELAY);
        }

        TRIGGER_NEW_FRAME();

        if (!strmh->running) {
            xSemaphoreGive(strmh->cb_mutex);
            ESP_LOGW(TAG, "NO Stream Running");
            break;
        }

        last_seq = strmh->hold_seq;
        _uvc_populate_frame(strmh);
        //user callback for decode and display
        xSemaphoreGive(strmh->cb_mutex);
        strmh->user_cb(&strmh->frame, strmh->user_ptr);
        /* code */
    } while (1);

    xEventGroupClearBits(device_handle->event_group, SAMPLE_PROC_RUNNING);
    xEventGroupSetBits(device_handle->event_group, SAMPLE_PROC_STOP_DONE);

    ESP_LOGW(TAG, "_sample_processing_task deleted");
    ESP_LOGW(TAG, "_sample_processing_task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}

/***************************************************** Public API *********************************************************************/
esp_err_t uvc_streaming_config(const uvc_config_t *config)
{
    UVC_CHECK(config != NULL, "config can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->dev_speed == USB_SPEED_FULL,
              "Only Support Full Speed, 12 Mbps", ESP_ERR_INVALID_ARG);
    UVC_CHECK((config->frame_interval >= FRAME_MIN_INTERVAL && config->frame_interval <= FRAME_MAX_INTERVAL),
              "frame_interval Support 333333~2000000 ns", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->isoc_ep_mps <= USB_EP_ISOC_MAX_MPS,
              "MPS Must < USB_EP_ISOC_MAX_MPS", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_buffer_size != 0, "frame_buffer_size can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_size != 0, "xfer_buffer_size can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_a != NULL, "xfer_buffer_a can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_b != NULL, "xfer_buffer_b can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_buffer != NULL, "frame_buffer can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->interface != 0, "Interface num must > 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->configuration != 0, "Configuration num must > 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->interface_alt != 0, "Interface alt num must > 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->isoc_ep_addr & 0x80, "Endpoint direction must IN", ESP_ERR_INVALID_ARG);

    s_uvc_dev.dev_speed = config->dev_speed;
    s_uvc_dev.dev_addr = USB_DEVICE_ADDR;
    s_uvc_dev.configuration = config->configuration;
    s_uvc_dev.interface = config->interface;
    s_uvc_dev.interface_alt = config->interface_alt;
    s_uvc_dev.isoc_ep_addr = config->isoc_ep_addr;
    s_uvc_dev.isoc_ep_mps = config->isoc_ep_mps;
    s_uvc_dev.xfer_buffer_size = config->xfer_buffer_size;
    s_uvc_dev.xfer_buffer_a = config->xfer_buffer_a;
    s_uvc_dev.xfer_buffer_b = config->xfer_buffer_b;
    s_uvc_dev.frame_buffer_size = config->frame_buffer_size;
    s_uvc_dev.frame_buffer = config->frame_buffer;

    s_uvc_dev.frame_width = config->frame_width;
    s_uvc_dev.frame_height = config->frame_height;

    s_uvc_dev.ctrl_set = s_uvc_stream_ctrl_default;
    s_uvc_dev.ctrl_set.bFormatIndex = config->format_index;
    s_uvc_dev.ctrl_set.bFrameIndex = config->frame_index;
    s_uvc_dev.ctrl_set.dwFrameInterval = config->frame_interval;
    s_uvc_dev.ctrl_set.dwMaxVideoFrameSize = config->frame_buffer_size;
    s_uvc_dev.ctrl_set.dwMaxPayloadTransferSize = config->isoc_ep_mps;

    TRIGGER_INIT();

    s_uvc_dev.active = true;

    ESP_LOGI(TAG, "UVC Streaming Config Succeed");

    return ESP_OK;
}

esp_err_t uvc_streaming_start(uvc_frame_callback_t *cb, void *user_ptr)
{
    UVC_CHECK(s_uvc_dev.active == true, "streaming not configured", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_uvc_dev.user_cb == NULL, "only one streaming supported", ESP_ERR_INVALID_STATE);
    UVC_CHECK(cb != NULL, "frame callback can't be NULL", ESP_ERR_INVALID_ARG);

    s_uvc_dev.user_cb = cb;
    s_uvc_dev.user_ptr = user_ptr;
    s_uvc_dev.event_group = xEventGroupCreate();
    UVC_CHECK(s_uvc_dev.event_group  != NULL, "Create event group failed", ESP_FAIL);
    s_uvc_dev.queue_hdl = xQueueCreate(UVC_EVENT_QUEUE_LEN, sizeof(_uvc_event_msg_t));
    UVC_CHECK(s_uvc_dev.queue_hdl != NULL, "Create event queue failed", ESP_FAIL);

    xTaskCreatePinnedToCore(_usb_processing_task, USB_PROC_TASK_NAME, USB_PROC_TASK_STACK_SIZE, (void *)(&s_uvc_dev),
                            USB_PROC_TASK_PRIORITY, &s_usb_processing_task_hdl, USB_PROC_TASK_CORE);

    vTaskDelay(50 / portTICK_PERIOD_MS); //wait for usb camera ready
    //Start the port task
    xTaskNotifyGive(s_usb_processing_task_hdl);
    ESP_LOGI(TAG, "UVC Streaming Starting");
    return ESP_OK;
}

esp_err_t uvc_streaming_suspend(void)
{
    if (!(xEventGroupGetBits(s_uvc_dev.event_group) & USB_STREAM_INIT_DONE)) {
        ESP_LOGW(TAG, "UVC Streaming not started");
        return ESP_ERR_INVALID_STATE;
    }

    if (!(xEventGroupGetBits(s_uvc_dev.event_group) & USB_STREAM_RUNNING)) {
        ESP_LOGW(TAG, "UVC Streaming Not runing");
        return ESP_OK;
    }

    _uvc_event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._handle.port_hdl = NULL,
        ._event.user_cmd = STREAM_SUSPEND,
    };

    xQueueSend(s_uvc_dev.queue_hdl, &event_msg, portMAX_DELAY);
    xEventGroupWaitBits(s_uvc_dev.event_group, USB_STREAM_SUSPEND_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
    ESP_LOGW(TAG, "UVC Streaming Suspend Done");
    return ESP_OK;
}

esp_err_t uvc_streaming_resume(void)
{
    if (!(xEventGroupGetBits(s_uvc_dev.event_group) & USB_STREAM_INIT_DONE)) {
        ESP_LOGW(TAG, "UVC Streaming not started");
        return ESP_ERR_INVALID_STATE;
    }

    if (xEventGroupGetBits(s_uvc_dev.event_group) & USB_STREAM_RUNNING) {
        ESP_LOGW(TAG, "UVC Streaming is runing");
        return ESP_OK;
    }

    _uvc_event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._handle.port_hdl = NULL,
        ._event.user_cmd = STREAM_RESUME,
    };

    xQueueSend(s_uvc_dev.queue_hdl, &event_msg, portMAX_DELAY);
    xEventGroupWaitBits(s_uvc_dev.event_group, USB_STREAM_RESUME_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
    ESP_LOGW(TAG, "UVC Streaming Resume Done");
    return ESP_OK;
}

esp_err_t uvc_streaming_stop(void)
{
    if (!(xEventGroupGetBits(s_uvc_dev.event_group) & USB_STREAM_INIT_DONE)) {
        ESP_LOGW(TAG, "UVC Streaming not started");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = uvc_streaming_suspend();
    UVC_CHECK(ret == ESP_OK, "UVC Streaming Stop Failed", ret)

    _uvc_event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._handle.port_hdl = NULL,
        ._event.user_cmd = STREAM_STOP,
    };

    xQueueSend(s_uvc_dev.queue_hdl, &event_msg, portMAX_DELAY);
    xEventGroupWaitBits(s_uvc_dev.event_group, USB_STREAM_STOP_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
    vEventGroupDelete(s_uvc_dev.event_group);
    vQueueDelete(s_uvc_dev.queue_hdl);
    memset(&s_uvc_dev, 0, sizeof(_uvc_device_t));
    s_usb_processing_task_hdl = NULL;
    ESP_LOGW(TAG, "UVC Streaming Stop Done");
    return ESP_OK;
}

/***************************************************** Simulation API *********************************************************************/

#ifdef CONFIG_SOURCE_SIMULATE
#define INTERNAL_PIC_NUM 15
extern uint8_t _binary_0001_jpg_start;
extern uint8_t _binary_0001_jpg_end;
extern uint8_t _binary_0002_jpg_start;
extern uint8_t _binary_0002_jpg_end;
extern uint8_t _binary_0003_jpg_start;
extern uint8_t _binary_0003_jpg_end;
extern uint8_t _binary_0004_jpg_start;
extern uint8_t _binary_0004_jpg_end;
extern uint8_t _binary_0005_jpg_start;
extern uint8_t _binary_0005_jpg_end;
extern uint8_t _binary_0006_jpg_start;
extern uint8_t _binary_0006_jpg_end;
extern uint8_t _binary_0007_jpg_start;
extern uint8_t _binary_0007_jpg_end;
extern uint8_t _binary_0008_jpg_start;
extern uint8_t _binary_0008_jpg_end;
extern uint8_t _binary_0009_jpg_start;
extern uint8_t _binary_0009_jpg_end;
extern uint8_t _binary_0010_jpg_start;
extern uint8_t _binary_0010_jpg_end;
extern uint8_t _binary_0011_jpg_start;
extern uint8_t _binary_0011_jpg_end;
extern uint8_t _binary_0012_jpg_start;
extern uint8_t _binary_0012_jpg_end;
extern uint8_t _binary_0013_jpg_start;
extern uint8_t _binary_0013_jpg_end;
extern uint8_t _binary_0014_jpg_start;
extern uint8_t _binary_0014_jpg_end;
extern uint8_t _binary_0015_jpg_start;
extern uint8_t _binary_0015_jpg_end;

static TaskHandle_t simulate_task_hdl = NULL;

static void _sample_processing_simulate_task(void *arg)
{
    assert(arg != NULL);
    _uvc_stream_handle_t *strmh = (_uvc_stream_handle_t *)(arg);
    uint32_t last_seq = 0;

    uint8_t *p_jpeg = NULL;
    size_t pic_size[INTERNAL_PIC_NUM] = {
        &_binary_0001_jpg_end - &_binary_0001_jpg_start, &_binary_0002_jpg_end - &_binary_0002_jpg_start, &_binary_0003_jpg_end - &_binary_0003_jpg_start,
        &_binary_0004_jpg_end - &_binary_0004_jpg_start, &_binary_0005_jpg_end - &_binary_0005_jpg_start, &_binary_0006_jpg_end - &_binary_0006_jpg_start,
        &_binary_0007_jpg_end - &_binary_0007_jpg_start, &_binary_0008_jpg_end - &_binary_0008_jpg_start, &_binary_0009_jpg_end - &_binary_0009_jpg_start,
        &_binary_0010_jpg_end - &_binary_0010_jpg_start, &_binary_0011_jpg_end - &_binary_0011_jpg_start, &_binary_0012_jpg_end - &_binary_0012_jpg_start,
        &_binary_0013_jpg_end - &_binary_0013_jpg_start, &_binary_0014_jpg_end - &_binary_0014_jpg_start, &_binary_0015_jpg_end - &_binary_0015_jpg_start
    };
    uint8_t *pic_address[INTERNAL_PIC_NUM] = {
        &_binary_0001_jpg_start, &_binary_0002_jpg_start, &_binary_0003_jpg_start, &_binary_0004_jpg_start, &_binary_0005_jpg_start,
        &_binary_0006_jpg_start, &_binary_0007_jpg_start, &_binary_0008_jpg_start, &_binary_0009_jpg_start, &_binary_0010_jpg_start,
        &_binary_0011_jpg_start, &_binary_0012_jpg_start, &_binary_0013_jpg_start, &_binary_0014_jpg_start, &_binary_0015_jpg_start
    };

    uvc_frame_t *frame = &strmh->frame;

    frame->frame_format = UVC_FRAME_FORMAT_MJPEG;
    frame->width = s_uvc_dev.frame_width;
    frame->height = s_uvc_dev.frame_height;

    frame->sequence = 0;
    frame->data_bytes = 0;
    strmh->running = 1;

    do {
        frame->sequence = ++last_seq;
        int index = last_seq % 15;

        /* copy the image data from the hold buffer to the frame (unnecessary extra buf?) */
        if (frame->data_bytes < pic_size[index]) {
            frame->data = realloc(frame->data, pic_size[index]);

            if (frame->data == NULL) {
                ESP_LOGE(TAG, "line-%u No Enough Ram Reserved=%d, Want=%d ", __LINE__, esp_get_free_heap_size(), strmh->hold_bytes);
                assert(0);
            }
        }

        frame->data_bytes = pic_size[index];
        memcpy(frame->data, pic_address[index], pic_size[index]);
        vTaskDelay(66 / portTICK_PERIOD_MS); //15fps
        strmh->user_cb(&strmh->frame, strmh->user_ptr);
    } while (1);

    free(p_jpeg);
}

esp_err_t uvc_streaming_simulate_start(uvc_frame_callback_t *cb, void *user_ptr)
{
    if (cb == NULL) {
        ESP_LOGE(TAG, "line:%u cb can't be NULL", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }

    _uvc_stream_handle_t *strmh = calloc(1, sizeof(*strmh));

    if (!strmh) {
        ESP_LOGE(TAG, "line:%u NO MEMORY", __LINE__);
        return ESP_ERR_NO_MEM;
    }

    strmh->frame.library_owns_data = 1;
    strmh->running = 0;
    strmh->user_cb = cb;
    strmh->user_ptr = user_ptr;

    xTaskCreatePinnedToCore(_sample_processing_simulate_task, "simulate", 4096, (void *)(strmh), 2, &simulate_task_hdl, 0);
    ESP_LOGI(TAG, "Simulate stream Running");
    return ESP_OK;
}
#else
esp_err_t uvc_streaming_simulate_start(uvc_frame_callback_t *cb, void *user_ptr)
{
    (void)cb;
    (void)user_ptr;
    ESP_LOGW(TAG, "Simulate Stream NOT Enabled");
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
