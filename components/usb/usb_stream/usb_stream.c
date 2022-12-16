// Copyright 2022-2024 Espressif Systems (Shanghai) PTE LTD
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
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "esp_log.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0))
#include "hal/usb_dwc_hal.h"
#include "hal/usb_dwc_ll.h"
#else
#include "hal/usb_hal.h"
#include "hal/usbh_ll.h"
#endif
#include "hcd.h"
#include "usb/usb_types_stack.h"
#include "usb/usb_helpers.h"
#include "esp_private/usb_phy.h"
#include "usb_private.h"
#include "usb_stream.h"
#include "uvc_debug.h"

#define UVC_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define UVC_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

const char *TAG = "UVC_STREAM";

/**
 * @brief General params for UVC device
 *
 */
#define USB_PORT_NUM                         1                                       //Default port number
#define USB_CONFIG_NUM                       1                                       //Default configuration number
#define USB_DEVICE_ADDR                      1                                       //Default UVC device address
#define USB_ENUM_SHORT_DESC_REQ_LEN          8                                       //Number of bytes to request when getting a short descriptor (just enough to get bMaxPacketSize0 or wTotalLength)
#define USB_EP_CTRL_DEFAULT_MPS              64                                      //Default MPS(max payload size) of Endpoint 0
#define USB_EP_ISOC_MAX_MPS                  600                                     //Max MPS ESP32-S2/S3 can handle
#define USB_EP_BULK_FS_MPS                   64                                      //Default MPS of full speed bulk transfer
#define CTRL_TRANSFER_DATA_MAX_BYTES         CONFIG_CTRL_TRANSFER_DATA_MAX_BYTES     //Max data length assumed in control transfer
#define NUM_BULK_STREAM_URBS                 CONFIG_NUM_BULK_STREAM_URBS             //Number of bulk stream URBS created for continuous enqueue
#define NUM_BULK_BYTES_PER_URB               CONFIG_NUM_BULK_BYTES_PER_URB           //Required transfer bytes of each URB, check
#define NUM_ISOC_STREAM_URBS                 CONFIG_NUM_ISOC_STREAM_URBS             //Number of isochronous stream URBS created for continuous enqueue
#define NUM_PACKETS_PER_URB_URB              CONFIG_NUM_PACKETS_PER_URB_URB          //Number of packets in each isochronous stream URB
#define UVC_EVENT_QUEUE_LEN                  30                                      //UVC event queue length 
#define FRAME_MAX_INTERVAL                   2000000                                 //Specified in 100 ns units, General max frame interval (5 FPS)
#define FRAME_MIN_INTERVAL                   333333                                  //General min frame interval (30 FPS)
#define TIMEOUT_USB_CTRL_XFER_MS             4000                                    //Timeout for USB control transfer
#define TIMEOUT_USER_COMMAND                 5000                                    //Timeout for USER COMMAND control transfer
#define NUM_ISOC_SPK_URBS                    3                                       //Number of isochronous stream URBS created for continuous enqueue
#define NUM_ISOC_MIC_URBS                    3                                       //Number of isochronous stream URBS created for continuous enqueue
#define UAC_VOLUME_LEVEL_DEFAULT             50                                      //50%
#define UAC_MIC_CB_MIN_MS_DEFAULT            20                                      //20MS
#define UAC_SPK_ST_MAX_MS_DEFAULT            20                                      //20MS
#define UAC_MIC_PACKET_COMPENSATION          1                                       //padding data if mic packet loss

/**
 * @brief Task for USB I/O request and payload processing,
 * can not be blocked, higher task priority is suggested.
 *
 */
#define USB_PROC_TASK_NAME "usb_proc"
#define USB_PROC_TASK_PRIORITY CONFIG_USB_PROC_TASK_PRIORITY
#define USB_PROC_TASK_STACK_SIZE (1024 * 3)
#define USB_PROC_TASK_CORE CONFIG_USB_PROC_TASK_CORE

/**
 * @brief Task for sample processing, run user callback when new frame ready.
 *
 */
#define SAMPLE_PROC_TASK_NAME "sample_proc"
#define SAMPLE_PROC_TASK_PRIORITY CONFIG_SAMPLE_PROC_TASK_PRIORITY
#define SAMPLE_PROC_TASK_STACK_SIZE (1024 * 3)
#define SAMPLE_PROC_TASK_CORE CONFIG_SAMPLE_PROC_TASK_CORE

/**
 * @brief Task for audio processing, polling and send data from ring buffer, run user callback when date received
 *
 */
#define UAC_TASK_NAME             "vs_ac_proc"                        /*! audio task name */
#define UAC_TASK_PRIORITY         CONFIG_USB_PROC_TASK_PRIORITY       /*! audio task priority, higher to handle event quick */
#define UAC_TASK_STACK_SIZE       (1024 * 3)                          /*! audio task stack size */
#define UAC_TASK_CORE             CONFIG_USB_PROC_TASK_CORE           /*! audio task core id */

/**
 * @brief Events bit map
 *
 */
#define USB_HOST_INIT_DONE               BIT1         //set if uvc stream initted
#define USB_HOST_STOP_DONE               BIT2         //set if uvc streaming successfully stop
#define USB_DEVICE_CONNECTED             BIT3         //set if uvc device connected
#define USB_UVC_STREAM_RUNNING           BIT4         //set if uvc streaming
#define UAC_SPK_STREAM_RUNNING           BIT5         //set if speaker streaming
#define UAC_MIC_STREAM_RUNNING           BIT6         //set if mic streaming
#define USB_CTRL_PROC_SUCCEED            BIT7         //set if control successfully operate
#define USB_CTRL_PROC_FAILED             BIT8         //set if control failed operate
#define UVC_SAMPLE_PROC_STOP_DONE        BIT9         //set if sample processing successfully stop
#define USB_STREAM_TASK_KILL_BIT         BIT10        //event bit to kill stream task
#define USB_STREAM_TASK_RESET_BIT        BIT11        //event bit to reset stream task
#define USB_STREAM_TASK_PROC_SUCCEED     BIT12        //set if stream task successfully operate
#define USB_STREAM_TASK_PROC_FAILED      BIT13        //set if stream task failed operate

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
        (ctrl_req_ptr)->bmRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = UVC_SET_CUR;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_COMMIT_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

#define USB_CTRL_UVC_PROBE_SET_REQ(ctrl_req_ptr) ({  \
        (ctrl_req_ptr)->bmRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = UVC_SET_CUR;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_PROBE_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

#define USB_CTRL_UVC_PROBE_GET_REQ(ctrl_req_ptr) ({  \
        (ctrl_req_ptr)->bmRequestType = 0xA1;   \
        (ctrl_req_ptr)->bRequest = UVC_GET_CUR;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_PROBE_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

#define USB_CTRL_UVC_PROBE_GET_GENERAL_REQ(ctrl_req_ptr, req) ({  \
        (ctrl_req_ptr)->bmRequestType = 0xA1;   \
        (ctrl_req_ptr)->bRequest = req;    \
        (ctrl_req_ptr)->wValue = (UVC_VS_PROBE_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex =  1;    \
        (ctrl_req_ptr)->wLength = 26;   \
    })

enum uac_ep_ctrl_cs {
    UAC_EP_CONTROL_UNDEFINED = 0x00,
    UAC_SAMPLING_FREQ_CONTROL = 0x01,
    UAC_PITCH_CONTROL = 0x02,
};

enum uac_fu_ctrl_cs {
    UAC_FU_CONTROL_UNDEFINED = 0x00,
    UAC_FU_MUTE_CONTROL = 0x01,
    UAC_FU_VOLUME_CONTROL = 0x02,
};

#define UAC_SPK_VOLUME_MAX            0xfff0
#define UAC_SPK_VOLUME_MIN            0xe3a0
#define UAC_SPK_VOLUME_STEP           ((UAC_SPK_VOLUME_MAX - UAC_SPK_VOLUME_MIN)/100)

#define USB_CTRL_UAC_SET_EP_FREQ(ctrl_req_ptr, ep_addr) ({  \
        (ctrl_req_ptr)->bmRequestType = 0x22;   \
        (ctrl_req_ptr)->bRequest = 1;    \
        (ctrl_req_ptr)->wValue = (UAC_SAMPLING_FREQ_CONTROL << 8); \
        (ctrl_req_ptr)->wIndex = (0x00ff & ep_addr);    \
        (ctrl_req_ptr)->wLength = 3;   \
    })

#define USB_CTRL_UAC_SET_FU_VOLUME(ctrl_req_ptr, logic_ch, uint_id, ac_itf) ({  \
        (ctrl_req_ptr)->bmRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = 1;    \
        (ctrl_req_ptr)->wValue = ((UAC_FU_VOLUME_CONTROL << 8) | logic_ch); \
        (ctrl_req_ptr)->wIndex =  ((uint_id << 8) | (0x00ff & ac_itf));    \
        (ctrl_req_ptr)->wLength = 2;   \
    })

#define USB_CTRL_UAC_SET_FU_MUTE(ctrl_req_ptr, logic_ch, uint_id, ac_itf) ({  \
        (ctrl_req_ptr)->bmRequestType = 0x21;   \
        (ctrl_req_ptr)->bRequest = 1;    \
        (ctrl_req_ptr)->wValue = ((UAC_FU_MUTE_CONTROL << 8) | logic_ch); \
        (ctrl_req_ptr)->wIndex =  ((uint_id << 8) | (0x00ff & ac_itf));    \
        (ctrl_req_ptr)->wLength = 1;   \
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
    fprintf(stream, "dwFrameInterval: %"PRIu32"\n", ctrl->dwFrameInterval);
    fprintf(stream, "wKeyFrameRate: %d\n", ctrl->wKeyFrameRate);
    fprintf(stream, "wPFrameRate: %d\n", ctrl->wPFrameRate);
    fprintf(stream, "wCompQuality: %d\n", ctrl->wCompQuality);
    fprintf(stream, "wCompWindowSize: %d\n", ctrl->wCompWindowSize);
    fprintf(stream, "wDelay: %d\n", ctrl->wDelay);
    fprintf(stream, "dwMaxVideoFrameSize: %"PRIu32"\n", ctrl->dwMaxVideoFrameSize);
    fprintf(stream, "dwMaxPayloadTransferSize: %"PRIu32"\n", ctrl->dwMaxPayloadTransferSize);
    fprintf(stream, "dwClockFrequency: %"PRIu32"\n", ctrl->dwClockFrequency);
    fprintf(stream, "bmFramingInfo: %u\n", ctrl->bmFramingInfo);
    fprintf(stream, "bPreferredVersion: %u\n", ctrl->bPreferredVersion);
    fprintf(stream, "bMinVersion: %u\n", ctrl->bMinVersion);
    fprintf(stream, "bMaxVersion: %u\n", ctrl->bMaxVersion);
    fprintf(stream, "bInterfaceNumber: %d\n", ctrl->bInterfaceNumber);
}

//320*240@15 fps param for demo camera
#define DEFAULT_UVC_STREAM_CTRL() {\
    .bmHint = 1,\
    .bFormatIndex = 2,\
    .bFrameIndex = 3,\
    .dwFrameInterval = 666666,\
    .wKeyFrameRate = 0,\
    .wPFrameRate = 0,\
    .wCompQuality = 0,\
    .wCompWindowSize = 0,\
    .wDelay = 0,\
    .dwMaxVideoFrameSize = 35000,\
    .dwMaxPayloadTransferSize = 512,\
}

/*************************************** Internal Types ********************************************/
/**
 * @brief Internal UVC device defination
 *
 */
typedef struct {
    uvc_xfer_t xfer_type;
    usb_stream_t type;
    uint16_t interface;
    uint16_t interface_alt;
    uint8_t ep_addr;
    uint32_t ep_mps;
    const char *name;
    uint32_t evt_bit;
    urb_t **urb_list;
    uint32_t urb_num;
    uint32_t packets_per_urb;
    uint32_t bytes_per_packet;
    hcd_pipe_handle_t pipe_handle;
} _stream_ifc_t;

typedef struct {
    usb_speed_t dev_speed;
    uint16_t configuration;
    uint8_t dev_addr;
    QueueHandle_t queue_hdl;
    QueueHandle_t stream_queue_hdl;
    EventGroupHandle_t event_group;
    SemaphoreHandle_t ctrl_mutex;
    TaskHandle_t vs_as_taskh;
} _usb_device_t;

typedef struct {
    uvc_xfer_t xfer_type;
    uvc_stream_ctrl_t ctrl_set;
    uvc_stream_ctrl_t ctrl_probed;
    uint32_t xfer_buffer_size;
    uint8_t *xfer_buffer_a;
    uint8_t *xfer_buffer_b;
    uint32_t frame_buffer_size;
    uint8_t *frame_buffer;
    uint16_t frame_width;
    uint16_t frame_height;
    uvc_frame_callback_t *user_cb;
    void *user_ptr;
    _stream_ifc_t *vs_ifc;
    bool active;
    _usb_device_t *parent;
} _uvc_device_t;

typedef struct _uac_device {
    uint8_t  ac_interface;
    uint16_t mic_bit_resolution;
    uint32_t mic_samples_frequence;
    uint32_t mic_min_bytes;
    uint8_t  mic_fu_id;
    bool     mic_mute;
    uint32_t mic_volume;
    uint16_t spk_bit_resolution;
    uint32_t spk_samples_frequence;
    uint32_t spk_max_xfer_size;
    uint32_t spk_buf_size;
    uint32_t mic_buf_size;
    uint8_t  spk_fu_id;
    bool     spk_mute;
    uint32_t spk_volume;
    _stream_ifc_t *spk_as_ifc;
    _stream_ifc_t *mic_as_ifc;
    RingbufHandle_t spk_ringbuf_hdl;
    RingbufHandle_t mic_ringbuf_hdl;
    mic_callback_t *user_cb;
    void *user_ptr;
    bool mic_active;
    bool spk_active;
    _usb_device_t *parent;
} _uac_device_t;

/**
 * @brief Singleton Pattern, Only one camera supported
 *
 */
static _stream_ifc_t s_usb_itf[STREAM_MAX] = {{0}};
static _usb_device_t s_usb_dev = {0};
static _uvc_device_t s_uvc_dev = {0};
static _uac_device_t s_uac_dev = {0};

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
    SemaphoreHandle_t cb_mutex;
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
    UAC_MUTE,
    UAC_VOLUME,
} _uvc_user_cmd_t;

typedef struct {
    _uvc_event_type_t _type;
    union {
        void *user_hdl;
        hcd_port_handle_t port_hdl;
        hcd_pipe_handle_t pipe_handle;
    } _handle;
    union {
        _uvc_user_cmd_t user_cmd;
        hcd_port_event_t port_event;
        hcd_pipe_event_t pipe_event;
    } _event;
    void *_event_data;
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

static hcd_port_event_t _usb_port_event_dflt_process(hcd_port_handle_t port_hdl, hcd_port_event_t event)
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
        if (s_usb_dev.event_group) {
            xEventGroupClearBits(s_usb_dev.event_group, USB_DEVICE_CONNECTED);
        }
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

static hcd_pipe_event_t _pipe_event_dflt_process(hcd_pipe_handle_t pipe_handle, const char* pipe_name, hcd_pipe_event_t pipe_event);

static esp_err_t _usb_port_event_wait(hcd_port_handle_t expected_port_hdl, hcd_port_event_t expected_event, TickType_t xTicksToWait)
{
    //Wait for port callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_port_get_context(expected_port_hdl);
    assert(queue_hdl != NULL);
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    BaseType_t queue_ret = errQUEUE_EMPTY;
    _uvc_event_msg_t evt_msg;

    do {
        queue_ret = xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            break;
        }

        if (PIPE_EVENT == evt_msg._type) {
            _pipe_event_dflt_process(evt_msg._handle.pipe_handle, "default", evt_msg._event.pipe_event);
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

static usb_phy_handle_t s_phy_handle = NULL;

static hcd_port_handle_t _usb_port_init(void *context, void *callback_arg)
{
    UVC_CHECK(context != NULL && callback_arg != NULL, "invalid args", NULL);
    esp_err_t ret = ESP_OK;
    hcd_port_handle_t port_hdl = NULL;

    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_HOST,
        .otg_speed = USB_PHY_SPEED_UNDEFINED,   //In Host mode, the speed is determined by the connected device
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        .otg_io_conf = NULL,
#else
        .gpio_conf = NULL,
#endif
    };
    ret = usb_new_phy(&phy_config, &s_phy_handle);
    UVC_CHECK(ESP_OK == ret, "USB PHY init failed", NULL);

    hcd_config_t hcd_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL2,
    };
    ret = hcd_install(&hcd_config);
    UVC_CHECK_GOTO(ESP_OK == ret, "HCD Install failed", hcd_init_err);

    hcd_port_config_t port_cfg = {
        .fifo_bias = HCD_PORT_FIFO_BIAS_BALANCED,
        .callback = _usb_port_callback,
        .callback_arg = callback_arg,
        .context = context,
    };

    ret = hcd_port_init(USB_PORT_NUM, &port_cfg, &port_hdl);
    UVC_CHECK_GOTO(ESP_OK == ret, "HCD Port init failed", port_init_err);

    ESP_LOGI(TAG, "Port=%d init succeed", USB_PORT_NUM);
    return port_hdl;

port_init_err:
    hcd_uninstall();
hcd_init_err:
    usb_del_phy(s_phy_handle);
    return NULL;
}

static esp_err_t _usb_port_deinit(hcd_port_handle_t port_hdl)
{
    esp_err_t ret;
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_DISABLE);

    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "port disable failed");
    }

    _usb_port_event_dflt_process(port_hdl, 0);
    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_OFF);

    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "Port power off failed");
    }

    ESP_LOGI(TAG, "Waiting for USB disconnection");
    ret = _usb_port_event_wait(port_hdl, HCD_PORT_EVENT_DISCONNECTION, 200 / portTICK_PERIOD_MS);
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

void _usb_urb_clear(urb_t *urb, uint32_t num_isoc_packets)
{
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
    uint8_t *data_buffer = transfer_dummy->data_buffer;
    int num_isoc = transfer_dummy->num_isoc_packets;
    void *context = transfer_dummy->context;
    memset(urb, 0, sizeof(urb_t) + (num_isoc_packets * sizeof(usb_isoc_packet_desc_t)));
    transfer_dummy->data_buffer = data_buffer;
    transfer_dummy->num_isoc_packets = num_isoc;
    transfer_dummy->context = context;
}

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

static void _usb_urb_context_update(urb_t *urb, void *context)
{
    //Initialize URB and underlying transfer structure. Need to cast to dummy due to const fields
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&urb->transfer;
    transfer_dummy->context = context;
    ESP_LOGD(TAG, "urb context update");
}

static void _usb_urb_free(urb_t *urb)
{
    //Free data buffers of each URB
    if (urb && urb->transfer.data_buffer) {
        heap_caps_free(urb->transfer.data_buffer);
    }
    //Free the URB list
    if (urb) {
        heap_caps_free(urb);
    }
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

static hcd_pipe_event_t _pipe_event_dflt_process(hcd_pipe_handle_t pipe_handle, const char *pipe_name, hcd_pipe_event_t pipe_event)
{
    assert(pipe_handle != NULL);
    hcd_pipe_event_t actual_evt = pipe_event;
    switch (pipe_event) {
    case HCD_PIPE_EVENT_NONE:
        break;
    case HCD_PIPE_EVENT_URB_DONE:
        ESP_LOGV(TAG, "Pipe(%s): XFER_DONE", pipe_name);
        break;

    case HCD_PIPE_EVENT_ERROR_XFER:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_XFER", pipe_name);
        break;

    case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_URB_NOT_AVAIL", pipe_name);
        break;

    case HCD_PIPE_EVENT_ERROR_OVERFLOW:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_OVERFLOW", pipe_name);
        break;

    case HCD_PIPE_EVENT_ERROR_STALL:
        ESP_LOGW(TAG, "Pipe(%s): ERROR_STALL", pipe_name);
        break;

    default:
        ESP_LOGW(TAG, "Pipe(%s): invalid EVENT = %d", pipe_name, pipe_event);
        break;
    }
    return actual_evt;
}

static void _processing_uvc_pipe(hcd_pipe_handle_t pipe_handle, bool if_enqueue)
{
    assert(pipe_handle != NULL);
    bool enqueue_flag = if_enqueue;

    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);

    if (urb_done == NULL) {
        ESP_LOGV(TAG, "uvc pipe dequeue failed");
        enqueue_flag = false;
        return;
    }

    TRIGGER_URB_DEQUEUE();
    _uvc_stream_handle_t *strmh = (_uvc_stream_handle_t *)(urb_done->transfer.context);
    if (urb_done->transfer.num_isoc_packets == 0) { // Bulk transfer
        if (urb_done->transfer.actual_num_bytes > 0) {
            _uvc_process_payload(strmh, urb_done->transfer.data_buffer, urb_done->transfer.actual_num_bytes);
        }
    } else { // isoc transfer
        size_t index = 0;
        for (size_t i = 0; i < urb_done->transfer.num_isoc_packets; i++) {
            if (urb_done->transfer.isoc_packet_desc[i].status != USB_TRANSFER_STATUS_COMPLETED) {
                ESP_LOGV(TAG, "line:%u bad iso transit status %d", __LINE__, urb_done->transfer.isoc_packet_desc[i].status);
                continue;
            }

            uint8_t *simplebuffer = urb_done->transfer.data_buffer + (index * s_uvc_dev.vs_ifc->ep_mps);
            ++index;
            ESP_LOGV(TAG, "process payload=%u", index);
            _uvc_process_payload(strmh, simplebuffer, (urb_done->transfer.isoc_packet_desc[i].actual_num_bytes));
        }
    }

    if (enqueue_flag) {
        esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_done);

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "line:%u URB ENQUEUE FAILED %s", __LINE__, esp_err_to_name(ret));
        }

        TRIGGER_URB_ENQUEUE();
    }

}

static hcd_pipe_handle_t _usb_pipe_init(hcd_port_handle_t port_hdl, usb_ep_desc_t *ep_desc, uint8_t dev_addr, usb_speed_t dev_speed, void *context, void *callback_arg)
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
    ESP_LOGD(TAG, "pipe init succeed");
    return pipe_handle;
}

static esp_err_t _default_pipe_event_wait_until(hcd_pipe_handle_t expected_pipe_hdl, hcd_pipe_event_t expected_event, TickType_t xTicksToWait, bool if_handle_others);

static esp_err_t _usb_pipe_deinit(hcd_pipe_handle_t pipe_hdl, size_t urb_num)
{
    UVC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "pipe state = %d", hcd_pipe_get_state(pipe_hdl));

    esp_err_t ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p halt failed", pipe_hdl);
    }

    ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_FLUSH);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p flush failed", pipe_hdl);
    }
    _default_pipe_event_wait_until(pipe_hdl, HCD_PIPE_EVENT_URB_DONE, 200 / portTICK_PERIOD_MS, false);

    for (size_t i = 0; i < urb_num; i++) {
        urb_t *urb = hcd_urb_dequeue(pipe_hdl);
        ESP_LOGD(TAG, "urb dequeue handle = %p", urb);
    }

    ret = hcd_pipe_free(pipe_hdl);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p free failed", pipe_hdl);
        return ret;
    }

    ESP_LOGD(TAG, "pipe deinit succeed");
    return ESP_OK;
}

static esp_err_t _usb_pipe_flush(hcd_pipe_handle_t pipe_hdl, size_t urb_num)
{
    UVC_CHECK(pipe_hdl != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    ESP_LOGD(TAG, "pipe flushing: state = %d", hcd_pipe_get_state(pipe_hdl));

    esp_err_t ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_HALT);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p halt failed", pipe_hdl);
    }

    ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_FLUSH);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p flush failed", pipe_hdl);
    }
    _default_pipe_event_wait_until(pipe_hdl, HCD_PIPE_EVENT_URB_DONE, 200 / portTICK_PERIOD_MS, false);

    for (size_t i = 0; i < urb_num; i++) {
        urb_t *urb = hcd_urb_dequeue(pipe_hdl);
        ESP_LOGD(TAG, "urb dequeue handle = %p", urb);
    }

    ret = hcd_pipe_command(pipe_hdl, HCD_PIPE_CMD_CLEAR);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "pipe=%p clear failed", pipe_hdl);
    }

    ESP_LOGD(TAG, "pipe flush succeed");
    return ESP_OK;
}

static esp_err_t _default_pipe_event_wait_until(hcd_pipe_handle_t expected_pipe_hdl,
        hcd_pipe_event_t expected_event, TickType_t xTicksToWait, bool if_handle_others)
{
    //Wait for pipe callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_pipe_get_context(expected_pipe_hdl);
    _uvc_event_msg_t evt_msg;
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    BaseType_t queue_ret = errQUEUE_EMPTY;

    do {
        queue_ret = xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);

        if (queue_ret != pdPASS) {
            ESP_LOGW(TAG, "Pipe wait event timeout");
            ret = ESP_ERR_TIMEOUT;
            break;
        }

        if (USER_EVENT == evt_msg._type) {
            ESP_LOGW(TAG, "Got unexpected user event");
            ret = ESP_ERR_NOT_FOUND;
        } else if (PORT_EVENT == evt_msg._type) {
            _usb_port_event_dflt_process(evt_msg._handle.port_hdl, evt_msg._event.port_event);
            ret = ESP_ERR_NOT_FOUND;
        } else {
            if (expected_pipe_hdl != evt_msg._handle.pipe_handle) {
                // should never into here
                ESP_LOGW(TAG, "Got unexpected pipe:%p", evt_msg._handle.pipe_handle);
                ret = ESP_ERR_NOT_FOUND;
            } else if (expected_event != evt_msg._event.pipe_event) {
                _pipe_event_dflt_process(evt_msg._handle.pipe_handle, "default", evt_msg._event.pipe_event);
                ret = ESP_ERR_INVALID_RESPONSE;
            } else {
                ESP_LOGD(TAG, "Got expected pipe_handle and event");
                ret = ESP_OK;
            }
        }
    } while (ret == ESP_ERR_NOT_FOUND);

    return ret;
}

/*------------------------------------------------ USB Control Process Code ----------------------------------------------------*/
#ifdef CONFIG_UVC_GET_DEVICE_DESC
static esp_err_t _usb_get_dev_desc(hcd_pipe_handle_t pipe_handle, usb_device_desc_t *device_desc)
{
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
    urb_t *urb_done = NULL;
    ESP_LOGI(TAG, "get device desc");
    USB_SETUP_PACKET_INIT_GET_DEVICE_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(sizeof(usb_device_desc_t), USB_EP_CTRL_DEFAULT_MPS);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    UVC_CHECK_GOTO((urb_done->transfer.actual_num_bytes <= sizeof(usb_setup_packet_t) + sizeof(usb_device_desc_t)), "urb status: data overflow", free_urb_);
    ESP_LOGI(TAG, "get device desc, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);
    usb_device_desc_t *dev_desc = (usb_device_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    if (device_desc != NULL ) {
        *device_desc = *dev_desc;
    }
    usb_print_device_descriptor(dev_desc);
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}
#endif

#ifdef CONFIG_UVC_GET_CONFIG_DESC
extern void _print_uvc_class_descriptors_cb(const usb_standard_desc_t *desc);
static esp_err_t _usb_get_config_desc(hcd_pipe_handle_t pipe_handle, int index, usb_config_desc_t **config_desc)
{
    (void)config_desc;
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
    urb_t *urb_done = NULL;
    ESP_LOGI(TAG, "get short config desc");
    USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, index - 1, USB_ENUM_SHORT_DESC_REQ_LEN);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(sizeof(usb_config_desc_t), USB_EP_CTRL_DEFAULT_MPS);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    UVC_CHECK_GOTO((urb_done->transfer.actual_num_bytes <= sizeof(usb_setup_packet_t) + sizeof(usb_config_desc_t)), "urb status: data overflow", free_urb_);
    ESP_LOGI(TAG, "get config desc, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);
    usb_config_desc_t *cfg_desc = (usb_config_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    uint16_t full_config_length = cfg_desc->wTotalLength;
    if (cfg_desc->wTotalLength > CTRL_TRANSFER_DATA_MAX_BYTES) {
        ESP_LOGW(TAG, "Configuration descriptor larger than control transfer max length");
        goto free_urb_;
    }
    ESP_LOGI(TAG, "get full config desc");
    USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, index - 1, full_config_length);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(full_config_length, USB_EP_CTRL_DEFAULT_MPS);
    ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    UVC_CHECK_GOTO((urb_done->transfer.actual_num_bytes <= sizeof(usb_setup_packet_t) + full_config_length), "urb status: data overflow", free_urb_);
    ESP_LOGI(TAG, "get full config desc, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);
    cfg_desc = (usb_config_desc_t *)(urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
    usb_print_config_descriptor(cfg_desc, _print_uvc_class_descriptors_cb);
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
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    //STD: Set ADDR
    USB_SETUP_PACKET_INIT_SET_ADDR((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, dev_addr);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t); //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Addr = %u", dev_addr);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ret = hcd_pipe_update_dev_addr(pipe_handle, dev_addr);
    UVC_CHECK_GOTO(ESP_OK == ret, "default pipe update address failed", free_urb_);
    ret = hcd_pipe_update_mps(pipe_handle, USB_EP_CTRL_DEFAULT_MPS);
    UVC_CHECK_GOTO(ESP_OK == ret, "default pipe update MPS failed", free_urb_);
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
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(configuration != 0, "configuration can't be 0", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    USB_SETUP_PACKET_INIT_SET_CONFIG((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, configuration);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t); //No data stage
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Configuration = %u", configuration);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Configuration Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _usb_set_device_interface(hcd_pipe_handle_t pipe_handle, uint16_t interface, uint16_t interface_alt)
{
    UVC_CHECK(pipe_handle != NULL, "pipe_handle can't be NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(interface != 0, "interface can't be 0", ESP_ERR_INVALID_ARG);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    USB_SETUP_PACKET_INIT_SET_INTERFACE((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, interface, interface_alt);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t);
    //Enqueue it
    ESP_LOGI(TAG, "Set Device Interface = %u, Alt = %u", interface, interface_alt);
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "Set Device Interface Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _uvc_vs_commit_control(hcd_pipe_handle_t pipe_handle, uvc_stream_ctrl_t *ctrl_set, uvc_stream_ctrl_t *ctrl_probed)
{
    assert(ctrl_set != NULL);
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    urb_t *urb_done = NULL;

    ESP_LOGI(TAG, "SET_CUR Probe");
    USB_CTRL_UVC_PROBE_SET_REQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    _uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_set);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "SET_CUR Probe Done");

    ESP_LOGI(TAG, "GET_CUR Probe");
    USB_CTRL_UVC_PROBE_GET_REQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    urb_ctrl->transfer.num_bytes = USB_EP_CTRL_DEFAULT_MPS; //IN should be integer multiple of MPS
    ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    UVC_CHECK_GOTO((urb_done->transfer.actual_num_bytes <= sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength), "urb status: data overflow", free_urb_);
    _buf_to_uvc_stream_ctrl((urb_done->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_done->transfer.data_buffer)->wLength, ctrl_probed);
#ifdef CONFIG_UVC_PRINT_PROBE_RESULT
    _uvc_stream_ctrl_printf(stdout, ctrl_probed);
#endif
    ESP_LOGI(TAG, "GET_CUR Probe Done, actual_num_bytes:%d", urb_done->transfer.actual_num_bytes);

    ESP_LOGI(TAG, "SET_CUR COMMIT");
    USB_CTRL_UVC_COMMIT_REQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    _uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_probed);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", free_urb_);
    ESP_LOGI(TAG, "SET_CUR COMMIT Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _uac_as_control_set_mute(hcd_pipe_handle_t pipe_handle, uint16_t ac_itc, uint8_t fu_id, bool if_mute)
{
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    urb_t *urb_done = NULL;
    ESP_LOGI(TAG, "SET_CUR mute, %d", if_mute);
    USB_CTRL_UAC_SET_FU_MUTE((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, 1, fu_id, ac_itc);
    unsigned char *p_data = urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t);
    p_data[0] = if_mute;
    //_uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_set);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", flush_urb_);
    ESP_LOGI(TAG, "SET_CUR mute Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _uac_as_control_set_volume(hcd_pipe_handle_t pipe_handle, uint16_t ac_itc, uint8_t fu_id, uint32_t volume)
{
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    urb_t *urb_done = NULL;
    USB_CTRL_UAC_SET_FU_VOLUME((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, 1, fu_id, ac_itc);
    unsigned char *p_data = urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t);
    uint32_t volume_db = UAC_SPK_VOLUME_MIN + volume * UAC_SPK_VOLUME_STEP;
    p_data[0] = volume_db & 0x00ff;
    p_data[1] = (volume_db & 0xff00) >> 8;
    ESP_LOGI(TAG, "SET_CUR volume %04x (%"PRIu32")", (uint16_t)(volume_db & 0xffff), volume);
    //_uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_set);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", flush_urb_);
    ESP_LOGI(TAG, "SET_CUR volume Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
free_urb_:
    _usb_urb_free(urb_ctrl);
    return ret;
}

static esp_err_t _uac_as_control_set_freq(hcd_pipe_handle_t pipe_handle, uint8_t ep_addr, uint32_t freq)
{
    //malloc URB for default control
    urb_t *urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);

    urb_t *urb_done = NULL;
    ESP_LOGI(TAG, "SET_CUR frequence %"PRIu32"", freq);
    USB_CTRL_UAC_SET_EP_FREQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, ep_addr);
    unsigned char *p_data = urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t);
    p_data[0] = freq & 0x0000ff;
    p_data[1] = (freq & 0x00ff00) >> 8;
    p_data[2] = (freq & 0xff0000) >> 16;
    //_uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_set);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_ctrl);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb enqueue failed", free_urb_);
    ret = _default_pipe_event_wait_until(pipe_handle, HCD_PIPE_EVENT_URB_DONE, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS), false);
    UVC_CHECK_GOTO(ESP_OK == ret, "urb event error", flush_urb_);
    urb_done = hcd_urb_dequeue(pipe_handle);
    UVC_CHECK_GOTO(urb_done == urb_ctrl, "urb status: not same", free_urb_);
    UVC_CHECK_GOTO(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", flush_urb_);
    ESP_LOGI(TAG, "SET_CUR Done");
    goto free_urb_;

flush_urb_:
    _usb_pipe_flush(pipe_handle, 1);
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
    /* to prevent the latest data from being lost
    * if take mutex timeout, we should drop the last frame */
    size_t timeout_ms = 0;
    if (s_uvc_dev.xfer_type == UVC_XFER_BULK) {
        // for full speed bulk mode, max 19 packet can be transmit during each millisecond
        timeout_ms = (s_uvc_dev.vs_ifc->urb_num - 1) * (s_uvc_dev.vs_ifc->bytes_per_packet * 0.052 / USB_EP_BULK_FS_MPS);
    } else {
        timeout_ms = (s_uvc_dev.vs_ifc->urb_num - 1) * s_uvc_dev.vs_ifc->packets_per_urb;
    }
    size_t timeout_tick = pdMS_TO_TICKS(timeout_ms);
    if (timeout_tick == 0) {
        timeout_tick = 1;
    }
    if (xSemaphoreTake(strmh->cb_mutex, timeout_tick) == pdTRUE) {
        /* code */
        /* swap the buffers */
        uint8_t *tmp_buf = strmh->holdbuf;
        strmh->hold_bytes = strmh->got_bytes;
        strmh->holdbuf = strmh->outbuf;
        strmh->outbuf = tmp_buf;
        strmh->hold_last_scr = strmh->last_scr;
        strmh->hold_pts = strmh->pts;
        strmh->hold_seq = strmh->seq;
        xTaskNotifyGive(strmh->taskh);
        xSemaphoreGive(strmh->cb_mutex);
    } else {
        ESP_LOGD(TAG, "timeout drop frame = %"PRIu32"", strmh->seq);
    }

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
        ESP_LOGV(TAG, "payload_len == 0");
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

    ESP_LOGV(TAG, "len=%u info=%02x, payload_len = %u, last_pts = %"PRIu32" , last_scr = %"PRIu32"", header_len, header_info, payload_len, strmh->pts, strmh->last_scr);
    ESP_LOGV(TAG, "uvc payload = %02x %02x...%02x %02x\n", payload[header_len], payload[header_len+1], payload[payload_len-2], payload[payload_len-1]);
    /* ERR bit defined in Stream Header*/
    if (header_info & 0x40) {
        ESP_LOGW(TAG, "bad packet: error bit set");
        return;
    }

    if (strmh->fid != (header_info & 1) && strmh->got_bytes != 0) {
        /* The frame ID bit was flipped, but we have image data sitting
            around from prior transfers. This means the camera didn't send
            an EOF for the last transfer of the previous frame. */
        ESP_LOGD(TAG, "SWAP NO EOF %d", strmh->got_bytes);
        /* clean whole no-flipped buffer */
        if (s_uvc_dev.xfer_type == UVC_XFER_BULK) {
            // compatible with no standard camera
            _uvc_swap_buffers(strmh);
        } else {
            _uvc_drop_buffers(strmh);
        }
    }

    strmh->fid = header_info & 1;

    if (header_info & (1 << 2)) {
        strmh->pts = DW_TO_INT(payload + variable_offset);
        variable_offset += 4;
    }

    if (header_info & (1 << 3)) {
        strmh->last_scr = DW_TO_INT(payload + variable_offset);
        variable_offset += 6;
    }

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
 * @param strmhp return stream handle if succeed
 * @param config ctrl configs
 * @return uvc_error_t
 */
static uvc_error_t uvc_stream_open_ctrl(uvc_device_handle_t *devh, _uvc_stream_handle_t **strmhp, uvc_stream_ctrl_t *ctrl)
{
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
    strmh->outbuf = s_uvc_dev.xfer_buffer_a;
    strmh->holdbuf = s_uvc_dev.xfer_buffer_b;
    strmh->frame.data = s_uvc_dev.frame_buffer;

    strmh->cb_mutex = xSemaphoreCreateMutex();

    if (strmh->cb_mutex == NULL) {
        ESP_LOGE(TAG, "line-%u Mutex create failed", __LINE__);
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
static uvc_error_t uvc_stream_start(_uvc_stream_handle_t *strmh, uvc_frame_callback_t *cb, void *user_ptr, uint8_t flags)
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
        BaseType_t ret = xTaskCreatePinnedToCore(_sample_processing_task, SAMPLE_PROC_TASK_NAME, SAMPLE_PROC_TASK_STACK_SIZE, (void *)strmh,
                        SAMPLE_PROC_TASK_PRIORITY, &strmh->taskh, SAMPLE_PROC_TASK_CORE);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "sample task create failed");
            return UVC_ERROR_OTHER;
        }
        ESP_LOGI(TAG, "Sample processing task started");
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
static uvc_error_t uvc_stream_stop(_uvc_stream_handle_t *strmh)
{
    if (strmh->running == 0) {
        ESP_LOGW(TAG, "line:%u UVC_ERROR_INVALID_PARAM", __LINE__);
        return UVC_ERROR_INVALID_PARAM;
    }
    strmh->running = 0;
    _uvc_device_t *device_handle = (_uvc_device_t *)(strmh->devh);
    xTaskNotifyGive(strmh->taskh);
    xEventGroupWaitBits(device_handle->parent->event_group, UVC_SAMPLE_PROC_STOP_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
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
static void uvc_stream_close(_uvc_stream_handle_t *strmh)
{
    vSemaphoreDelete(strmh->cb_mutex);
    free(strmh);
    return;
}

/****************************************************** Task Code ******************************************************/

static esp_err_t _user_event_wait(hcd_port_handle_t port_hdl,
                                _uvc_user_cmd_t waiting_cmd, TickType_t xTicksToWait)
{
    //Wait for port callback to send an event message
    QueueHandle_t queue_hdl = (QueueHandle_t)hcd_port_get_context(port_hdl);
    assert(queue_hdl != NULL);
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    BaseType_t queue_ret = errQUEUE_EMPTY;
    _uvc_event_msg_t evt_msg;

    do {
        queue_ret = xQueueReceive(queue_hdl, &evt_msg, xTicksToWait);
        if (queue_ret != pdPASS) {
            ret = ESP_ERR_NOT_FOUND;
            break;
        }

        if (evt_msg._type == USER_EVENT && evt_msg._event.user_cmd == waiting_cmd) {
            ret = ESP_OK;
        } else {
            ret = ESP_ERR_NOT_FOUND;
        }
    } while (ret == ESP_ERR_NOT_FOUND);

    return ret;
}

static size_t _ring_buffer_get_len(RingbufHandle_t ringbuf_hdl)
{
    if (ringbuf_hdl == NULL) {
        return 0;
    }
    size_t size = 0;
    vRingbufferGetInfo(ringbuf_hdl, NULL, NULL, NULL, NULL, &size);
    return size;
}

static void _ring_buffer_flush(RingbufHandle_t ringbuf_hdl)
{
    if (ringbuf_hdl == NULL) {
        return;
    }
    size_t read_bytes = 0;
    size_t uxItemsWaiting = 0;
    vRingbufferGetInfo(ringbuf_hdl, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(ringbuf_hdl, &read_bytes, 1, uxItemsWaiting);

    if (buf_rcv) {
        vRingbufferReturnItem(ringbuf_hdl, (void *)(buf_rcv));
    }
    ESP_LOGD(TAG, "buffer flush -%u = %u", read_bytes, uxItemsWaiting);
}

static esp_err_t _ring_buffer_push(RingbufHandle_t ringbuf_hdl, uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    if (ringbuf_hdl == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (buf == NULL) {
        ESP_LOGD(TAG, "can not push NULL buffer");
        return ESP_ERR_INVALID_STATE;
    }
    int res = xRingbufferSend(ringbuf_hdl, buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "buffer is too small, push failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t _ring_buffer_pop(RingbufHandle_t ringbuf_hdl, uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    if (ringbuf_hdl == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (buf == NULL) {
        ESP_LOGD(TAG, "can not pop buffer to NULL");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(ringbuf_hdl, read_bytes, ticks_to_wait, req_bytes);

    if (buf_rcv) {
        memcpy(buf, buf_rcv, *read_bytes);
        vRingbufferReturnItem(ringbuf_hdl, (void *)(buf_rcv));
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void _processing_mic_pipe(hcd_pipe_handle_t pipe_hdl, mic_callback_t *user_cb, void *user_ptr, bool if_enqueue)
{
    if (pipe_hdl == NULL) {
        return;
    }

    bool enqueue_flag = if_enqueue;
    urb_t *urb_done = hcd_urb_dequeue(pipe_hdl);
    if (urb_done == NULL) {
        ESP_LOGE(TAG, "mic urb dequeue error");
        return;
    }

    size_t xfered_size = 0;
    mic_frame_t mic_frame = {
        .bit_resolution = s_uac_dev.mic_bit_resolution,
        .samples_frequence = s_uac_dev.mic_samples_frequence,
        .data = urb_done->transfer.data_buffer,
    };

    for (size_t i = 0; i < urb_done->transfer.num_isoc_packets; i++) {
        if (urb_done->transfer.isoc_packet_desc[i].status != USB_TRANSFER_STATUS_COMPLETED) {
            ESP_LOGW(TAG, "line:%u bad iso transit status %d", __LINE__, urb_done->transfer.isoc_packet_desc[i].status);
            break;
        } else {
            int actual_num_bytes = urb_done->transfer.isoc_packet_desc[i].actual_num_bytes;
#if UAC_MIC_PACKET_COMPENSATION
            if (actual_num_bytes == 0) {
                int num_bytes = urb_done->transfer.isoc_packet_desc[i].num_bytes;
                uint8_t *packet_buffer = urb_done->transfer.data_buffer + (i * num_bytes);
                if (i == 0) {
                    //if first packet loss (small probability), we just padding all 0
                    memset(packet_buffer, 0, num_bytes);
                    ESP_LOGV(TAG, "MIC: padding 0");
                } else {
                    //if other packets loss, we just padding the last packet
                    uint8_t *packet_last_buffer = urb_done->transfer.data_buffer + (i * num_bytes - num_bytes);
                    memcpy(packet_buffer, packet_last_buffer, num_bytes);
                    ESP_LOGV(TAG, "MIC: padding data");
                }
                actual_num_bytes = num_bytes;
            }
#endif
            xfered_size += actual_num_bytes;
        }
    }
    mic_frame.data_bytes = xfered_size;
    ESP_LOGV(TAG, "MIC RC %d", xfered_size);
    ESP_LOGV(TAG, "mic payload = %02x %02x...%02x %02x\n", urb_done->transfer.data_buffer[0], urb_done->transfer.data_buffer[1], urb_done->transfer.data_buffer[xfered_size-2], urb_done->transfer.data_buffer[xfered_size-1]);

    if (s_uac_dev.mic_ringbuf_hdl) {
        esp_err_t ret = _ring_buffer_push(s_uac_dev.mic_ringbuf_hdl, mic_frame.data, mic_frame.data_bytes, 1);
        if (ret != ESP_OK) {
            ESP_LOGD(TAG, "mic ringbuf too small, please pop in time");
        }
    }

    if (xfered_size  > 0 && user_cb != NULL) {
        user_cb(&mic_frame, user_ptr);
    }

    if (enqueue_flag) {
        esp_err_t ret = hcd_urb_enqueue(pipe_hdl, urb_done);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "line:%u URB ENQUEUE FAILED %s", __LINE__, esp_err_to_name(ret));
        }
    }

}

esp_err_t uac_spk_streaming_write(void *data, size_t data_bytes, size_t timeout_ms)
{
    if (s_uac_dev.spk_active != true) {
        ESP_LOGD(TAG, "spk stream not config");
        return ESP_ERR_INVALID_STATE;
    }

    size_t remind_timeout = timeout_ms;
    bool check_valid = false;
    do {
        if (xEventGroupGetBits(s_usb_dev.event_group) & UAC_SPK_STREAM_RUNNING) {
            check_valid = true;
            break;
        } else {
            if (remind_timeout >= portTICK_PERIOD_MS) {
                remind_timeout -= portTICK_PERIOD_MS;
                vTaskDelay(1);
            } else {
                remind_timeout = 0;
                break;
            }
        }
    } while (remind_timeout > 0 && !check_valid);

    if (!check_valid) {
        ESP_LOGD(TAG, "spk stream not ready");
        return ESP_ERR_INVALID_STATE;
    }
    return _ring_buffer_push(s_uac_dev.spk_ringbuf_hdl, data, data_bytes, remind_timeout);
}

esp_err_t uac_mic_streaming_read(void *buf, size_t buf_size, size_t *data_bytes, size_t timeout_ms)
{
    if (s_uac_dev.mic_active != true) {
        ESP_LOGD(TAG, "mic stream not config");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_uac_dev.mic_ringbuf_hdl == NULL) {
        ESP_LOGD(TAG, "mic ringbuffer not config");
        return ESP_ERR_INVALID_STATE;
    }
    size_t remind_timeout = timeout_ms;
    bool check_valid = false;
    do {
        if (xEventGroupGetBits(s_usb_dev.event_group) & UAC_MIC_STREAM_RUNNING) {
            check_valid = true;
            break;
        } else {
            if (remind_timeout >= portTICK_PERIOD_MS) {
                remind_timeout -= portTICK_PERIOD_MS;
                vTaskDelay(1);
            } else {
                remind_timeout = 0;
                break;
            }
        }
    } while (remind_timeout > 0 && !check_valid);

    if (!check_valid) {
        ESP_LOGD(TAG, "spk stream not ready");
        return ESP_ERR_INVALID_STATE;
    }
    return _ring_buffer_pop(s_uac_dev.mic_ringbuf_hdl, buf, buf_size, data_bytes, remind_timeout);
}

static void _processing_spk_pipe(hcd_pipe_handle_t pipe_hdl, bool if_dequeue, bool reset)
{
    if (pipe_hdl == NULL) {
        return;
    }

    static size_t pending_urb_num = 0;
    static urb_t *pending_urb[NUM_ISOC_SPK_URBS] = {NULL};
    esp_err_t ret = ESP_FAIL;

    if (reset) {
        pending_urb_num = 0;
        for (size_t j = 0; j < NUM_ISOC_SPK_URBS; j++) {
            pending_urb[j] = NULL;
        }
        return;
    }

    if (!pending_urb_num && !if_dequeue) {
        return;
    }

    if (if_dequeue) {
        size_t xfered_size = 0;
        urb_t *urb_done = hcd_urb_dequeue(pipe_hdl);
        if (urb_done == NULL) {
            ESP_LOGE(TAG, "spk urb dequeue error");
            return;
        }
        for (size_t i = 0; i < urb_done->transfer.num_isoc_packets; i++) {
            if (urb_done->transfer.isoc_packet_desc[i].status != USB_TRANSFER_STATUS_COMPLETED) {
                ESP_LOGW(TAG, "line:%u bad iso transit status %d", __LINE__, urb_done->transfer.isoc_packet_desc[i].status);
                break;
            } else {
                xfered_size += urb_done->transfer.isoc_packet_desc[i].actual_num_bytes;
            }
        }
        ESP_LOGV(TAG, "SPK ST actual = %d", xfered_size);
        /* add done urb to pending urb list */
        for (size_t i = 0; i < NUM_ISOC_SPK_URBS; i++) {
            if (pending_urb[i] == NULL) {
                pending_urb[i] = urb_done;
                ++pending_urb_num;
                assert(pending_urb_num <= NUM_ISOC_SPK_URBS);
                break;
            }
        }
    }
    /* check if we have buffered data need to send */
    if (_ring_buffer_get_len(s_uac_dev.spk_ringbuf_hdl) < s_uac_dev.spk_as_ifc->bytes_per_packet) {
        return;
    }

    /* fetch a pending urb from list */
    urb_t *next_urb = NULL;
    size_t next_index = 0;
    for (; next_index < NUM_ISOC_SPK_URBS; next_index++) {
        if (pending_urb[next_index] != NULL) {
            next_urb = pending_urb[next_index];
            break;
        }
    }
    assert(next_urb != NULL);

    size_t num_bytes_to_send = 0;
    size_t buffer_size = s_uac_dev.spk_max_xfer_size;
    uint8_t *buffer = next_urb->transfer.data_buffer;
    ret = _ring_buffer_pop(s_uac_dev.spk_ringbuf_hdl, buffer, buffer_size, &num_bytes_to_send, 0);
    if (ret != ESP_OK || num_bytes_to_send == 0) {
        //should never happened
        return;
    }
    // may drop some data here?
    next_urb->transfer.num_bytes = num_bytes_to_send - num_bytes_to_send % s_uac_dev.spk_as_ifc->bytes_per_packet;
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&next_urb->transfer;
    transfer_dummy->num_isoc_packets = num_bytes_to_send / s_uac_dev.spk_as_ifc->bytes_per_packet;
    for (size_t j = 0; j < transfer_dummy->num_isoc_packets; j++) {
        //We need to initialize each individual isoc packet descriptor of the URB
        next_urb->transfer.isoc_packet_desc[j].num_bytes = s_uac_dev.spk_as_ifc->bytes_per_packet;
    }
    hcd_urb_enqueue(pipe_hdl, next_urb);
    pending_urb[next_index] = NULL;
    --pending_urb_num;
    assert(pending_urb_num <= NUM_ISOC_SPK_URBS);
    ESP_LOGV(TAG, "SPK ST %d",  num_bytes_to_send);
    ESP_LOGV(TAG, "spk payload = %02x %02x...%02x %02x\n", buffer[0], buffer[1], buffer[num_bytes_to_send-2], buffer[num_bytes_to_send-1]);
}

static void _uac_deinit_cb(int time_out_ms)
{
    xEventGroupSetBits(s_usb_dev.event_group, USB_STREAM_TASK_KILL_BIT);
    int counter = time_out_ms;
    while (xEventGroupGetBits(s_usb_dev.event_group) & USB_STREAM_TASK_KILL_BIT) {
        vTaskDelay(pdMS_TO_TICKS(10));
        counter -= 10;
        if (counter <= 0) {
            ESP_LOGE(TAG, "%s uac deinit timeout", __func__);
            break;
        }
    }
    xEventGroupClearBits(s_usb_dev.event_group, USB_STREAM_TASK_KILL_BIT);
}

static void _uac_disconnect_cb(int time_out_ms)
{
    if (xEventGroupGetBits(s_usb_dev.event_group) & (USB_UVC_STREAM_RUNNING | UAC_MIC_STREAM_RUNNING | UAC_SPK_STREAM_RUNNING)) {
        xEventGroupSetBits(s_usb_dev.event_group, USB_STREAM_TASK_RESET_BIT);
        int counter = time_out_ms;
        while (xEventGroupGetBits(s_usb_dev.event_group) & USB_STREAM_TASK_RESET_BIT) {
            vTaskDelay(pdMS_TO_TICKS(10));
            counter -= 10;
            if (counter <= 0) {
                ESP_LOGE(TAG, "%s uac discon timeout", __func__);
                break;
            }
        }
        xEventGroupClearBits(s_usb_dev.event_group, USB_STREAM_TASK_RESET_BIT);
    }
}

static void _usb_stream_handle_task(void *arg)
{
    hcd_port_handle_t port_hdl = (hcd_port_handle_t)arg;
    _uac_device_t *uac_dev = &s_uac_dev;
    _uvc_device_t *uvc_dev = &s_uvc_dev;
    _uvc_stream_handle_t *uvc_stream_hdl = NULL;
    usb_ep_desc_t stream_ep_desc[STREAM_MAX] = {{}};
    bool error_reset = false;

    stream_ep_desc[STREAM_UVC] = (usb_ep_desc_t){
        .bLength = USB_EP_DESC_SIZE,
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
        .bmAttributes = USB_BM_ATTRIBUTES_XFER_ISOC,
        .bInterval = 1,
    };
    stream_ep_desc[STREAM_UAC_MIC] = (usb_ep_desc_t){
        .bLength = USB_EP_DESC_SIZE,
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
        .bmAttributes = USB_BM_ATTRIBUTES_XFER_ISOC,
        .bInterval = 1,
    };
    stream_ep_desc[STREAM_UAC_SPK] = (usb_ep_desc_t){
        .bLength = USB_EP_DESC_SIZE,
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
        .bmAttributes = USB_BM_ATTRIBUTES_XFER_ISOC,
        .bInterval = 1,
    };
    ESP_LOGI(TAG, "usb stream task start");

    if (uac_dev->mic_active) {
        stream_ep_desc[STREAM_UAC_MIC].bEndpointAddress = uac_dev->mic_as_ifc->ep_addr;
        stream_ep_desc[STREAM_UAC_MIC].wMaxPacketSize = uac_dev->mic_as_ifc->ep_mps;
        //prepare urb and data buffer for stream pipe
        uac_dev->mic_as_ifc->urb_list = heap_caps_calloc(uac_dev->mic_as_ifc->urb_num, sizeof(urb_t *), MALLOC_CAP_DMA);
        UVC_CHECK_GOTO(uac_dev->mic_as_ifc->urb_list != NULL, "p_urb alloc failed", free_urb_);
        for (int i = 0; i < uac_dev->mic_as_ifc->urb_num; i++) {
            uac_dev->mic_as_ifc->urb_list[i] = _usb_urb_alloc(uac_dev->mic_as_ifc->packets_per_urb, uac_dev->mic_as_ifc->bytes_per_packet, NULL);
            UVC_CHECK_GOTO(uac_dev->mic_as_ifc->urb_list[i] != NULL, "stream urb alloc failed", free_urb_);
            uac_dev->mic_as_ifc->urb_list[i]->transfer.num_bytes = uac_dev->mic_as_ifc->packets_per_urb * uac_dev->mic_as_ifc->bytes_per_packet;
            for (size_t j = 0; j < uac_dev->mic_as_ifc->packets_per_urb; j++) {
                //We need to initialize each individual isoc packet descriptor of the URB
                uac_dev->mic_as_ifc->urb_list[i]->transfer.isoc_packet_desc[j].num_bytes = uac_dev->mic_as_ifc->bytes_per_packet;
            }
        }
        ESP_LOGI(TAG, "mic stream urb ready");
    }

    if (uac_dev->spk_active) {
        stream_ep_desc[STREAM_UAC_SPK].bEndpointAddress = uac_dev->spk_as_ifc->ep_addr;
        stream_ep_desc[STREAM_UAC_SPK].wMaxPacketSize = uac_dev->spk_as_ifc->ep_mps;
        //prepare urb and data buffer for stream pipe
        uac_dev->spk_as_ifc->urb_list = heap_caps_calloc(uac_dev->spk_as_ifc->urb_num, sizeof(urb_t *), MALLOC_CAP_DMA);
        UVC_CHECK_GOTO(uac_dev->spk_as_ifc->urb_list != NULL, "p_urb alloc failed", free_urb_);
        for (int i = 0; i < uac_dev->spk_as_ifc->urb_num; i++) {
            uac_dev->spk_as_ifc->urb_list[i] = _usb_urb_alloc(uac_dev->spk_as_ifc->packets_per_urb, uac_dev->spk_as_ifc->bytes_per_packet, NULL);
            UVC_CHECK_GOTO(uac_dev->spk_as_ifc->urb_list[i] != NULL, "stream urb alloc failed", free_urb_);
            uac_dev->spk_as_ifc->urb_list[i]->transfer.num_bytes = 0;
            for (size_t j = 0; j < uac_dev->spk_as_ifc->packets_per_urb; j++) {
                //We need to initialize each individual isoc packet descriptor of the URB
                uac_dev->spk_as_ifc->urb_list[i]->transfer.isoc_packet_desc[j].num_bytes = 0;
            }
        }
        ESP_LOGI(TAG, "spk stream urb ready");
    }

    if (uvc_dev->active) {
        stream_ep_desc[STREAM_UVC].bEndpointAddress = uvc_dev->vs_ifc->ep_addr;
        stream_ep_desc[STREAM_UVC].wMaxPacketSize = uvc_dev->vs_ifc->ep_mps;
        //prepare urb and data buffer for stream pipe
        uvc_dev->vs_ifc->urb_list = heap_caps_calloc(uvc_dev->vs_ifc->urb_num, sizeof(urb_t *), MALLOC_CAP_DMA);
        UVC_CHECK_GOTO(uvc_dev->vs_ifc->urb_list != NULL, "p_urb alloc failed", free_urb_);

        if (s_uvc_dev.xfer_type == UVC_XFER_BULK) {
            stream_ep_desc[STREAM_UVC].bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK;
            stream_ep_desc[STREAM_UVC].bInterval = 0;
            for (int i = 0; i < uvc_dev->vs_ifc->urb_num; i++) {
                uvc_dev->vs_ifc->urb_list[i] = _usb_urb_alloc(0, uvc_dev->vs_ifc->bytes_per_packet, NULL);
                UVC_CHECK_GOTO(uvc_dev->vs_ifc->urb_list[i] != NULL, "stream urb alloc failed", free_urb_);
                uvc_dev->vs_ifc->urb_list[i]->transfer.num_bytes = uvc_dev->vs_ifc->bytes_per_packet;
            }
        } else {
            for (int i = 0; i < uvc_dev->vs_ifc->urb_num; i++) {
                uvc_dev->vs_ifc->urb_list[i] = _usb_urb_alloc(uvc_dev->vs_ifc->packets_per_urb, uvc_dev->vs_ifc->bytes_per_packet, NULL);
                UVC_CHECK_GOTO(uvc_dev->vs_ifc->urb_list[i] != NULL, "stream urb alloc failed", free_urb_);
                uvc_dev->vs_ifc->urb_list[i]->transfer.num_bytes = uvc_dev->vs_ifc->packets_per_urb * uvc_dev->vs_ifc->bytes_per_packet;

                for (size_t j = 0; j < uvc_dev->vs_ifc->packets_per_urb; j++) {
                    //We need to initialize each individual isoc packet descriptor of the URB
                    uvc_dev->vs_ifc->urb_list[i]->transfer.isoc_packet_desc[j].num_bytes = uvc_dev->vs_ifc->bytes_per_packet;
                }
            }
        }
        ESP_LOGI(TAG, "uvc stream urb ready");
        // start uvc streaming
        uvc_error_t uvc_ret = UVC_SUCCESS;
        uvc_ret = uvc_stream_open_ctrl((uvc_device_handle_t *)(&uvc_dev), &uvc_stream_hdl, &uvc_dev->ctrl_probed);
        UVC_CHECK_GOTO(uvc_ret == UVC_SUCCESS, "open stream failed", free_stream_);
        uvc_ret = uvc_stream_start(uvc_stream_hdl, uvc_dev->user_cb, uvc_dev->user_ptr, 0);
        UVC_CHECK_GOTO(uvc_ret == UVC_SUCCESS, "start stream failed", free_stream_);
    }

    // Waiting for usbh ready
    while (!(xEventGroupGetBits(uvc_dev->parent->event_group) & (USB_STREAM_TASK_KILL_BIT | USB_STREAM_TASK_RESET_BIT))) {
        if (ulTaskNotifyTake(pdTRUE, 1) == 0 && error_reset == false) {
            continue;
        }
        // Create pipe when device connect
        xQueueReset(uvc_dev->parent->stream_queue_hdl);
        error_reset = false;

        if (uac_dev->mic_active) {
            ESP_LOGI(TAG, "Creating mic in pipe itf = %d, ep = 0x%02X, ", uac_dev->mic_as_ifc->interface, uac_dev->mic_as_ifc->ep_addr);
            uac_dev->mic_as_ifc->pipe_handle = _usb_pipe_init(port_hdl, &stream_ep_desc[STREAM_UAC_MIC], uvc_dev->parent->dev_addr, uvc_dev->parent->dev_speed,
                                          (void *)uvc_dev->parent->stream_queue_hdl, (void *)uvc_dev->parent->stream_queue_hdl);
            assert( uac_dev->mic_as_ifc->pipe_handle != NULL);
            for (size_t i = 0; i < uac_dev->mic_as_ifc->urb_num; i++) {
                esp_err_t ret = hcd_urb_enqueue( uac_dev->mic_as_ifc->pipe_handle, uac_dev->mic_as_ifc->urb_list[i]);
                assert(ESP_OK == ret);
            }
            xEventGroupSetBits(uvc_dev->parent->event_group, UAC_MIC_STREAM_RUNNING);
            ESP_LOGI(TAG, "mic streaming...");
        }

        if (uac_dev->spk_active) {
            ESP_LOGI(TAG, "Creating spk out pipe itf = %d, ep = 0x%02X", uac_dev->spk_as_ifc->interface, uac_dev->spk_as_ifc->ep_addr);
            uac_dev->spk_as_ifc->pipe_handle = _usb_pipe_init(port_hdl, &stream_ep_desc[STREAM_UAC_SPK], uvc_dev->parent->dev_addr, uvc_dev->parent->dev_speed,
                                          (void *)uvc_dev->parent->stream_queue_hdl, (void *)uvc_dev->parent->stream_queue_hdl);
            assert(uac_dev->spk_as_ifc->pipe_handle != NULL);
            // No need to enqueue init spk packets?
            for (size_t i = 0; i < uac_dev->spk_as_ifc->urb_num; i++) {
                esp_err_t ret = hcd_urb_enqueue(uac_dev->spk_as_ifc->pipe_handle, uac_dev->spk_as_ifc->urb_list[i]);
                assert(ESP_OK == ret);
            }
            xEventGroupSetBits(uvc_dev->parent->event_group, UAC_SPK_STREAM_RUNNING);
            ESP_LOGI(TAG, "spk streaming...");
        }

        if (uvc_dev->active) {
            ESP_LOGI(TAG, "Creating uvc in(%s) pipe itf = %d-%d, ep = 0x%02X", s_uvc_dev.xfer_type==UVC_XFER_ISOC?"isoc":"bulk", uvc_dev->vs_ifc->interface, uvc_dev->vs_ifc->interface_alt, uvc_dev->vs_ifc->ep_addr);
            // Initialize uvc stream pipe
            uvc_dev->vs_ifc->pipe_handle = _usb_pipe_init(port_hdl, &stream_ep_desc[STREAM_UVC], uvc_dev->parent->dev_addr,
                        uvc_dev->parent->dev_speed, (void *)uvc_dev->parent->stream_queue_hdl, (void *)uvc_dev->parent->stream_queue_hdl);
            assert(uvc_dev->vs_ifc->pipe_handle != NULL);
            for (int i = 0; i < uvc_dev->vs_ifc->urb_num; i++) {
                _usb_urb_context_update(uvc_dev->vs_ifc->urb_list[i], uvc_stream_hdl);
                esp_err_t ret = hcd_urb_enqueue(uvc_dev->vs_ifc->pipe_handle, uvc_dev->vs_ifc->urb_list[i]);
                assert(ESP_OK == ret);
            }
            xEventGroupSetBits(uvc_dev->parent->event_group, USB_UVC_STREAM_RUNNING);
            ESP_LOGI(TAG, "uvc streaming...");
        }

        _processing_spk_pipe(uac_dev->spk_as_ifc->pipe_handle, false, true);
        while (!(xEventGroupGetBits(uvc_dev->parent->event_group) & (USB_STREAM_TASK_KILL_BIT | USB_STREAM_TASK_RESET_BIT))) {
            _uvc_event_msg_t evt_msg = {};
            if (xQueueReceive(uvc_dev->parent->stream_queue_hdl, &evt_msg, 1) != pdTRUE) {
                _processing_spk_pipe(uac_dev->spk_as_ifc->pipe_handle, false, false);
                continue;
            }
            switch (evt_msg._type) {
            case USER_EVENT:
                if (evt_msg._event.user_cmd == STREAM_SUSPEND) {
                    _stream_ifc_t *p_itf = (_stream_ifc_t *)evt_msg._handle.user_hdl;
                    if (p_itf->pipe_handle) {
                        xEventGroupClearBits(uvc_dev->parent->event_group, p_itf->evt_bit);
                        ESP_LOGI(TAG, "deinit %s pipe", p_itf->name);
                        _usb_pipe_deinit(p_itf->pipe_handle, p_itf->urb_num);
                        p_itf->pipe_handle = NULL;
                    }
                    if (p_itf->type == STREAM_UAC_SPK) {
                        _processing_spk_pipe(uac_dev->spk_as_ifc->pipe_handle, false, true);
                        _ring_buffer_flush(uac_dev->spk_ringbuf_hdl);
                    }
                    if (p_itf->type == STREAM_UAC_MIC && uac_dev->mic_ringbuf_hdl) {
                        _ring_buffer_flush(uac_dev->mic_ringbuf_hdl);
                    }
                    xEventGroupSetBits(uvc_dev->parent->event_group, USB_STREAM_TASK_PROC_SUCCEED);
                } else if (evt_msg._event.user_cmd == STREAM_RESUME) {
                    _stream_ifc_t *p_itf = (_stream_ifc_t *)evt_msg._handle.user_hdl;
                    if (p_itf->pipe_handle == NULL) {
                        ESP_LOGI(TAG, "Creating %s pipe itf = %d, ep = 0x%02X", p_itf->name, p_itf->interface, p_itf->ep_addr);
                        p_itf->pipe_handle = _usb_pipe_init(port_hdl, &stream_ep_desc[p_itf->type], uvc_dev->parent->dev_addr, uvc_dev->parent->dev_speed,
                                                    (void *)uvc_dev->parent->stream_queue_hdl, (void *)uvc_dev->parent->stream_queue_hdl);
                        assert(p_itf->pipe_handle != NULL);
                        for (size_t i = 0; i < p_itf->urb_num; i++) {
                            _usb_urb_clear(p_itf->urb_list[i], p_itf->packets_per_urb);
                            if (p_itf->type == STREAM_UAC_SPK) {
                                p_itf->urb_list[i]->transfer.num_bytes = 0;
                                for (size_t j = 0; j < p_itf->packets_per_urb; j++) {
                                    //We need to initialize each individual isoc packet descriptor of the URB
                                    p_itf->urb_list[i]->transfer.isoc_packet_desc[j].num_bytes = 0;
                                }
                            }
                            if (p_itf->type == STREAM_UAC_MIC || (p_itf->type == STREAM_UVC && p_itf->xfer_type == UVC_XFER_ISOC)) {
                                p_itf->urb_list[i]->transfer.num_bytes = p_itf->packets_per_urb * p_itf->bytes_per_packet;
                                for (size_t j = 0; j < p_itf->packets_per_urb; j++) {
                                    p_itf->urb_list[i]->transfer.isoc_packet_desc[j].num_bytes = p_itf->bytes_per_packet;
                                }
                                printf("p_itf->urb_list[i]->transfer.num_bytes = %d\n", p_itf->urb_list[i]->transfer.num_bytes);
                            }
                            if (p_itf->type == STREAM_UVC ) {
                                if( p_itf->xfer_type == UVC_XFER_BULK) {
                                    p_itf->urb_list[i]->transfer.num_bytes = p_itf->bytes_per_packet;
                                }
                            }
                            esp_err_t ret = hcd_urb_enqueue(p_itf->pipe_handle, p_itf->urb_list[i]);
                            assert(ESP_OK == ret);
                        }
                        xEventGroupSetBits(uvc_dev->parent->event_group, p_itf->evt_bit);
                    }
                    xEventGroupSetBits(uvc_dev->parent->event_group, USB_STREAM_TASK_PROC_SUCCEED);
                }
                break;
            case PIPE_EVENT:
                _pipe_event_dflt_process(evt_msg._handle.pipe_handle, evt_msg._handle.pipe_handle == uac_dev->mic_as_ifc->pipe_handle?"mic"
                                        :(evt_msg._handle.pipe_handle == uac_dev->spk_as_ifc->pipe_handle?"spk"
                                        :(evt_msg._handle.pipe_handle == uvc_dev->vs_ifc->pipe_handle?"uvc":"null")), evt_msg._event.pipe_event);
                switch (evt_msg._event.pipe_event) {
                case HCD_PIPE_EVENT_URB_DONE:
                    if (evt_msg._handle.pipe_handle == uac_dev->mic_as_ifc->pipe_handle) {
                        _processing_mic_pipe(uac_dev->mic_as_ifc->pipe_handle, uac_dev->user_cb, uac_dev->user_ptr, true);
                    } else if (evt_msg._handle.pipe_handle == uac_dev->spk_as_ifc->pipe_handle) {
                        _processing_spk_pipe(uac_dev->spk_as_ifc->pipe_handle, true, false);
                    } else if (evt_msg._handle.pipe_handle == uvc_dev->vs_ifc->pipe_handle) {
                        _processing_uvc_pipe(uvc_dev->vs_ifc->pipe_handle, true);
                    }
                    break;
                case HCD_PIPE_EVENT_ERROR_XFER:
                    error_reset = true;
                    goto _usb_stream_reset;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
_usb_stream_reset:
        ESP_LOGI(TAG, "usb stream task reset");
        xEventGroupClearBits(uvc_dev->parent->event_group, USB_UVC_STREAM_RUNNING | UAC_SPK_STREAM_RUNNING | UAC_MIC_STREAM_RUNNING);

        ESP_LOGI(TAG, "Resetting stream Driver");
        if (uac_dev->mic_as_ifc->pipe_handle) {
            ESP_LOGI(TAG, "Resetting mic pipe");
            _usb_pipe_deinit(uac_dev->mic_as_ifc->pipe_handle, uac_dev->mic_as_ifc->urb_num);
            uac_dev->mic_as_ifc->pipe_handle = NULL;
        }
        if (uac_dev->spk_as_ifc->pipe_handle) {
            ESP_LOGI(TAG, "Resetting spk pipe");
            _usb_pipe_deinit(uac_dev->spk_as_ifc->pipe_handle, uac_dev->spk_as_ifc->urb_num);
            uac_dev->spk_as_ifc->pipe_handle = NULL;
        }
        if (uvc_dev->vs_ifc->pipe_handle) {
            ESP_LOGI(TAG, "Resetting uvc pipe");
            _usb_pipe_deinit(uvc_dev->vs_ifc->pipe_handle, uvc_dev->vs_ifc->urb_num);
            uvc_dev->vs_ifc->pipe_handle = NULL;
        }
        _ring_buffer_flush(uac_dev->spk_ringbuf_hdl);
        _ring_buffer_flush(uac_dev->mic_ringbuf_hdl);
        xEventGroupClearBits(uvc_dev->parent->event_group, USB_STREAM_TASK_RESET_BIT);
        ESP_LOGI(TAG, "usb stream task reset done");
    } //handle hotplug
free_stream_:
    if (uvc_stream_hdl) {
        uvc_stream_stop(uvc_stream_hdl);
        uvc_stream_close(uvc_stream_hdl);
    }
    uvc_stream_hdl = NULL;
free_urb_:
    if (uvc_dev->vs_ifc->urb_list) {
        for (size_t i = 0; i < uvc_dev->vs_ifc->urb_num; i++) {
            _usb_urb_free(uvc_dev->vs_ifc->urb_list[i]);
            uvc_dev->vs_ifc->urb_list[i] = NULL;
        }
        free(uvc_dev->vs_ifc->urb_list);
        uvc_dev->vs_ifc->urb_list = NULL;
    }
    if (uac_dev->mic_as_ifc->urb_list) {
        for (size_t i = 0; i < uac_dev->mic_as_ifc->urb_num; i++) {
            _usb_urb_free(uac_dev->mic_as_ifc->urb_list[i]);
            uac_dev->mic_as_ifc->urb_list[i] = NULL;
        }
        free(uac_dev->mic_as_ifc->urb_list);
        uac_dev->mic_as_ifc->urb_list = NULL;
    }
    if (uac_dev->spk_as_ifc->urb_list) {
        for (size_t i = 0; i < uac_dev->spk_as_ifc->urb_num; i++) {
            _usb_urb_free(uac_dev->spk_as_ifc->urb_list[i]);
            uac_dev->spk_as_ifc->urb_list[i] = NULL;
        }
        free(uac_dev->spk_as_ifc->urb_list);
        uac_dev->spk_as_ifc->urb_list = NULL;
    }
    xEventGroupClearBits(uvc_dev->parent->event_group, USB_STREAM_TASK_KILL_BIT | USB_UVC_STREAM_RUNNING | UAC_SPK_STREAM_RUNNING | UAC_MIC_STREAM_RUNNING);
    ESP_LOGI(TAG, "audio task deleted");
    ESP_LOGD(TAG, "audio task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}

static void _usb_processing_task(void *arg)
{
    _uvc_device_t *uvc_dev = &s_uvc_dev;
    _uac_device_t *uac_dev = &s_uac_dev;
    esp_err_t ret = ESP_OK;
    usb_speed_t port_speed = 0;
    hcd_port_handle_t port_hdl = NULL;
    hcd_port_event_t port_actual_evt;
    hcd_pipe_handle_t pipe_hdl_dflt = NULL;
    _uvc_event_msg_t evt_msg = {};
    bool keep_running = true;


    //Waitting for camera ready
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    port_hdl = _usb_port_init((void *)uvc_dev->parent->queue_hdl, (void *)uvc_dev->parent->queue_hdl);
    UVC_CHECK_GOTO(port_hdl != NULL, "USB Port init failed", free_task_);
    xEventGroupSetBits(uvc_dev->parent->event_group, USB_HOST_INIT_DONE);
    while (keep_running) {
        ESP_LOGI(TAG, "Waiting USB Connection");

        if (hcd_port_get_state(port_hdl) == HCD_PORT_STATE_NOT_POWERED) {
            ret = hcd_port_command(port_hdl, HCD_PORT_CMD_POWER_ON);
            UVC_CHECK_GOTO(ESP_OK == ret, "Port Set POWER_ON failed", usb_driver_reset_);
        }

        while (1) {
            if (xQueueReceive(uvc_dev->parent->queue_hdl, &evt_msg, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            switch (evt_msg._type) {
            case PORT_EVENT:
                port_actual_evt = _usb_port_event_dflt_process(port_hdl, evt_msg._event.port_event);
                switch (port_actual_evt) {
                case HCD_PORT_EVENT_CONNECTION:
                    // wait for device ready
                    vTaskDelay(pdMS_TO_TICKS(50));
                    // Reset port and get speed
                    ESP_LOGI(TAG, "Resetting Port");
                    ret = hcd_port_command(port_hdl, HCD_PORT_CMD_RESET);
                    UVC_CHECK_GOTO(ESP_OK == ret, "Port Reset failed", usb_driver_reset_);
                    ESP_LOGI(TAG, "Setting Port FIFO");
                    ret = hcd_port_set_fifo_bias(port_hdl, HCD_PORT_FIFO_BIAS_RX);
                    UVC_CHECK_GOTO(ESP_OK == ret, "Port Set fifo failed", usb_driver_reset_);
                    ESP_LOGI(TAG, "Getting Port Speed");
                    ret = _usb_port_get_speed(port_hdl, &port_speed);
                    UVC_CHECK_GOTO(ESP_OK == ret, "USB Port get speed failed", usb_driver_reset_);
                    ESP_LOGI(TAG, "USB Speed: %s-speed", port_speed ? "full" : "low");
                    uvc_dev->parent->dev_speed = port_speed;
                    // Initialize default pipe
                    if (!pipe_hdl_dflt) pipe_hdl_dflt = _usb_pipe_init(port_hdl, NULL, 0, port_speed,
                                                            (void *)uvc_dev->parent->queue_hdl, (void *)uvc_dev->parent->queue_hdl);

                    UVC_CHECK_GOTO(pipe_hdl_dflt != NULL, "default pipe create failed", usb_driver_reset_);
                    // UVC enum process
                    ret = _usb_set_device_addr(pipe_hdl_dflt, uvc_dev->parent->dev_addr);
                    UVC_CHECK_GOTO(ESP_OK == ret, "Set device address failed", usb_driver_reset_);
#ifdef CONFIG_UVC_GET_DEVICE_DESC
                    ret = _usb_get_dev_desc(pipe_hdl_dflt, NULL);
                    UVC_CHECK_GOTO(ESP_OK == ret, "Get device descriptor failed", usb_driver_reset_);
#endif
#ifdef CONFIG_UVC_GET_CONFIG_DESC
                    ret = _usb_get_config_desc(pipe_hdl_dflt, uvc_dev->parent->configuration, NULL);
                    UVC_CHECK_GOTO(ESP_OK == ret, "Get config descriptor failed", usb_driver_reset_);
#endif
                    ret = _usb_set_device_config(pipe_hdl_dflt, uvc_dev->parent->configuration);
                    UVC_CHECK_GOTO(ESP_OK == ret, "Set device configuration failed", usb_driver_reset_);
                    if (uvc_dev->active) {
                        // UVC class specified enum process
                        ret = _uvc_vs_commit_control(pipe_hdl_dflt, &uvc_dev->ctrl_set, &uvc_dev->ctrl_probed);
                        UVC_CHECK_GOTO(ESP_OK == ret, "UVC negotiate failed", usb_driver_reset_);
                        ret = _usb_set_device_interface(pipe_hdl_dflt, uvc_dev->vs_ifc->interface, uvc_dev->vs_ifc->interface_alt);
                        UVC_CHECK_GOTO(ESP_OK == ret, "Set device interface failed", usb_driver_reset_);
                    }
                    if (uac_dev->mic_active) {
                        ret = _usb_set_device_interface(pipe_hdl_dflt, uac_dev->mic_as_ifc->interface, uac_dev->mic_as_ifc->interface_alt);
                        UVC_CHECK_GOTO(ESP_OK == ret, "Set device interface failed", usb_driver_reset_);
                        if (uac_dev->ac_interface && uac_dev->mic_fu_id) {
                            ret = _uac_as_control_set_mute(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->mic_fu_id, uac_dev->mic_mute);
                            UVC_CHECK_GOTO(ESP_OK == ret, "mic mute set failed", usb_driver_reset_);
                            ret = _uac_as_control_set_volume(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->mic_fu_id, uac_dev->mic_volume);
                            UVC_CHECK_GOTO(ESP_OK == ret, "mic volume set failed", usb_driver_reset_);
                        }
                        ret = _uac_as_control_set_freq(pipe_hdl_dflt, uac_dev->mic_as_ifc->ep_addr, uac_dev->mic_samples_frequence);
                        UVC_CHECK_GOTO(ESP_OK == ret, "mic frequence set failed", usb_driver_reset_);
                    }
                    if (uac_dev->spk_active) {
                        ret = _usb_set_device_interface(pipe_hdl_dflt, uac_dev->spk_as_ifc->interface, uac_dev->spk_as_ifc->interface_alt);
                        UVC_CHECK_GOTO(ESP_OK == ret, "Set device interface failed", usb_driver_reset_);
                        if (uac_dev->ac_interface && uac_dev->spk_fu_id) {
                            ret = _uac_as_control_set_mute(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->spk_fu_id, uac_dev->spk_mute);
                            UVC_CHECK_GOTO(ESP_OK == ret, "spk mute set failed", usb_driver_reset_);
                            ret = _uac_as_control_set_volume(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->spk_fu_id, uac_dev->spk_volume);
                            UVC_CHECK_GOTO(ESP_OK == ret, "spk volume set failed", usb_driver_reset_);
                        }
                        ret = _uac_as_control_set_freq(pipe_hdl_dflt, uac_dev->spk_as_ifc->ep_addr, uac_dev->spk_samples_frequence);
                        UVC_CHECK_GOTO(ESP_OK == ret, "mic frequence set failed", usb_driver_reset_);
                    }
                    if (uac_dev->mic_active || uac_dev->spk_active || uvc_dev->active) {
                        if (uac_dev->parent->vs_as_taskh == NULL) {
                            xTaskCreatePinnedToCore(_usb_stream_handle_task, UAC_TASK_NAME, UAC_TASK_STACK_SIZE, (void *)port_hdl,
                                                    UAC_TASK_PRIORITY, &uac_dev->parent->vs_as_taskh, UAC_TASK_CORE);
                            UVC_CHECK_GOTO(uac_dev->parent->vs_as_taskh != NULL, "Create usb task failed", usb_driver_reset_);
                        }
                        xTaskNotifyGive(uac_dev->parent->vs_as_taskh);
                    }
                    xEventGroupSetBits(uvc_dev->parent->event_group, USB_DEVICE_CONNECTED);
                    break;
                case HCD_PORT_EVENT_DISCONNECTION:
                    ESP_LOGD(TAG, "hcd port state after disconnect = %d", hcd_port_get_state(port_hdl));
                    xEventGroupClearBits(uvc_dev->parent->event_group, USB_DEVICE_CONNECTED);
                    if (uac_dev->parent->vs_as_taskh != NULL) {
                        _uac_disconnect_cb(3000);
                    }
                    goto usb_driver_reset_;
                    break;
                default:
                    break;
                }
                break;

            case PIPE_EVENT:
                if (evt_msg._handle.pipe_handle == pipe_hdl_dflt) {
                    hcd_pipe_event_t dflt_pipe_evt = _pipe_event_dflt_process(pipe_hdl_dflt, "default", evt_msg._event.pipe_event);
                    switch (dflt_pipe_evt) {
                    case HCD_PIPE_EVENT_ERROR_STALL:
                    case HCD_PIPE_EVENT_ERROR_XFER:
                        goto usb_driver_reset_;
                        break;
                    default:
                        break;
                    }
                }
                break;

            case USER_EVENT:
                if (evt_msg._event.user_cmd == STREAM_SUSPEND) {
                    _stream_ifc_t *p_itf = (_stream_ifc_t *)evt_msg._handle.user_hdl;
                    ret = _usb_set_device_interface(pipe_hdl_dflt, p_itf->interface, 0);
                    UVC_CHECK_GOTO(ESP_OK == ret, "suspend alt_interface failed", suspend_failed_driver_reset_);
                    xQueueSend(uvc_dev->parent->stream_queue_hdl, &evt_msg, portMAX_DELAY);
                    EventBits_t uxBits = xEventGroupWaitBits(uvc_dev->parent->event_group, (USB_STREAM_TASK_PROC_SUCCEED), pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND));
                    UVC_CHECK_GOTO(uxBits & USB_STREAM_TASK_PROC_SUCCEED, "stream suspend failed", suspend_failed_driver_reset_);
                    xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_SUCCEED);
                    ESP_LOGI(TAG, "%s interface(%u) suspended: alt set to 0!", p_itf->name, p_itf->interface);
                } else if (evt_msg._event.user_cmd == STREAM_RESUME) {
                    _stream_ifc_t *p_itf = (_stream_ifc_t *)evt_msg._handle.user_hdl;
                    if (p_itf->type == STREAM_UVC) {
                        ret = _uvc_vs_commit_control(pipe_hdl_dflt, &uvc_dev->ctrl_set, &uvc_dev->ctrl_probed);
                        UVC_CHECK_GOTO(ESP_OK == ret, "vs commit failed", resume_failed_driver_reset_);
                    }
                    ret = _usb_set_device_interface(pipe_hdl_dflt, p_itf->interface, p_itf->interface_alt);
                    UVC_CHECK_GOTO(ESP_OK == ret, "resume alt_interface failed", resume_failed_driver_reset_);
                    xQueueSend(uvc_dev->parent->stream_queue_hdl, &evt_msg, portMAX_DELAY);
                    EventBits_t uxBits = xEventGroupWaitBits(uvc_dev->parent->event_group, (USB_STREAM_TASK_PROC_SUCCEED), pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND));
                    UVC_CHECK_GOTO(uxBits & USB_STREAM_TASK_PROC_SUCCEED, "stream suspend failed", resume_failed_driver_reset_);
                    xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_SUCCEED);
                    ESP_LOGI(TAG, "%s interface(%u) resumed: alt set to %u!", p_itf->name, p_itf->interface, p_itf->interface_alt);
                } else if (evt_msg._event.user_cmd == UAC_MUTE) {
                    _stream_ifc_t *p_itf = (_stream_ifc_t *)evt_msg._handle.user_hdl;
                    bool if_mute = (bool)evt_msg._event_data;
                    if (p_itf->type == STREAM_UAC_MIC) {
                        ret = _uac_as_control_set_mute(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->mic_fu_id, if_mute);
                    } else {
                        ret = _uac_as_control_set_mute(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->spk_fu_id, if_mute);
                    }
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "%s set %s succeed", p_itf->name, if_mute?"mute":"un-mute");
                        xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_SUCCEED);
                    } else {
                        ESP_LOGW(TAG, "%s set %s failed", p_itf->name, if_mute?"mute":"un-mute");
                        xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_FAILED);
                    }
                } else if (evt_msg._event.user_cmd == UAC_VOLUME) {
                    _stream_ifc_t *p_itf = (_stream_ifc_t *)evt_msg._handle.user_hdl;
                    uint32_t volume = (uint32_t)evt_msg._event_data;
                    if (p_itf->type == STREAM_UAC_MIC) {
                        ret = _uac_as_control_set_volume(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->mic_fu_id, volume);
                    } else {
                        ret = _uac_as_control_set_volume(pipe_hdl_dflt, uac_dev->ac_interface, uac_dev->spk_fu_id, volume);
                    }
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "%s set volume level=%"PRIu32" succeed", p_itf->name, volume);
                        xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_SUCCEED);
                    } else {
                        ESP_LOGW(TAG, "%s set volume level=%"PRIu32" failed", p_itf->name, volume);
                        xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_FAILED);
                    }
                } else if (evt_msg._event.user_cmd == STREAM_STOP) {
                    keep_running = false;
                    goto usb_driver_reset_;
                } else {
                    ESP_LOGW(TAG, "Undefined User Event");
                }
                break;
            default:
                break;
            }
        }

suspend_failed_driver_reset_:
        ESP_LOGE(TAG, "Streaming suspend failed");
        xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_FAILED);
        goto usb_driver_reset_;
resume_failed_driver_reset_:
        ESP_LOGE(TAG, "Streaming resume failed");
        xEventGroupSetBits(uvc_dev->parent->event_group, USB_CTRL_PROC_FAILED);
usb_driver_reset_:
        if (uac_dev->parent->vs_as_taskh) {
            _uac_deinit_cb(2000);
            uac_dev->parent->vs_as_taskh = NULL;
        }

        xEventGroupClearBits(uvc_dev->parent->event_group, USB_DEVICE_CONNECTED);
        hcd_port_handle_event(port_hdl);
        ESP_LOGI(TAG, "Resetting USB Driver");
        if (pipe_hdl_dflt) {
            ESP_LOGI(TAG, "Resetting default pipe");
            _usb_pipe_deinit(pipe_hdl_dflt, 1);
            pipe_hdl_dflt = NULL;
        }
        ESP_LOGI(TAG, "hcd recovering");
        hcd_port_recover(port_hdl);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "hcd port state after recovering = %d", hcd_port_get_state(port_hdl));
        if (_user_event_wait(port_hdl, STREAM_STOP, pdMS_TO_TICKS(200)) == ESP_OK) {
            ESP_LOGI(TAG, "handle user stop command");
            break;
        }
    }

    /* delete_usb_driver_: */
    if (port_hdl) {
        _usb_port_deinit(port_hdl);
        port_hdl = NULL;
    }
    xEventGroupClearBits(uvc_dev->parent->event_group, USB_HOST_INIT_DONE);
    xEventGroupSetBits(uvc_dev->parent->event_group, USB_HOST_STOP_DONE);
free_task_:
    ESP_LOGI(TAG, "_usb_processing_task deleted");
    ESP_LOGD(TAG, "_usb_processing_task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}

/*populate frame then call user callback*/
static void _sample_processing_task(void *arg)
{
    assert(arg != NULL);
    _uvc_stream_handle_t *strmh = (_uvc_stream_handle_t *)(arg);
    _uvc_device_t *device_handle = (_uvc_device_t *)(strmh->devh);
    uint32_t last_seq = 0;

    xEventGroupClearBits(device_handle->parent->event_group, UVC_SAMPLE_PROC_STOP_DONE);

    do {
        xSemaphoreTake(strmh->cb_mutex, portMAX_DELAY);

        while (strmh->running && last_seq == strmh->hold_seq) {
            xSemaphoreGive(strmh->cb_mutex);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ESP_LOGV(TAG, "GOT LENGTH %d", strmh->hold_bytes);
            xSemaphoreTake(strmh->cb_mutex, portMAX_DELAY);
        }

        TRIGGER_NEW_FRAME();

        if (!strmh->running) {
            xSemaphoreGive(strmh->cb_mutex);
            ESP_LOGI(TAG, "sample processing stop");
            break;
        }

        last_seq = strmh->hold_seq;
        _uvc_populate_frame(strmh);
        xSemaphoreGive(strmh->cb_mutex);
        //user callback for decode and display,
        strmh->user_cb(&strmh->frame, strmh->user_ptr);
        /* code */
    } while (1);

    xEventGroupSetBits(device_handle->parent->event_group, UVC_SAMPLE_PROC_STOP_DONE);

    ESP_LOGI(TAG, "_sample_processing_task deleted");
    ESP_LOGD(TAG, "_sample_processing_task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelete(NULL);
}

/***************************************************** Public API *********************************************************************/
esp_err_t uac_streaming_config(const uac_config_t *config)
{
    if (config->spk_interface) {
        UVC_CHECK(!(config->spk_ep_addr & 0x80), "spk endpoint direction must OUT", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->spk_buf_size > config->spk_ep_mps, "spk buffer size should larger than endpoint size", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->spk_ep_mps, "speaker endpoint mps must > 0", ESP_ERR_INVALID_ARG);
    }
    if (config->mic_interface) {
        UVC_CHECK(config->mic_min_bytes == 0 || ((config->mic_ep_mps <= config->mic_min_bytes) && 
                (config->mic_min_bytes <= config->mic_ep_mps * 32)), "mic_min_bytes can't lower than mic_ep_mps, or more than 32*mic_ep_mps", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->mic_ep_addr & 0x80, "mic endpoint direction must IN", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->mic_ep_mps, "mic endpoint mps must > 0", ESP_ERR_INVALID_ARG);
    }
    s_uac_dev.mic_bit_resolution = config->mic_bit_resolution;
    s_uac_dev.mic_samples_frequence = config->mic_samples_frequence;
    s_uac_dev.mic_min_bytes = config->mic_min_bytes;
    s_uac_dev.spk_bit_resolution = config->spk_bit_resolution;
    s_uac_dev.spk_samples_frequence = config->spk_samples_frequence;
    s_uac_dev.user_cb = config->mic_cb;
    s_uac_dev.user_ptr = config->mic_cb_arg;
    s_uac_dev.spk_buf_size = config->spk_buf_size;
    s_uac_dev.mic_buf_size = config->mic_buf_size;
    s_uac_dev.ac_interface = config->ac_interface;
    s_uac_dev.mic_fu_id = config->mic_fu_id;
    s_uac_dev.spk_fu_id = config->spk_fu_id;
    s_uac_dev.spk_mute = 0;
    s_uac_dev.mic_mute = 0;
    s_uac_dev.spk_volume = UAC_VOLUME_LEVEL_DEFAULT;
    s_uac_dev.mic_volume = UAC_VOLUME_LEVEL_DEFAULT;

    if (config->mic_interface && s_uac_dev.mic_min_bytes == 0) {
        //IF mic_min_bytes not set, using config->mic_ep_mps * UAC_MIC_CB_MIN_MS_DEFAULT by default
        s_uac_dev.mic_min_bytes = config->mic_ep_mps * UAC_MIC_CB_MIN_MS_DEFAULT;
        ESP_LOGI(TAG, "mic_buf_size set to default %"PRIu32"*%d = %"PRIu32, config->mic_ep_mps, UAC_MIC_CB_MIN_MS_DEFAULT, config->mic_ep_mps*UAC_MIC_CB_MIN_MS_DEFAULT);
        if (s_uac_dev.mic_buf_size < s_uac_dev.mic_min_bytes) {
            s_uac_dev.mic_buf_size = s_uac_dev.mic_min_bytes;
            ESP_LOGW(TAG, "mic_buf_size should bigger than mic_min_bytes");
        }
    }

    s_usb_itf[STREAM_UAC_MIC].type = STREAM_UAC_MIC;
    s_usb_itf[STREAM_UAC_MIC].name = "MIC";
    s_usb_itf[STREAM_UAC_MIC].xfer_type = UVC_XFER_ISOC;
    s_usb_itf[STREAM_UAC_MIC].interface = config->mic_interface;
    s_usb_itf[STREAM_UAC_MIC].interface_alt = 1;
    s_usb_itf[STREAM_UAC_MIC].ep_addr = config->mic_ep_addr;
    s_usb_itf[STREAM_UAC_MIC].ep_mps = config->mic_ep_mps;
    s_usb_itf[STREAM_UAC_MIC].evt_bit = UAC_MIC_STREAM_RUNNING;
    s_usb_itf[STREAM_UAC_MIC].urb_num = NUM_ISOC_MIC_URBS;
    s_usb_itf[STREAM_UAC_MIC].bytes_per_packet = config->mic_ep_mps;

    s_usb_itf[STREAM_UAC_SPK].type = STREAM_UAC_SPK;
    s_usb_itf[STREAM_UAC_SPK].name = "SPK";
    s_usb_itf[STREAM_UAC_SPK].xfer_type = UVC_XFER_ISOC;
    s_usb_itf[STREAM_UAC_SPK].interface = config->spk_interface;
    s_usb_itf[STREAM_UAC_SPK].interface_alt = 1;
    s_usb_itf[STREAM_UAC_SPK].ep_addr = config->spk_ep_addr;
    s_usb_itf[STREAM_UAC_SPK].ep_mps = config->spk_ep_mps;
    s_usb_itf[STREAM_UAC_SPK].evt_bit = UAC_SPK_STREAM_RUNNING;
    s_usb_itf[STREAM_UAC_SPK].urb_num = NUM_ISOC_SPK_URBS;
    s_usb_itf[STREAM_UAC_SPK].bytes_per_packet = config->spk_ep_mps;
    s_usb_itf[STREAM_UAC_SPK].packets_per_urb = UAC_SPK_ST_MAX_MS_DEFAULT;
    s_uac_dev.spk_max_xfer_size = s_usb_itf[STREAM_UAC_SPK].packets_per_urb * s_usb_itf[STREAM_UAC_SPK].bytes_per_packet;

    s_usb_dev.dev_speed = USB_SPEED_FULL;
    s_usb_dev.dev_addr = USB_DEVICE_ADDR;
    s_usb_dev.configuration = USB_CONFIG_NUM;

    TRIGGER_INIT();
    if (config->mic_interface) {
        s_usb_itf[STREAM_UAC_MIC].packets_per_urb = s_uac_dev.mic_min_bytes / s_usb_itf[STREAM_UAC_MIC].bytes_per_packet;
        s_uac_dev.mic_active = true;
    }
    if (config->spk_interface) {
        s_uac_dev.spk_active = true;
    }
    ESP_LOGI(TAG, "UAC Streaming Config Succeed");
    return ESP_OK;
}

esp_err_t uvc_streaming_config(const uvc_config_t *config)
{
    UVC_CHECK(config != NULL, "config can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK((config->frame_interval >= FRAME_MIN_INTERVAL && config->frame_interval <= FRAME_MAX_INTERVAL),
              "frame_interval Support 333333~2000000", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_buffer_size != 0, "frame_buffer_size can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_size != 0, "xfer_buffer_size can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_a != NULL, "xfer_buffer_a can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_b != NULL, "xfer_buffer_b can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_buffer != NULL, "frame_buffer can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->interface != 0, "Interface num must > 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->ep_addr & 0x80, "Endpoint direction must IN", ESP_ERR_INVALID_ARG);
    if (config->xfer_type == UVC_XFER_ISOC) {
        UVC_CHECK((s_usb_itf[STREAM_UAC_MIC].ep_mps + config->ep_mps) <= USB_EP_ISOC_MAX_MPS, "Isoc total MPS must < USB_EP_ISOC_MAX_MPS", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->interface_alt != 0, "Isoc interface alt num must > 0", ESP_ERR_INVALID_ARG);
    } else {
        UVC_CHECK(config->interface_alt == 0, "Bulk interface alt num must == 0", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->ep_mps == USB_EP_BULK_FS_MPS, "Full speed bulk MPS must == USB_EP_BULK_FS_MPS", ESP_ERR_INVALID_ARG);
    }

    s_usb_dev.dev_speed = USB_SPEED_FULL;
    s_usb_dev.dev_addr = USB_DEVICE_ADDR;
    s_usb_dev.configuration = USB_CONFIG_NUM;

    s_uvc_dev.xfer_type = config->xfer_type;
    s_usb_itf[STREAM_UVC].type = STREAM_UVC;
    s_usb_itf[STREAM_UVC].name = "UVC";
    s_usb_itf[STREAM_UVC].interface = config->interface;
    s_usb_itf[STREAM_UVC].interface_alt = config->interface_alt;
    s_usb_itf[STREAM_UVC].ep_addr = config->ep_addr;
    s_usb_itf[STREAM_UVC].ep_mps = config->ep_mps;
    s_usb_itf[STREAM_UVC].evt_bit = USB_UVC_STREAM_RUNNING;

    s_uvc_dev.xfer_buffer_size = config->xfer_buffer_size;
    s_uvc_dev.xfer_buffer_a = config->xfer_buffer_a;
    s_uvc_dev.xfer_buffer_b = config->xfer_buffer_b;
    s_uvc_dev.frame_buffer_size = config->frame_buffer_size;
    s_uvc_dev.frame_buffer = config->frame_buffer;

    s_uvc_dev.frame_width = config->frame_width;
    s_uvc_dev.frame_height = config->frame_height;
    s_uvc_dev.user_cb = config->frame_cb;
    s_uvc_dev.user_ptr = config->frame_cb_arg;

    s_uvc_dev.ctrl_set = (uvc_stream_ctrl_t)DEFAULT_UVC_STREAM_CTRL();
    s_uvc_dev.ctrl_set.bFormatIndex = config->format_index;
    s_uvc_dev.ctrl_set.bFrameIndex = config->frame_index;
    s_uvc_dev.ctrl_set.dwFrameInterval = config->frame_interval;
    s_uvc_dev.ctrl_set.dwMaxVideoFrameSize = config->frame_buffer_size;
    s_uvc_dev.ctrl_set.dwMaxPayloadTransferSize = config->ep_mps;

    if (s_uvc_dev.xfer_type == UVC_XFER_BULK) {
        // raw cost: NUM_BULK_STREAM_URBS * 1 * ep_mps
        s_usb_itf[STREAM_UVC].xfer_type = UVC_XFER_BULK;
        s_usb_itf[STREAM_UVC].urb_num = NUM_BULK_STREAM_URBS;
        s_usb_itf[STREAM_UVC].packets_per_urb = 1;
        // Note: According to UVC 1.5 2.4.3.2.1, each packet should have a header added, but most cameras don't.
        // Therefore, a large buffer should be prepared to ensure that a frame can be received.
#ifdef CONFIG_BULK_BYTES_PER_URB_SAME_AS_FRAME
        s_usb_itf[STREAM_UVC].bytes_per_packet = config->frame_buffer_size;
#else
        s_usb_itf[STREAM_UVC].bytes_per_packet = NUM_BULK_BYTES_PER_URB;
#endif
        ESP_LOGI(TAG, "Bulk transfer mode");
    } else {
        // raw cost: NUM_ISOC_STREAM_URBS * NUM_PACKETS_PER_URB_URB * ep_mps
        s_usb_itf[STREAM_UVC].xfer_type = UVC_XFER_ISOC;
        s_usb_itf[STREAM_UVC].urb_num = NUM_ISOC_STREAM_URBS;
        s_usb_itf[STREAM_UVC].packets_per_urb = NUM_PACKETS_PER_URB_URB;
        s_usb_itf[STREAM_UVC].bytes_per_packet = config->ep_mps;
        ESP_LOGI(TAG, "Isoc transfer mode");
    }

    TRIGGER_INIT();
    s_uvc_dev.active = true;
    ESP_LOGI(TAG, "UVC Streaming Config Succeed");
    return ESP_OK;
}

esp_err_t usb_streaming_start()
{
    UVC_CHECK(s_usb_dev.event_group == NULL, "usb streaming is running", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_uac_dev.mic_active == true || s_uac_dev.spk_active == true || s_uvc_dev.active == true, "uac/uvc streaming not configured", ESP_ERR_INVALID_STATE);
    // s_usb_dev.event_group also for common use
    s_usb_dev.event_group = xEventGroupCreate();
    UVC_CHECK(s_usb_dev.event_group  != NULL, "Create event group failed", ESP_FAIL);
    s_usb_dev.queue_hdl = xQueueCreate(UVC_EVENT_QUEUE_LEN, sizeof(_uvc_event_msg_t));
    UVC_CHECK_GOTO(s_usb_dev.queue_hdl != NULL, "Create event queue failed", free_resource_);
    s_usb_dev.stream_queue_hdl = xQueueCreate(UVC_EVENT_QUEUE_LEN, sizeof(_uvc_event_msg_t));
    UVC_CHECK_GOTO(s_usb_dev.stream_queue_hdl != NULL, "Create event queue failed", free_resource_);
    s_usb_dev.ctrl_mutex = xSemaphoreCreateMutex();
    UVC_CHECK_GOTO(s_usb_dev.ctrl_mutex != NULL, "Create ctrl mutex failed", free_resource_);
    s_uvc_dev.vs_ifc = &s_usb_itf[STREAM_UVC];
    s_uac_dev.spk_as_ifc = &s_usb_itf[STREAM_UAC_SPK];
    s_uac_dev.mic_as_ifc = &s_usb_itf[STREAM_UAC_MIC];
    s_uac_dev.parent = &s_usb_dev;
    s_uvc_dev.parent = &s_usb_dev;
    if (s_uac_dev.spk_active && s_uac_dev.spk_buf_size) {
        s_uac_dev.spk_ringbuf_hdl = xRingbufferCreate(s_uac_dev.spk_buf_size, RINGBUF_TYPE_BYTEBUF);
        UVC_CHECK_GOTO(s_uac_dev.spk_ringbuf_hdl != NULL, "Create speak buffer failed", free_resource_);
    }
    if (s_uac_dev.mic_active && s_uac_dev.mic_buf_size) {
        s_uac_dev.mic_ringbuf_hdl = xRingbufferCreate(s_uac_dev.mic_buf_size, RINGBUF_TYPE_BYTEBUF);
        UVC_CHECK_GOTO(s_uac_dev.mic_ringbuf_hdl != NULL, "Create speak buffer failed", free_resource_);
    }

    TaskHandle_t usbh_taskh = NULL;
    xTaskCreatePinnedToCore(_usb_processing_task, USB_PROC_TASK_NAME, USB_PROC_TASK_STACK_SIZE, NULL,
                            USB_PROC_TASK_PRIORITY, &usbh_taskh, USB_PROC_TASK_CORE);
    UVC_CHECK_GOTO(usbh_taskh != NULL, "Create usb proc task failed", free_resource_);
    xTaskNotifyGive(usbh_taskh);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "USB Streaming Starting");
    return ESP_OK;

free_resource_:
    if (s_usb_dev.event_group) {
        vEventGroupDelete(s_usb_dev.event_group);
        s_usb_dev.event_group = NULL;
    }
    if (s_usb_dev.queue_hdl) {
        vQueueDelete(s_usb_dev.queue_hdl);
        s_usb_dev.queue_hdl = NULL;
    }
    if (s_usb_dev.stream_queue_hdl) {
        vQueueDelete(s_usb_dev.stream_queue_hdl);
        s_usb_dev.stream_queue_hdl = NULL;
    }
    if (s_uac_dev.spk_ringbuf_hdl) {
        vRingbufferDelete(s_uac_dev.spk_ringbuf_hdl);
        s_uac_dev.spk_ringbuf_hdl = NULL;
    }
    if (s_uac_dev.mic_ringbuf_hdl) {
        vRingbufferDelete(s_uac_dev.mic_ringbuf_hdl);
        s_uac_dev.mic_ringbuf_hdl = NULL;
    }
    if (s_usb_dev.ctrl_mutex) {
        vSemaphoreDelete(s_usb_dev.ctrl_mutex);
        s_usb_dev.ctrl_mutex = NULL;
    }
    return ESP_FAIL;
}

esp_err_t usb_streaming_stop(void)
{
    if (!s_usb_dev.event_group || !(xEventGroupGetBits(s_usb_dev.event_group) & USB_HOST_INIT_DONE)) {
        ESP_LOGW(TAG, "UVC Streaming not started");
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_usb_dev.ctrl_mutex, portMAX_DELAY);
    _uvc_event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._handle.port_hdl = NULL,
        ._event.user_cmd = STREAM_STOP,
    };

    xQueueSend(s_usb_dev.queue_hdl, &event_msg, portMAX_DELAY);
    EventBits_t uxBits = xEventGroupWaitBits(s_usb_dev.event_group, USB_HOST_STOP_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND));
    if (!(uxBits & USB_HOST_STOP_DONE)) {
        ESP_LOGE(TAG, "UVC Streaming Stop Failed");
        return ESP_ERR_TIMEOUT;
    }

    if (s_usb_dev.queue_hdl) {
        vQueueDelete(s_usb_dev.queue_hdl);
    }
    if (s_usb_dev.stream_queue_hdl) {
        vQueueDelete(s_usb_dev.stream_queue_hdl);
    }
    if (s_uac_dev.spk_ringbuf_hdl) {
        vRingbufferDelete(s_uac_dev.spk_ringbuf_hdl);
    }
    if (s_uac_dev.mic_ringbuf_hdl) {
        vRingbufferDelete(s_uac_dev.mic_ringbuf_hdl);
    }
    vEventGroupDelete(s_usb_dev.event_group);
    xSemaphoreGive(s_usb_dev.ctrl_mutex);
    vSemaphoreDelete(s_usb_dev.ctrl_mutex);
    memset(&s_uvc_dev, 0, sizeof(_uvc_device_t));
    memset(&s_uac_dev, 0, sizeof(_uac_device_t));
    memset(&s_usb_dev, 0, sizeof(_usb_device_t));
    for (size_t i = 0; i < STREAM_MAX; i++) {
        memset(&s_usb_itf[i], 0, sizeof(s_usb_itf[0]));
    }
    ESP_LOGI(TAG, "UVC Streaming Stop Done");
    return ESP_OK;
}

static esp_err_t _streaming_suspend(usb_stream_t stream)
{
    UVC_CHECK(stream < STREAM_MAX, "invalid stream type", ESP_ERR_INVALID_ARG);
    UVC_CHECK(s_usb_itf[stream].interface, "interface not config", ESP_ERR_INVALID_STATE);
    if (stream == STREAM_UVC) {
        UVC_CHECK(s_uvc_dev.xfer_type != UVC_XFER_BULK, "bulk transfer can't suspend/resume, use start/stop instead", ESP_ERR_NOT_SUPPORTED);
    }
    _stream_ifc_t *p_itf = &s_usb_itf[stream];

    if (!s_usb_dev.event_group || !(xEventGroupGetBits(s_usb_dev.event_group) & USB_HOST_INIT_DONE)) {
        ESP_LOGW(TAG, "USB Host not started");
        return ESP_ERR_INVALID_STATE;
    }

    if (!(xEventGroupGetBits(s_usb_dev.event_group) & USB_DEVICE_CONNECTED)) {
        ESP_LOGW(TAG, "Device Not Connected");
        return ESP_ERR_INVALID_STATE;
    }

    if (!(xEventGroupGetBits(s_usb_dev.event_group) & (p_itf->evt_bit))) {
        ESP_LOGW(TAG, "%s Streaming Not running", p_itf->name);
        return ESP_OK;
    }

    _uvc_event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._handle.port_hdl = (void *)p_itf,
        ._event.user_cmd = STREAM_SUSPEND,
    };

    xQueueSend(s_usb_dev.queue_hdl, &event_msg, portMAX_DELAY);
    EventBits_t uxBits = xEventGroupWaitBits(s_usb_dev.event_group, USB_CTRL_PROC_SUCCEED | USB_CTRL_PROC_FAILED, pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND));
    if (uxBits & USB_CTRL_PROC_SUCCEED) {
        ESP_LOGI(TAG, "%s Streaming Suspend Done", p_itf->name);
        return ESP_OK;
    } else if (uxBits & USB_CTRL_PROC_FAILED) {
        ESP_LOGE(TAG, "%s Streaming Suspend Failed", p_itf->name);
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t _streaming_resume(usb_stream_t stream)
{
    UVC_CHECK(stream < STREAM_MAX, "invalid stream type", ESP_ERR_INVALID_ARG);
    UVC_CHECK(s_usb_itf[stream].interface, "interface not config", ESP_ERR_INVALID_STATE);
    if (stream == STREAM_UVC) {
        UVC_CHECK(s_uvc_dev.xfer_type != UVC_XFER_BULK, "bulk transfer can't suspend/resume, use start/stop instead", ESP_ERR_INVALID_STATE);
    }
    _stream_ifc_t *p_itf = &s_usb_itf[stream];

    if (!s_usb_dev.event_group || !(xEventGroupGetBits(s_usb_dev.event_group) & USB_HOST_INIT_DONE)) {
        ESP_LOGW(TAG, "USB Host not started");
        return ESP_ERR_INVALID_STATE;
    }

    if (!(xEventGroupGetBits(s_usb_dev.event_group) & USB_DEVICE_CONNECTED)) {
        ESP_LOGW(TAG, "Device Not Connected");
        return ESP_ERR_INVALID_STATE;
    }

    if (xEventGroupGetBits(s_usb_dev.event_group) & (p_itf->evt_bit)) {
        ESP_LOGW(TAG, "%s Streaming is running", p_itf->name);
        return ESP_OK;
    }

    _uvc_event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._handle.port_hdl = (void *)p_itf,
        ._event.user_cmd = STREAM_RESUME,
    };

    xQueueSend(s_usb_dev.queue_hdl, &event_msg, portMAX_DELAY);
    EventBits_t uxBits = xEventGroupWaitBits(s_usb_dev.event_group, USB_CTRL_PROC_SUCCEED | USB_CTRL_PROC_FAILED, pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND));
    if (uxBits & USB_CTRL_PROC_SUCCEED) {
        ESP_LOGI(TAG, "%s Streaming Resume Done", p_itf->name);
        return ESP_OK;
    } else if (uxBits & USB_CTRL_PROC_FAILED) {
        ESP_LOGE(TAG, "%s Streaming Resume Failed", p_itf->name);
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

static void _uac_feature_modify(usb_stream_t stream, stream_ctrl_t ctrl_type, uint32_t ctrl_value)
{
    switch (ctrl_type)
    {  
        case CTRL_UAC_MUTE:
            if (stream == STREAM_UAC_SPK) {
                s_uac_dev.spk_mute = ctrl_value;
            } else {
                s_uac_dev.mic_mute = ctrl_value;
            }
        break; 
        case CTRL_UAC_VOLUME:
            if (stream == STREAM_UAC_SPK) {
                s_uac_dev.spk_volume = ctrl_value;
            } else {
                s_uac_dev.mic_volume = ctrl_value;
            }
        break; 
        default:
        break;
    }
}

static esp_err_t _uac_feature_control(usb_stream_t stream, stream_ctrl_t ctrl_type, void *ctrl_value)
{
    UVC_CHECK(s_usb_itf[stream].interface, "interface not config", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_uac_dev.ac_interface, "control interface not config", ESP_ERR_NOT_SUPPORTED);
    UVC_CHECK((stream == STREAM_UAC_SPK && s_uac_dev.spk_fu_id) || (stream == STREAM_UAC_MIC && s_uac_dev.mic_fu_id), "feature unit not config", ESP_ERR_NOT_SUPPORTED);
    UVC_CHECK(s_uac_dev.ac_interface, "control interface not config", ESP_ERR_NOT_SUPPORTED);
    _stream_ifc_t *p_itf = &s_usb_itf[stream];

    if (!s_usb_dev.event_group || !(xEventGroupGetBits(s_usb_dev.event_group) & USB_HOST_INIT_DONE)) {
        ESP_LOGW(TAG, "USB Host not started");
        return ESP_ERR_INVALID_STATE;
    }

    if (!(xEventGroupGetBits(s_usb_dev.event_group) & USB_DEVICE_CONNECTED)) {
        ESP_LOGW(TAG, "Device Not Connected");
        _uac_feature_modify(stream, ctrl_type, (uint32_t)ctrl_value);
        ESP_LOGI(TAG, "Will take effect after device connected");
        return ESP_OK;
    }

    if (!(xEventGroupGetBits(s_usb_dev.event_group) & (p_itf->evt_bit))) {
        ESP_LOGW(TAG, "%s Streaming Not running", p_itf->name);
        _uac_feature_modify(stream, ctrl_type, (uint32_t)ctrl_value);
        ESP_LOGI(TAG, "Will take effect after re-connect");
        return ESP_OK;
    }

    _uvc_event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._handle.user_hdl = (void*)p_itf,
        ._event_data = ctrl_value,
    };

    switch (ctrl_type)
    {
        case CTRL_UAC_MUTE:
            event_msg._event.user_cmd = UAC_MUTE;
            break;
        case CTRL_UAC_VOLUME:
            event_msg._event.user_cmd = UAC_VOLUME;
            break;
        default:
            break;
    }

    xQueueSend(s_usb_dev.queue_hdl, &event_msg, portMAX_DELAY);
    EventBits_t uxBits = xEventGroupWaitBits(s_usb_dev.event_group, USB_CTRL_PROC_SUCCEED | USB_CTRL_PROC_FAILED, pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND));
    if (uxBits & USB_CTRL_PROC_SUCCEED) {
        ESP_LOGI(TAG, "%s UAC Feature Control Done", p_itf->name);
        // update value
        _uac_feature_modify(stream, ctrl_type, (uint32_t)ctrl_value);
        return ESP_OK;
    } else if (uxBits & USB_CTRL_PROC_FAILED) {
        ESP_LOGE(TAG, "%s UAC Feature Control Failed", p_itf->name);
    }
    return ESP_FAIL;
}

static esp_err_t _uvc_streaming_control(stream_ctrl_t ctrl_type, void *ctrl_value)
{
    switch (ctrl_type)
    {
        case CTRL_SUSPEND:
            return _streaming_suspend(STREAM_UVC);
        case CTRL_RESUME:
            return _streaming_resume(STREAM_UVC);
        default:
            break;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t _uac_streaming_control(usb_stream_t stream, stream_ctrl_t ctrl_type, void *ctrl_value)
{
    switch (ctrl_type)
    {
        case CTRL_SUSPEND:
            return _streaming_suspend(stream);
        case CTRL_RESUME:
            return _streaming_resume(stream);
        case CTRL_UAC_MUTE:
        case CTRL_UAC_VOLUME:
            return _uac_feature_control(stream, ctrl_type, ctrl_value);
        default:
            break;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t usb_streaming_control(usb_stream_t stream, stream_ctrl_t ctrl_type, void *ctrl_value)
{
    UVC_CHECK(s_usb_dev.ctrl_mutex != NULL, "streaming not configured", ESP_ERR_INVALID_STATE);
    xSemaphoreTake(s_usb_dev.ctrl_mutex, portMAX_DELAY);
    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;
    switch (stream)
    {
        case STREAM_UVC:
            ret = _uvc_streaming_control(ctrl_type, ctrl_value);
        break;    
        case STREAM_UAC_SPK:  
        case STREAM_UAC_MIC:
            ret = _uac_streaming_control(stream, ctrl_type, ctrl_value);
        break;    
        default:
        break;
    }
    xSemaphoreGive(s_usb_dev.ctrl_mutex);
    return ret;
}

/*********************************** Note: below is uvc_stream v0.1 api, will be abandoned **************************/
esp_err_t uvc_streaming_start(uvc_frame_callback_t *cb, void *user_ptr)
{
    UVC_CHECK(s_uvc_dev.active == true, "streaming not configured", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_uvc_dev.user_cb == NULL, "only one streaming supported", ESP_ERR_INVALID_STATE);
    UVC_CHECK(cb != NULL, "frame callback can't be NULL", ESP_ERR_INVALID_ARG);
    s_uac_dev.mic_active = false;
    s_uac_dev.spk_active = false;
    s_uvc_dev.user_cb = cb;
    s_uvc_dev.user_ptr = user_ptr;
    esp_err_t ret = usb_streaming_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UVC Streaming Start Failed");
        return ret;
    }
    ESP_LOGI(TAG, "UVC Streaming Starting");
    return ESP_OK;

}

esp_err_t uvc_streaming_stop(void)
{
    return usb_streaming_stop();
}

esp_err_t uvc_streaming_suspend(void)
{
    return usb_streaming_control(STREAM_UVC, CTRL_SUSPEND, NULL);
}

esp_err_t uvc_streaming_resume(void)
{
    return usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
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
                ESP_LOGE(TAG, "line-%u No Enough Ram Reserved=%"PRIu32", Want=%"PRIu32"", __LINE__, esp_get_free_heap_size(), strmh->hold_bytes);
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
