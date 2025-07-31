/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"
#include "esp_bit_defs.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "hcd.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#include "hal/usb_dwc_ll.h"
#endif
#include "usb/usb_types_stack.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_helpers.h"
#include "usb_private.h"
#include "usb_stream_descriptor.h"
#include "usb_host_helpers.h"
#include "usb_stream.h"
#include "usb_stream_sysview.h"

static const char *TAG = "USB_STREAM";

#define USB_CONFIG_NUM                       1                                           //Default configuration number
#define USB_DEVICE_ADDR                      1                                           //Default UVC device address
#define USB_ENUM_SHORT_DESC_REQ_LEN          8                                           //Number of bytes to request when getting a short descriptor (just enough to get bMaxPacketSize0 or wTotalLength)
#define USB_EP0_FS_DEFAULT_MPS               64                                          //Default MPS(max payload size) of Endpoint 0 for full speed device
#define USB_EP0_LS_DEFAULT_MPS               8                                           //Default MPS(max payload size) of Endpoint 0 for low speed device
#define USB_SHORT_DESC_REQ_LEN               8                                           //Number of bytes to request when getting a short descriptor (just enough to get bMaxPacketSize0 or wTotalLength)
#define USB_EP_ISOC_IN_MAX_MPS               512                                         //Max MPS ESP32-S2/S3 can handle
#define USB_EP_BULK_FS_MPS                   64                                          //Default MPS of full speed bulk transfer
#define USB_EP_BULK_HS_MPS                   512                                         //Default MPS of high speed bulk transfer
#define USB_EP_DIR_MASK                      0x80                                        //Mask for endpoint direction
#define USB_EVENT_QUEUE_LEN                  8                                           //USB event queue length
#define USB_STREAM_EVENT_QUEUE_LEN           32                                          //Stream event queue length
#define FRAME_MAX_INTERVAL                   2000000                                     //Specified in 100 ns units, General max frame interval (5 FPS)
#define FRAME_MIN_INTERVAL                   166666                                      //General min frame interval (60 FPS)
#define TIMEOUT_USB_CTRL_XFER_MS             CONFIG_USB_CTRL_XFER_TIMEOUT_MS             //Timeout for USB control transfer
#define WAITING_CTRL_XFER_MUTEX_MS           (TIMEOUT_USB_CTRL_XFER_MS + 100)            //Timeout for USER COMMAND control transfer
#define WAITING_TASK_RESOURCE_RELEASE_MS     50                                          //Additional waiting time for task resource release
#define WAITING_DEVICE_CONTROL_APPLY_MS      50                                          //Delay to avoid device control too frequently
#define TIMEOUT_USB_STREAM_DEINIT_MS         (TIMEOUT_USB_CTRL_XFER_MS + 100)            //Timeout for usb stream deinit
#define TIMEOUT_USB_STREAM_DISCONNECT_MS     (TIMEOUT_USB_CTRL_XFER_MS + 100)            //Timeout for usb stream disconnect
#define TIMEOUT_USER_COMMAND_MS              (TIMEOUT_USB_CTRL_XFER_MS + 200)            //Timeout for USER COMMAND control transfer
#define ACTIVE_DEBOUNCE_TIME_MS              100                                         //Debounce time for active state
#define CTRL_TRANSFER_DATA_MAX_BYTES         CONFIG_CTRL_TRANSFER_DATA_MAX_BYTES         //Max data length assumed in control transfer
#define NUM_BULK_STREAM_URBS                 CONFIG_NUM_BULK_STREAM_URBS                 //Number of bulk stream URBS created for continuous enqueue
#define NUM_BULK_BYTES_PER_URB               CONFIG_NUM_BULK_BYTES_PER_URB               //Required transfer bytes of each URB, check
#define NUM_ISOC_UVC_URBS                    CONFIG_NUM_ISOC_UVC_URBS                    //Number of isochronous stream URBS created for continuous enqueue
#define NUM_PACKETS_PER_URB                  CONFIG_NUM_PACKETS_PER_URB                  //Number of packets in each isochronous stream URB
#define WAITING_USB_AFTER_CONNECTION_MS      CONFIG_USB_WAITING_AFTER_CONN_MS            //Waiting n ms for usb device ready after connection
#define NUM_ISOC_SPK_URBS                    CONFIG_NUM_ISOC_SPK_URBS                    //Number of isochronous stream URBS created for continuous enqueue
#define NUM_ISOC_MIC_URBS                    CONFIG_NUM_ISOC_MIC_URBS                    //Number of isochronous stream URBS created for continuous enqueue
#define UAC_MIC_CB_MIN_MS_DEFAULT            CONFIG_UAC_MIC_CB_MIN_MS_DEFAULT            //Default min ms for mic callback
#define UAC_SPK_ST_MAX_MS_DEFAULT            CONFIG_UAC_SPK_ST_MAX_MS_DEFAULT            //Default max ms for speaker stream
#define UAC_MIC_PACKET_COMPENSATION          CONFIG_UAC_MIC_PACKET_COMPENSATION          //padding data if mic packet loss
#define UAC_SPK_PACKET_COMPENSATION          CONFIG_UAC_SPK_PACKET_COMPENSATION          //padding zero if speaker buffer empty
#define UAC_SPK_PACKET_COMPENSATION_SIZE_MS    CONFIG_UAC_SPK_PACKET_COMPENSATION_SIZE_MS    //padding n MS zero if speaker buffer empty
#define UAC_SPK_PACKET_COMPENSATION_TIMEOUT_MS CONFIG_UAC_SPK_PACKET_COMPENSATION_TIMEOUT_MS //padding n MS after wait timeout
#define UAC_SPK_PACKET_COMPENSATION_CONTINUOUS CONFIG_UAC_SPK_PACKET_COMPENSATION_CONTINUOUS //continuous padding zero at timeout interval
#define USB_PRE_ALLOC_CTRL_TRANSFER_URB        CONFIG_USB_PRE_ALLOC_CTRL_TRANSFER_URB        //Pre-allocate URB for control transfer

/**
 * @brief Task for USB I/O request and control transfer processing,
 * can not be blocked, higher task priority is suggested.
 *
 */
#define USB_PROC_TASK_NAME "usb_proc"
#define USB_PROC_TASK_PRIORITY (CONFIG_USB_PROC_TASK_PRIORITY+1)
#define USB_PROC_TASK_STACK_SIZE CONFIG_USB_PROC_TASK_STACK_SIZE
#define USB_PROC_TASK_CORE CONFIG_USB_PROC_TASK_CORE

/**
 * @brief Task for video/audio usb stream payload processing, polling and send data from ring buffer
 *
 */
#define USB_STREAM_NAME             "usb_stream_proc"
#define USB_STREAM_PRIORITY         CONFIG_USB_PROC_TASK_PRIORITY
#define USB_STREAM_STACK_SIZE       CONFIG_USB_PROC_TASK_STACK_SIZE
#define USB_STREAM_CORE             CONFIG_USB_PROC_TASK_CORE

/**
 * @brief Task for uvc sample processing, run user callback when new frame ready.
 *
 */
#define SAMPLE_PROC_TASK_NAME "sample_proc"
#define SAMPLE_PROC_TASK_PRIORITY CONFIG_SAMPLE_PROC_TASK_PRIORITY
#define SAMPLE_PROC_TASK_STACK_SIZE CONFIG_SAMPLE_PROC_TASK_STACK_SIZE
#if (CONFIG_SAMPLE_PROC_TASK_CORE == -1)
#define SAMPLE_PROC_TASK_CORE tskNO_AFFINITY
#else
#define SAMPLE_PROC_TASK_CORE CONFIG_SAMPLE_PROC_TASK_CORE
#endif

/**
 * @brief Events bit map
 *
 */
#define USB_HOST_INIT_DONE               BIT1         //set if uvc stream initted
#define USB_HOST_TASK_KILL_BIT           BIT2         //event bit to kill usb task
#define USB_UVC_STREAM_RUNNING           BIT3         //set if uvc streaming
#define UAC_SPK_STREAM_RUNNING           BIT4         //set if speaker streaming
#define UAC_MIC_STREAM_RUNNING           BIT5         //set if mic streaming
#define USB_CTRL_PROC_SUCCEED            BIT6         //set if control successfully operate
#define USB_CTRL_PROC_FAILED             BIT7         //set if control failed operate
#define UVC_SAMPLE_PROC_STOP_DONE        BIT8         //set if sample processing successfully stop
#define USB_STREAM_TASK_KILL_BIT         BIT9         //event bit to kill stream task
#define USB_STREAM_TASK_RECOVER_BIT      BIT10        //event bit to recover stream task
#define USB_STREAM_TASK_PROC_SUCCEED     BIT11        //set if stream task successfully operate
#define USB_STREAM_TASK_PROC_FAILED      BIT12        //set if stream task failed operate
#define USB_STREAM_DEVICE_READY_BIT      BIT13        //set if uvc device ready

/**
 * @brief Actions bit map
 *
 */
#define ACTION_PORT_RECOVER             BIT1          //recover port from error state
#define ACTION_PORT_DISABLE             BIT2          //disable port if user stop streaming
#define ACTION_DEVICE_CONNECT           BIT3          //find the connected device
#define ACTION_DEVICE_DISCONNECT        BIT4          //lost the connected device
#define ACTION_DEVICE_ENUM_RECOVER      BIT9          //recover enum the connected device
#define ACTION_DEVICE_ENUM              BIT10         //enum the connected device
#define ACTION_PIPE_DFLT_RECOVER        BIT11         //recover pipe from error state
#define ACTION_PIPE_DFLT_CLEAR          BIT12         //Clear pipe from error state
#define ACTION_PIPE_DFLT_DISABLE        BIT13         //disable pipe from error state
#define ACTION_PIPE_XFER_DONE           BIT14         //handle pipe transfer event
#define ACTION_PIPE_XFER_FAIL           BIT15         //handle pipe transfer event

/********************************************** Helper Functions **********************************************/
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

/*************************************** UVC Probe Helpers ********************************************/

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
    UVC_CHECK_RETURN_VOID(len >= 26, "len must >= 26");
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
    UVC_CHECK_RETURN_VOID(len >= 26, "len must >= 26");

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
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    fprintf(stream, "bmHint: 0x%04x\n", ctrl->bmHint);
#endif
    fprintf(stream, "bFormatIndex: %d\n", ctrl->bFormatIndex);
    fprintf(stream, "bFrameIndex: %d\n", ctrl->bFrameIndex);
    fprintf(stream, "dwFrameInterval: %"PRIu32"\n", ctrl->dwFrameInterval);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    fprintf(stream, "wKeyFrameRate: %d\n", ctrl->wKeyFrameRate);
    fprintf(stream, "wPFrameRate: %d\n", ctrl->wPFrameRate);
    fprintf(stream, "wCompQuality: %d\n", ctrl->wCompQuality);
    fprintf(stream, "wCompWindowSize: %d\n", ctrl->wCompWindowSize);
    fprintf(stream, "wDelay: %d\n", ctrl->wDelay);
    fprintf(stream, "dwMaxVideoFrameSize: %"PRIu32"\n", ctrl->dwMaxVideoFrameSize);
#endif
    fprintf(stream, "dwMaxPayloadTransferSize: %"PRIu32"\n", ctrl->dwMaxPayloadTransferSize);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    fprintf(stream, "dwClockFrequency: %"PRIu32"\n", ctrl->dwClockFrequency);
    fprintf(stream, "bmFramingInfo: %u\n", ctrl->bmFramingInfo);
    fprintf(stream, "bPreferredVersion: %u\n", ctrl->bPreferredVersion);
    fprintf(stream, "bMinVersion: %u\n", ctrl->bMinVersion);
    fprintf(stream, "bMaxVersion: %u\n", ctrl->bMaxVersion);
#endif
    fprintf(stream, "bInterfaceNumber: %d\n", ctrl->bInterfaceNumber);
}

/*************************************** Internal Types ********************************************/
typedef enum {
    STATE_NONE,
    STATE_DEVICE_INSTALLED,
    STATE_DEVICE_RECOVER,
    STATE_DEVICE_CONNECTED,
    STATE_DEVICE_ENUM,
    STATE_DEVICE_ENUM_FAILED,
    STATE_DEVICE_ACTIVE,
} _device_state_t;

typedef enum {
    ENUM_STAGE_NONE = 0,                        /*!< There is no device awaiting enumeration. Start requires device connection and first reset. */
    ENUM_STAGE_START = 1,                       /*!< A device has connected and has already been reset once. Allocate a device object in USBH */
    //Basic device enumeration
    ENUM_STAGE_GET_SHORT_DEV_DESC = 2,          /*!< Getting short dev desc (wLength is ENUM_SHORT_DESC_REQ_LEN) */
    ENUM_STAGE_CHECK_SHORT_DEV_DESC = 3,        /*!< Save bMaxPacketSize0 from the short dev desc. Update the MPS of the enum pipe */
    ENUM_STAGE_SET_ADDR = 4,                    /*!< Send SET_ADDRESS request */
    ENUM_STAGE_CHECK_ADDR = 5,                  /*!< Update the enum pipe's target address */
    ENUM_STAGE_GET_FULL_DEV_DESC = 6,           /*!< Get the full dev desc */
    ENUM_STAGE_CHECK_FULL_DEV_DESC = 7,         /*!< Check the full dev desc, fill it into the device object in USBH. Save the string descriptor indexes*/
    ENUM_STAGE_GET_SHORT_CONFIG_DESC = 8,       /*!< Getting a short config desc (wLength is ENUM_SHORT_DESC_REQ_LEN) */
    ENUM_STAGE_CHECK_SHORT_CONFIG_DESC = 9,     /*!< Save wTotalLength of the short config desc */
    ENUM_STAGE_GET_FULL_CONFIG_DESC = 10,       /*!< Get the full config desc (wLength is the saved wTotalLength) */
    ENUM_STAGE_CHECK_FULL_CONFIG_DESC = 11,     /*!< Check the full config desc, fill it into the device object in USBH */
    ENUM_STAGE_SET_CONFIG = 12,                 /*!< Send SET_CONFIGURATION request */
    ENUM_STAGE_CHECK_CONFIG = 13,               /*!< Check that SET_CONFIGURATION request was successful */
    ENUM_STAGE_FAILED = 14,                     /*!< Failed enum */
} _enum_stage_t;

const char *const STAGE_STR[] = {
    "NONE",
    "START",
    "GET_SHORT_DEV_DESC",
    "CHECK_SHORT_DEV_DESC",
    "SET_ADDR",
    "CHECK_ADDR",
    "GET_FULL_DEV_DESC",
    "CHECK_FULL_DEV_DESC",
    "GET_SHORT_CONFIG_DESC",
    "CHECK_SHORT_CONFIG_DESC",
    "GET_FULL_CONFIG_DESC",
    "CHECK_FULL_CONFIG_DESC",
    "SET_CONFIG",
    "CHECK_CONFIG",
    "FAILED",
};

typedef struct {
    // dynamic values should be protect
    bool     suspended;
    uint16_t interface;
    uint16_t interface_alt;
    uint8_t ep_addr;
    // dynamic values, but using in single thread
    bool not_found;
    uvc_xfer_t xfer_type;
    usb_stream_t type;
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
    /** if true, stream is running (streaming video to host) */
    uvc_device_handle_t devh;
    uint8_t running;
    /** Current control block */
    struct uvc_stream_ctrl cur_ctrl;
    uint8_t fid;
    uint8_t reassemble_flag;
    uint8_t reassembling;
    uint32_t seq, hold_seq;
    uint32_t pts, hold_pts;
    uint32_t last_scr, hold_last_scr;
    size_t got_bytes, hold_bytes;
    uint8_t *outbuf, *holdbuf;
    uvc_frame_callback_t user_cb;
    void *user_ptr;
    SemaphoreHandle_t cb_mutex;
    TaskHandle_t taskh;
    struct uvc_frame frame;
    enum uvc_frame_format frame_format;
    struct timespec capture_time_finished;
} _uvc_stream_handle_t;

typedef struct {
    _stream_ifc_t *vs_ifc;
    _uvc_stream_handle_t *uvc_stream_hdl;
    enum uvc_frame_format frame_format;
    uint8_t format_index;
    // dynamic values should be protect
    uvc_frame_size_t *frame_size;
    uint8_t frame_num;
    uint8_t frame_index;
    uint16_t frame_width;
    uint16_t frame_height;
    uint32_t frame_interval;
} _uvc_device_t;

typedef enum {
    UAC_SPK,
    UAC_MIC,
    UAC_MAX,
} _uac_internal_stream_t;

typedef struct {
    // dynamic values, but using in single thread
    _stream_ifc_t *as_ifc[UAC_MAX];
    RingbufHandle_t ringbuf_hdl[UAC_MAX];
    // dynamic values should be protect
    // index 0: speaker, index 1: mic
    uint16_t ac_interface;
    uint8_t *mic_frame_buf;
    uint32_t mic_frame_buf_size;
    uint32_t mic_ms_bytes;
    uint32_t spk_ms_bytes;
    uint32_t spk_max_xfer_size;
    uac_frame_size_t *frame_size[UAC_MAX];
    uint8_t frame_num[UAC_MAX];
    uint8_t frame_index[UAC_MAX];
    uint8_t ch_num[UAC_MAX];
    uint16_t bit_resolution[UAC_MAX];
    uint32_t samples_frequence[UAC_MAX];
    bool     freq_ctrl_support[UAC_MAX];
    uint8_t  fu_id[UAC_MAX];
    uint8_t  mute_ch[UAC_MAX];
    uint8_t  volume_ch[UAC_MAX];
    bool     mute[UAC_MAX];
    uint32_t volume[UAC_MAX];
} _uac_device_t;

typedef struct {
    int in_mps;
    int non_periodic_out_mps;
    int periodic_out_mps;
} fifo_mps_limits_t;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)

#ifndef USB_DWC_FIFO_RX_LINES_DEFAULT
#define USB_DWC_FIFO_RX_LINES_DEFAULT 104
#endif

#ifndef USB_DWC_FIFO_NPTX_LINES_DEFAULT
#define USB_DWC_FIFO_NPTX_LINES_DEFAULT 48
#endif

#ifndef USB_DWC_FIFO_PTX_LINES_DEFAULT
#define USB_DWC_FIFO_PTX_LINES_DEFAULT 48
#endif

#ifndef USB_DWC_FIFO_RX_LINES_BIASRX
#define USB_DWC_FIFO_RX_LINES_BIASRX 152
#endif

#ifndef USB_DWC_FIFO_NPTX_LINES_BIASRX
#define USB_DWC_FIFO_NPTX_LINES_BIASRX 16
#endif

#ifndef USB_DWC_FIFO_PTX_LINES_BIASRX
#define USB_DWC_FIFO_PTX_LINES_BIASRX 32
#endif

const fifo_mps_limits_t s_mps_limits_default = {
    .in_mps = (USB_DWC_FIFO_RX_LINES_DEFAULT - 2) * 4,
    .non_periodic_out_mps = USB_DWC_FIFO_NPTX_LINES_DEFAULT * 4,
    .periodic_out_mps = USB_DWC_FIFO_PTX_LINES_DEFAULT * 4,
};
const fifo_mps_limits_t s_mps_limits_bias_rx = {
    .in_mps = (USB_DWC_FIFO_RX_LINES_BIASRX - 2) * 4,
    .non_periodic_out_mps = USB_DWC_FIFO_NPTX_LINES_BIASRX * 4,
    .periodic_out_mps = USB_DWC_FIFO_PTX_LINES_BIASRX * 4,
};
#else
extern const fifo_mps_limits_t mps_limits_default;
extern const fifo_mps_limits_t mps_limits_bias_rx;
#define s_mps_limits_default mps_limits_default
#define s_mps_limits_bias_rx mps_limits_bias_rx
#endif

typedef struct {
    // const user config values
    uac_config_t uac_cfg;
    uvc_config_t uvc_cfg;
    // const values after usb stream start
    bool enabled[STREAM_MAX];
    hcd_port_handle_t port_hdl;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 3)
    hcd_port_fifo_bias_t fifo_bias;
#endif
    const fifo_mps_limits_t *mps_limits;
    uint16_t configuration;
    uint8_t dev_addr;
    QueueHandle_t queue_hdl;
    QueueHandle_t stream_queue_hdl;
    TaskHandle_t stream_task_hdl;
    EventGroupHandle_t event_group_hdl;
    SemaphoreHandle_t xfer_mutex_hdl;
    SemaphoreHandle_t ctrl_smp_hdl;
    urb_t *ctrl_urb;
    // const values after usb enum
    hcd_pipe_handle_t dflt_pipe_hdl;
    usb_speed_t dev_speed;
    uint8_t ep_mps;
    _uac_device_t *uac;
    _uvc_device_t *uvc;
    _stream_ifc_t *ifc[STREAM_MAX];
    // only operate in single thread
    _enum_stage_t enum_stage;
    // dynamic values should be protect
    _device_state_t state;
    state_callback_t state_cb;
    void *state_cb_arg;
    uint32_t flags;
} _usb_device_t;

/**
 * @brief Singleton Pattern, Only one device supported
 *
 */
static _usb_device_t s_usb_dev = {0};
static portMUX_TYPE s_uvc_lock = portMUX_INITIALIZER_UNLOCKED;
#define UVC_ENTER_CRITICAL()           portENTER_CRITICAL(&s_uvc_lock)
#define UVC_EXIT_CRITICAL()            portEXIT_CRITICAL(&s_uvc_lock)

typedef enum {
    USER_EVENT,
    PORT_EVENT,
    PIPE_EVENT,
} _event_type_t;

typedef enum {
    //accept by usb stream task
    STREAM_SUSPEND,
    STREAM_RESUME,
    //accept by usb task
    USB_RECOVER,
    PIPE_RECOVER,
} _user_cmd_t;

typedef struct {
    _event_type_t _type;
    union {
        void *user_hdl;
        hcd_port_handle_t port_hdl;
        hcd_pipe_handle_t pipe_handle;
    } _handle;
    union {
        _user_cmd_t user_cmd;
        hcd_port_event_t port_event;
        hcd_pipe_event_t pipe_event;
    } _event;
    void *_event_data;
} _event_msg_t;

IRAM_ATTR static _device_state_t _usb_device_get_state()
{
    UVC_ENTER_CRITICAL();
    _device_state_t state = s_usb_dev.state;
    UVC_EXIT_CRITICAL();
    return state;
}

IRAM_ATTR static bool _usb_port_callback(hcd_port_handle_t port_hdl, hcd_port_event_t port_event, void *user_arg, bool in_isr)
{
    QueueHandle_t usb_event_queue = (QueueHandle_t)user_arg;
    assert(in_isr);    //Current HCD implementation should never call a port callback in a task context
    _event_msg_t event_msg = {
        ._type = PORT_EVENT,
        ._handle.port_hdl = port_hdl,
        ._event.port_event = port_event,
    };
    BaseType_t xTaskWoken = pdFALSE;
    xQueueSendFromISR(usb_event_queue, &event_msg, &xTaskWoken);
    return (xTaskWoken == pdTRUE);
}

IRAM_ATTR static bool _usb_pipe_callback(hcd_pipe_handle_t pipe_handle, hcd_pipe_event_t pipe_event, void *user_arg, bool in_isr)
{
    QueueHandle_t usb_event_queue = (QueueHandle_t)user_arg;
    _event_msg_t event_msg = {
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

static void _uvc_process_payload(_uvc_stream_handle_t *strmh, size_t req_len, uint8_t *payload, size_t payload_len);

IRAM_ATTR static void _processing_uvc_pipe(_uvc_stream_handle_t *strmh, hcd_pipe_handle_t pipe_handle, bool if_enqueue)
{
    UVC_CHECK_RETURN_VOID(pipe_handle != NULL, "pipe handle can not be NULL");

    urb_t *urb_done = hcd_urb_dequeue(pipe_handle);

    if (urb_done == NULL) {
        ESP_LOGV(TAG, "uvc pipe dequeue failed");
        return;
    }

    if (urb_done->transfer.num_isoc_packets == 0) { // Bulk transfer
        if (urb_done->transfer.actual_num_bytes > 0) {
            _uvc_process_payload(strmh, urb_done->transfer.num_bytes, urb_done->transfer.data_buffer, urb_done->transfer.actual_num_bytes);
        }
    } else { // isoc transfer
        for (size_t i = 0; i < urb_done->transfer.num_isoc_packets; i++) {
            if (urb_done->transfer.isoc_packet_desc[i].status != USB_TRANSFER_STATUS_COMPLETED) {
                ESP_LOGV(TAG, "line:%u bad iso transit status %d", __LINE__, urb_done->transfer.isoc_packet_desc[i].status);
                continue;
            }

            uint8_t *simplebuffer = urb_done->transfer.data_buffer + (i * s_usb_dev.uvc->vs_ifc->ep_mps);
            ESP_LOGV(TAG, "process payload=%u, len = %d", i, urb_done->transfer.isoc_packet_desc[i].actual_num_bytes);
            _uvc_process_payload(strmh, (urb_done->transfer.isoc_packet_desc[i].num_bytes), simplebuffer, (urb_done->transfer.isoc_packet_desc[i].actual_num_bytes));
        }
    }

    if (if_enqueue) {
        esp_err_t ret = hcd_urb_enqueue(pipe_handle, urb_done);
        if (ret != ESP_OK) {
            // We not handle the error because the usb stream task will handle it
            ESP_LOGE(TAG, "UVC urb enqueue failed %s", esp_err_to_name(ret));
        }

    }

}

/*------------------------------------------------ USB Control Process Code ----------------------------------------------------*/
static esp_err_t _apply_pipe_config(usb_stream_t stream)
{
    _usb_device_t *usb_dev = &s_usb_dev;
    /* If users skipped the get descriptors process for quick start, use user-mandated configs */
    if (stream == STREAM_UVC && usb_dev->enabled[STREAM_UVC] && !usb_dev->ifc[STREAM_UVC]->not_found) {
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        usb_dev->ifc[STREAM_UVC]->interface = usb_dev->uvc_cfg.interface;
        usb_dev->ifc[STREAM_UVC]->interface_alt = usb_dev->uvc_cfg.interface_alt;
        usb_dev->ifc[STREAM_UVC]->ep_addr = usb_dev->uvc_cfg.ep_addr;
        usb_dev->ifc[STREAM_UVC]->ep_mps = usb_dev->uvc_cfg.ep_mps;
        usb_dev->ifc[STREAM_UVC]->xfer_type = usb_dev->uvc_cfg.xfer_type;
#endif
        if (usb_dev->ifc[STREAM_UVC]->ep_mps > USB_EP_ISOC_IN_MAX_MPS) {
            usb_dev->ifc[STREAM_UVC]->ep_mps = USB_EP_ISOC_IN_MAX_MPS;
        }
    }
    if (stream == STREAM_UAC_MIC && usb_dev->enabled[STREAM_UAC_MIC] && !usb_dev->ifc[STREAM_UAC_MIC]->not_found) {
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        usb_dev->ifc[STREAM_UAC_MIC]->interface = usb_dev->uac_cfg.mic_interface;
        usb_dev->ifc[STREAM_UAC_MIC]->interface_alt = 1;
        usb_dev->ifc[STREAM_UAC_MIC]->ep_addr = usb_dev->uac_cfg.mic_ep_addr;
        usb_dev->ifc[STREAM_UAC_MIC]->ep_mps = usb_dev->uac_cfg.mic_ep_mps;
        usb_dev->uac->ac_interface = usb_dev->uac_cfg.ac_interface;
        usb_dev->uac->fu_id[UAC_MIC] = usb_dev->uac_cfg.mic_fu_id;
        usb_dev->uac->mute_ch[UAC_MIC] = 1 << 0;
        usb_dev->uac->volume_ch[UAC_MIC] = 1 << 1;
#endif
        usb_dev->ifc[STREAM_UAC_MIC]->xfer_type = UVC_XFER_ISOC;
        usb_dev->uac->volume[UAC_MIC] = 80;
        usb_dev->uac->mute[UAC_MIC] = 0;
        if (usb_dev->ifc[STREAM_UAC_MIC]->ep_mps > usb_dev->mps_limits->in_mps) {
            usb_dev->ifc[STREAM_UAC_MIC]->ep_mps = usb_dev->mps_limits->in_mps;
        }
    }
    if (stream == STREAM_UAC_SPK && usb_dev->enabled[STREAM_UAC_SPK] && !usb_dev->ifc[STREAM_UAC_SPK]->not_found) {
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        usb_dev->ifc[STREAM_UAC_SPK]->interface = usb_dev->uac_cfg.spk_interface;
        usb_dev->ifc[STREAM_UAC_SPK]->interface_alt = 1;
        usb_dev->ifc[STREAM_UAC_SPK]->ep_addr = usb_dev->uac_cfg.spk_ep_addr;
        usb_dev->ifc[STREAM_UAC_SPK]->ep_mps = usb_dev->uac_cfg.spk_ep_mps;
        usb_dev->uac->ac_interface = usb_dev->uac_cfg.ac_interface;
        usb_dev->uac->fu_id[UAC_SPK] = usb_dev->uac_cfg.spk_fu_id;
        usb_dev->uac->mute_ch[UAC_SPK] = 1 << 0;
        usb_dev->uac->volume_ch[UAC_SPK] = 1 << 1;
#endif
        usb_dev->ifc[STREAM_UAC_SPK]->xfer_type = UVC_XFER_ISOC;
        usb_dev->uac->volume[UAC_SPK] = 80;
        usb_dev->uac->mute[UAC_SPK] = 0;
        if (usb_dev->ifc[STREAM_UAC_SPK]->ep_mps > usb_dev->mps_limits->periodic_out_mps) {
            usb_dev->ifc[STREAM_UAC_SPK]->ep_mps = usb_dev->mps_limits->periodic_out_mps;
        }
    }
    return ESP_OK;
}

static esp_err_t _apply_stream_config(usb_stream_t stream)
{
    ESP_LOGD(TAG, "apply stream config");
    _usb_device_t *usb_dev = &s_usb_dev;
    if (stream == STREAM_UVC && usb_dev->enabled[STREAM_UVC] && !usb_dev->ifc[STREAM_UVC]->not_found) {
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        static uvc_frame_size_t frame_size = {0};
        frame_size.width = usb_dev->uvc_cfg.frame_width;
        frame_size.height = usb_dev->uvc_cfg.frame_height;
        frame_size.interval_max = usb_dev->uvc_cfg.frame_interval;
        frame_size.interval_min = usb_dev->uvc_cfg.frame_interval;
        frame_size.interval = usb_dev->uvc_cfg.frame_interval;
        usb_dev->uvc->format_index = usb_dev->uvc_cfg.format_index;
        usb_dev->uvc->frame_index = usb_dev->uvc_cfg.frame_index;
        usb_dev->uvc->frame_height = usb_dev->uvc_cfg.frame_height;
        usb_dev->uvc->frame_width = usb_dev->uvc_cfg.frame_width;
        usb_dev->uvc->frame_num = 1;
        usb_dev->uvc->frame_size = &frame_size;
        usb_dev->uvc->frame_interval = usb_dev->uvc_cfg.frame_interval;
#else
        UVC_CHECK((usb_dev->uvc->frame_index <= usb_dev->uvc->frame_num), "invalid frame index", ESP_ERR_INVALID_STATE);
        if (usb_dev->uvc->frame_index == 0) {
            ESP_LOGD(TAG, "Expected uvc resolution not found, frame_index=%d", usb_dev->uvc->frame_index);
            return ESP_ERR_NOT_FOUND;
        }
        UVC_CHECK(usb_dev->uvc->format_index != 0, "invalid format index", ESP_ERR_INVALID_STATE);
        usb_dev->uvc->frame_width = usb_dev->uvc->frame_size[usb_dev->uvc->frame_index - 1].width;
        usb_dev->uvc->frame_height = usb_dev->uvc->frame_size[usb_dev->uvc->frame_index - 1].height;
        usb_dev->uvc->frame_interval = usb_dev->uvc->frame_size[usb_dev->uvc->frame_index - 1].interval;
#endif
        if (usb_dev->ifc[STREAM_UVC]->xfer_type == UVC_XFER_BULK) {
            usb_dev->ifc[STREAM_UVC]->urb_num = NUM_BULK_STREAM_URBS;
            usb_dev->ifc[STREAM_UVC]->packets_per_urb = 1;
            usb_dev->ifc[STREAM_UVC]->bytes_per_packet = NUM_BULK_BYTES_PER_URB;
        } else {
            usb_dev->ifc[STREAM_UVC]->urb_num = NUM_ISOC_UVC_URBS;
            usb_dev->ifc[STREAM_UVC]->packets_per_urb = NUM_PACKETS_PER_URB;
            usb_dev->ifc[STREAM_UVC]->bytes_per_packet = usb_dev->ifc[STREAM_UVC]->ep_mps;
        }
        ESP_LOGD(TAG, "UVC format_index=%"PRIu8", frame_index=%"PRIu8", frame_width=%"PRIu16", frame_height=%"PRIu16", frame_interval=%"PRIu32,
                 usb_dev->uvc->format_index, usb_dev->uvc->frame_index, usb_dev->uvc->frame_width, usb_dev->uvc->frame_height, usb_dev->uvc->frame_interval);
    }
    if (stream == STREAM_UAC_MIC && usb_dev->enabled[STREAM_UAC_MIC] && !usb_dev->ifc[STREAM_UAC_MIC]->not_found) {
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        static uac_frame_size_t frame_size = {0};
        frame_size.ch_num = (usb_dev->uac_cfg.mic_ch_num == 0) ? 1 : usb_dev->uac_cfg.mic_ch_num;
        frame_size.bit_resolution = usb_dev->uac_cfg.mic_bit_resolution;
        frame_size.samples_frequence = usb_dev->uac_cfg.mic_samples_frequence;
        usb_dev->uac->frame_index[UAC_MIC] = 1;
        usb_dev->uac->frame_num[UAC_MIC] = 1;
        usb_dev->uac->frame_size[UAC_MIC] = &frame_size;
        usb_dev->uac->ch_num[UAC_MIC] = (usb_dev->uac_cfg.mic_ch_num == 0) ? 1 : usb_dev->uac_cfg.mic_ch_num;
        usb_dev->uac->bit_resolution[UAC_MIC] = usb_dev->uac_cfg.mic_bit_resolution;
        usb_dev->uac->samples_frequence[UAC_MIC] = usb_dev->uac_cfg.mic_samples_frequence;
        usb_dev->uac->freq_ctrl_support[UAC_MIC] = true;
#else
        UVC_CHECK((usb_dev->uac->frame_index[UAC_MIC] <= usb_dev->uac->frame_num[UAC_MIC]), "invalid frame index", ESP_ERR_INVALID_STATE);
        if (usb_dev->uac->frame_index[UAC_MIC] == 0) {
            ESP_LOGD(TAG, "Expected uac channel/frequency/bits not found, frame_index=%d", usb_dev->uac->frame_index[UAC_MIC]);
            return ESP_ERR_NOT_FOUND;
        }
        usb_dev->uac->ch_num[UAC_MIC] = usb_dev->uac->frame_size[UAC_MIC][usb_dev->uac->frame_index[UAC_MIC] - 1].ch_num;
        usb_dev->uac->bit_resolution[UAC_MIC] = usb_dev->uac->frame_size[UAC_MIC][usb_dev->uac->frame_index[UAC_MIC] - 1].bit_resolution;
        usb_dev->uac->samples_frequence[UAC_MIC] = usb_dev->uac->frame_size[UAC_MIC][usb_dev->uac->frame_index[UAC_MIC] - 1].samples_frequence;
#endif
        usb_dev->ifc[STREAM_UAC_MIC]->urb_num = NUM_ISOC_MIC_URBS;
        usb_dev->ifc[STREAM_UAC_MIC]->packets_per_urb = UAC_MIC_CB_MIN_MS_DEFAULT;
        usb_dev->ifc[STREAM_UAC_MIC]->bytes_per_packet = usb_dev->ifc[STREAM_UAC_MIC]->ep_mps;
        usb_dev->uac->mic_ms_bytes = usb_dev->uac->ch_num[UAC_MIC] * usb_dev->uac->samples_frequence[UAC_MIC] / 1000 * usb_dev->uac->bit_resolution[UAC_MIC] / 8;
        uint32_t mic_min_bytes = usb_dev->uac->mic_ms_bytes * UAC_MIC_CB_MIN_MS_DEFAULT;
        ESP_LOGD(TAG, "min_bytes in mic callback = %"PRIu32, mic_min_bytes);
        if (usb_dev->uac_cfg.mic_buf_size && (usb_dev->uac_cfg.mic_buf_size < mic_min_bytes)) {
            ESP_LOGE(TAG, "mic_buf_size=%"PRIu32" must >= mic_min_bytes %"PRIu32, usb_dev->uac_cfg.mic_buf_size, mic_min_bytes);
            assert(0);
        }
        usb_dev->uac->mic_frame_buf = heap_caps_realloc(usb_dev->uac->mic_frame_buf, mic_min_bytes, MALLOC_CAP_INTERNAL);
        UVC_CHECK(usb_dev->uac->mic_frame_buf, "alloc mic frame buf failed", ESP_ERR_NO_MEM);
        usb_dev->uac->mic_frame_buf_size = mic_min_bytes;
        ESP_LOGD(TAG, "MIC ch_num=%"PRIu8", bit_resolution=%"PRIu16", samples_frequence=%"PRIu32", bytes_per_packet=%"PRIu32,
                 usb_dev->uac->ch_num[UAC_MIC], usb_dev->uac->bit_resolution[UAC_MIC], usb_dev->uac->samples_frequence[UAC_MIC], usb_dev->ifc[STREAM_UAC_MIC]->bytes_per_packet);
    }
    if (stream == STREAM_UAC_SPK && usb_dev->enabled[STREAM_UAC_SPK] && !usb_dev->ifc[STREAM_UAC_SPK]->not_found) {
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        static uac_frame_size_t frame_size = {0};
        frame_size.ch_num = (usb_dev->uac_cfg.spk_ch_num == 0) ? 1 : usb_dev->uac_cfg.spk_ch_num;
        frame_size.bit_resolution = usb_dev->uac_cfg.spk_bit_resolution;
        frame_size.samples_frequence = usb_dev->uac_cfg.spk_samples_frequence;
        usb_dev->uac->frame_index[UAC_SPK] = 1;
        usb_dev->uac->frame_num[UAC_SPK] = 1;
        usb_dev->uac->frame_size[UAC_SPK] = &frame_size;
        usb_dev->uac->ch_num[UAC_SPK] = (usb_dev->uac_cfg.spk_ch_num == 0) ? 1 : usb_dev->uac_cfg.spk_ch_num;
        usb_dev->uac->bit_resolution[UAC_SPK] = usb_dev->uac_cfg.spk_bit_resolution;
        usb_dev->uac->samples_frequence[UAC_SPK] = usb_dev->uac_cfg.spk_samples_frequence;
        usb_dev->uac->freq_ctrl_support[UAC_SPK] = true;
#else
        UVC_CHECK((usb_dev->uac->frame_index[UAC_SPK] <= usb_dev->uac->frame_num[UAC_SPK]), "invalid frame index", ESP_ERR_INVALID_STATE);
        if (usb_dev->uac->frame_index[UAC_SPK] == 0) {
            ESP_LOGD(TAG, "Expected uac channel/frequency/bits not found, frame_index=%d", usb_dev->uac->frame_index[UAC_SPK]);
            return ESP_ERR_NOT_FOUND;
        }
        usb_dev->uac->ch_num[UAC_SPK] = usb_dev->uac->frame_size[UAC_SPK][usb_dev->uac->frame_index[UAC_SPK] - 1].ch_num;
        usb_dev->uac->bit_resolution[UAC_SPK] = usb_dev->uac->frame_size[UAC_SPK][usb_dev->uac->frame_index[UAC_SPK] - 1].bit_resolution;
        usb_dev->uac->samples_frequence[UAC_SPK] = usb_dev->uac->frame_size[UAC_SPK][usb_dev->uac->frame_index[UAC_SPK] - 1].samples_frequence;
#endif
        usb_dev->ifc[STREAM_UAC_SPK]->urb_num = NUM_ISOC_SPK_URBS;
        usb_dev->ifc[STREAM_UAC_SPK]->packets_per_urb = UAC_SPK_ST_MAX_MS_DEFAULT;
        usb_dev->ifc[STREAM_UAC_SPK]->bytes_per_packet = usb_dev->uac->ch_num[UAC_SPK] * usb_dev->uac->samples_frequence[UAC_SPK] / 1000 * usb_dev->uac->bit_resolution[UAC_SPK] / 8;
        //TODO: Reset mps size to bytes_per_packet. to save tx fifo space
        //usb_dev->ifc[STREAM_UAC_SPK]->ep_mps = usb_dev->ifc[STREAM_UAC_SPK]->bytes_per_packet;
        usb_dev->uac->spk_max_xfer_size = usb_dev->ifc[STREAM_UAC_SPK]->packets_per_urb * usb_dev->ifc[STREAM_UAC_SPK]->bytes_per_packet;
        ESP_LOGD(TAG, "SPK ch_num=%"PRIu8", bit_resolution=%"PRIu16", samples_frequence=%"PRIu32", bytes_per_packet=%"PRIu32,
                 usb_dev->uac->ch_num[UAC_SPK], usb_dev->uac->bit_resolution[UAC_SPK], usb_dev->uac->samples_frequence[UAC_SPK], usb_dev->ifc[STREAM_UAC_SPK]->bytes_per_packet);
    }
    return ESP_OK;
}

static esp_err_t _update_config_from_descriptor(const usb_config_desc_t *cfg_desc)
{
    if (cfg_desc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    _uac_device_t *uac_dev = s_usb_dev.uac;
    _uvc_device_t *uvc_dev = s_usb_dev.uvc;
    _usb_device_t *usb_dev = &s_usb_dev;
    int offset = 0;
    bool already_next = false;
    uint16_t wTotalLength = cfg_desc->wTotalLength;
    /* flags indicate if required setting format and frame found */
    bool format_set_found = false;
    uint8_t format_idx = 0;
    uint8_t frame_num = 0;
    enum uvc_frame_format format = UVC_FRAME_FORMAT_UNKNOWN;
    /* flags user defined frame found */
    bool user_frame_found = false;
    uint8_t user_frame_idx = 0;
    /* flags indicate if suitable audio stream interface found */
    uint16_t uac_ver_found = 0;
    bool uac_format_others_found = 0;
    bool ac_intf_found = false;
    uint8_t ac_intf_idx = 0;
    bool as_mic_intf_found = false;
    uint8_t as_mic_feature_unit_idx = 0;
    uint8_t as_mic_intf_idx = 0;
    uint8_t as_mic_intf_ep_attr = 0;
    uint16_t as_mic_intf_ep_mps = 0;
    uint8_t as_mic_intf_ep_addr = 0;
    uint8_t as_mic_volume_ch = __UINT8_MAX__;
    uint8_t as_mic_mute_ch = __UINT8_MAX__;
    uint8_t as_mic_next_unit_idx = 0;
    uint8_t as_mic_output_terminal = 0;
    bool as_spk_intf_found = false;
    bool as_spk_freq_found = false;
    bool as_mic_freq_found = false;
    bool as_spk_bit_res_found = false;
    bool as_mic_bit_res_found = false;
    bool as_mic_freq_ctrl_found = false;
    bool as_spk_freq_ctrl_found = false;
    uint8_t as_spk_input_terminal = 0;
    uint8_t as_mic_input_terminal = 0;
    uint8_t as_spk_feature_unit_idx = 0;
    uint8_t as_spk_next_unit_idx = 0;
    uint8_t as_spk_volume_ch = __UINT8_MAX__;
    uint8_t as_spk_mute_ch = __UINT8_MAX__;
    uint8_t as_spk_intf_idx = 0;
    uint8_t as_spk_intf_ep_attr = 0;
    uint16_t as_spk_intf_ep_mps = 0;
    uint8_t as_spk_intf_ep_addr = 0;
    bool as_spk_ch_num_found = false;
    bool as_mic_ch_num_found = false;
    /* flags indicate if suitable video stream interface found */
    bool vs_intf_found = false;
    uint8_t vs_intf_idx = 0;
    uint8_t vs_intf_alt_idx = 0;
    uint8_t vs_intf_ep_attr = 0;
    uint16_t vs_intf_ep_mps = 0;
    uint8_t vs_intf_ep_addr = 0;
    uint8_t vs_intf1_idx = 0;
    uint8_t vs_intf1_alt_idx = 0;
    uint8_t vs_intf1_ep_attr = 0;
    uint16_t vs_intf1_ep_mps = 0;
    uint8_t vs_intf1_ep_addr = 0;
    /* flags indicate if suitable video stream interface found */
    uint8_t context_class = 0;
    uint8_t context_subclass = 0;
    uint8_t context_intf = 0;
    uint8_t context_intf_alt = 0;
    uint8_t context_connected_terminal = 0;
    const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *)cfg_desc;

    do {
        already_next = false;
        switch (next_desc->bDescriptorType) {
        case USB_B_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION: {
            const ifc_assoc_desc_t *assoc_desc = (const ifc_assoc_desc_t *)next_desc;
            if (assoc_desc->bFunctionClass == USB_CLASS_VIDEO) {
                ESP_LOGD(TAG, "-------------------- Video Descriptor Start ----------------------");
            } else if (assoc_desc->bFunctionClass == USB_CLASS_AUDIO) {
                ESP_LOGD(TAG, "-------------------- Audio Descriptor Start ----------------------");
            } else {
                print_assoc_desc((const uint8_t *)assoc_desc);
                break;
            }
            print_assoc_desc((const uint8_t *)assoc_desc);
        }
        break;
        case USB_B_DESCRIPTOR_TYPE_CONFIGURATION:
            print_cfg_desc((const uint8_t *)next_desc);
            break;
        case USB_B_DESCRIPTOR_TYPE_INTERFACE: {
            const usb_intf_desc_t *_intf_desc = (const usb_intf_desc_t *)next_desc;
            context_intf = _intf_desc->bInterfaceNumber;
            context_intf_alt = _intf_desc->bAlternateSetting;
            context_class = _intf_desc->bInterfaceClass;
            context_subclass = _intf_desc->bInterfaceSubClass;
            if (context_class == USB_CLASS_VIDEO && context_subclass == VIDEO_SUBCLASS_CONTROL) {
                ESP_LOGD(TAG, "Found Video Control interface");
            }
            if (context_class == USB_CLASS_VIDEO && context_subclass == VIDEO_SUBCLASS_STREAMING) {
                ESP_LOGD(TAG, "Found Video Stream interface, %d-%d", context_intf, context_intf_alt);
            }
            if (context_class == USB_CLASS_AUDIO && context_subclass == AUDIO_SUBCLASS_CONTROL) {
                ESP_LOGD(TAG, "Found Audio Control interface");
            }
            if (context_class == USB_CLASS_AUDIO && context_subclass == AUDIO_SUBCLASS_STREAMING) {
                ESP_LOGD(TAG, "Found Audio Stream interface, %d-%d", context_intf, context_intf_alt);
            }
            print_intf_desc((const uint8_t *)_intf_desc);
        }
        break;
        case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
            print_ep_desc((const uint8_t *)next_desc);
            if (context_class == USB_CLASS_VIDEO && context_subclass == VIDEO_SUBCLASS_STREAMING) {
                uint16_t ep_mps = 0;
                uint8_t ep_attr = 0;
                uint8_t ep_addr = 0;
                parse_ep_desc((const uint8_t *)next_desc, &ep_mps, &ep_addr, &ep_attr);
                if (ep_mps <= USB_EP_ISOC_IN_MAX_MPS && ep_mps > vs_intf_ep_mps) {
                    vs_intf_found = true;
                    vs_intf_ep_mps = ep_mps;
                    vs_intf_ep_attr = ep_attr;
                    vs_intf_ep_addr = ep_addr;
                    vs_intf_idx = context_intf;
                    vs_intf_alt_idx = context_intf_alt;
                }
                if (context_intf_alt == 1) {
                    //workaround for some camera with error desc
                    vs_intf1_ep_mps = ep_mps;
                    vs_intf1_ep_attr = ep_attr;
                    vs_intf1_ep_addr = ep_addr;
                    vs_intf1_idx = context_intf;
                    vs_intf1_alt_idx = context_intf_alt;
                }
            } else if (context_class == USB_CLASS_AUDIO && context_subclass == AUDIO_SUBCLASS_STREAMING) {
                if (context_intf_alt > 1) {
                    ESP_LOGD(TAG, "Found Audio Stream interface %d-%d, skip", context_intf, context_intf_alt);
                    break;
                }
                uint16_t ep_mps = 0;
                uint8_t ep_attr = 0;
                uint8_t ep_addr = 0;
                parse_ep_desc((const uint8_t *)next_desc, &ep_mps, &ep_addr, &ep_attr);
                if (ep_addr & 0x80) {
                    as_mic_intf_found = true;
                    as_mic_intf_ep_mps = ep_mps;
                    as_mic_intf_ep_attr = ep_attr;
                    as_mic_intf_ep_addr = ep_addr;
                    as_mic_intf_idx = context_intf;
                } else {
                    as_spk_intf_found = true;
                    as_spk_intf_ep_mps = ep_mps;
                    as_spk_intf_ep_attr = ep_attr;
                    as_spk_intf_ep_addr = ep_addr;
                    as_spk_intf_idx = context_intf;
                }
            }
            ESP_LOGV(TAG, "descriptor parsed %d/%d, vs interface %d-%d", offset, wTotalLength, context_intf, context_intf_alt);
            break;
        case CS_INTERFACE_DESC:
            if (context_class == USB_CLASS_VIDEO && context_subclass == VIDEO_SUBCLASS_CONTROL) {
                const desc_header_t *header = (const desc_header_t *)next_desc;
                switch (header->bDescriptorSubtype) {
                default:
                    ESP_LOGD(TAG, "Found video control entity, skip");
                    break;
                }
            } else if (context_class == USB_CLASS_VIDEO && context_subclass == VIDEO_SUBCLASS_STREAMING) {
                if (context_intf_alt == 0) {
                    //this is format related desc
                    uint16_t _frame_width = 0;
                    uint16_t _frame_heigh = 0;
                    uint8_t _frame_idx = 0;
                    const desc_header_t *header = (const desc_header_t *)next_desc;
                    switch (header->bDescriptorSubtype) {
                    case VIDEO_CS_ITF_VS_INPUT_HEADER:
                        print_uvc_header_desc((const uint8_t *)next_desc, VIDEO_SUBCLASS_STREAMING);
                        break;
                    case VIDEO_CS_ITF_VS_FORMAT_MJPEG:
                        if (usb_dev->uvc_cfg.format != UVC_FORMAT_MJPEG) {
                            break;
                        }
                        parse_vs_format_mjpeg_desc((const uint8_t *)next_desc, &format_idx, &frame_num, &format);
                        if (uvc_dev) {
                            uvc_frame_size_t *frame_size = uvc_dev->frame_size;
                            frame_size = (uvc_frame_size_t *)heap_caps_realloc(frame_size, frame_num * sizeof(uvc_frame_size_t), MALLOC_CAP_DEFAULT);
                            UVC_CHECK(frame_size, "alloc uvc frame size failed", ESP_ERR_NO_MEM);
                            UVC_ENTER_CRITICAL();
                            uvc_dev->frame_num = frame_num;
                            uvc_dev->frame_size = frame_size;
                            uvc_dev->frame_format = format;
                            UVC_EXIT_CRITICAL();
                        }
                        format_set_found = true;
                        break;
                    case VIDEO_CS_ITF_VS_FRAME_MJPEG: {
                        if (usb_dev->uvc_cfg.format != UVC_FORMAT_MJPEG) {
                            break;
                        }
                        uint8_t interval_type = 0;
                        const uint32_t *pp_interval = NULL;
                        uint32_t dflt_interval = 0;
                        uint32_t max_interval = 0;
                        uint32_t min_interval = 0;
                        uint32_t step_interval = 0;
                        uint32_t final_interval = 0;
                        parse_vs_frame_mjpeg_desc((const uint8_t *)next_desc, &_frame_idx, &_frame_width, &_frame_heigh, &interval_type, &pp_interval, &dflt_interval);
                        if (interval_type) {
                            min_interval = pp_interval[0];
                            for (size_t i = 0; i < interval_type; i++) {
                                if (usb_dev->uvc_cfg.frame_interval == pp_interval[i]) {
                                    final_interval = pp_interval[i];
                                }
                                if (pp_interval[i] > max_interval) {
                                    max_interval = pp_interval[i];
                                }
                                if (pp_interval[i] < min_interval) {
                                    min_interval = pp_interval[i];
                                }
                            }
                        } else {
                            min_interval = pp_interval[0];
                            max_interval = pp_interval[1];
                            step_interval = pp_interval[2];
                            if (usb_dev->uvc_cfg.frame_interval >= min_interval && usb_dev->uvc_cfg.frame_interval <= max_interval) {
                                for (uint32_t i = min_interval; i < max_interval; i += step_interval) {
                                    if (usb_dev->uvc_cfg.frame_interval >= i && usb_dev->uvc_cfg.frame_interval < (i + step_interval)) {
                                        final_interval = i;
                                    }
                                }
                            }
                        }
                        if (final_interval == 0) {
                            final_interval = dflt_interval;
                            ESP_LOGD(TAG, "UVC frame interval %" PRIu32 " not found, using default = %" PRIu32, usb_dev->uvc_cfg.frame_interval, final_interval);
                        } else {
                            ESP_LOGD(TAG, "UVC frame interval %" PRIu32 " found = %" PRIu32, usb_dev->uvc_cfg.frame_interval, final_interval);
                        }
                        if (uvc_dev) {
                            assert((_frame_idx - 1) < uvc_dev->frame_num); //should not happen
                            UVC_ENTER_CRITICAL();
                            uvc_dev->frame_size[_frame_idx - 1].width = _frame_width;
                            uvc_dev->frame_size[_frame_idx - 1].height = _frame_heigh;
                            uvc_dev->frame_size[_frame_idx - 1].interval = final_interval;
                            uvc_dev->frame_size[_frame_idx - 1].interval_min = min_interval;
                            uvc_dev->frame_size[_frame_idx - 1].interval_max = max_interval;
                            uvc_dev->frame_size[_frame_idx - 1].interval_step = step_interval;
                            UVC_EXIT_CRITICAL();
                        }
                        if (user_frame_found == true) {
                            break;
                        }
                        if (((_frame_width == usb_dev->uvc_cfg.frame_width) || (FRAME_RESOLUTION_ANY == usb_dev->uvc_cfg.frame_width))
                                && ((_frame_heigh == usb_dev->uvc_cfg.frame_height) || (FRAME_RESOLUTION_ANY == usb_dev->uvc_cfg.frame_height))) {
                            user_frame_found = true;
                            user_frame_idx = _frame_idx;
                        } else if ((_frame_width == usb_dev->uvc_cfg.frame_height) && (_frame_heigh == usb_dev->uvc_cfg.frame_width)) {
                            ESP_LOGW(TAG, "found width*height %u * %u , orientation swap?", _frame_heigh, _frame_width);
                        }
                        break;
                    }
                    case VIDEO_CS_ITF_VS_FORMAT_FRAME_BASED:
                        if (usb_dev->uvc_cfg.format != UVC_FORMAT_FRAME_BASED) {
                            break;
                        }
                        parse_vs_format_frame_based_desc((const uint8_t *)next_desc, &format_idx, &frame_num, &format);
                        if (uvc_dev) {
                            uvc_frame_size_t *frame_size = uvc_dev->frame_size;
                            frame_size = (uvc_frame_size_t *)heap_caps_realloc(frame_size, frame_num * sizeof(uvc_frame_size_t), MALLOC_CAP_DEFAULT);
                            UVC_CHECK(frame_size, "alloc uvc frame size failed", ESP_ERR_NO_MEM);
                            UVC_ENTER_CRITICAL();
                            uvc_dev->frame_num = frame_num;
                            uvc_dev->frame_size = frame_size;
                            uvc_dev->frame_format = format;
                            UVC_EXIT_CRITICAL();
                        }
                        format_set_found = true;
                        break;
                    case VIDEO_CS_ITF_VS_FRAME_FRAME_BASED: {
                        if (usb_dev->uvc_cfg.format != UVC_FORMAT_FRAME_BASED) {
                            break;
                        }
                        uint8_t interval_type = 0;
                        const uint32_t *pp_interval = NULL;
                        uint32_t dflt_interval = 0;
                        uint32_t max_interval = 0;
                        uint32_t min_interval = 0;
                        uint32_t step_interval = 0;
                        uint32_t final_interval = 0;
                        parse_vs_frame_frame_based_desc((const uint8_t *)next_desc, &_frame_idx, &_frame_width, &_frame_heigh, &interval_type, &pp_interval, &dflt_interval);
                        if (interval_type) {
                            min_interval = pp_interval[0];
                            for (size_t i = 0; i < interval_type; i++) {
                                if (usb_dev->uvc_cfg.frame_interval == pp_interval[i]) {
                                    final_interval = pp_interval[i];
                                }
                                if (pp_interval[i] > max_interval) {
                                    max_interval = pp_interval[i];
                                }
                                if (pp_interval[i] < min_interval) {
                                    min_interval = pp_interval[i];
                                }
                            }
                        } else {
                            min_interval = pp_interval[0];
                            max_interval = pp_interval[1];
                            step_interval = pp_interval[2];
                            if (usb_dev->uvc_cfg.frame_interval >= min_interval && usb_dev->uvc_cfg.frame_interval <= max_interval) {
                                for (uint32_t i = min_interval; i < max_interval; i += step_interval) {
                                    if (usb_dev->uvc_cfg.frame_interval >= i && usb_dev->uvc_cfg.frame_interval < (i + step_interval)) {
                                        final_interval = i;
                                    }
                                }
                            }
                        }
                        if (final_interval == 0) {
                            final_interval = dflt_interval;
                            ESP_LOGD(TAG, "UVC frame interval %" PRIu32 " not found, using default = %" PRIu32, usb_dev->uvc_cfg.frame_interval, final_interval);
                        } else {
                            ESP_LOGD(TAG, "UVC frame interval %" PRIu32 " found = %" PRIu32, usb_dev->uvc_cfg.frame_interval, final_interval);
                        }
                        if (uvc_dev) {
                            assert((_frame_idx - 1) < uvc_dev->frame_num); //should not happen
                            UVC_ENTER_CRITICAL();
                            uvc_dev->frame_size[_frame_idx - 1].width = _frame_width;
                            uvc_dev->frame_size[_frame_idx - 1].height = _frame_heigh;
                            uvc_dev->frame_size[_frame_idx - 1].interval = final_interval;
                            uvc_dev->frame_size[_frame_idx - 1].interval_min = min_interval;
                            uvc_dev->frame_size[_frame_idx - 1].interval_max = max_interval;
                            uvc_dev->frame_size[_frame_idx - 1].interval_step = step_interval;
                            UVC_EXIT_CRITICAL();
                        }
                        if (user_frame_found == true) {
                            break;
                        }
                        if (((_frame_width == usb_dev->uvc_cfg.frame_width) || (FRAME_RESOLUTION_ANY == usb_dev->uvc_cfg.frame_width))
                                && ((_frame_heigh == usb_dev->uvc_cfg.frame_height) || (FRAME_RESOLUTION_ANY == usb_dev->uvc_cfg.frame_height))) {
                            user_frame_found = true;
                            user_frame_idx = _frame_idx;
                        } else if ((_frame_width == usb_dev->uvc_cfg.frame_height) && (_frame_heigh == usb_dev->uvc_cfg.frame_width)) {
                            ESP_LOGW(TAG, "found width*height %u * %u , orientation swap?", _frame_heigh, _frame_width);
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            } else if (context_class == USB_CLASS_AUDIO && context_subclass == AUDIO_SUBCLASS_CONTROL) {
                const desc_header_t *header = (const desc_header_t *)next_desc;
                switch (header->bDescriptorSubtype) {
                case AUDIO_CS_AC_INTERFACE_HEADER:
                    parse_ac_header_desc((const uint8_t *)next_desc, &uac_ver_found, NULL);
                    if (uac_ver_found != UAC_VERSION_1) {
                        ESP_LOGW(TAG, "UAC version 0x%04x Not supported", uac_ver_found);
                    }
                    break;
                case AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL: {
                    uint8_t terminal_idx = 0;
                    uint16_t terminal_type = 0;
                    parse_ac_input_desc((const uint8_t *)next_desc, &terminal_idx, &terminal_type);
                    if (terminal_type == AUDIO_TERM_TYPE_USB_STREAMING) {
                        as_spk_input_terminal = terminal_idx;
                        as_spk_next_unit_idx = terminal_idx;
                    } else if (terminal_type == AUDIO_TERM_TYPE_IN_GENERIC_MIC || terminal_type == AUDIO_TERM_TYPE_HEADSET) {
                        as_mic_input_terminal = terminal_idx;
                        as_mic_next_unit_idx = terminal_idx;
                    } else {
                        ESP_LOGW(TAG, "Input terminal type 0x%04x Not supported", terminal_type);
                    }
                }
                break;
                case AUDIO_CS_AC_INTERFACE_FEATURE_UNIT: {
                    ac_intf_found = true;
                    ac_intf_idx = context_intf;
                    uint8_t source_idx = 0;
                    uint8_t volume_ch = 0;
                    uint8_t mute_ch = 0;
                    uint8_t feature_unit_idx = 0;
                    // Note: we prefer using master channel for feature control
                    parse_ac_feature_desc((const uint8_t *)next_desc, &source_idx, &feature_unit_idx, &volume_ch, &mute_ch);
                    // We just assume the next unit of input terminal is feature unit
                    if (source_idx == as_spk_next_unit_idx || source_idx == as_spk_input_terminal) {
                        as_spk_next_unit_idx = feature_unit_idx;
                        as_spk_feature_unit_idx = feature_unit_idx;
                        as_spk_mute_ch = mute_ch;
                        as_spk_volume_ch = volume_ch;
                    } else if (source_idx == as_mic_next_unit_idx || source_idx == as_mic_input_terminal) {
                        as_mic_next_unit_idx = feature_unit_idx;
                        as_mic_feature_unit_idx = feature_unit_idx;
                        as_mic_mute_ch = mute_ch;
                        as_mic_volume_ch = volume_ch;
                    }
                }
                break;
                case AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL: {
                    uint8_t terminal_idx = 0;
                    uint16_t terminal_type = 0;
                    parse_ac_output_desc((const uint8_t *)next_desc, &terminal_idx, &terminal_type);
                    if (terminal_type == AUDIO_TERM_TYPE_USB_STREAMING) {
                        as_mic_output_terminal = terminal_idx;
                        as_spk_next_unit_idx = terminal_idx;
                    } else if (terminal_type == AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER || terminal_type == AUDIO_TERM_TYPE_HEADSET) {
                        as_mic_next_unit_idx = terminal_idx;
                    } else {
                        ESP_LOGW(TAG, "Output terminal type 0x%04x Not supported", terminal_type);
                    }
                }
                break;
                default:
                    break;
                }
            } else if (context_class == USB_CLASS_AUDIO && context_subclass == AUDIO_SUBCLASS_STREAMING) {
                const desc_header_t *header = (const desc_header_t *)next_desc;
                if (context_intf_alt > 1) {
                    ESP_LOGD(TAG, "Found audio interface sub-desc:%d of %d-%d, skip", header->bDescriptorSubtype, context_intf, context_intf_alt);
                    break;
                }
                switch (header->bDescriptorSubtype) {
                case AUDIO_CS_AS_INTERFACE_AS_GENERAL: {
                    uint16_t format_tag = 0;
                    uint8_t source_idx = 0;
                    parse_as_general_desc((const uint8_t *)next_desc, &source_idx, &format_tag);
                    context_connected_terminal = source_idx;
                    //we only support TYPEI
                    if (format_tag != UAC_FORMAT_TYPEI) {
                        uac_format_others_found = true;
                        ESP_LOGW(TAG, "Audio format type 0x%04x Not supported", format_tag);
                    }
                }
                break;
                case AUDIO_CS_AS_INTERFACE_FORMAT_TYPE: {
                    uint8_t channel_num = 0;
                    uint8_t bit_resolution = 0;
                    uint8_t freq_type = 0;
                    const uint8_t *p_samfreq = NULL;
                    parse_as_type_desc((const uint8_t *)next_desc, &channel_num, &bit_resolution, &freq_type, &p_samfreq);
                    if (context_connected_terminal == as_mic_output_terminal) {
                        if (usb_dev->enabled[STREAM_UAC_MIC]) {
                            uint8_t frame_num = freq_type == 0 ? 1 : freq_type;
                            uac_frame_size_t *frame_size = uac_dev->frame_size[UAC_MIC];
                            frame_size = (uac_frame_size_t *)heap_caps_realloc(frame_size, frame_num * sizeof(uac_frame_size_t), MALLOC_CAP_DEFAULT);
                            UVC_CHECK(frame_size, "alloc mic frame size failed", ESP_ERR_NO_MEM);
                            UVC_ENTER_CRITICAL();
                            uac_dev->frame_num[UAC_MIC] = frame_num;
                            uac_dev->frame_size[UAC_MIC] = frame_size;
                            UVC_EXIT_CRITICAL();
                        }
                        if (channel_num == usb_dev->uac_cfg.mic_ch_num || usb_dev->uac_cfg.mic_ch_num == UAC_CH_ANY) {
                            as_mic_ch_num_found = true;
                        }

                        if (bit_resolution == usb_dev->uac_cfg.mic_bit_resolution || usb_dev->uac_cfg.mic_bit_resolution == UAC_BITS_ANY) {
                            as_mic_bit_res_found = true;
                        }
                        if (freq_type == 0) {
                            uint32_t min_samfreq = (p_samfreq[2] << 16) + (p_samfreq[1] << 8) + p_samfreq[0];
                            uint32_t max_samfreq = (p_samfreq[5] << 16) + (p_samfreq[4] << 8) + p_samfreq[3];
                            uint32_t mic_frame_frequence = 0;
                            uint8_t mic_frame_index = 0;
                            if (usb_dev->uac_cfg.mic_samples_frequence <= max_samfreq && usb_dev->uac_cfg.mic_samples_frequence >= min_samfreq) {
                                as_mic_freq_found = true;
                                mic_frame_index = 1;
                                mic_frame_frequence = usb_dev->uac_cfg.mic_samples_frequence;
                            } else if (usb_dev->uac_cfg.mic_samples_frequence == UAC_FREQUENCY_ANY) {
                                as_mic_freq_found = true;
                                mic_frame_index = 1;
                                mic_frame_frequence = min_samfreq;
                            }
                            if (usb_dev->enabled[STREAM_UAC_MIC]) {
                                UVC_ENTER_CRITICAL();
                                uac_dev->frame_size[UAC_MIC][0].ch_num = channel_num;
                                uac_dev->frame_size[UAC_MIC][0].bit_resolution = bit_resolution;
                                uac_dev->frame_size[UAC_MIC][0].samples_frequence = mic_frame_frequence;
                                uac_dev->frame_size[UAC_MIC][0].samples_frequence_min = min_samfreq;
                                uac_dev->frame_size[UAC_MIC][0].samples_frequence_max = max_samfreq;
                                uac_dev->frame_index[UAC_MIC] = mic_frame_index;
                                UVC_EXIT_CRITICAL();
                            }
                        } else {
                            uint8_t mic_frame_index = 0;
                            for (int i = 0; i < freq_type; ++i) {
                                if (usb_dev->uac_cfg.mic_samples_frequence == UAC_FREQUENCY_ANY) {
                                    as_mic_freq_found = true;
                                    mic_frame_index = 1;
                                } else if (((p_samfreq[3 * i + 2] << 16) + (p_samfreq[3 * i + 1] << 8) + p_samfreq[3 * i]) == usb_dev->uac_cfg.mic_samples_frequence) {
                                    as_mic_freq_found = true;
                                    mic_frame_index = i + 1;
                                }
                                if (usb_dev->enabled[STREAM_UAC_MIC]) {
                                    UVC_ENTER_CRITICAL();
                                    uac_dev->frame_size[UAC_MIC][i].ch_num = channel_num;
                                    uac_dev->frame_size[UAC_MIC][i].bit_resolution = bit_resolution;
                                    uac_dev->frame_size[UAC_MIC][i].samples_frequence = ((p_samfreq[3 * i + 2] << 16) + (p_samfreq[3 * i + 1] << 8) + p_samfreq[3 * i]);
                                    uac_dev->frame_size[UAC_MIC][i].samples_frequence_min = 0;
                                    uac_dev->frame_size[UAC_MIC][i].samples_frequence_max = 0;
                                    uac_dev->frame_index[UAC_MIC] = mic_frame_index;
                                    UVC_EXIT_CRITICAL();
                                }
                            }
                        }

                    } else if (context_connected_terminal == as_spk_input_terminal) {
                        if (usb_dev->enabled[STREAM_UAC_SPK]) {
                            uint8_t frame_num = freq_type == 0 ? 1 : freq_type;
                            uac_frame_size_t *frame_size = uac_dev->frame_size[UAC_SPK];
                            frame_size = (uac_frame_size_t *)heap_caps_realloc(frame_size, frame_num * sizeof(uac_frame_size_t), MALLOC_CAP_DEFAULT);
                            UVC_CHECK(frame_size, "alloc spk frame size failed", ESP_ERR_NO_MEM);
                            UVC_ENTER_CRITICAL();
                            uac_dev->frame_num[UAC_SPK] = frame_num;
                            uac_dev->frame_size[UAC_SPK] = frame_size;
                            UVC_EXIT_CRITICAL();
                        }
                        if (channel_num == usb_dev->uac_cfg.spk_ch_num || usb_dev->uac_cfg.spk_ch_num == UAC_CH_ANY) {
                            as_spk_ch_num_found = true;
                        }
                        if (bit_resolution == usb_dev->uac_cfg.spk_bit_resolution || usb_dev->uac_cfg.spk_bit_resolution == UAC_BITS_ANY) {
                            as_spk_bit_res_found = true;
                        }
                        if (freq_type == 0) {
                            uint32_t min_samfreq = (p_samfreq[2] << 16) + (p_samfreq[1] << 8) + p_samfreq[0];
                            uint32_t max_samfreq = (p_samfreq[5] << 16) + (p_samfreq[4] << 8) + p_samfreq[3];
                            uint32_t spk_frame_frequence = 0;
                            uint8_t spk_frame_index = 0;
                            if (usb_dev->uac_cfg.spk_samples_frequence <= max_samfreq && usb_dev->uac_cfg.spk_samples_frequence >= min_samfreq) {
                                as_spk_freq_found = true;
                                spk_frame_index = 1;
                                spk_frame_frequence = usb_dev->uac_cfg.spk_samples_frequence;
                            } else if (usb_dev->uac_cfg.spk_samples_frequence == UAC_FREQUENCY_ANY) {
                                as_spk_freq_found = true;
                                spk_frame_index = 1;
                                spk_frame_frequence = min_samfreq;
                            }
                            if (usb_dev->enabled[STREAM_UAC_SPK]) {
                                UVC_ENTER_CRITICAL();
                                uac_dev->frame_size[UAC_SPK][0].ch_num = channel_num;
                                uac_dev->frame_size[UAC_SPK][0].bit_resolution = bit_resolution;
                                uac_dev->frame_size[UAC_SPK][0].samples_frequence = spk_frame_frequence;
                                uac_dev->frame_size[UAC_SPK][0].samples_frequence_min = min_samfreq;
                                uac_dev->frame_size[UAC_SPK][0].samples_frequence_max = max_samfreq;
                                uac_dev->frame_index[UAC_SPK] = spk_frame_index;
                                UVC_EXIT_CRITICAL();
                            }
                        } else {
                            uint8_t spk_frame_index = 0;
                            for (int i = 0; i < freq_type; ++i) {
                                if (usb_dev->uac_cfg.spk_samples_frequence == UAC_FREQUENCY_ANY) {
                                    as_spk_freq_found = true;
                                    spk_frame_index = 1;
                                } else if (((p_samfreq[3 * i + 2] << 16) + (p_samfreq[3 * i + 1] << 8) + p_samfreq[3 * i]) == usb_dev->uac_cfg.spk_samples_frequence) {
                                    as_spk_freq_found = true;
                                    spk_frame_index = i + 1;
                                }
                                if (usb_dev->enabled[STREAM_UAC_SPK]) {
                                    UVC_ENTER_CRITICAL();
                                    uac_dev->frame_size[UAC_SPK][i].ch_num = channel_num;
                                    uac_dev->frame_size[UAC_SPK][i].bit_resolution = bit_resolution;
                                    uac_dev->frame_size[UAC_SPK][i].samples_frequence = ((p_samfreq[3 * i + 2] << 16) + (p_samfreq[3 * i + 1] << 8) + p_samfreq[3 * i]);
                                    uac_dev->frame_size[UAC_SPK][i].samples_frequence_min = 0;
                                    uac_dev->frame_size[UAC_SPK][i].samples_frequence_max = 0;
                                    uac_dev->frame_index[UAC_SPK] = spk_frame_index;
                                    UVC_EXIT_CRITICAL();
                                }
                            }
                        }
                    }
                }
                break;
                default:
                    break;
                }
            }
            ESP_LOGV(TAG, "descriptor parsed %d/%d, vs interface %d-%d", offset, wTotalLength, context_intf, context_intf_alt);
            break;
        case CS_ENDPOINT_DESC:
            if (context_class == USB_CLASS_AUDIO && context_subclass == AUDIO_SUBCLASS_STREAMING) {
                if (context_intf_alt > 1) {
                    ESP_LOGD(TAG, "Found audio endpoint desc of interface %d-%d, skip", context_intf, context_intf_alt);
                    break;
                }
                as_cs_ep_desc_t *desc = (as_cs_ep_desc_t *)next_desc;
                if (context_connected_terminal == as_spk_input_terminal) {
                    as_spk_freq_ctrl_found = AUDIO_EP_CONTROL_SAMPLING_FEQ & desc->bmAttributes;
                } else if (context_connected_terminal == as_mic_output_terminal) {
                    as_mic_freq_ctrl_found = AUDIO_EP_CONTROL_SAMPLING_FEQ & desc->bmAttributes;
                }
            }
            break;
        default:
            break;
        }
        if (!already_next && next_desc) {
            next_desc = usb_parse_next_descriptor(next_desc, wTotalLength, &offset);
        }
        ESP_LOGV(TAG, "descriptor parsed %d/%d", offset, wTotalLength);
    } while (next_desc != NULL);

    // check all params we get
    if (usb_dev->enabled[STREAM_UVC]) {
        if (vs_intf_found) {
            //Re-config uvc device
            UVC_ENTER_CRITICAL();
            uvc_dev->vs_ifc->interface = vs_intf_idx;
            uvc_dev->vs_ifc->interface_alt = vs_intf_alt_idx;
            uvc_dev->vs_ifc->ep_addr = vs_intf_ep_addr;
            UVC_EXIT_CRITICAL();
            uvc_dev->vs_ifc->ep_mps = vs_intf_ep_mps;
            uvc_dev->vs_ifc->xfer_type = (vs_intf_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_ISOC ? UVC_XFER_ISOC
                                         : ((vs_intf_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK ? UVC_XFER_BULK : UVC_XFER_UNKNOWN);
            ESP_LOGI(TAG, "Actual VS Interface(MPS <= %d) found, interface = %u, alt = %u", USB_EP_ISOC_IN_MAX_MPS, vs_intf_idx, vs_intf_alt_idx);
            ESP_LOGI(TAG, "\tEndpoint(%s) Addr = 0x%x, MPS = %u", uvc_dev->vs_ifc->xfer_type == UVC_XFER_ISOC ? "ISOC"
                     : (uvc_dev->vs_ifc->xfer_type == UVC_XFER_BULK ? "BULK" : "Unknown"), vs_intf_ep_addr, vs_intf_ep_mps);
        } else if (usb_dev->uvc_cfg.interface) {
            //Try with user's config
            ESP_LOGW(TAG, "VS Interface(MPS <= %d) NOT found", USB_EP_ISOC_IN_MAX_MPS);
            ESP_LOGW(TAG, "Try with user's config");
            UVC_ENTER_CRITICAL();
            uvc_dev->vs_ifc->interface = usb_dev->uvc_cfg.interface;
            uvc_dev->vs_ifc->interface_alt = usb_dev->uvc_cfg.interface_alt;
            uvc_dev->vs_ifc->ep_addr = usb_dev->uvc_cfg.ep_addr;
            UVC_EXIT_CRITICAL();
            uvc_dev->vs_ifc->ep_mps = usb_dev->uvc_cfg.ep_mps;
            uvc_dev->vs_ifc->xfer_type = usb_dev->uvc_cfg.xfer_type;
            vs_intf_found = true;
        } else {
            //Try with first interface
            UVC_ENTER_CRITICAL();
            uvc_dev->vs_ifc->interface = vs_intf1_idx;
            uvc_dev->vs_ifc->interface_alt = vs_intf1_alt_idx;
            uvc_dev->vs_ifc->ep_addr = vs_intf1_ep_addr;
            UVC_EXIT_CRITICAL();
            uvc_dev->vs_ifc->ep_mps = vs_intf1_ep_mps;
            uvc_dev->vs_ifc->xfer_type = (vs_intf1_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_ISOC ? UVC_XFER_ISOC
                                         : ((vs_intf1_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK ? UVC_XFER_BULK : UVC_XFER_UNKNOWN);
            vs_intf_found = true;
            ESP_LOGW(TAG, "VS Interface(MPS <= %d) NOT found", USB_EP_ISOC_IN_MAX_MPS);
            ESP_LOGW(TAG, "Try with first alt-interface config");
        }
        if (format_set_found) {
            ESP_LOGI(TAG, "Actual %s format index, format index = %u, contains %u frames", usb_dev->uvc_cfg.format == UVC_FORMAT_FRAME_BASED ? "Frame Based" : "MJPEG", format_idx, frame_num);
            uvc_dev->format_index = format_idx;
        } else if (usb_dev->uvc_cfg.format_index) {
            ESP_LOGW(TAG, "Setting format: %d NOT found", usb_dev->uvc_cfg.format);
            ESP_LOGW(TAG, "Try with user's config");
            uvc_dev->format_index = usb_dev->uvc_cfg.format_index;
        } else {
            ESP_LOGE(TAG, "Setting format: %d NOT found", usb_dev->uvc_cfg.format);
            // We treat MJPEG format as mandatory
            vs_intf_found = false;
        }
        if (user_frame_found) {
            UVC_ENTER_CRITICAL();
            uvc_dev->frame_index = user_frame_idx;
            UVC_EXIT_CRITICAL();
            ESP_LOGI(TAG, "Actual Frame: %d, width*height: %u*%u, frame index = %u", uvc_dev->frame_format, usb_dev->uvc_cfg.frame_width, usb_dev->uvc_cfg.frame_height, user_frame_idx);
        } else if (usb_dev->uvc_cfg.frame_index) {
            ESP_LOGW(TAG, "Frame: %d, width*height: %u*%u, NOT found", uvc_dev->frame_format, usb_dev->uvc_cfg.frame_width, usb_dev->uvc_cfg.frame_height);
            ESP_LOGW(TAG, "Try with user's config");
            UVC_ENTER_CRITICAL();
            uvc_dev->frame_index = usb_dev->uvc_cfg.frame_index;
            uvc_dev->frame_height = usb_dev->uvc_cfg.frame_height;
            uvc_dev->frame_width = usb_dev->uvc_cfg.frame_width;
            UVC_EXIT_CRITICAL();
        } else {
            // No suitable frame found, we need suspend UVC interface during start
            ESP_LOGW(TAG, "Frame: %d, width*height: %u*%u, NOT found", uvc_dev->frame_format, usb_dev->uvc_cfg.frame_width, usb_dev->uvc_cfg.frame_height);
            vs_intf_found = false;
        }
    }

    if (usb_dev->enabled[STREAM_UAC_MIC] || usb_dev->enabled[STREAM_UAC_SPK]) {
        if (ac_intf_found) {
            ESP_LOGI(TAG, "Audio control interface = %d", ac_intf_idx);
            if (as_spk_feature_unit_idx) {
                ESP_LOGI(TAG, "Speaker feature unit = %d", as_spk_feature_unit_idx);
            } else if (usb_dev->uac_cfg.spk_fu_id) {
                as_spk_feature_unit_idx = usb_dev->uac_cfg.spk_fu_id;
                ESP_LOGW(TAG, "Speaker feature unit NOT found, try with user's config");
            }
            if (as_spk_volume_ch != __UINT8_MAX__) {
                ESP_LOGI(TAG, "\tSupport volume control, ch = %d", as_spk_volume_ch);
            } else {
                as_spk_volume_ch = 0;
            }
            if (as_spk_mute_ch != __UINT8_MAX__) {
                ESP_LOGI(TAG, "\tSupport mute control, ch = %d", as_spk_mute_ch);
            } else {
                as_spk_mute_ch = 0;
            }
            if (as_mic_feature_unit_idx) {
                ESP_LOGI(TAG, "Mic feature unit = %d", as_mic_feature_unit_idx);
            } else if (usb_dev->uac_cfg.mic_fu_id) {
                as_mic_feature_unit_idx = usb_dev->uac_cfg.mic_fu_id;
                ESP_LOGW(TAG, "Mic feature unit NOT found, try with user's config");
            }
            if (as_mic_volume_ch != __UINT8_MAX__) {
                ESP_LOGI(TAG, "\tSupport volume control, ch = %d", as_mic_volume_ch);
            } else {
                as_mic_volume_ch = 0;
            }
            if (as_mic_mute_ch != __UINT8_MAX__) {
                ESP_LOGI(TAG, "\tSupport mute control, ch = %d", as_mic_mute_ch);
            } else {
                as_mic_mute_ch = 0;
            }
        } else if (usb_dev->uac_cfg.mic_fu_id || usb_dev->uac_cfg.spk_fu_id) {
            ac_intf_idx = usb_dev->uac_cfg.ac_interface;
            as_mic_feature_unit_idx = usb_dev->uac_cfg.mic_fu_id;
            as_spk_feature_unit_idx = usb_dev->uac_cfg.spk_fu_id;
            as_spk_volume_ch = 0;
            as_spk_mute_ch = 0;
            as_mic_volume_ch = 0;
            as_mic_mute_ch = 0;
            ESP_LOGW(TAG, "Audio control interface NOT found");
            ESP_LOGW(TAG, "Try with user's config");
        } else {
            ESP_LOGW(TAG, "Audio control interface NOT found");
        }
        UVC_ENTER_CRITICAL();
        uac_dev->ac_interface = ac_intf_idx;
        uac_dev->fu_id[UAC_SPK] = as_spk_feature_unit_idx;
        uac_dev->volume_ch[UAC_SPK] = as_spk_volume_ch;
        uac_dev->mute_ch[UAC_SPK] = as_spk_mute_ch;
        uac_dev->fu_id[UAC_MIC] = as_mic_feature_unit_idx;
        uac_dev->volume_ch[UAC_MIC] = as_mic_volume_ch;
        uac_dev->mute_ch[UAC_MIC] = as_mic_mute_ch;
        UVC_EXIT_CRITICAL();
    }

    if (usb_dev->enabled[STREAM_UAC_SPK] && as_spk_intf_found && uac_ver_found == UAC_VERSION_1 && !uac_format_others_found) {
        UVC_ENTER_CRITICAL();
        uac_dev->as_ifc[UAC_SPK]->interface = as_spk_intf_idx;
        uac_dev->as_ifc[UAC_SPK]->interface_alt = 1;
        uac_dev->as_ifc[UAC_SPK]->ep_addr = as_spk_intf_ep_addr;
        UVC_EXIT_CRITICAL();
        uac_dev->as_ifc[UAC_SPK]->ep_mps = as_spk_intf_ep_mps;
        ESP_LOGI(TAG, "Speaker Interface found, interface = %u", as_spk_intf_idx);
        ESP_LOGI(TAG, "\tEndpoint(%s) Addr = 0x%x, MPS = %u", (as_spk_intf_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_ISOC ? "ISOC"
                 : ((as_spk_intf_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK ? "BULK" : "Unknown"), as_spk_intf_ep_addr, as_spk_intf_ep_mps);
        if (!as_spk_ch_num_found) {
            ESP_LOGW(TAG, "\tSpeaker channel num %d Not supported", usb_dev->uac_cfg.spk_ch_num);
        }
        if (!as_spk_bit_res_found) {
            ESP_LOGW(TAG, "\tSpeaker bit resolution %d Not supported", usb_dev->uac_cfg.spk_bit_resolution);
        }
        if (!as_spk_freq_found) {
            ESP_LOGW(TAG, "\tSpeaker frequency %"PRIu32" Not supported", usb_dev->uac_cfg.spk_samples_frequence);
        } else  {
            uac_dev->freq_ctrl_support[UAC_SPK] = as_spk_freq_ctrl_found;
            ESP_LOGI(TAG, "\tSpeaker frequency control %s Support", as_spk_freq_ctrl_found ? "" : "Not");
        }
    } else if (usb_dev->enabled[STREAM_UAC_SPK] && usb_dev->uac_cfg.spk_interface) {
        UVC_ENTER_CRITICAL();
        uac_dev->as_ifc[UAC_SPK]->interface = usb_dev->uac_cfg.spk_interface;
        uac_dev->as_ifc[UAC_SPK]->interface_alt = 1;
        uac_dev->as_ifc[UAC_SPK]->ep_addr = usb_dev->uac_cfg.spk_ep_addr;
        UVC_EXIT_CRITICAL();
        uac_dev->as_ifc[UAC_SPK]->ep_mps = usb_dev->uac_cfg.spk_ep_mps;
        ESP_LOGW(TAG, "Speaker Interface NOT found");
        ESP_LOGW(TAG, "Try with user's config");
        as_spk_intf_found = true;
    } else {
        as_spk_intf_found = false;
    }

    if (usb_dev->enabled[STREAM_UAC_MIC] && as_mic_intf_found && uac_ver_found == UAC_VERSION_1 && !uac_format_others_found) {
        UVC_ENTER_CRITICAL();
        uac_dev->as_ifc[UAC_MIC]->interface = as_mic_intf_idx;
        uac_dev->as_ifc[UAC_MIC]->interface_alt = 1;
        uac_dev->as_ifc[UAC_MIC]->ep_addr = as_mic_intf_ep_addr;
        UVC_EXIT_CRITICAL();
        uac_dev->as_ifc[UAC_MIC]->ep_mps = as_mic_intf_ep_mps;
        ESP_LOGI(TAG, "Mic Interface found interface = %u", as_mic_intf_idx);
        ESP_LOGI(TAG, "\tEndpoint(%s) Addr = 0x%x, MPS = %u", (as_mic_intf_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_ISOC ? "ISOC"
                 : ((as_mic_intf_ep_attr & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_BULK ? "BULK" : "Unknown"), as_mic_intf_ep_addr, as_mic_intf_ep_mps);
        if (!as_mic_ch_num_found) {
            ESP_LOGW(TAG, "\tMic channel num %d Not supported", usb_dev->uac_cfg.mic_ch_num);
        }
        if (!as_mic_bit_res_found) {
            ESP_LOGW(TAG, "\tMic bit resolution %d Not supported", usb_dev->uac_cfg.mic_bit_resolution);
        }
        if (!as_mic_freq_found) {
            ESP_LOGW(TAG, "\tMic frequency %"PRIu32" Not supported", usb_dev->uac_cfg.mic_samples_frequence);
        } else {
            uac_dev->freq_ctrl_support[UAC_MIC] = as_mic_freq_ctrl_found;
            ESP_LOGI(TAG, "\tMic frequency control %s Support", as_mic_freq_ctrl_found ? "" : "Not");
        }
    } else if (usb_dev->enabled[STREAM_UAC_MIC] && usb_dev->uac_cfg.mic_interface) {
        UVC_ENTER_CRITICAL();
        uac_dev->as_ifc[UAC_MIC]->interface = usb_dev->uac_cfg.mic_interface;
        uac_dev->as_ifc[UAC_MIC]->interface_alt = 1;
        uac_dev->as_ifc[UAC_MIC]->ep_addr = usb_dev->uac_cfg.mic_ep_addr;
        UVC_EXIT_CRITICAL();
        uac_dev->as_ifc[UAC_MIC]->ep_mps = usb_dev->uac_cfg.mic_ep_mps;
        ESP_LOGW(TAG, "Mic interface NOT found");
        ESP_LOGW(TAG, "Try with user's config");
        as_mic_intf_found = true;
    } else {
        as_mic_intf_found = false;
    }

    if (usb_dev->enabled[STREAM_UAC_MIC]) {
        uac_dev->as_ifc[UAC_MIC]->not_found = !as_mic_intf_found;
    }
    if (usb_dev->enabled[STREAM_UAC_SPK]) {
        uac_dev->as_ifc[UAC_SPK]->not_found = !as_spk_intf_found;
    }
    if (usb_dev->enabled[STREAM_UVC]) {
        uvc_dev->vs_ifc->not_found = !vs_intf_found;
    }

    if (!(as_mic_intf_found || as_spk_intf_found || vs_intf_found)) {
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static esp_err_t _usb_ctrl_xfer(urb_t *urb, TickType_t xTicksToWait)
{
    UVC_CHECK(_usb_device_get_state() > STATE_DEVICE_INSTALLED, "USB Device not active", ESP_ERR_INVALID_STATE);
    UVC_CHECK(_usb_device_get_state() != STATE_DEVICE_RECOVER, "USB Device not active", ESP_ERR_INVALID_STATE);
    UVC_CHECK(urb != NULL, "invalid args", ESP_ERR_INVALID_ARG);
    // Dequeue the previous timeout urb
    ESP_LOGD(TAG, "Control Transfer Start");
    hcd_urb_dequeue(s_usb_dev.dflt_pipe_hdl);
    xEventGroupClearBits(s_usb_dev.event_group_hdl, USB_CTRL_PROC_SUCCEED | USB_CTRL_PROC_FAILED);
    SYSVIEW_DFLT_PIPE_XFER_START();
    esp_err_t ret = hcd_urb_enqueue(s_usb_dev.dflt_pipe_hdl, urb);
    UVC_CHECK(ESP_OK == ret, "urb enqueue failed", ESP_FAIL);
    EventBits_t uxBits = xEventGroupWaitBits(s_usb_dev.event_group_hdl, USB_CTRL_PROC_SUCCEED | USB_CTRL_PROC_FAILED, pdTRUE, pdFALSE, xTicksToWait);
    SYSVIEW_DFLT_PIPE_XFER_STOP();
    if (uxBits & USB_CTRL_PROC_SUCCEED) {
        // If not failed, handle the result of the transfer.
        ESP_LOGD(TAG, "Control Transfer Done");
    } else if (uxBits & USB_CTRL_PROC_FAILED) {
        // If failed, the internal error handler will handle the urb.
        ESP_LOGE(TAG, "Control Transfer Failed");
        return ESP_FAIL;
    } else {
        // If timeout, dequeue the urb before next transfer.
        ESP_LOGE(TAG, "Control Transfer Timeout");
        // Notify the usb task to halt the transfer.
        _event_msg_t evt_msg = {
            ._type = USER_EVENT,
            ._event.user_cmd = PIPE_RECOVER
        };
        xQueueSend(s_usb_dev.queue_hdl, &evt_msg, xTicksToWait);
        // wait for recover done
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGD(TAG, "Control Transfer Recover");
        return ESP_ERR_TIMEOUT;
    }
    urb_t *urb_done = hcd_urb_dequeue(s_usb_dev.dflt_pipe_hdl);
    UVC_CHECK(urb_done == urb, "urb status: not same", ESP_FAIL);
    UVC_CHECK(USB_TRANSFER_STATUS_COMPLETED == urb_done->transfer.status, "urb status: not complete", ESP_FAIL);
    return ESP_OK;
}

static esp_err_t _usb_set_device_interface(uint16_t interface, uint16_t interface_alt)
{
    UVC_CHECK(interface != 0, "interface can't be 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    urb_t *urb_ctrl = s_usb_dev.ctrl_urb;
    bool need_free = false;

    if (urb_ctrl == NULL) {
        urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t), NULL);
        UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
        need_free = true;
    }
    xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
    USB_SETUP_PACKET_INIT_SET_INTERFACE((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, interface, interface_alt);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t);
    ESP_LOGI(TAG, "Set Device Interface = %u, Alt = %u", interface, interface_alt);
    esp_err_t ret = _usb_ctrl_xfer(urb_ctrl, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
    if (ESP_OK == ret) {
        ESP_LOGD(TAG, "Set Device Interface Done");
    } else {
        ESP_LOGE(TAG, "Set Device Interface Failed");
    }

    if (need_free) {
        _usb_urb_free(urb_ctrl);
    }
    return ret;
}

static esp_err_t _uvc_vs_commit_control(uvc_stream_ctrl_t *ctrl_set, uvc_stream_ctrl_t *ctrl_probed)
{
    UVC_CHECK(ctrl_set != NULL && ctrl_probed != NULL, "pointer can not be NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    urb_t *urb_ctrl = s_usb_dev.ctrl_urb;
    bool need_free = false;
    if (urb_ctrl == NULL) {
        urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + 128, NULL);
        UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
        need_free = true;
    }

    ESP_LOGD(TAG, "SET_CUR Probe");
    xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
    USB_CTRL_UVC_PROBE_SET_REQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    _uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_set);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    esp_err_t ret = _usb_ctrl_xfer(urb_ctrl, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
    UVC_CHECK_GOTO(ESP_OK == ret, "SET_CUR Probe failed", free_urb_);
    ESP_LOGD(TAG, "SET_CUR Probe Done");

    ESP_LOGD(TAG, "GET_CUR Probe");
    xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
    USB_CTRL_UVC_PROBE_GET_REQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, s_usb_dev.ep_mps); //IN should be integer multiple of MPS
    ret = _usb_ctrl_xfer(urb_ctrl, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
    UVC_CHECK_GOTO(ESP_OK == ret, "GET_CUR Probe failed", free_urb_);
    if ((urb_ctrl->transfer.actual_num_bytes > sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength)) {
        ESP_LOGW(TAG, "Probe data overflow");
    }
    _buf_to_uvc_stream_ctrl((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_probed);
    ESP_LOGD(TAG, "GET_CUR Probe Done, actual_num_bytes:%d", urb_ctrl->transfer.actual_num_bytes);
#ifdef CONFIG_UVC_PRINT_PROBE_RESULT
    _uvc_stream_ctrl_printf(stdout, ctrl_probed);
#endif

    ESP_LOGD(TAG, "SET_CUR Commit");
    xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
    USB_CTRL_UVC_COMMIT_REQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer);
    _uvc_stream_ctrl_to_buf((urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t)), ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength, ctrl_probed);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    ret = _usb_ctrl_xfer(urb_ctrl, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
    UVC_CHECK_GOTO(ESP_OK == ret, "SET_CUR Commit failed", free_urb_);
    ESP_LOGD(TAG, "SET_CUR Commit Done");

free_urb_:
    if (need_free) {
        _usb_urb_free(urb_ctrl);
    }
    return ret;
}

static esp_err_t _uac_as_control_set_mute(uint16_t ac_itc, uint8_t ch, uint8_t fu_id, bool if_mute)
{
    UVC_CHECK(fu_id != 0, "invalid fu_id", ESP_ERR_INVALID_ARG);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    //check if enum done
    urb_t *urb_ctrl = s_usb_dev.ctrl_urb;
    bool need_free = false;
    if (urb_ctrl == NULL) {
        urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + 64, NULL);
        UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
        need_free = true;
    }
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < 8; i++) {
        if (ch & (1 << i)) {
            ESP_LOGD(TAG, "SET_CUR mute, ac_itc = %u, ch = %u, fu_id = %u", ac_itc, i, fu_id);
            ESP_LOGD(TAG, "%s CH%u", if_mute ? "Mute" : "UnMute", i);
            xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
            USB_CTRL_UAC_SET_FU_MUTE((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, i, fu_id, ac_itc);
            unsigned char *p_data = urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t);
            p_data[0] = if_mute;
            urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
            ret = _usb_ctrl_xfer(urb_ctrl, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
            xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
            UVC_CHECK_GOTO(ESP_OK == ret, "SET_CUR mute failed", free_urb_);
            ESP_LOGD(TAG, "SET_CUR mute Done");
        }
    }

free_urb_:
    if (need_free) {
        _usb_urb_free(urb_ctrl);
    }
    return ret;
}

static esp_err_t _uac_as_control_set_volume(uint16_t ac_itc, uint8_t ch, uint8_t fu_id, uint32_t volume)
{
    UVC_CHECK(fu_id != 0, "invalid fu_id", ESP_ERR_INVALID_ARG);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    urb_t *urb_ctrl = s_usb_dev.ctrl_urb;
    bool need_free = false;
    if (urb_ctrl == NULL) {
        urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + 64, NULL);
        UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
        need_free = true;
    }
    uint32_t volume_db = UAC_SPK_VOLUME_MIN + volume * UAC_SPK_VOLUME_STEP;
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < 8; i++) {
        if (ch & (1 << i)) {
            ESP_LOGD(TAG, "SET_CUR volume 0x%04x (%"PRIu32") ac_itc=%u, ch=%u, fu_id=%u", (uint16_t)(volume_db & 0xffff), volume, ac_itc, i, fu_id);
            ESP_LOGD(TAG, "Set volume CH%u: 0x%04xdb (%"PRIu32")", i, (uint16_t)(volume_db & 0xffff), volume);
            xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
            USB_CTRL_UAC_SET_FU_VOLUME((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, i, fu_id, ac_itc);
            unsigned char *p_data = urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t);
            urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
            p_data[0] = volume_db & 0x00ff;
            p_data[1] = (volume_db & 0xff00) >> 8;
            ret = _usb_ctrl_xfer(urb_ctrl, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
            xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
            UVC_CHECK_GOTO(ESP_OK == ret, "SET_CUR volume failed", free_urb_);
            ESP_LOGD(TAG, "SET_CUR volume Done");
        }
    }

free_urb_:
    if (need_free) {
        _usb_urb_free(urb_ctrl);
    }
    return ret;
}

static esp_err_t _uac_as_control_set_freq(uint8_t ep_addr, uint32_t freq)
{
    UVC_CHECK(ep_addr != 0, "invalid ep_addr", ESP_ERR_INVALID_ARG);
    UVC_CHECK(freq != 0, "invalid freq", ESP_ERR_INVALID_ARG)
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    if (ep_addr & USB_EP_DIR_MASK) {
        UVC_CHECK(s_usb_dev.uac->freq_ctrl_support[UAC_MIC], "Mic frequency control not support", ESP_ERR_NOT_SUPPORTED);
    } else {
        UVC_CHECK(s_usb_dev.uac->freq_ctrl_support[UAC_SPK], "Speaker frequency control not support", ESP_ERR_NOT_SUPPORTED);
    }

    urb_t *urb_ctrl = s_usb_dev.ctrl_urb;
    bool need_free = false;
    if (urb_ctrl == NULL) {
        urb_ctrl = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + 64, NULL);
        UVC_CHECK(urb_ctrl != NULL, "alloc urb failed", ESP_ERR_NO_MEM);
        need_free = true;
    }
    ESP_LOGI(TAG, "Set frequency endpoint 0x%02x: (%"PRIu32") Hz", ep_addr, freq);
    ESP_LOGD(TAG, "SET_CUR frequency %"PRIu32"", freq);
    xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
    USB_CTRL_UAC_SET_EP_FREQ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer, ep_addr);
    unsigned char *p_data = urb_ctrl->transfer.data_buffer + sizeof(usb_setup_packet_t);
    urb_ctrl->transfer.num_bytes = sizeof(usb_setup_packet_t) + ((usb_setup_packet_t *)urb_ctrl->transfer.data_buffer)->wLength;
    p_data[0] = freq & 0x0000ff;
    p_data[1] = (freq & 0x00ff00) >> 8;
    p_data[2] = (freq & 0xff0000) >> 16;
    esp_err_t ret = _usb_ctrl_xfer(urb_ctrl, pdMS_TO_TICKS(TIMEOUT_USB_CTRL_XFER_MS));
    xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
    UVC_CHECK_GOTO(ESP_OK == ret, "SET_CUR frequency failed", free_urb_);
    ESP_LOGD(TAG, "SET_CUR frequency Done");

free_urb_:
    if (need_free) {
        _usb_urb_free(urb_ctrl);
    }
    return ret;
}

/***************************************************LibUVC API Implements****************************************/
/**
 * @brief Swap the working buffer with the presented buffer and notify consumers
 */
IRAM_ATTR static void _uvc_swap_buffers(_uvc_stream_handle_t *strmh)
{
    /* to prevent the latest data from being lost
    * if take mutex timeout, we should drop the last frame */
    size_t timeout_ms = 0;
    if (s_usb_dev.uvc->vs_ifc->xfer_type == UVC_XFER_BULK) {
        // for full speed bulk mode, max 19 packet can be transmit during each millisecond
        timeout_ms = (s_usb_dev.uvc->vs_ifc->urb_num - 1) * (s_usb_dev.uvc->vs_ifc->bytes_per_packet * 0.052 / USB_EP_BULK_FS_MPS);
    } else {
        timeout_ms = (s_usb_dev.uvc->vs_ifc->urb_num - 1) * s_usb_dev.uvc->vs_ifc->packets_per_urb;
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
        ESP_LOGV(TAG, "uvc swap buffer length = %d", strmh->hold_bytes);
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
IRAM_ATTR static void _uvc_drop_buffers(_uvc_stream_handle_t *strmh)
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
    if (strmh->hold_bytes > s_usb_dev.uvc_cfg.frame_buffer_size) {
        ESP_LOGW(TAG, "Frame Buffer Overflow, framesize = %u", strmh->hold_bytes);
        return;
    }

    uvc_frame_t *frame = &strmh->frame;
    frame->frame_format = strmh->frame_format;
    frame->width = s_usb_dev.uvc->frame_width;
    frame->height = s_usb_dev.uvc->frame_height;

    frame->step = 0;
    frame->sequence = strmh->hold_seq;
    frame->capture_time_finished = strmh->capture_time_finished;
    frame->data_bytes = strmh->hold_bytes;
    memcpy(frame->data, strmh->holdbuf, frame->data_bytes);
}

/**
 * @brief Process each payload of uvc transfer
 *
 * @param strmh stream handle
 * @param payload payload buffer
 * @param payload_len payload buffer length
 */
IRAM_ATTR static void _uvc_process_payload(_uvc_stream_handle_t *strmh, size_t req_len, uint8_t *payload, size_t payload_len)
{
    size_t header_len = 0;
    size_t variable_offset = 0;
    uint8_t header_info = 0;
    size_t data_len = 0;
    bool bulk_xfer = (s_usb_dev.uvc->vs_ifc->xfer_type == UVC_XFER_BULK) ? true : false;
    bool reassemble = (strmh->reassemble_flag) ? true : false;
    uint8_t flag_lstp = 0;
    uint8_t flag_zlp = 0;
    uint8_t flag_rsb = 0;

    // analyze the payload handling logic depending on the transfer type
    // and the reassembly flag
    if (bulk_xfer && reassemble) {
        if (payload_len == req_len) {
            //payload transfer not complete
            flag_rsb = 1;
        } else if (payload_len == 0) {
            //payload transfer complete with zero length packet
            flag_zlp = 1;
            ESP_LOGV(TAG, "payload_len == 0");
        } else {
            //payload transfer complete with short packet
            flag_lstp = 1;
        }
    } else if (bulk_xfer && payload_len < req_len) {
        flag_lstp = 1;
    } else if (payload_len == 0) {
        // ignore empty payload for isoc transfer
        return;
    }

#ifdef CONFIG_UVC_PRINT_PAYLOAD_HEX
    ESP_LOG_BUFFER_HEXDUMP("UVC_HEX", payload, payload_len, ESP_LOG_VERBOSE);
#endif
    /********************* processing header *******************/
    if (!flag_zlp) {
        ESP_LOGV(TAG, "zlp=%d, lstp=%d, req_len=%d, payload_len=%d, first=0x%02x, second=0x%02x", flag_zlp, flag_lstp, req_len, payload_len, payload[0], payload_len > 1 ? payload[1] : 0);
        // make sure this is a header, judge from header length and bit field
        // For SCR, PTS, some vendors not set bit, but also offer 12 Bytes header. so we just check SET condition
        if (payload_len >= payload[0]
                && (payload[0] == 12 || (payload[0] == 2 && !(payload[1] & 0x0C)) || (payload[0] == 6 && !(payload[1] & 0x08)))
                && !(payload[1] & 0x30)
#ifdef CONFIG_UVC_CHECK_HEADER_EOH
                /* EOH bit, when set, indicates the end of the BFH fields
                 * Most camera set this bit to 1 in each header, but some vendors may not set it.
                 */
                && (payload[1] & 0x80)
#endif
#ifdef CONFIG_UVC_CHECK_BULK_JPEG_HEADER
                && (!reassemble || ((payload[payload[0]] == 0xff) && (payload[payload[0] + 1] == 0xd8)))
#endif
           ) {
            header_len = payload[0];
            data_len = payload_len - header_len;
            /* checking the end-of-header */
            variable_offset = 2;
            header_info = payload[1];

            ESP_LOGV(TAG, "header=%u info=0x%02x, payload_len = %u, last_pts = %"PRIu32" , last_scr = %"PRIu32"", header_len, header_info, payload_len, strmh->pts, strmh->last_scr);
            if (flag_rsb) {
                ESP_LOGV(TAG, "reassembling start ...");
                strmh->reassembling = 1;
            }
            /* ERR bit defined in Stream Header*/
            if (header_info & 0x40) {
                ESP_LOGW(TAG, "bad packet: error bit set");
                strmh->reassembling = 0;
                return;
            }
        } else if (strmh->reassembling) {
            ESP_LOGV(TAG, "reassembling %u + %u", strmh->got_bytes, payload_len);
            data_len = payload_len;
        } else {
            if (payload_len > 1) {
#ifdef CONFIG_UVC_CHECK_HEADER_EOH
                /* Give warning if EOH check enable, but camera not have*/
                if (!(payload[1] & 0x80)) {
                    ESP_LOGD(TAG, "bogus packet: EOH bit not set");
                }
#endif
                ESP_LOGD(TAG, "bogus packet: len = %u %02x %02x...%02x %02x\n", payload_len, payload[0], payload[1], payload[payload_len - 2], payload[payload_len - 1]);
            }
            return;
        }
    }

    if (header_len >= 2) {
        if (strmh->fid != (header_info & 1) && strmh->got_bytes != 0) {
            /* The frame ID bit was flipped, but we have image data sitting
                around from prior transfers. This means the camera didn't send
                an EOF for the last transfer of the previous frame. */
#if CONFIG_UVC_DROP_NO_EOF_FRAME
            ESP_LOGW(TAG, "DROP NO EOF, got data=%u B", strmh->got_bytes);
            _uvc_drop_buffers(strmh);
#else
            ESP_LOGD(TAG, "SWAP NO EOF %d", strmh->got_bytes);
            _uvc_swap_buffers(strmh);
#endif
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
    }

    /********************* processing data *****************/
    if (data_len >= 1) {
        if (strmh->got_bytes + data_len > s_usb_dev.uvc_cfg.xfer_buffer_size) {
            /* This means transfer buffer Not enough for whole frame, just drop whole buffer here.
            Please increase buffer size to handle big frame*/
#if CONFIG_UVC_DROP_OVERFLOW_FRAME
            ESP_LOGW(TAG, "Transfer buffer overflow, got data=%u B, last=%u (%02x %02x...%02x %02x)", strmh->got_bytes + data_len, data_len
                     , payload[0], payload_len > 1 ? payload[1] : 0, payload_len > 1 ? payload[payload_len - 2] : 0, payload_len > 1 ? payload[payload_len - 1] : 0);
            _uvc_drop_buffers(strmh);
#else
            ESP_LOGD(TAG, "SWAP overflow %d", strmh->got_bytes);
            _uvc_swap_buffers(strmh);
#endif
            strmh->reassembling = 0;
            return;
        } else {
            if (payload_len > 1) {
                ESP_LOGV(TAG, "uvc payload = %02x %02x...%02x %02x\n", payload[header_len], payload[header_len + 1], payload[payload_len - 2], payload[payload_len - 1]);
            }
            memcpy(strmh->outbuf + strmh->got_bytes, payload + header_len, data_len);
        }

        strmh->got_bytes += data_len;
    }

    if (flag_lstp || flag_zlp) {
        strmh->reassembling = 0;
    }

#if CONFIG_UVC_CHECK_HEADER_EOF
    if (header_info & (1 << 1)) {
        /* The EOF bit is set, so publish the complete frame */
        if (strmh->got_bytes != 0) {
            _uvc_swap_buffers(strmh);
        }
        strmh->reassembling = 0;
    }
#endif
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
    strmh = heap_caps_calloc(1, sizeof(_uvc_stream_handle_t), MALLOC_CAP_INTERNAL);

    if (!strmh) {
        ret = UVC_ERROR_NO_MEM;
        goto fail_;
    }

    strmh->devh = devh;
    strmh->frame.library_owns_data = 1;
    strmh->cur_ctrl = *ctrl;
    strmh->running = 0;
    strmh->outbuf = s_usb_dev.uvc_cfg.xfer_buffer_a;
    strmh->holdbuf = s_usb_dev.uvc_cfg.xfer_buffer_b;
    strmh->frame.data = s_usb_dev.uvc_cfg.frame_buffer;
    strmh->frame_format = s_usb_dev.uvc->frame_format;

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
static uvc_error_t uvc_stream_start(_uvc_stream_handle_t *strmh, uvc_frame_callback_t cb, void *user_ptr, uint8_t flags)
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
    strmh->user_cb = cb;
    strmh->user_ptr = user_ptr;

    if (cb && strmh->taskh == NULL) {
        xTaskCreatePinnedToCore(_sample_processing_task, SAMPLE_PROC_TASK_NAME, SAMPLE_PROC_TASK_STACK_SIZE, (void *)strmh,
                                SAMPLE_PROC_TASK_PRIORITY, &strmh->taskh, SAMPLE_PROC_TASK_CORE);
        UVC_CHECK(strmh->taskh != NULL, "sample task create failed", UVC_ERROR_OTHER);
        ESP_LOGD(TAG, "Sample processing task created");
    }

    return UVC_SUCCESS;
}

/** @brief Stop stream.
 * @ingroup streaming
 *
 * Stops stream, ends threads and cancels pollers
 *
 */
static uvc_error_t uvc_stream_stop(_uvc_stream_handle_t *strmh)
{
    if (strmh->running == 0) {
        ESP_LOGW(TAG, "line:%u UVC_ERROR_INVALID_PARAM", __LINE__);
        return UVC_ERROR_INVALID_PARAM;
    }
    strmh->running = 0;
    xTaskNotifyGive(strmh->taskh);
    xEventGroupWaitBits(s_usb_dev.event_group_hdl, UVC_SAMPLE_PROC_STOP_DONE, pdTRUE, pdFALSE, portMAX_DELAY);
    strmh->taskh = NULL;
    vTaskDelay(pdMS_TO_TICKS(WAITING_TASK_RESOURCE_RELEASE_MS));
    ESP_LOGD(TAG, "uvc_stream_stop done");
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
IRAM_ATTR static size_t _ring_buffer_get_len(RingbufHandle_t ringbuf_hdl)
{
    if (ringbuf_hdl == NULL) {
        return 0;
    }
    size_t size = 0;
    vRingbufferGetInfo(ringbuf_hdl, NULL, NULL, NULL, NULL, &size);
    return size;
}

IRAM_ATTR static void _ring_buffer_flush(RingbufHandle_t ringbuf_hdl)
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
    ESP_LOGD(TAG, "buffer %u, flush -%u", uxItemsWaiting, read_bytes);
}

IRAM_ATTR static esp_err_t _ring_buffer_push(RingbufHandle_t ringbuf_hdl, uint8_t *buf, size_t write_bytes, TickType_t xTicksToWait)
{
    if (ringbuf_hdl == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (buf == NULL) {
        ESP_LOGD(TAG, "can not push NULL buffer");
        return ESP_ERR_INVALID_ARG;
    }
    int res = xRingbufferSend(ringbuf_hdl, buf, write_bytes, xTicksToWait);

    if (res != pdTRUE) {
        ESP_LOGD(TAG, "buffer is too small, push failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

IRAM_ATTR static esp_err_t _ring_buffer_pop(RingbufHandle_t ringbuf_hdl, uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
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

IRAM_ATTR static void _processing_mic_pipe(hcd_pipe_handle_t pipe_hdl, mic_callback_t user_cb, void *user_ptr, bool if_enqueue)
{
    if (pipe_hdl == NULL) {
        return;
    }

    bool enqueue_flag = if_enqueue;
    urb_t *urb_done = hcd_urb_dequeue(pipe_hdl);
    if (urb_done == NULL) {
        ESP_LOGD(TAG, "mic urb dequeue error");
        return;
    }

    size_t xfered_size = 0;
    mic_frame_t mic_frame = {
        .bit_resolution = s_usb_dev.uac->bit_resolution[UAC_MIC],
        .samples_frequence = s_usb_dev.uac->samples_frequence[UAC_MIC],
        .data = s_usb_dev.uac->mic_frame_buf,
    };

    for (size_t i = 0; i < urb_done->transfer.num_isoc_packets; i++) {
        if (urb_done->transfer.isoc_packet_desc[i].status != USB_TRANSFER_STATUS_COMPLETED) {
            ESP_LOGW(TAG, "line:%u bad iso transit status %d", __LINE__, urb_done->transfer.isoc_packet_desc[i].status);
            continue;
        } else {
            int actual_num_bytes = urb_done->transfer.isoc_packet_desc[i].actual_num_bytes;
            int num_bytes = urb_done->transfer.isoc_packet_desc[i].num_bytes;
            uint8_t *packet_buffer = urb_done->transfer.data_buffer + (i * num_bytes);
#if UAC_MIC_PACKET_COMPENSATION
            int num_bytes_ms = s_usb_dev.uac->mic_ms_bytes;
            if (actual_num_bytes != num_bytes_ms) {
                ESP_LOGV(TAG, "Mic receive overflow %d, %d", actual_num_bytes, num_bytes_ms);
                if (i == 0) {
                    //if first packet loss (small probability), we just padding all 0
                    memset(packet_buffer, 0, num_bytes_ms);
                    ESP_LOGV(TAG, "MIC: padding 0");
                } else {
                    //if other packets loss or overflow, we just padding the last packet
                    uint8_t *packet_last_buffer = urb_done->transfer.data_buffer + (i * num_bytes - num_bytes);
                    memcpy(packet_buffer, packet_last_buffer, num_bytes_ms);
                    ESP_LOGV(TAG, "MIC: padding data");
                }
                actual_num_bytes = num_bytes_ms;
            }
#endif
            if (xfered_size + actual_num_bytes <= s_usb_dev.uac->mic_frame_buf_size) {
                memcpy(mic_frame.data + xfered_size, packet_buffer, actual_num_bytes);
                xfered_size += actual_num_bytes;
            }
        }
    }
    mic_frame.data_bytes = xfered_size;
    ESP_LOGV(TAG, "MIC RC %d", xfered_size);
    ESP_LOGV(TAG, "mic payload = %02x %02x...%02x %02x\n", urb_done->transfer.data_buffer[0], urb_done->transfer.data_buffer[1], urb_done->transfer.data_buffer[xfered_size - 2], urb_done->transfer.data_buffer[xfered_size - 1]);

    if (s_usb_dev.uac->ringbuf_hdl[UAC_MIC]) {
        esp_err_t ret = _ring_buffer_push(s_usb_dev.uac->ringbuf_hdl[UAC_MIC], mic_frame.data, mic_frame.data_bytes, 0);
        if (ret != ESP_OK) {
            ESP_LOGV(TAG, "mic ringbuf too small, please pop in time");
        }
    }

    if (xfered_size  > 0 && user_cb != NULL) {
        user_cb(&mic_frame, user_ptr);
    }

    if (enqueue_flag) {
        esp_err_t ret = hcd_urb_enqueue(pipe_hdl, urb_done);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "MIC urb enqueue failed %s", esp_err_to_name(ret));
        }
    }
}

IRAM_ATTR static void _processing_spk_pipe(hcd_pipe_handle_t pipe_hdl, bool if_dequeue, bool reset)
{
    static size_t pending_urb_num = 0;
    static size_t zero_counter = 0;
    static urb_t *pending_urb[NUM_ISOC_SPK_URBS] = {NULL};
    esp_err_t ret = ESP_FAIL;

    if (reset) {
        pending_urb_num = 0;
        zero_counter = 0;
        for (size_t j = 0; j < NUM_ISOC_SPK_URBS; j++) {
            pending_urb[j] = NULL;
        }
        ESP_LOGD(TAG, "spk processing reset");
        return;
    }

    if (pipe_hdl == NULL) {
        return;
    }

    if (!pending_urb_num && !if_dequeue) {
        return;
    }

    if (if_dequeue) {
        size_t xfered_size = 0;
        urb_t *urb_done = hcd_urb_dequeue(pipe_hdl);
        if (urb_done == NULL) {
            ESP_LOGD(TAG, "spk urb dequeue error");
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
                assert(pending_urb_num <= NUM_ISOC_SPK_URBS); //should never happened
                break;
            }
        }
    }
    /* check if we have buffered data need to send */
    if (_ring_buffer_get_len(s_usb_dev.uac->ringbuf_hdl[UAC_SPK]) < s_usb_dev.uac->as_ifc[UAC_SPK]->bytes_per_packet) {
#if (!UAC_SPK_PACKET_COMPENSATION)
        /* if speaker packet compensation not enable, just return here */
        return;
#endif
    } else {
        zero_counter = 0;
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
    UVC_CHECK_RETURN_VOID(next_urb != NULL, "free urb not found");

    size_t num_bytes_to_send = 0;
    size_t buffer_size = s_usb_dev.uac->spk_max_xfer_size;
    uint8_t *buffer = next_urb->transfer.data_buffer;
    ret = _ring_buffer_pop(s_usb_dev.uac->ringbuf_hdl[UAC_SPK], buffer, buffer_size, &num_bytes_to_send, 0);
    if (ret != ESP_OK || num_bytes_to_send == 0) {
#if (!UAC_SPK_PACKET_COMPENSATION)
        //should never happened
        return;
#else
        if (pending_urb_num == NUM_ISOC_SPK_URBS) {
            if (++zero_counter == (UAC_SPK_PACKET_COMPENSATION_TIMEOUT_MS / portTICK_PERIOD_MS)) {
                /* if speaker packets compensation enable, we padding 0 to speaker */
                num_bytes_to_send = s_usb_dev.uac->as_ifc[UAC_SPK]->bytes_per_packet * (UAC_SPK_PACKET_COMPENSATION_SIZE_MS > UAC_SPK_ST_MAX_MS_DEFAULT ? UAC_SPK_ST_MAX_MS_DEFAULT : UAC_SPK_PACKET_COMPENSATION_SIZE_MS);
                memset(buffer, 0, num_bytes_to_send);
#if UAC_SPK_PACKET_COMPENSATION_CONTINUOUS
                zero_counter = 0;
#endif
                ESP_LOGV(TAG, "SPK: padding 0 length = %d", num_bytes_to_send);
            }
        }
#endif
    }

    if (num_bytes_to_send == 0) {
        return;
    }

    // may drop some data here?
    //size_t num_bytes_send = num_bytes_to_send - num_bytes_to_send % s_usb_dev.uac->as_ifc[UAC_SPK]->bytes_per_packet;
    size_t num_bytes_send = num_bytes_to_send;
    size_t last_packet_bytes = num_bytes_send % s_usb_dev.uac->as_ifc[UAC_SPK]->bytes_per_packet;
    next_urb->transfer.num_bytes = num_bytes_send;
    usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)&next_urb->transfer;
    transfer_dummy->num_isoc_packets = num_bytes_send / s_usb_dev.uac->as_ifc[UAC_SPK]->bytes_per_packet + (last_packet_bytes ? 1 : 0);
    for (size_t j = 0; j < transfer_dummy->num_isoc_packets; j++) {
        //We need to initialize each individual isoc packet descriptor of the URB
        next_urb->transfer.isoc_packet_desc[j].num_bytes = s_usb_dev.uac->as_ifc[UAC_SPK]->bytes_per_packet;
    }
    if (last_packet_bytes) {
        next_urb->transfer.isoc_packet_desc[transfer_dummy->num_isoc_packets - 1].num_bytes = last_packet_bytes;
    }

    ret = hcd_urb_enqueue(pipe_hdl, next_urb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPK urb enqueue failed %s", esp_err_to_name(ret));
        return;
    }

    pending_urb[next_index] = NULL;
    --pending_urb_num;
    assert(pending_urb_num <= NUM_ISOC_SPK_URBS); //should never happened
    ESP_LOGV(TAG, "SPK ST %d/%d", num_bytes_send, num_bytes_to_send);
    ESP_LOGV(TAG, "spk payload = %02x %02x...%02x %02x\n", buffer[0], buffer[1], buffer[num_bytes_send - 2], buffer[num_bytes_send - 1]);
}

static esp_err_t _event_set_bits_wait_cleared(EventGroupHandle_t evt_group, EventBits_t bits, TickType_t timeout)
{
    xEventGroupSetBits(evt_group, bits);
    TickType_t counter = timeout;
    while (xEventGroupGetBits(evt_group) & bits) {
        vTaskDelay(1);
        counter -= 1;
        if (counter <= 0) {
            ESP_LOGE(TAG, "bits(0x%04X) clear timeout %"PRIu32, (unsigned int)bits, timeout);
            return ESP_ERR_TIMEOUT;
        }
    }
    xEventGroupClearBits(evt_group, bits);
    return ESP_OK;
}

static void _usb_stream_kill_cb(int time_out_ms)
{
    ESP_LOGD(TAG, "usb stream kill");
    _event_set_bits_wait_cleared(s_usb_dev.event_group_hdl, USB_STREAM_TASK_KILL_BIT, pdMS_TO_TICKS(time_out_ms));
    //wait for task resource release
    vTaskDelay(pdMS_TO_TICKS(WAITING_TASK_RESOURCE_RELEASE_MS));
    ESP_LOGD(TAG, "usb stream kill succeed");
}

static void _usb_stream_connect_cb(void)
{
    if ((s_usb_dev.enabled[STREAM_UAC_MIC] && !s_usb_dev.uac->as_ifc[UAC_MIC]->not_found)
            || (s_usb_dev.enabled[STREAM_UAC_SPK] && !s_usb_dev.uac->as_ifc[UAC_SPK]->not_found)
            || (s_usb_dev.enabled[STREAM_UVC] && !s_usb_dev.uvc->vs_ifc->not_found)) {
        if (s_usb_dev.stream_task_hdl != NULL) {
            xTaskNotifyGive(s_usb_dev.stream_task_hdl);
        }
    }
}

static void _usb_stream_recover_cb(int time_out_ms)
{
    _event_set_bits_wait_cleared(s_usb_dev.event_group_hdl, USB_STREAM_TASK_RECOVER_BIT, pdMS_TO_TICKS(time_out_ms));
}

static esp_err_t _usb_streaming_suspend(usb_stream_t stream)
{
    UVC_CHECK(stream < STREAM_MAX, "invalid stream type", ESP_ERR_INVALID_ARG);
    UVC_CHECK(_usb_device_get_state() > STATE_DEVICE_INSTALLED, "USB Device not active", ESP_ERR_INVALID_STATE);
    _stream_ifc_t *p_itf = s_usb_dev.ifc[stream];
    esp_err_t ret = ESP_OK;
    UVC_ENTER_CRITICAL();
    uint16_t interface_idx = p_itf->interface;
    UVC_EXIT_CRITICAL();
    if (_usb_device_get_state() == STATE_DEVICE_ACTIVE) {
        ret = _usb_set_device_interface(interface_idx, 0);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "Set interface %u-%u failed", interface_idx, 0);
        }
    }
    if (stream == STREAM_UVC && s_usb_dev.uvc->uvc_stream_hdl) {
        uvc_stream_stop(s_usb_dev.uvc->uvc_stream_hdl);
        uvc_stream_close(s_usb_dev.uvc->uvc_stream_hdl);
        s_usb_dev.uvc->uvc_stream_hdl = NULL;
    }
    return ret;
}

static esp_err_t _uvc_streaming_resume(void)
{
    _uvc_device_t *uvc_dev = s_usb_dev.uvc;
    uvc_stream_ctrl_t ctrl_set = (uvc_stream_ctrl_t)DEFAULT_UVC_STREAM_CTRL();
    uvc_stream_ctrl_t ctrl_probed = {0};
    uvc_frame_size_t frame_size = {0};
    /* UVC negotiation process */
    UVC_ENTER_CRITICAL();
    ctrl_set.bFormatIndex = uvc_dev->format_index;
    ctrl_set.bFrameIndex = uvc_dev->frame_index;
    ctrl_set.dwFrameInterval = uvc_dev->frame_interval;
    ctrl_set.dwMaxVideoFrameSize = s_usb_dev.uvc_cfg.frame_buffer_size;
    /* For bulk transfer, payload size config by NUM_BULK_BYTES_PER_URB for better performance */
    ctrl_set.dwMaxPayloadTransferSize = (uvc_dev->vs_ifc->xfer_type == UVC_XFER_BULK) ? NUM_BULK_BYTES_PER_URB : (uvc_dev->vs_ifc->ep_mps);
    frame_size.width = uvc_dev->frame_width;
    frame_size.height = uvc_dev->frame_height;
    UVC_EXIT_CRITICAL();
    ESP_LOGI(TAG, "Probe Format(%u), Frame(%u) %u*%u, interval(%"PRIu32")", ctrl_set.bFormatIndex,
             ctrl_set.bFrameIndex, frame_size.width, frame_size.height, ctrl_set.dwFrameInterval);
    ESP_LOGI(TAG, "Probe payload size = %"PRIu32, ctrl_set.dwMaxPayloadTransferSize);
    esp_err_t ret = _uvc_vs_commit_control(&ctrl_set, &ctrl_probed);
    UVC_CHECK(ESP_OK == ret, "UVC negotiate failed", ESP_FAIL);
    if (ctrl_set.dwMaxPayloadTransferSize != ctrl_probed.dwMaxPayloadTransferSize) {
        ESP_LOGI(TAG, "dwMaxPayloadTransferSize set = %" PRIu32 ", probed = %" PRIu32, ctrl_set.dwMaxPayloadTransferSize, ctrl_probed.dwMaxPayloadTransferSize);
    }
    /* start uvc streaming */
    uvc_error_t uvc_ret = UVC_SUCCESS;
    uvc_ret = uvc_stream_open_ctrl(NULL, &uvc_dev->uvc_stream_hdl, &ctrl_probed);
    UVC_CHECK(uvc_ret == UVC_SUCCESS, "open uvc stream failed", ESP_FAIL);

    uvc_ret = uvc_stream_start(uvc_dev->uvc_stream_hdl, s_usb_dev.uvc_cfg.frame_cb, s_usb_dev.uvc_cfg.frame_cb_arg, 0);
    UVC_CHECK_GOTO(uvc_ret == UVC_SUCCESS, "start uvc stream failed", free_stream_);
    if (uvc_dev->vs_ifc->xfer_type == UVC_XFER_ISOC) {
        UVC_ENTER_CRITICAL();
        uint16_t interface = uvc_dev->vs_ifc->interface;
        uint16_t alt_interface = uvc_dev->vs_ifc->interface_alt;
        UVC_EXIT_CRITICAL();
        ret = _usb_set_device_interface(interface, alt_interface);
        UVC_CHECK_GOTO(ESP_OK == ret, "Resume uvc interface failed", free_stream_);
    }
    ESP_LOGD(TAG, "UVC Streaming...");
    return ESP_OK;

free_stream_:
    if (uvc_dev->uvc_stream_hdl) {
        uvc_stream_stop(uvc_dev->uvc_stream_hdl);
        uvc_stream_close(uvc_dev->uvc_stream_hdl);
    }
    uvc_dev->uvc_stream_hdl = NULL;
    return ESP_FAIL;
}

static esp_err_t _uac_streaming_resume(usb_stream_t stream)
{
    UVC_CHECK(stream == STREAM_UAC_MIC || stream == STREAM_UAC_SPK, "invalid stream type", ESP_ERR_INVALID_ARG);
    _uac_device_t *uac_dev = s_usb_dev.uac;
    esp_err_t ret = ESP_OK;
    _uac_internal_stream_t uac_stream = (stream == STREAM_UAC_MIC) ? UAC_MIC : UAC_SPK;
    UVC_ENTER_CRITICAL();
    uint16_t interface = uac_dev->as_ifc[uac_stream]->interface;
    uint16_t interface_alt = uac_dev->as_ifc[uac_stream]->interface_alt;
    UVC_EXIT_CRITICAL();
    ret = _usb_set_device_interface(interface, interface_alt);
    UVC_CHECK(ESP_OK == ret, "Set device interface failed", ESP_FAIL);

    UVC_ENTER_CRITICAL();
    bool freq_ctrl_support = uac_dev->freq_ctrl_support[uac_stream];
    uint8_t ep_addr = uac_dev->as_ifc[uac_stream]->ep_addr;
    uint32_t samples_frequence = uac_dev->samples_frequence[uac_stream];
    UVC_EXIT_CRITICAL();
    if (freq_ctrl_support) {
        ret = _uac_as_control_set_freq(ep_addr, samples_frequence);
        UVC_CHECK_CONTINUE(ESP_OK == ret, "frequency set failed");
    }
    ESP_LOGD(TAG, "%s Streaming...", (stream == STREAM_UAC_MIC) ? "MIC" : "SPK");
    return ESP_OK;
}

static esp_err_t _usb_streaming_resume(usb_stream_t stream)
{
    UVC_CHECK(stream < STREAM_MAX, "invalid stream type", ESP_ERR_INVALID_ARG);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    esp_err_t ret = ESP_OK;

    switch (stream) {
    case STREAM_UVC:
        ret = _uvc_streaming_resume();
        break;
    case STREAM_UAC_SPK:
    case STREAM_UAC_MIC:
        ret = _uac_streaming_resume(stream);
        break;
    default:
        break;
    }

    return ret;
}

static void _usb_stream_handle_task(void *arg)
{
    _uac_device_t *uac_dev = s_usb_dev.uac;
    _uvc_device_t *uvc_dev = s_usb_dev.uvc;
    _usb_device_t *usb_dev = &s_usb_dev;
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "USB stream task start");
    // Waiting for usbh ready
    while (!(xEventGroupGetBits(usb_dev->event_group_hdl) & (USB_STREAM_TASK_KILL_BIT))) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == 0 || (_usb_device_get_state() != STATE_DEVICE_ACTIVE)) {
            //waiting for usb ready, we are in recover state
            xEventGroupClearBits(usb_dev->event_group_hdl, USB_STREAM_TASK_RECOVER_BIT);
            continue;
        }
        // Create pipe when device connect
        xQueueReset(usb_dev->stream_queue_hdl);
        // Check user flags to set initial stream state
        for (size_t i = 0; i < STREAM_MAX; i++) {
            if (usb_dev->enabled[i]) {
                uint32_t suspend_flag = i == STREAM_UVC ? FLAG_UVC_SUSPEND_AFTER_START
                                        : (i == STREAM_UAC_MIC ? FLAG_UAC_MIC_SUSPEND_AFTER_START
                                           : FLAG_UAC_SPK_SUSPEND_AFTER_START);
                if (usb_dev->flags & suspend_flag) {
                    usb_dev->ifc[i]->suspended = true;
                } else {
                    usb_dev->ifc[i]->suspended = false;
                }
                ESP_LOGD(TAG, "initial %s stream state %s", usb_dev->ifc[i]->name, usb_dev->ifc[i]->suspended ? "suspend" : "resume");
            }
        }

        for (size_t i = 0; i < STREAM_MAX; i++) {
            if (usb_dev->enabled[i] && !usb_dev->ifc[i]->not_found) {
                _apply_pipe_config(i);
                ret = _apply_stream_config(i);
                /* the not found means, not found suitable configs from device descriptor */
                UVC_CHECK_GOTO((ret == ESP_OK || ret == ESP_ERR_NOT_FOUND), "apply streaming config failed", _apply_config_failed);
            }
        }

        // We detach the user callback, users may reset the suspended flag in callback
        if (usb_dev->state_cb) {
            usb_dev->state_cb(STREAM_CONNECTED, usb_dev->state_cb_arg);
        }
        //prepare urb and data buffer for stream pipe
        for (size_t i = 0; i < STREAM_MAX; i++) {
            if (usb_dev->enabled[i] && !usb_dev->ifc[i]->not_found) {
                usb_ep_desc_t stream_ep_desc = (usb_ep_desc_t) {
                    .bLength = USB_EP_DESC_SIZE,
                    .bDescriptorType = USB_B_DESCRIPTOR_TYPE_ENDPOINT,
                    .bEndpointAddress = usb_dev->ifc[i]->ep_addr,
                    .bmAttributes = USB_BM_ATTRIBUTES_XFER_ISOC,
                    .wMaxPacketSize = usb_dev->ifc[i]->ep_mps,
                    .bInterval = 1,
                };

                if (usb_dev->ifc[i]->xfer_type == UVC_XFER_BULK) {
                    stream_ep_desc.bmAttributes = USB_BM_ATTRIBUTES_XFER_BULK;
                    stream_ep_desc.bInterval = 0;
                }
                ESP_LOGD(TAG, "Creating %s pipe: ifc=%d-%d, ep=0x%02X, mps=%"PRIu32, usb_dev->ifc[i]->name, usb_dev->ifc[i]->interface, usb_dev->ifc[i]->interface_alt,
                         usb_dev->ifc[i]->ep_addr, usb_dev->ifc[i]->ep_mps);
                usb_dev->ifc[i]->pipe_handle = _usb_pipe_init(usb_dev->port_hdl, &stream_ep_desc, usb_dev->dev_addr, usb_dev->dev_speed,
                                                              (void *)usb_dev->ifc[i]->type, &_usb_pipe_callback, (void *)usb_dev->stream_queue_hdl);
                UVC_CHECK_GOTO(usb_dev->ifc[i]->pipe_handle != NULL, "pipe init failed", _usb_stream_recover);
                /* If resume the interface, depend on whether the user flags suspend the stream
                * Please Note that, when disconnect and reconnect the device, the stream state will be reset
                */
                if (usb_dev->ifc[i]->suspended) {
                    ESP_LOGD(TAG, "Suspend %s stream. Reason: user suspend", usb_dev->ifc[i]->name);
                    continue;
                } else if (ret == ESP_ERR_NOT_FOUND) {
                    ESP_LOGW(TAG, "Suspend %s stream. Reason: user's expected not found", usb_dev->ifc[i]->name);
                    continue;
                }

                ret = _usb_streaming_resume(i);
                UVC_CHECK_GOTO(ret == ESP_OK, "streaming resume failed", _usb_stream_recover);
                _event_msg_t evt_msg = {
                    ._type = USER_EVENT,
                    ._event.user_cmd = STREAM_RESUME,
                    ._event_data = (void *)i,
                    ._handle.user_hdl = (void *)usb_dev->stream_task_hdl, //used to identify where the event come from
                };
                xQueueSend(usb_dev->stream_queue_hdl, &evt_msg, 0);
            }
        }
        xEventGroupSetBits(usb_dev->event_group_hdl, USB_STREAM_DEVICE_READY_BIT);
        while (!(xEventGroupGetBits(usb_dev->event_group_hdl) & (USB_STREAM_TASK_KILL_BIT | USB_STREAM_TASK_RECOVER_BIT))) {
            _event_msg_t evt_msg = {};
            if (xQueueReceive(usb_dev->stream_queue_hdl, &evt_msg, 1) != pdTRUE) {
                // check if ringbuffer has data and we have free urb, send out here
                if (usb_dev->enabled[STREAM_UAC_SPK] && (xEventGroupGetBits(usb_dev->event_group_hdl) & UAC_SPK_STREAM_RUNNING)) {
                    _processing_spk_pipe(uac_dev->as_ifc[UAC_SPK]->pipe_handle, false, false);
                }
                continue;
            }
            switch (evt_msg._type) {
            case USER_EVENT: {
                EventBits_t ack_bits = USB_STREAM_TASK_PROC_FAILED;
                usb_stream_t stream = (usb_stream_t)evt_msg._event_data;
                _stream_ifc_t *p_itf = usb_dev->ifc[stream];
                if (evt_msg._event.user_cmd == STREAM_SUSPEND) {
                    UVC_CHECK_GOTO((xEventGroupGetBits(usb_dev->event_group_hdl) & p_itf->evt_bit), "stream suspend: already suspend", _feedback_result);
                    ESP_LOGD(TAG, "%s stream suspend: flush transfer", p_itf->name);
                    ret = _usb_pipe_flush(p_itf->pipe_handle, p_itf->urb_num);
                    UVC_CHECK_GOTO(ret == ESP_OK, "stream suspend: flush transfer, failed", _feedback_result);
                    if (p_itf->urb_list) {
                        _usb_urb_list_free(p_itf->urb_list, p_itf->urb_num);
                        p_itf->urb_list = NULL;
                        ESP_LOGD(TAG, "%s stream suspend: free urb list succeed", p_itf->name);
                    }
                    if (stream == STREAM_UAC_SPK || stream == STREAM_UAC_MIC) {
                        _ring_buffer_flush(uac_dev->ringbuf_hdl[stream == STREAM_UAC_SPK ? UAC_SPK : UAC_MIC]);
                    }
                    ack_bits = USB_STREAM_TASK_PROC_SUCCEED;
                    xEventGroupClearBits(usb_dev->event_group_hdl, p_itf->evt_bit);
                    ESP_LOGD(TAG, "%s stream suspend, flush transfer succeed", p_itf->name);
                } else if (evt_msg._event.user_cmd == STREAM_RESUME) {
                    UVC_CHECK_GOTO(!(xEventGroupGetBits(usb_dev->event_group_hdl) & p_itf->evt_bit), "stream resume: already resume", _feedback_result);
                    ESP_LOGD(TAG, "%s stream resume: enqueue transfer", p_itf->name);
                    if (evt_msg._handle.user_hdl != usb_dev->stream_task_hdl) {
                        ret = _apply_stream_config(stream);
                        UVC_CHECK_GOTO(ret == ESP_OK, "stream resume: apply config", _feedback_result);
                    }
                    if (p_itf->type == STREAM_UVC && p_itf->xfer_type == UVC_XFER_BULK) {
                        uvc_dev->uvc_stream_hdl->reassemble_flag = 0;
                        if (uvc_dev->uvc_stream_hdl->cur_ctrl.dwMaxPayloadTransferSize < p_itf->bytes_per_packet) {
                            p_itf->bytes_per_packet = uvc_dev->uvc_stream_hdl->cur_ctrl.dwMaxPayloadTransferSize;
                        } else if (uvc_dev->uvc_stream_hdl->cur_ctrl.dwMaxPayloadTransferSize > p_itf->bytes_per_packet) {
                            // in most case, the payload size is very large in bulk transfer (one sample or part of sample),
                            // to save memory, we transfer with smaller size, and reassemble payload.
                            uvc_dev->uvc_stream_hdl->reassemble_flag = 1;
                            ESP_LOGD(TAG, "UVC Bulk Packet Reassemble Enable");
                        }
                    }
                    if (p_itf->urb_list == NULL && p_itf->xfer_type == UVC_XFER_BULK) {
                        p_itf->urb_list = _usb_urb_list_alloc(p_itf->urb_num, 0, p_itf->bytes_per_packet);
                        UVC_CHECK_ABORT(p_itf->urb_list != NULL, "p_urb alloc failed");
                        ESP_LOGD(TAG, "%s stream resume: alloc urb list succeed", p_itf->name);
                    } else if (p_itf->urb_list == NULL) {
                        p_itf->urb_list = _usb_urb_list_alloc(p_itf->urb_num, p_itf->packets_per_urb, p_itf->bytes_per_packet);
                        UVC_CHECK_ABORT(p_itf->urb_list != NULL, "p_urb alloc failed");
                        ESP_LOGD(TAG, "%s stream resume: alloc urb list succeed", p_itf->name);
                    }
                    if (p_itf->type == STREAM_UAC_SPK) {
                        // For out stream, we set a minimum start zero packet
                        for (int j = 0; j < p_itf->urb_num; j++) {
                            usb_transfer_dummy_t *transfer_dummy = (usb_transfer_dummy_t *)(&(p_itf->urb_list[j]->transfer));
                            transfer_dummy->num_isoc_packets = 1;
                            transfer_dummy->num_bytes = p_itf->bytes_per_packet;
                            transfer_dummy->isoc_packet_desc[0].num_bytes = p_itf->bytes_per_packet;
                        }
                        _processing_spk_pipe(NULL, false, true);
                    }
                    ret = _usb_pipe_clear(p_itf->pipe_handle, p_itf->urb_num);
                    UVC_CHECK_GOTO(ret == ESP_OK, "stream resume: clear pipe, failed", _feedback_result);
                    ret = _usb_urb_list_enqueue(p_itf->pipe_handle, p_itf->urb_list, p_itf->urb_num);
                    UVC_CHECK_GOTO(ret == ESP_OK, "stream resume: enqueue transfer, failed", _feedback_result);
                    ack_bits = USB_STREAM_TASK_PROC_SUCCEED;
                    xEventGroupSetBits(usb_dev->event_group_hdl, p_itf->evt_bit);
                    ESP_LOGD(TAG, "%s stream resume: enqueue transfer succeed", p_itf->name);
                }
_feedback_result:
                if (evt_msg._handle.user_hdl != usb_dev->stream_task_hdl) {
                    xEventGroupSetBits(usb_dev->event_group_hdl, ack_bits);
                }
                break;
            }
            case PIPE_EVENT: {
                usb_stream_t stream = (usb_stream_t)hcd_pipe_get_context(evt_msg._handle.pipe_handle);
                _pipe_event_dflt_process(evt_msg._handle.pipe_handle, usb_dev->ifc[stream]->name, evt_msg._event.pipe_event);
                switch (evt_msg._event.pipe_event) {
                case HCD_PIPE_EVENT_URB_DONE:
                    if (stream == STREAM_UAC_MIC) {
                        SYSVIEW_UAC_MIC_PIPE_HANDLE_START();
                        _processing_mic_pipe(uac_dev->as_ifc[UAC_MIC]->pipe_handle, usb_dev->uac_cfg.mic_cb, usb_dev->uac_cfg.mic_cb_arg, true);
                        SYSVIEW_UAC_MIC_PIPE_HANDLE_STOP();
                    } else if (stream == STREAM_UAC_SPK) {
                        SYSVIEW_UAC_SPK_PIPE_HANDLE_START();
                        _processing_spk_pipe(uac_dev->as_ifc[UAC_SPK]->pipe_handle, true, false);
                        SYSVIEW_UAC_SPK_PIPE_HANDLE_STOP();
                    } else if (stream == STREAM_UVC) {
                        SYSVIEW_UVC_PIPE_HANDLE_START();
                        _processing_uvc_pipe(uvc_dev->uvc_stream_hdl, uvc_dev->vs_ifc->pipe_handle, true);
                        SYSVIEW_UVC_PIPE_HANDLE_STOP();
                    }
                    break;
                case HCD_PIPE_EVENT_ERROR_OVERFLOW:
                case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
                case HCD_PIPE_EVENT_ERROR_XFER:
                    goto _usb_stream_recover;
                    break;
                case HCD_PIPE_EVENT_ERROR_STALL: {
                    _event_msg_t evt_msg = {
                        ._type = USER_EVENT,
                        ._event.user_cmd = STREAM_SUSPEND,
                        ._event_data = (void *)stream,
                    };
                    //if stall, we just suspend then resume the pipe
                    xQueueSend(usb_dev->stream_queue_hdl, &evt_msg, 0);
                    evt_msg._event.user_cmd = STREAM_RESUME;
                    xQueueSend(usb_dev->stream_queue_hdl, &evt_msg, 0);
                    break;
                }
                default:
                    break;
                }
                break;
            }
            default:
                break;
            }
        }
_usb_stream_recover:
        xEventGroupClearBits(usb_dev->event_group_hdl, USB_STREAM_DEVICE_READY_BIT);
        /* check if reset trigger by disconnect */
        ESP_LOGI(TAG, "usb stream task wait reset");
        EventBits_t uxBits = xEventGroupWaitBits(usb_dev->event_group_hdl, USB_STREAM_TASK_KILL_BIT |
                                                 USB_STREAM_TASK_RECOVER_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
        if (uxBits & (USB_STREAM_TASK_KILL_BIT | USB_STREAM_TASK_RECOVER_BIT)) {
            // if reset trigger by disconnect, we just reset to default state
            ESP_LOGI(TAG, "usb stream task reset, reason: device %s", (uxBits & USB_STREAM_TASK_KILL_BIT) ? "disconnect" : "recover");
        } else if (_usb_device_get_state() == STATE_DEVICE_ACTIVE) {
            // if reset trigger by other reason and device is active, we just recover the stream
            xTaskNotifyGive(usb_dev->stream_task_hdl);
            ESP_LOGW(TAG, "usb stream task recover, reason: stream error");
        } else {
            // if reset trigger by other reason and device is not active, we just reset to default state
            ESP_LOGW(TAG, "usb stream task reset, reason: device not active");
        }

        for (size_t i = 0; i < STREAM_MAX; i++) {
            if (usb_dev->enabled[i] && !usb_dev->ifc[i]->not_found) {
                ESP_LOGI(TAG, "Resetting %s pipe", usb_dev->ifc[i]->name);
                _usb_streaming_suspend(i);
                _usb_pipe_deinit(usb_dev->ifc[i]->pipe_handle, usb_dev->ifc[i]->urb_num);
                usb_dev->ifc[i]->pipe_handle = NULL;
                _usb_urb_list_free(usb_dev->ifc[i]->urb_list, usb_dev->ifc[i]->urb_num);
                usb_dev->ifc[i]->urb_list = NULL;
            }
        }
        if (uac_dev != NULL) {
            _ring_buffer_flush(uac_dev->ringbuf_hdl[UAC_SPK]);
            _ring_buffer_flush(uac_dev->ringbuf_hdl[UAC_MIC]);
        }
        xEventGroupClearBits(usb_dev->event_group_hdl, USB_UVC_STREAM_RUNNING | UAC_SPK_STREAM_RUNNING | UAC_MIC_STREAM_RUNNING);
        if (usb_dev->state_cb) {
            usb_dev->state_cb(STREAM_DISCONNECTED, usb_dev->state_cb_arg);
        }
        xEventGroupClearBits(usb_dev->event_group_hdl, USB_STREAM_TASK_RECOVER_BIT);
_apply_config_failed:
        continue;
    } //handle hotplug
    ESP_LOGI(TAG, "USB stream task deleted");
    ESP_LOGD(TAG, "USB stream task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    xEventGroupClearBits(usb_dev->event_group_hdl, USB_STREAM_TASK_KILL_BIT);
    vTaskDelete(NULL);
}
static uint32_t _usb_port_actions_update(hcd_port_event_t port_evt, uint32_t action_bits)
{
    switch (port_evt) {
    case HCD_PORT_EVENT_CONNECTION:
        action_bits &= ~ACTION_DEVICE_DISCONNECT;
        action_bits = ACTION_DEVICE_CONNECT;
        break;
    case HCD_PORT_EVENT_DISCONNECTION:
        action_bits &= ~ACTION_DEVICE_CONNECT;
        action_bits = ACTION_DEVICE_DISCONNECT;
        action_bits |= ACTION_PORT_RECOVER;
        action_bits |= ACTION_PIPE_DFLT_DISABLE;
        break;
    case HCD_PORT_EVENT_ERROR:
    case HCD_PORT_EVENT_OVERCURRENT:
        action_bits = ACTION_PORT_RECOVER;
        action_bits |= ACTION_PIPE_DFLT_DISABLE;
        break;
    default:
        break;
    }
    ESP_LOGD(TAG, "port update: action_bits = 0x%04X", (unsigned int)action_bits);
    return action_bits;
}

static uint32_t _usb_pipe_actions_update(hcd_pipe_event_t pipe_evt, uint32_t action_bits)
{
    switch (pipe_evt) {
    case HCD_PIPE_EVENT_URB_DONE:
        action_bits |= ACTION_PIPE_XFER_DONE;
        break;
    case HCD_PIPE_EVENT_ERROR_XFER:
    case HCD_PIPE_EVENT_ERROR_URB_NOT_AVAIL:
    case HCD_PIPE_EVENT_ERROR_OVERFLOW:
        action_bits |= ACTION_PIPE_DFLT_RECOVER;
        action_bits |= ACTION_PIPE_DFLT_CLEAR;
        action_bits |= ACTION_PIPE_XFER_FAIL;
        break;
    case HCD_PIPE_EVENT_ERROR_STALL:
        action_bits |= ACTION_PIPE_DFLT_CLEAR;
        action_bits |= ACTION_PIPE_XFER_FAIL;
        break;
    default:
        break;
    }
    ESP_LOGD(TAG, "pipe update: action_bits = 0x%04X", (unsigned int)action_bits);
    return action_bits;
}

static uint32_t _user_actions_update(_user_cmd_t usr_cmd, uint32_t action_bits)
{
    switch (usr_cmd) {
    case USB_RECOVER:
        action_bits = ACTION_PORT_RECOVER;
        action_bits |= ACTION_PIPE_DFLT_DISABLE;
        break;
    case PIPE_RECOVER:
        action_bits |= ACTION_PIPE_DFLT_RECOVER;
        action_bits |= ACTION_PIPE_DFLT_CLEAR;
        break;
    default:
        break;
    }
    ESP_LOGD(TAG, "user update: action_bits = 0x%04X", (unsigned int)action_bits);
    return action_bits;
}

static esp_err_t _uvc_uac_device_enum(bool abort_process, bool *waiting_urb_done)
{
    UVC_CHECK_GOTO(!abort_process, "Enum abort", stage_failed_);
    _usb_device_t *usb_dev = &s_usb_dev;
    urb_t *enum_done = NULL;
    static urb_t *ctrl_urb = NULL;
    static bool urb_need_free = false;
    static uint16_t full_config_length = 0;
    bool urb_need_enqueue = false;
    esp_err_t ret = ESP_FAIL;
    usb_transfer_t *enum_transfer = NULL;
    if (waiting_urb_done) {
        *waiting_urb_done = false;
    }
    if (ctrl_urb != NULL) {
        enum_transfer = &ctrl_urb->transfer;
    }

    if (usb_dev->enum_stage == ENUM_STAGE_NONE) {
        usb_dev->enum_stage = ENUM_STAGE_START;
    } else if (usb_dev->enum_stage % 2) { // Check if we are in check stage, which is always odd number
        assert(usb_dev->dflt_pipe_hdl); //should not be NULL
        enum_done = hcd_urb_dequeue(usb_dev->dflt_pipe_hdl);
        UVC_CHECK_GOTO(enum_done == ctrl_urb, "urb cleared: enum abort", stage_failed_);

        if (enum_transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
            ESP_LOGE(TAG, "Bad transfer status %d", enum_transfer->status);
            if (enum_transfer->status == USB_TRANSFER_STATUS_STALL) {
                hcd_pipe_command(usb_dev->dflt_pipe_hdl, HCD_PIPE_CMD_CLEAR);
            }
            goto stage_failed_;
        }
        UVC_CHECK_GOTO(enum_transfer->actual_num_bytes <= enum_transfer->num_bytes, "urb status: data overflow", stage_failed_);
    }
    switch (usb_dev->enum_stage) {
    case ENUM_STAGE_START: {
        if (!usb_dev->dflt_pipe_hdl) {
            ESP_ERROR_CHECK(_usb_port_get_speed(usb_dev->port_hdl, &(usb_dev->dev_speed)));
            ESP_LOGI(TAG, "USB Speed: %s-speed", usb_dev->dev_speed == USB_SPEED_FULL ? "full" : "low");
            xSemaphoreTake(usb_dev->xfer_mutex_hdl, portMAX_DELAY);
            usb_dev->dflt_pipe_hdl = _usb_pipe_init(usb_dev->port_hdl, NULL, 0, usb_dev->dev_speed,
                                                    (void *)usb_dev->queue_hdl, &_usb_pipe_callback, (void *)usb_dev->queue_hdl);
            xSemaphoreGive(usb_dev->xfer_mutex_hdl);
            UVC_CHECK_GOTO(usb_dev->dflt_pipe_hdl != NULL, "default pipe create failed", stage_failed_);
            usb_dev->ep_mps = usb_dev->dev_speed == USB_SPEED_FULL ? USB_EP0_FS_DEFAULT_MPS : USB_EP0_LS_DEFAULT_MPS;
        }
        //else malloc a new one for enum stage
        if (enum_transfer == NULL) {
            //if we already have a ctrl urb, just reuse it. else malloc a new one
            if (usb_dev->ctrl_urb != NULL) {
                ctrl_urb = usb_dev->ctrl_urb;
            } else {
                ctrl_urb = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
                UVC_CHECK_GOTO(ctrl_urb != NULL, "default pipe create failed", stage_failed_);
                urb_need_free = true;
            }
        }
#ifndef CONFIG_UVC_GET_DEVICE_DESC
        usb_dev->enum_stage = ENUM_STAGE_CHECK_SHORT_DEV_DESC;
        ret = hcd_pipe_update_mps(usb_dev->dflt_pipe_hdl, usb_dev->ep_mps);
        ESP_LOGI(TAG, "Default pipe endpoint MPS update to %d", usb_dev->ep_mps);
        UVC_CHECK_GOTO(ESP_OK == ret, "default pipe update MPS failed", stage_failed_);
#endif
        break;
    }
    case ENUM_STAGE_GET_SHORT_DEV_DESC: {
        USB_SETUP_PACKET_INIT_GET_DEVICE_DESC((usb_setup_packet_t *)enum_transfer->data_buffer);
        ((usb_setup_packet_t *)enum_transfer->data_buffer)->wLength = USB_SHORT_DESC_REQ_LEN;
        enum_transfer->num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(USB_SHORT_DESC_REQ_LEN, usb_dev->ep_mps);
        urb_need_enqueue = true;
        break;
    }
    case ENUM_STAGE_CHECK_SHORT_DEV_DESC: {
        usb_device_desc_t *dev_desc = (usb_device_desc_t *)(enum_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
        ret = hcd_pipe_update_mps(usb_dev->dflt_pipe_hdl, dev_desc->bMaxPacketSize0);
        UVC_CHECK_GOTO(ESP_OK == ret, "default pipe update MPS failed", stage_failed_);
        ESP_LOGI(TAG, "Default pipe endpoint MPS update to %d", dev_desc->bMaxPacketSize0);
        usb_dev->ep_mps = dev_desc->bMaxPacketSize0;
        break;
    }
    case ENUM_STAGE_SET_ADDR: {
        USB_SETUP_PACKET_INIT_SET_ADDR((usb_setup_packet_t *)enum_transfer->data_buffer, usb_dev->dev_addr);
        enum_transfer->num_bytes = sizeof(usb_setup_packet_t);
        urb_need_enqueue = true;
        break;
    }
    case ENUM_STAGE_CHECK_ADDR: {
        ESP_ERROR_CHECK(hcd_pipe_update_dev_addr(usb_dev->dflt_pipe_hdl, usb_dev->dev_addr));
        vTaskDelay(pdMS_TO_TICKS(10)); //Wait SET ADDRESS recovery interval
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        usb_dev->enum_stage = ENUM_STAGE_CHECK_FULL_CONFIG_DESC;
#endif
        break;
    }
    case ENUM_STAGE_GET_FULL_DEV_DESC: {
        USB_SETUP_PACKET_INIT_GET_DEVICE_DESC((usb_setup_packet_t *)enum_transfer->data_buffer);
        enum_transfer->num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(sizeof(usb_device_desc_t), s_usb_dev.ep_mps);
        urb_need_enqueue = true;
        break;
    }
    case ENUM_STAGE_CHECK_FULL_DEV_DESC: {
        usb_device_desc_t *dev_desc = (usb_device_desc_t *)(enum_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
        print_device_descriptor((const uint8_t *)dev_desc);
        break;
    }
    case ENUM_STAGE_GET_SHORT_CONFIG_DESC: {
        USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)enum_transfer->data_buffer, usb_dev->configuration - 1, USB_ENUM_SHORT_DESC_REQ_LEN);
        enum_transfer->num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(USB_ENUM_SHORT_DESC_REQ_LEN, s_usb_dev.ep_mps);
        urb_need_enqueue = true;
        break;
    }
    case ENUM_STAGE_CHECK_SHORT_CONFIG_DESC: {
        usb_config_desc_t *cfg_desc = (usb_config_desc_t *)(enum_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
        full_config_length = cfg_desc->wTotalLength;
        UVC_CHECK_GOTO(full_config_length <= CTRL_TRANSFER_DATA_MAX_BYTES, "Configuration descriptor larger than control transfer max length", stage_failed_);
        break;
    }
    case ENUM_STAGE_GET_FULL_CONFIG_DESC: {
        USB_SETUP_PACKET_INIT_GET_CONFIG_DESC((usb_setup_packet_t *)enum_transfer->data_buffer, usb_dev->configuration - 1, full_config_length);
        enum_transfer->num_bytes = sizeof(usb_setup_packet_t) + usb_round_up_to_mps(full_config_length, s_usb_dev.ep_mps);
        urb_need_enqueue = true;
        break;
    }
    case ENUM_STAGE_CHECK_FULL_CONFIG_DESC: {
        usb_config_desc_t *cfg_desc = (usb_config_desc_t *)(enum_done->transfer.data_buffer + sizeof(usb_setup_packet_t));
        ret = _update_config_from_descriptor(cfg_desc);
        UVC_CHECK_GOTO(ret == ESP_OK, "Descriptor Parse Result: No matching configurations", stage_failed_);
        break;
    }
    case ENUM_STAGE_SET_CONFIG: {
        USB_SETUP_PACKET_INIT_SET_CONFIG((usb_setup_packet_t *)enum_transfer->data_buffer, usb_dev->configuration);
        enum_transfer->num_bytes = sizeof(usb_setup_packet_t);
        urb_need_enqueue = true;
        break;
    }
    case ENUM_STAGE_CHECK_CONFIG: {
        // Enum process done
        break;
    }
    default:
        break;
    }
    if (urb_need_enqueue) {
        ret = hcd_urb_enqueue(usb_dev->dflt_pipe_hdl, ctrl_urb);
        UVC_CHECK_GOTO(ret == ESP_OK, "ctrl urb enqueue failed", stage_failed_);
        if (waiting_urb_done) {
            *waiting_urb_done = true;
        }
    }
    if (usb_dev->enum_stage < ENUM_STAGE_CHECK_CONFIG) {
        ESP_LOGI(TAG, "ENUM Stage %s, Succeed", STAGE_STR[usb_dev->enum_stage]);
        usb_dev->enum_stage++;
        return ESP_OK;
    } else {
        usb_dev->enum_stage = ENUM_STAGE_NONE;
    }
stage_failed_:
    if (urb_need_free) {
        _usb_urb_free(ctrl_urb);
        urb_need_free = false;
    }
    ctrl_urb = NULL;
    if (usb_dev->enum_stage == ENUM_STAGE_NONE) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "ENUM Stage %s, Failed", STAGE_STR[usb_dev->enum_stage]);
    usb_dev->enum_stage = ENUM_STAGE_FAILED;
    return ESP_FAIL;
}

static void _usb_processing_task(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    _usb_device_t *usb_dev = &s_usb_dev;
    esp_err_t ret = ESP_OK;
    _event_msg_t evt_msg = {};
    uint32_t action_bits = 0;
#ifdef CONFIG_USB_ENUM_FAILED_RETRY
    int enum_retry_delay_ms = 0;
    int enum_retry_count = 0;
#endif
    ESP_LOGD(TAG, "USB task start");
    usb_dev->port_hdl = _usb_port_init(&_usb_port_callback, (void *)usb_dev->queue_hdl);
    UVC_CHECK_GOTO(usb_dev->port_hdl != NULL, "USB Port init failed", free_task_);
    xEventGroupSetBits(usb_dev->event_group_hdl, USB_HOST_INIT_DONE);
    UVC_ENTER_CRITICAL();
    usb_dev->state = STATE_DEVICE_INSTALLED;
    UVC_EXIT_CRITICAL();
    ESP_LOGD(TAG, "USB Task: Waiting Connection");
    int debug_counter = 0;
    while (1) {
        if (xQueueReceive(usb_dev->queue_hdl, &evt_msg, 1) == pdTRUE) {
            // update action bits based on events
            if (evt_msg._type == PORT_EVENT) {
                hcd_port_event_t port_actual_evt = _usb_port_event_dflt_process(usb_dev->port_hdl, evt_msg._event.port_event);
                action_bits = _usb_port_actions_update(port_actual_evt, action_bits);
#ifdef CONFIG_USB_ENUM_FAILED_RETRY
                //enum retry count reset to default if new device is connected
                if (port_actual_evt == HCD_PORT_EVENT_CONNECTION) {
                    enum_retry_count = CONFIG_USB_ENUM_FAILED_RETRY_COUNT;
                }
#endif
            } else if (evt_msg._type == PIPE_EVENT) {
                hcd_pipe_event_t dflt_pipe_evt = _pipe_event_dflt_process(usb_dev->dflt_pipe_hdl, "default", evt_msg._event.pipe_event);
                action_bits = _usb_pipe_actions_update(dflt_pipe_evt, action_bits);
            } else if (evt_msg._type == USER_EVENT) {
                // process event from usb stream task
                action_bits = _user_actions_update(evt_msg._event.user_cmd, action_bits);
            }
        }
        if (xEventGroupGetBits(usb_dev->event_group_hdl) & USB_HOST_TASK_KILL_BIT) {
            action_bits = ACTION_PORT_DISABLE;
            action_bits |= ACTION_PIPE_DFLT_DISABLE;
        }
        // process action bits
        if (action_bits == 0) {
            // no action bits set, wait for event
            if (++debug_counter % 300 == 0) {
                ESP_LOGD(TAG, "USB task running %d", debug_counter);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (usb_dev->state == STATE_DEVICE_ENUM && (action_bits & ACTION_PIPE_XFER_FAIL)) {
            // transfer failed during enum process, back to enum action to judge if need retry
            action_bits |= ACTION_DEVICE_ENUM;
        }
        if (usb_dev->state == STATE_DEVICE_ENUM && (action_bits & (ACTION_PORT_RECOVER
                                                                   | ACTION_PORT_DISABLE | ACTION_PIPE_DFLT_DISABLE))) {
            // If user disable, or port error, or disconnect happened, Force end the enum process without retry
            _uvc_uac_device_enum(true, NULL);
            action_bits &= ~ACTION_DEVICE_ENUM;
            action_bits &= ~ACTION_DEVICE_ENUM_RECOVER;
        }
        if (usb_dev->state == STATE_DEVICE_ACTIVE && (action_bits & (ACTION_PIPE_XFER_FAIL
                                                                     | ACTION_PIPE_DFLT_CLEAR | ACTION_PIPE_DFLT_RECOVER | ACTION_PIPE_DFLT_DISABLE))) {
            // If transfer fail or pipe recovering, send a signal to transfer invoker
            xEventGroupSetBits(usb_dev->event_group_hdl, USB_CTRL_PROC_FAILED);
        }

#ifdef CONFIG_USB_ENUM_FAILED_RETRY
        if (action_bits & ACTION_DEVICE_ENUM_RECOVER) {
            // we only retry if port not disabled or reconnect during the retry delay
            if (enum_retry_delay_ms > 0) {
                enum_retry_delay_ms -= 10;
                ESP_LOGD(TAG, "USB enum failed, retrying in %d ms", enum_retry_delay_ms);
                vTaskDelay(pdMS_TO_TICKS(10));
            } else {
                ESP_LOGW(TAG, "USB enum failed, retry now");
                usb_dev->enum_stage = ENUM_STAGE_NONE;
                action_bits |= ACTION_DEVICE_ENUM;
                action_bits &= ~ACTION_DEVICE_ENUM_RECOVER;
            }
        }
#endif

        if (action_bits & (ACTION_PORT_RECOVER | ACTION_PORT_DISABLE)) {
            ESP_LOGI(TAG, "Recover Stream Task");
            UVC_ENTER_CRITICAL();
            usb_dev->state = STATE_DEVICE_RECOVER;
            UVC_EXIT_CRITICAL();
            if (usb_dev->stream_task_hdl != NULL) {
                _usb_stream_recover_cb(TIMEOUT_USB_STREAM_DISCONNECT_MS);
            }
        }
        if (action_bits & ACTION_PIPE_XFER_FAIL) {
            // we do nothing here, because all conditions has been handled
            ESP_LOGD(TAG, "Action: ACTION_PIPE_XFER_FAIL");
            action_bits &= ~ACTION_PIPE_XFER_FAIL;
            ESP_LOGD(TAG, "Action: ACTION_PIPE_XFER_FAIL, Done!");
        }
        if (action_bits & ACTION_PIPE_DFLT_RECOVER) {
            ESP_LOGI(TAG, "Action: ACTION_PIPE_DFLT_RECOVER");
            UVC_ENTER_CRITICAL();
            usb_dev->state = STATE_DEVICE_RECOVER;
            UVC_EXIT_CRITICAL();
            if (usb_dev->dflt_pipe_hdl) {
                ret = _usb_pipe_flush(usb_dev->dflt_pipe_hdl, 1);
                UVC_CHECK_CONTINUE(ESP_OK == ret, "Default pipe flush failed");
            }
            action_bits &= ~ACTION_PIPE_DFLT_RECOVER;
            ESP_LOGD(TAG, "Action: ACTION_PIPE_DFLT_RECOVER, Done!");
        }
        if (action_bits & ACTION_PIPE_DFLT_CLEAR) {
            ESP_LOGI(TAG, "Action: ACTION_PIPE_DFLT_CLEAR");
            hcd_urb_dequeue(usb_dev->dflt_pipe_hdl);
            ret = hcd_pipe_command(usb_dev->dflt_pipe_hdl, HCD_PIPE_CMD_CLEAR);
            UVC_CHECK_CONTINUE(ESP_OK == ret, "Default pipe clear failed");
            action_bits &= ~ACTION_PIPE_DFLT_CLEAR;
            UVC_ENTER_CRITICAL();
            usb_dev->state = STATE_DEVICE_ACTIVE;
            UVC_EXIT_CRITICAL();
            ESP_LOGD(TAG, "Action: ACTION_PIPE_DFLT_CLEAR, Done!");
        }
        if (action_bits & ACTION_PIPE_DFLT_DISABLE) {
            ESP_LOGI(TAG, "Action: ACTION_PIPE_DFLT_DISABLE");
            if (usb_dev->dflt_pipe_hdl) {
                ESP_LOGD(TAG, "Resetting default pipe");
                xSemaphoreTake(s_usb_dev.xfer_mutex_hdl, portMAX_DELAY);
                ret = _usb_pipe_deinit(usb_dev->dflt_pipe_hdl, 1);
                usb_dev->dflt_pipe_hdl = NULL;
                xSemaphoreGive(s_usb_dev.xfer_mutex_hdl);
                UVC_CHECK_CONTINUE(ESP_OK == ret, "Default pipe disable failed");
            }
            action_bits &= ~ACTION_PIPE_DFLT_DISABLE;
            ESP_LOGD(TAG, "Action: ACTION_PIPE_DFLT_DISABLE, Done!");
        }
        if (action_bits & ACTION_PORT_RECOVER) {
            ESP_LOGI(TAG, "Action: ACTION_PORT_RECOVER");
            ESP_LOGD(TAG, "USB port recovering from state(%d)", hcd_port_get_state(usb_dev->port_hdl));
            ret = hcd_port_recover(usb_dev->port_hdl);
            UVC_CHECK_CONTINUE(ESP_OK == ret, "PORT Recover failed");
            ESP_LOGD(TAG, "USB port after recover, state(%d)", hcd_port_get_state(usb_dev->port_hdl));
            ret = hcd_port_command(usb_dev->port_hdl, HCD_PORT_CMD_POWER_ON);
            UVC_CHECK_CONTINUE(ESP_OK == ret, "PORT Power failed");
            action_bits &= ~ACTION_PORT_RECOVER;
            ESP_LOGD(TAG, "Action: ACTION_PORT_RECOVER, Done!");
            xQueueReset(usb_dev->queue_hdl);
        }
        if (action_bits & ACTION_PORT_DISABLE) {
            ESP_LOGI(TAG, "Action: ACTION_PORT_DISABLE");
            if (usb_dev->stream_task_hdl != NULL) {
                _usb_stream_kill_cb(TIMEOUT_USB_STREAM_DEINIT_MS);
            }
            action_bits &= ~ACTION_PORT_DISABLE;
            ESP_LOGD(TAG, "Action: ACTION_PORT_DISABLE, Done!");
            break;
        }
        if (action_bits & ACTION_DEVICE_DISCONNECT) {
            ESP_LOGI(TAG, "Action: ACTION_DEVICE_DISCONN");
            action_bits &= ~ACTION_DEVICE_DISCONNECT;
            ESP_LOGD(TAG, "Action: ACTION_DEVICE_DISCONN, Done!");
            ESP_LOGI(TAG, "Waiting USB Connection");
        }
        if (action_bits & ACTION_DEVICE_CONNECT) {
            ESP_LOGI(TAG, "Action: ACTION_DEVICE_CONNECT");
            vTaskDelay(pdMS_TO_TICKS(WAITING_USB_AFTER_CONNECTION_MS));
            ESP_LOGI(TAG, "Resetting Port");
            static int reset_retry = 3;
            ret = hcd_port_command(usb_dev->port_hdl, HCD_PORT_CMD_RESET);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Port Reset failed, retry = %d", reset_retry--);
                vTaskDelay(pdMS_TO_TICKS(100));
                if (reset_retry == 0) {
                    reset_retry = 3;
                    action_bits &= ~ACTION_DEVICE_CONNECT;
                    UVC_ENTER_CRITICAL();
                    usb_dev->state = STATE_DEVICE_RECOVER;
                    UVC_EXIT_CRITICAL();
                    ESP_LOGE(TAG, "Port Reset failed, retry failed");
                }
                continue;
            }
            reset_retry = 3;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 3)
            ESP_LOGI(TAG, "Setting Port FIFO, %d", usb_dev->fifo_bias);
            ESP_ERROR_CHECK(hcd_port_set_fifo_bias(usb_dev->port_hdl, usb_dev->fifo_bias));
#endif
            action_bits &= ~ACTION_DEVICE_CONNECT;
            action_bits |= ACTION_DEVICE_ENUM;
            usb_dev->enum_stage = ENUM_STAGE_NONE;
            UVC_ENTER_CRITICAL();
            usb_dev->state = STATE_DEVICE_CONNECTED;
            UVC_EXIT_CRITICAL();
            ESP_LOGD(TAG, "Action: ACTION_DEVICE_CONNECT, Done!");
        }
        if (action_bits & ACTION_DEVICE_ENUM) {
            UVC_ENTER_CRITICAL();
            usb_dev->state = STATE_DEVICE_ENUM;
            UVC_EXIT_CRITICAL();
            _enum_stage_t enum_stage = usb_dev->enum_stage;
            ESP_LOGD(TAG, "Action: ACTION_DEVICE_ENUM Stage %s (%d)", STAGE_STR[enum_stage], enum_stage);
            bool if_waiting = false;
            ret = _uvc_uac_device_enum(false, &if_waiting);
            if (if_waiting) {
                action_bits &= ~ACTION_DEVICE_ENUM;
                ESP_LOGD(TAG, "Action: ACTION_DEVICE_ENUM, Waiting URB Done");
            }
            if (ret != ESP_OK) {
                action_bits &= ~ACTION_DEVICE_ENUM;
#ifdef CONFIG_USB_ENUM_FAILED_RETRY
                // if enum failed, we only retry if port not disabled or recovered
                if (--enum_retry_count > 0) {
                    enum_retry_delay_ms = CONFIG_USB_ENUM_FAILED_RETRY_DELAY_MS;
                    action_bits |= ACTION_DEVICE_ENUM_RECOVER;
                    ESP_LOGW(TAG, "USB enum failed, retrying in %d ms...", enum_retry_delay_ms);
                } else {
                    ESP_LOGE(TAG, "USB enum failed, no more retry");
                }
                UVC_ENTER_CRITICAL();
                usb_dev->state = (action_bits & ACTION_DEVICE_ENUM_RECOVER) ? STATE_DEVICE_RECOVER : STATE_DEVICE_ENUM_FAILED;
                UVC_EXIT_CRITICAL();
#else
                // encounter failed, block in enum failed state
                UVC_ENTER_CRITICAL();
                usb_dev->state = STATE_DEVICE_ENUM_FAILED;
                UVC_EXIT_CRITICAL();
#endif
            } else if (usb_dev->enum_stage == ENUM_STAGE_NONE) {
                UVC_ENTER_CRITICAL();
                usb_dev->state = STATE_DEVICE_ACTIVE;
                UVC_EXIT_CRITICAL();
                action_bits &= ~ACTION_DEVICE_ENUM;
                _usb_stream_connect_cb();
            }
            ESP_LOGD(TAG, "Action: ACTION_DEVICE_ENUM, Done (%d)", enum_stage);
        }
        if (action_bits & ACTION_PIPE_XFER_DONE) {
            ESP_LOGD(TAG, "Action: ACTION_PIPE_XFER_DONE");
            if (usb_dev->state == STATE_DEVICE_ENUM) {
                action_bits |= ACTION_DEVICE_ENUM;
            } else {
                xEventGroupSetBits(usb_dev->event_group_hdl, USB_CTRL_PROC_SUCCEED);
            }
            action_bits &= ~ACTION_PIPE_XFER_DONE;
            ESP_LOGD(TAG, "Action: ACTION_PIPE_XFER_DONE, Done!");
        }
    }
    if (usb_dev->port_hdl) {
        _usb_port_deinit(usb_dev->port_hdl);
        usb_dev->port_hdl = NULL;
    }
free_task_:
    UVC_ENTER_CRITICAL();
    usb_dev->state = STATE_NONE;
    UVC_EXIT_CRITICAL();
    ESP_LOGI(TAG, "_usb_processing_task deleted");
    ESP_LOGD(TAG, "_usb_processing_task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    xEventGroupClearBits(usb_dev->event_group_hdl, (USB_HOST_INIT_DONE | USB_HOST_TASK_KILL_BIT));
    vTaskDelete(NULL);
}

/*populate frame then call user callback*/
static void _sample_processing_task(void *arg)
{
    UVC_CHECK_RETURN_VOID(arg != NULL, "sample task arg should be _uvc_stream_handle_t *");
    _uvc_stream_handle_t *strmh = (_uvc_stream_handle_t *)(arg);
    uint32_t last_seq = 0;
    ESP_LOGI(TAG, "Sample processing task start");

    xEventGroupClearBits(s_usb_dev.event_group_hdl, UVC_SAMPLE_PROC_STOP_DONE);
    do {
        xSemaphoreTake(strmh->cb_mutex, portMAX_DELAY);

        while (strmh->running && last_seq == strmh->hold_seq) {
            xSemaphoreGive(strmh->cb_mutex);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ESP_LOGV(TAG, "GOT LENGTH %d", strmh->hold_bytes);
            xSemaphoreTake(strmh->cb_mutex, portMAX_DELAY);
        }

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

    ESP_LOGI(TAG, "Sample processing task deleted");
    ESP_LOGD(TAG, "Sample processing task watermark = %d B", uxTaskGetStackHighWaterMark(NULL));
    xEventGroupSetBits(s_usb_dev.event_group_hdl, UVC_SAMPLE_PROC_STOP_DONE);
    vTaskDelete(NULL);
}

static esp_err_t usb_stream_control(usb_stream_t stream, stream_ctrl_t ctrl_type)
{
    UVC_CHECK(ctrl_type == CTRL_SUSPEND || ctrl_type == CTRL_RESUME, "USB Device not active", ESP_ERR_INVALID_ARG);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();
    if (task_handle == s_usb_dev.stream_task_hdl) {
        //If resume/suspend is called in stream task, we just set flag and return
        if (ctrl_type == CTRL_SUSPEND) {
            s_usb_dev.ifc[stream]->suspended = true;
        } else {
            s_usb_dev.ifc[stream]->suspended = false;
        }
        ESP_LOGD(TAG, "%s %s in state callback", ctrl_type == CTRL_SUSPEND ? "Suspend" : "Resume", s_usb_dev.ifc[stream]->name);
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;
    _stream_ifc_t *p_itf = s_usb_dev.ifc[stream];
    _event_msg_t event_msg = {
        ._type = USER_EVENT,
        ._event_data = (void *)stream,
        ._event.user_cmd = STREAM_SUSPEND,
    };

    if (ctrl_type == CTRL_RESUME) {
        //check if streaming is not running
        UVC_CHECK(!(xEventGroupGetBits(s_usb_dev.event_group_hdl) & p_itf->evt_bit), "Streaming is running", ESP_ERR_INVALID_STATE);
        ESP_LOGI(TAG, "Resume %s streaming", p_itf->name);
        //resume streaming interface first
        ret = _usb_streaming_resume(stream);
        UVC_CHECK(ret == ESP_OK, "Resume interface Failed", ESP_FAIL);
        //send resume command to resume transfer
        event_msg._event.user_cmd = STREAM_RESUME;
        xEventGroupClearBits(s_usb_dev.event_group_hdl, USB_STREAM_TASK_PROC_SUCCEED | USB_STREAM_TASK_PROC_FAILED);
        xQueueSend(s_usb_dev.stream_queue_hdl, &event_msg, portMAX_DELAY);
        EventBits_t uxBits = xEventGroupWaitBits(s_usb_dev.event_group_hdl, (USB_STREAM_TASK_PROC_SUCCEED | USB_STREAM_TASK_PROC_FAILED), pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND_MS));
        UVC_CHECK(uxBits & (USB_STREAM_TASK_PROC_SUCCEED), "Reset transfer failed/timeout", ESP_FAIL);
        ESP_LOGD(TAG, "Resume %s streaming Done", p_itf->name);
    } else if (ctrl_type == CTRL_SUSPEND) {
        //check if streaming is running
        UVC_CHECK((xEventGroupGetBits(s_usb_dev.event_group_hdl) & p_itf->evt_bit), "Streaming not running", ESP_ERR_INVALID_STATE);
        ESP_LOGI(TAG, "Suspend %s streaming", p_itf->name);
        //send suspend command to stop transfer first
        event_msg._event.user_cmd = STREAM_SUSPEND;
        xEventGroupClearBits(s_usb_dev.event_group_hdl, USB_STREAM_TASK_PROC_SUCCEED | USB_STREAM_TASK_PROC_FAILED);
        xQueueSend(s_usb_dev.stream_queue_hdl, &event_msg, portMAX_DELAY);
        EventBits_t uxBits = xEventGroupWaitBits(s_usb_dev.event_group_hdl, (USB_STREAM_TASK_PROC_SUCCEED | USB_STREAM_TASK_PROC_FAILED), pdTRUE, pdFALSE, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND_MS));
        UVC_CHECK(uxBits & (USB_STREAM_TASK_PROC_SUCCEED), "Reset transfer failed/timeout", ESP_FAIL);
        //suspend streaming interface
        ret = _usb_streaming_suspend(stream);
        UVC_CHECK(ret == ESP_OK, "Resume interface Failed", ESP_FAIL);
        ESP_LOGD(TAG, "Suspend %s streaming Done", p_itf->name);
    }
    return ESP_OK;
}

static esp_err_t uac_feature_control(usb_stream_t stream, stream_ctrl_t ctrl_type, void *ctrl_value)
{
    _stream_ifc_t *p_itf = s_usb_dev.ifc[stream];
    esp_err_t ret = ESP_OK;
    bool submit_ctrl = true;
    if (_usb_device_get_state() != STATE_DEVICE_ACTIVE) {
        submit_ctrl = false;
    }

    _uac_internal_stream_t uac_stream = (stream == STREAM_UAC_SPK ? UAC_SPK : UAC_MIC);
    UVC_ENTER_CRITICAL();
    uint16_t ac_interface = s_usb_dev.uac->ac_interface;
    uint8_t mute_ch = s_usb_dev.uac->mute_ch[uac_stream];
    uint8_t fu_id = s_usb_dev.uac->fu_id[uac_stream];
    uint8_t volume_ch = s_usb_dev.uac->volume_ch[uac_stream];
    UVC_EXIT_CRITICAL();

    switch (ctrl_type) {
    case CTRL_UAC_MUTE:
        UVC_ENTER_CRITICAL();
        s_usb_dev.uac->mute[uac_stream] = (uint32_t)ctrl_value;
        UVC_EXIT_CRITICAL();
        if (submit_ctrl && fu_id != 0) {
            ret = _uac_as_control_set_mute(ac_interface, mute_ch, fu_id, (uint32_t)ctrl_value);
            ESP_LOGI(TAG, "Set %s %s", stream == STREAM_UAC_SPK ? "SPK" : "MIC", (uint32_t)ctrl_value ? "Mute" : "UnMute");
        } else if (fu_id != 0) {
            ret = ESP_ERR_INVALID_SIZE;
        }
        break;
    case CTRL_UAC_VOLUME:
        UVC_ENTER_CRITICAL();
        s_usb_dev.uac->volume[uac_stream] = (uint32_t)ctrl_value;
        UVC_EXIT_CRITICAL();
        if (submit_ctrl && fu_id != 0) {
            ret = _uac_as_control_set_volume(ac_interface, volume_ch, fu_id, (uint32_t)ctrl_value);
            ESP_LOGI(TAG, "Set %s volume = %" PRIu32, stream == STREAM_UAC_SPK ? "SPK" : "MIC", (uint32_t)ctrl_value);
        } else if (fu_id != 0) {
            ret = ESP_ERR_INVALID_SIZE;
        }
        break;
    default:
        break;
    }

    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "%s Feature Control: type = %d value = %p, Done", p_itf->name, ctrl_type, ctrl_value);
    } else {
        ESP_LOGW(TAG, "%s Feature Control: type = %d value = %p, Failed", p_itf->name, ctrl_type, ctrl_value);
    }
    return ret;
}

/***************************************************** Public API *********************************************************************/
esp_err_t uac_streaming_config(const uac_config_t *config)
{
    UVC_CHECK(s_usb_dev.event_group_hdl == NULL, "usb streaming is running", ESP_ERR_INVALID_STATE);
    //We do not prevent re-config, but the configs will take effect after re-connect/recover/re-enumeration/
    UVC_CHECK(config != NULL, "config can't NULL", ESP_ERR_INVALID_ARG);
    if (config->mic_samples_frequence && config->mic_bit_resolution) {
        //using samples_frequence and bit_resolution as enable condition
        UVC_CHECK(config->mic_samples_frequence == UAC_FREQUENCY_ANY || (config->mic_samples_frequence >= 1000 && config->mic_samples_frequence <= 48000),
                  "mic samples frequency must <= 48000Hz and >= 1000 Hz ", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->mic_bit_resolution == UAC_BITS_ANY || (config->mic_bit_resolution >= 8 && config->mic_bit_resolution <= 24),
                  "mic bit resolution must >= 8 bit and <=24 bit", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->mic_ch_num == UAC_CH_ANY || (config->mic_ch_num >= 1 && config->mic_ch_num <= 2),
                  "mic channel number must >= 1 and <=2", ESP_ERR_INVALID_ARG);
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        UVC_CHECK(config->mic_interface, "mic interface can not be 0", ESP_ERR_INVALID_ARG);
        if (config->ac_interface) {
            UVC_CHECK(config->mic_fu_id, "mic feature unit id can not be 0", ESP_ERR_INVALID_ARG);
        }
#endif
        //below params act as backup configs, if suitable config not found from device descriptors
        if (config->mic_interface) {
            UVC_CHECK(config->mic_ep_addr & 0x80, "mic endpoint direction must IN", ESP_ERR_INVALID_ARG);
            UVC_CHECK(config->mic_ep_mps, "mic endpoint mps must > 0", ESP_ERR_INVALID_ARG);
            UVC_CHECK(config->mic_samples_frequence * config->mic_bit_resolution / 8000 <= config->mic_ep_mps, "mic packet size must <= endpoint mps", ESP_ERR_INVALID_ARG);
        }
    }
    if (config->spk_samples_frequence && config->spk_bit_resolution) {
        //using samples_frequence and bit_resolution as enable condition
        UVC_CHECK(config->spk_samples_frequence == UAC_FREQUENCY_ANY || (config->spk_samples_frequence >= 1000 && config->spk_samples_frequence <= 48000),
                  "speaker samples frequency must <= 48000Hz and >= 1000 Hz ", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->spk_bit_resolution == UAC_BITS_ANY || (config->spk_bit_resolution >= 8 && config->spk_bit_resolution <= 24),
                  "speaker bit resolution must >= 8 bit and <=24 bit", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->spk_ch_num == UAC_CH_ANY || (config->spk_ch_num >= 1 && config->spk_ch_num <= 2),
                  "speaker channel number must >= 1 and <=2", ESP_ERR_INVALID_ARG);
        UVC_CHECK(config->spk_buf_size, "spk buffer size can not be 0", ESP_ERR_INVALID_ARG);
#ifndef CONFIG_UVC_GET_CONFIG_DESC
        UVC_CHECK(config->spk_interface, "spk interface can not be 0", ESP_ERR_INVALID_ARG);
        if (config->ac_interface) {
            UVC_CHECK(config->spk_fu_id, "spk feature unit id can not be 0", ESP_ERR_INVALID_ARG);
        }
#endif
        if (config->spk_interface) {
            //if user set this interface manually, below param should also be set
            UVC_CHECK(!(config->spk_ep_addr & 0x80), "spk endpoint direction must OUT", ESP_ERR_INVALID_ARG);
            UVC_CHECK(config->spk_buf_size > config->spk_ep_mps, "spk buffer size should larger than endpoint size", ESP_ERR_INVALID_ARG);
            UVC_CHECK(config->spk_ep_mps, "speaker endpoint mps must > 0", ESP_ERR_INVALID_ARG);
            UVC_CHECK(config->spk_samples_frequence * config->spk_bit_resolution / 8000 <= config->spk_ep_mps, "spk packet size must <= endpoint mps", ESP_ERR_INVALID_ARG);
        }
    }
    s_usb_dev.uac_cfg = *config;
    s_usb_dev.flags |= config->flags;
    if (s_usb_dev.flags & FLAG_UAC_SPK_SUSPEND_AFTER_START) {
        ESP_LOGI(TAG, "SPK Streaming Suspend After Start");
    } else if (s_usb_dev.flags & FLAG_UAC_MIC_SUSPEND_AFTER_START) {
        ESP_LOGI(TAG, "MIC Streaming Suspend After Start");
    }
    ESP_LOGI(TAG, "UAC Streaming Config Succeed, Version: %d.%d.%d", USB_STREAM_VER_MAJOR, USB_STREAM_VER_MINOR, USB_STREAM_VER_PATCH);
    return ESP_OK;
}

esp_err_t uvc_streaming_config(const uvc_config_t *config)
{
    UVC_CHECK(s_usb_dev.event_group_hdl == NULL, "usb streaming is running", ESP_ERR_INVALID_STATE);
    //We do not prevent re-config, but the configs will take effect after re-connect/recover/re-enumeration/
    UVC_CHECK(config != NULL, "config can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK((config->frame_interval >= FRAME_MIN_INTERVAL && config->frame_interval <= FRAME_MAX_INTERVAL),
              "frame_interval Support 333333~2000000", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->format < UVC_FORMAT_MAX, "format can't larger than UVC_FORMAT_MAX", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_height != 0, "frame_height can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_width != 0, "frame_width can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_buffer_size != 0, "frame_buffer_size can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_size != 0, "xfer_buffer_size can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_a != NULL, "xfer_buffer_a can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_buffer_b != NULL, "xfer_buffer_b can't NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_buffer != NULL, "frame_buffer can't NULL", ESP_ERR_INVALID_ARG);
#ifndef CONFIG_UVC_GET_CONFIG_DESC
    //Additional check for quick start mode
    UVC_CHECK(config->interface, "interface can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->xfer_type == UVC_XFER_ISOC || config->xfer_type == UVC_XFER_BULK, "xfer_type must be UVC_XFER_ISOC or UVC_XFER_BULK", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->format_index != 0, "format_index can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_index != 0, "frame_index can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_width != FRAME_RESOLUTION_ANY, "frame_width can't FRAME_RESOLUTION_ANY", ESP_ERR_INVALID_ARG);
    UVC_CHECK(config->frame_height != FRAME_RESOLUTION_ANY, "frame_height can't FRAME_RESOLUTION_ANY", ESP_ERR_INVALID_ARG);
#endif
    //below params act as backup configs, if suitable config not found from device descriptors
    if (config->interface) {
        UVC_CHECK(config->ep_addr & 0x80, "Endpoint direction must IN", ESP_ERR_INVALID_ARG);
        if (config->xfer_type == UVC_XFER_ISOC) {
            UVC_CHECK(config->ep_mps <= USB_EP_ISOC_IN_MAX_MPS, "Isoc total MPS must < USB_EP_ISOC_IN_MAX_MPS", ESP_ERR_INVALID_ARG);
            UVC_CHECK(config->interface_alt != 0, "Isoc interface alt num must > 0", ESP_ERR_INVALID_ARG);
        } else {
            UVC_CHECK(config->interface_alt == 0, "Bulk interface alt num must == 0", ESP_ERR_INVALID_ARG);
            UVC_CHECK(config->ep_mps == USB_EP_BULK_FS_MPS || config->ep_mps == USB_EP_BULK_HS_MPS, "Bulk MPS must == 64 or 512", ESP_ERR_INVALID_ARG);
        }
    }
    s_usb_dev.uvc_cfg = *config;
    s_usb_dev.flags |= config->flags;
    if (s_usb_dev.flags & FLAG_UVC_SUSPEND_AFTER_START) {
        ESP_LOGI(TAG, "UVC Streaming Suspend After Start");
    }
    ESP_LOGI(TAG, "UVC Streaming Config Succeed, Version: %d.%d.%d", USB_STREAM_VER_MAJOR, USB_STREAM_VER_MINOR, USB_STREAM_VER_PATCH);
#ifdef CONFIG_USB_STREAM_QUICK_START
    // Please make sure your camera can skip the enumeration stage and start streaming directly
    ESP_LOGI(TAG, "Quick Start Mode Enabled");
#endif
    return ESP_OK;
}

esp_err_t usb_streaming_start()
{
    UVC_CHECK(s_usb_dev.event_group_hdl == NULL, "usb streaming is running", ESP_ERR_INVALID_STATE);
    s_usb_dev.event_group_hdl = xEventGroupCreate();
    UVC_CHECK(s_usb_dev.event_group_hdl  != NULL, "Create event group failed", ESP_FAIL);
    s_usb_dev.queue_hdl = xQueueCreate(USB_EVENT_QUEUE_LEN, sizeof(_event_msg_t));
    UVC_CHECK_GOTO(s_usb_dev.queue_hdl != NULL, "Create event queue failed", free_resource_);
    s_usb_dev.stream_queue_hdl = xQueueCreate(USB_STREAM_EVENT_QUEUE_LEN, sizeof(_event_msg_t));
    UVC_CHECK_GOTO(s_usb_dev.stream_queue_hdl != NULL, "Create event queue failed", free_resource_);
    s_usb_dev.xfer_mutex_hdl = xSemaphoreCreateMutex();
    UVC_CHECK_GOTO(s_usb_dev.xfer_mutex_hdl != NULL, "Create transfer mutex failed", free_resource_);
    s_usb_dev.ctrl_smp_hdl = xSemaphoreCreateBinary();
    UVC_CHECK_GOTO(s_usb_dev.ctrl_smp_hdl != NULL, "Create ctrl mutex failed", free_resource_);
    xSemaphoreGive(s_usb_dev.ctrl_smp_hdl);
#if USB_PRE_ALLOC_CTRL_TRANSFER_URB
    s_usb_dev.ctrl_urb = _usb_urb_alloc(0, sizeof(usb_setup_packet_t) + CTRL_TRANSFER_DATA_MAX_BYTES, NULL);
    UVC_CHECK_GOTO(s_usb_dev.ctrl_urb != NULL, "malloc ctrl urb failed", free_resource_);
    ESP_LOGI(TAG, "Pre-alloc ctrl urb succeed, size = %d", CTRL_TRANSFER_DATA_MAX_BYTES);
#endif
    s_usb_dev.dev_speed = USB_SPEED_FULL;
    s_usb_dev.dev_addr = USB_DEVICE_ADDR;
    s_usb_dev.configuration = USB_CONFIG_NUM;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 3)
    s_usb_dev.fifo_bias = HCD_PORT_FIFO_BIAS_BALANCED;
#endif
    s_usb_dev.mps_limits = &s_mps_limits_default;

    if (s_usb_dev.uac_cfg.spk_samples_frequence && s_usb_dev.uac_cfg.spk_bit_resolution) {
        //using samples_frequence and bit_resolution as enable condition
        s_usb_dev.uac = heap_caps_calloc(1, sizeof(_uac_device_t), MALLOC_CAP_INTERNAL);
        UVC_CHECK_GOTO(s_usb_dev.uac != NULL, "malloc failed", free_resource_);
        s_usb_dev.uac->as_ifc[UAC_SPK] = heap_caps_calloc(1, sizeof(_stream_ifc_t), MALLOC_CAP_INTERNAL);
        UVC_CHECK_GOTO(s_usb_dev.uac->as_ifc[UAC_SPK] != NULL, "malloc failed", free_resource_);
        s_usb_dev.ifc[STREAM_UAC_SPK] = s_usb_dev.uac->as_ifc[UAC_SPK];
        s_usb_dev.ifc[STREAM_UAC_SPK]->type = STREAM_UAC_SPK;
        s_usb_dev.ifc[STREAM_UAC_SPK]->name = "SPK";
        s_usb_dev.ifc[STREAM_UAC_SPK]->evt_bit = UAC_SPK_STREAM_RUNNING;
        if (s_usb_dev.uac_cfg.spk_buf_size) {
            s_usb_dev.uac->ringbuf_hdl[UAC_SPK] = xRingbufferCreate(s_usb_dev.uac_cfg.spk_buf_size, RINGBUF_TYPE_BYTEBUF);
            ESP_LOGD(TAG, "Speaker ringbuf create succeed, size = %"PRIu32, s_usb_dev.uac_cfg.spk_buf_size);
            UVC_CHECK_GOTO(s_usb_dev.uac->ringbuf_hdl[UAC_SPK] != NULL, "Create speak buffer failed", free_resource_);
        }
        ESP_LOGD(TAG, "Speaker instance created");
        s_usb_dev.enabled[STREAM_UAC_SPK] = true;
    }
    if (s_usb_dev.uac_cfg.mic_samples_frequence && s_usb_dev.uac_cfg.mic_bit_resolution) {
        //using samples_frequence and bit_resolution as enable condition
        if (s_usb_dev.uac == NULL) {
            s_usb_dev.uac = heap_caps_calloc(1, sizeof(_uac_device_t), MALLOC_CAP_INTERNAL);
            UVC_CHECK_GOTO(s_usb_dev.uac != NULL, "malloc failed", free_resource_);
        }
        s_usb_dev.uac->as_ifc[UAC_MIC] = heap_caps_calloc(1, sizeof(_stream_ifc_t), MALLOC_CAP_INTERNAL);
        UVC_CHECK_GOTO(s_usb_dev.uac->as_ifc[UAC_MIC] != NULL, "malloc failed", free_resource_);
        s_usb_dev.ifc[STREAM_UAC_MIC] = s_usb_dev.uac->as_ifc[UAC_MIC];
        s_usb_dev.ifc[STREAM_UAC_MIC]->type = STREAM_UAC_MIC;
        s_usb_dev.ifc[STREAM_UAC_MIC]->name = "MIC";
        s_usb_dev.ifc[STREAM_UAC_MIC]->evt_bit = UAC_MIC_STREAM_RUNNING;
        if (s_usb_dev.uac_cfg.mic_buf_size) {
            s_usb_dev.uac->ringbuf_hdl[UAC_MIC] = xRingbufferCreate(s_usb_dev.uac_cfg.mic_buf_size, RINGBUF_TYPE_BYTEBUF);
            ESP_LOGD(TAG, "MIC ringbuf create succeed, size = %"PRIu32, s_usb_dev.uac_cfg.mic_buf_size);
            UVC_CHECK_GOTO(s_usb_dev.uac->ringbuf_hdl[UAC_MIC] != NULL, "Create speak buffer failed", free_resource_);
        }
        ESP_LOGD(TAG, "Speaker instance created");
        s_usb_dev.enabled[STREAM_UAC_MIC] = true;
    }
    if (s_usb_dev.uvc_cfg.frame_width && s_usb_dev.uvc_cfg.frame_height) {
        //using frame_width and frame_height as enable condition
        s_usb_dev.uvc = heap_caps_calloc(1, sizeof(_uvc_device_t), MALLOC_CAP_INTERNAL);
        UVC_CHECK_GOTO(s_usb_dev.uvc != NULL, "malloc failed", free_resource_);
        s_usb_dev.uvc->vs_ifc = heap_caps_calloc(1, sizeof(_stream_ifc_t), MALLOC_CAP_INTERNAL);
        UVC_CHECK_GOTO(s_usb_dev.uvc->vs_ifc != NULL, "malloc failed", free_resource_);
        s_usb_dev.ifc[STREAM_UVC] = s_usb_dev.uvc->vs_ifc;
        s_usb_dev.ifc[STREAM_UVC]->type = STREAM_UVC;
        s_usb_dev.ifc[STREAM_UVC]->name = "UVC";
        s_usb_dev.ifc[STREAM_UVC]->evt_bit = USB_UVC_STREAM_RUNNING;
        ESP_LOGD(TAG, "Camera instance created");
        s_usb_dev.enabled[STREAM_UVC] = true;
        //if enable uvc, we should set fifo bias to RX
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 3)
        s_usb_dev.fifo_bias = HCD_PORT_FIFO_BIAS_RX;
#endif
        s_usb_dev.mps_limits = &s_mps_limits_bias_rx;
    }
    UVC_CHECK_GOTO(s_usb_dev.enabled[STREAM_UAC_MIC] == true || s_usb_dev.enabled[STREAM_UAC_SPK] == true || s_usb_dev.enabled[STREAM_UVC] == true, "uac/uvc streaming not configured", free_resource_);

    TaskHandle_t usbh_taskh = NULL;
    xTaskCreatePinnedToCore(_usb_processing_task, USB_PROC_TASK_NAME, USB_PROC_TASK_STACK_SIZE, NULL,
                            USB_PROC_TASK_PRIORITY, &usbh_taskh, USB_PROC_TASK_CORE);
    UVC_CHECK_GOTO(usbh_taskh != NULL, "Create usb processing task failed", free_resource_);
    xTaskCreatePinnedToCore(_usb_stream_handle_task, USB_STREAM_NAME, USB_STREAM_STACK_SIZE, NULL,
                            USB_STREAM_PRIORITY, &s_usb_dev.stream_task_hdl, USB_STREAM_CORE);
    assert(s_usb_dev.stream_task_hdl != NULL); //can not handle this error, just assert
    xTaskNotifyGive(usbh_taskh);
    xEventGroupWaitBits(s_usb_dev.event_group_hdl, USB_HOST_INIT_DONE, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "USB Streaming Start Succeed");
    return ESP_OK;

free_resource_:
    if (s_usb_dev.event_group_hdl) {
        vEventGroupDelete(s_usb_dev.event_group_hdl);
        s_usb_dev.event_group_hdl = NULL;
    }
    if (s_usb_dev.queue_hdl) {
        vQueueDelete(s_usb_dev.queue_hdl);
        s_usb_dev.queue_hdl = NULL;
    }
    if (s_usb_dev.stream_queue_hdl) {
        vQueueDelete(s_usb_dev.stream_queue_hdl);
        s_usb_dev.stream_queue_hdl = NULL;
    }
    if (s_usb_dev.xfer_mutex_hdl) {
        vSemaphoreDelete(s_usb_dev.xfer_mutex_hdl);
        s_usb_dev.xfer_mutex_hdl = NULL;
    }
    if (s_usb_dev.ctrl_smp_hdl) {
        vSemaphoreDelete(s_usb_dev.ctrl_smp_hdl);
        s_usb_dev.ctrl_smp_hdl = NULL;
    }
    if (s_usb_dev.ctrl_urb) {
        _usb_urb_free(s_usb_dev.ctrl_urb);
        s_usb_dev.ctrl_urb = NULL;
    }
    if (s_usb_dev.uvc) {
        if (s_usb_dev.uvc->vs_ifc) {
            free(s_usb_dev.uvc->vs_ifc);
        }
        s_usb_dev.ifc[STREAM_UVC] = NULL;
        free(s_usb_dev.uvc);
    }
    if (s_usb_dev.uac) {
        if (s_usb_dev.uac->ringbuf_hdl[UAC_SPK]) {
            vRingbufferDelete(s_usb_dev.uac->ringbuf_hdl[UAC_SPK]);
        }
        if (s_usb_dev.uac->ringbuf_hdl[UAC_MIC]) {
            vRingbufferDelete(s_usb_dev.uac->ringbuf_hdl[UAC_MIC]);
        }
        if (s_usb_dev.uac->as_ifc[UAC_SPK]) {
            free(s_usb_dev.uac->as_ifc[UAC_SPK]);
        }
        if (s_usb_dev.uac->as_ifc[UAC_MIC]) {
            free(s_usb_dev.uac->as_ifc[UAC_MIC]);
        }
        s_usb_dev.ifc[STREAM_UAC_SPK] = NULL;
        s_usb_dev.ifc[STREAM_UAC_MIC] = NULL;
        free(s_usb_dev.uac);
    }
    return ESP_FAIL;
}

esp_err_t usb_streaming_connect_wait(size_t timeout_ms)
{
    if (!s_usb_dev.event_group_hdl || !(xEventGroupGetBits(s_usb_dev.event_group_hdl) & USB_HOST_INIT_DONE)) {
        ESP_LOGW(TAG, "USB Streaming not started");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Waiting USB Device Connection");
    EventBits_t bits = xEventGroupWaitBits(s_usb_dev.event_group_hdl, USB_STREAM_DEVICE_READY_BIT, false, false, pdMS_TO_TICKS(timeout_ms));
    if (!(bits & USB_STREAM_DEVICE_READY_BIT)) {
        ESP_LOGW(TAG, "Waiting Device Connection, timeout");
        return ESP_ERR_TIMEOUT;
    }
    //lets wait a little more time for stream be handled
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "USB Device Connected");
    return ESP_OK;
}

esp_err_t usb_streaming_state_register(state_callback_t cb, void *user_ptr)
{
    if (s_usb_dev.event_group_hdl) {
        ESP_LOGW(TAG, "USB streaming is running, callback need register before start");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_usb_dev.state_cb) {
        ESP_LOGW(TAG, "Overwrite registered callback");
    }
    if (cb) {
        s_usb_dev.state_cb = cb;
        s_usb_dev.state_cb_arg = user_ptr;
    }
    ESP_LOGI(TAG, "USB streaming callback register succeed");
    return ESP_OK;
}

esp_err_t usb_streaming_stop(void)
{
    if (!s_usb_dev.event_group_hdl || !(xEventGroupGetBits(s_usb_dev.event_group_hdl) & USB_HOST_INIT_DONE)) {
        ESP_LOGW(TAG, "USB Streaming not started");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "USB Streaming Stop");
    esp_err_t ret = _event_set_bits_wait_cleared(s_usb_dev.event_group_hdl, USB_HOST_TASK_KILL_BIT, pdMS_TO_TICKS(TIMEOUT_USER_COMMAND_MS));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB Streaming Stop Failed");
        return ESP_ERR_TIMEOUT;
    }
    if (s_usb_dev.ctrl_urb) {
        _usb_urb_free(s_usb_dev.ctrl_urb);
    }
    if (s_usb_dev.uvc) {
        if (s_usb_dev.uvc->frame_size) {
#ifdef CONFIG_UVC_GET_CONFIG_DESC
            free(s_usb_dev.uvc->frame_size);
#endif
        }
        if (s_usb_dev.uvc->vs_ifc) {
            free(s_usb_dev.uvc->vs_ifc);
        }
        free(s_usb_dev.uvc);
    }
    if (s_usb_dev.uac) {
        for (size_t i = 0; i < UAC_MAX; i++) {
            if (s_usb_dev.uac->ringbuf_hdl[i]) {
                vRingbufferDelete(s_usb_dev.uac->ringbuf_hdl[i]);
            }
            if (s_usb_dev.uac->as_ifc[i]) {
                free(s_usb_dev.uac->as_ifc[i]);
            }
            if (s_usb_dev.uac->frame_size[i]) {
#ifdef CONFIG_UVC_GET_CONFIG_DESC
                free(s_usb_dev.uac->frame_size[i]);
#endif
            }
        }
        if (s_usb_dev.uac->mic_frame_buf) {
            free(s_usb_dev.uac->mic_frame_buf);
        }
        free(s_usb_dev.uac);
    }
    vQueueDelete(s_usb_dev.queue_hdl);
    vQueueDelete(s_usb_dev.stream_queue_hdl);
    vEventGroupDelete(s_usb_dev.event_group_hdl);
    vSemaphoreDelete(s_usb_dev.xfer_mutex_hdl);
    vSemaphoreDelete(s_usb_dev.ctrl_smp_hdl);
    memset(&s_usb_dev, 0, sizeof(_usb_device_t));
    //wait more time for task resources recovery
    vTaskDelay(pdMS_TO_TICKS(WAITING_TASK_RESOURCE_RELEASE_MS));
    ESP_LOGI(TAG, "USB Streaming Stop Done");
    return ESP_OK;
}

esp_err_t usb_streaming_control(usb_stream_t stream, stream_ctrl_t ctrl_type, void *ctrl_value)
{
    UVC_CHECK(stream < STREAM_MAX, "Invalid stream", ESP_ERR_INVALID_ARG);
    UVC_CHECK(ctrl_type < CTRL_MAX, "Invalid control type", ESP_ERR_INVALID_ARG);
    if (!s_usb_dev.event_group_hdl || !(xEventGroupGetBits(s_usb_dev.event_group_hdl) & USB_HOST_INIT_DONE)) {
        ESP_LOGW(TAG, "USB stream not started");
        return ESP_ERR_INVALID_STATE;
    }
    UVC_CHECK(s_usb_dev.enabled[stream], "stream not configured/enabled", ESP_ERR_INVALID_STATE);
    xSemaphoreTake(s_usb_dev.ctrl_smp_hdl, portMAX_DELAY);
    esp_err_t ret = ESP_OK;
    switch (ctrl_type) {
    case CTRL_SUSPEND:
    case CTRL_RESUME:
        ret = usb_stream_control(stream, ctrl_type);
        break;
    case CTRL_UAC_MUTE:
    case CTRL_UAC_VOLUME:
        ret = uac_feature_control(stream, ctrl_type, ctrl_value);
        break;
    default:
        break;
    }
    xSemaphoreGive(s_usb_dev.ctrl_smp_hdl);
    //Add delay to avoid too frequent control
    vTaskDelay(pdMS_TO_TICKS(WAITING_DEVICE_CONTROL_APPLY_MS));
    return ret;
}

esp_err_t uac_spk_streaming_write(void *data, size_t data_bytes, size_t timeout_ms)
{
    UVC_CHECK(data, "data is NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(data_bytes, "data_bytes is 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(s_usb_dev.enabled[STREAM_UAC_SPK], "spk stream not config", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_usb_dev.uac->ringbuf_hdl[UAC_SPK] != NULL, "spk ringbuf not created", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_usb_dev.uac->as_ifc[UAC_SPK] && !s_usb_dev.uac->as_ifc[UAC_SPK]->not_found, "spk interface not found", ESP_ERR_NOT_FOUND);

    size_t remind_timeout = timeout_ms;
    bool check_valid = false;
    esp_err_t ret = ESP_OK;
    do {
        if (xEventGroupGetBits(s_usb_dev.event_group_hdl) & UAC_SPK_STREAM_RUNNING) {
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
        ESP_LOGD(TAG, "write timeout: spk stream not ready");
        return ESP_ERR_TIMEOUT;
    }
    ret = _ring_buffer_push(s_usb_dev.uac->ringbuf_hdl[UAC_SPK], data, data_bytes, remind_timeout);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "write timeout: spk ringbuf full");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t uac_mic_streaming_read(void *buf, size_t buf_size, size_t *data_bytes, size_t timeout_ms)
{
    UVC_CHECK(buf, "buf is NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(buf_size, "buf_size is 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(data_bytes, "data_bytes is NULL", ESP_ERR_INVALID_ARG);
    UVC_CHECK(s_usb_dev.enabled[STREAM_UAC_MIC], "mic stream not config", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_usb_dev.uac->ringbuf_hdl[UAC_MIC] != NULL, "mic ringbuf not created", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_usb_dev.uac->as_ifc[UAC_MIC] && !s_usb_dev.uac->as_ifc[UAC_MIC]->not_found, "mic interface not found", ESP_ERR_NOT_FOUND);

    size_t remind_timeout = timeout_ms;
    bool check_valid = false;
    esp_err_t ret = ESP_OK;
    do {
        if (xEventGroupGetBits(s_usb_dev.event_group_hdl) & UAC_MIC_STREAM_RUNNING) {
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
        return ESP_ERR_TIMEOUT;
    }
    ret = _ring_buffer_pop(s_usb_dev.uac->ringbuf_hdl[UAC_MIC], buf, buf_size, data_bytes, remind_timeout);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read timeout: mic ringbuf empty");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t uac_frame_size_list_get(usb_stream_t stream, uac_frame_size_t *frame_list, size_t *list_size, size_t *cur_index)
{
    UVC_CHECK(stream == STREAM_UAC_SPK || stream == STREAM_UAC_MIC, "Invalid stream", ESP_ERR_INVALID_ARG);
    UVC_CHECK(s_usb_dev.enabled[stream], "uac stream not config", ESP_ERR_INVALID_STATE);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    if (!(s_usb_dev.ifc[stream] && !s_usb_dev.ifc[stream]->not_found)) {
        if (list_size) {
            *list_size = 0;
        }
        return ESP_ERR_INVALID_STATE;
    }
    _uac_internal_stream_t uac_stream = (stream == STREAM_UAC_SPK) ? UAC_SPK : UAC_MIC;
    UVC_ENTER_CRITICAL();
    uint8_t frame_num = s_usb_dev.uac->frame_num[uac_stream];
    if (list_size) {
        *list_size = frame_num;
    }
    if (frame_list && frame_num) {
        memcpy(&frame_list[0], s_usb_dev.uac->frame_size[uac_stream], sizeof(uac_frame_size_t) * frame_num);
    }
    if (cur_index) {
        *cur_index = s_usb_dev.uac->frame_index[uac_stream] - 1;
    }
    UVC_EXIT_CRITICAL();
    return ESP_OK;
}

esp_err_t uac_frame_size_reset(usb_stream_t stream, uint8_t ch_num, uint16_t bit_resolution, uint32_t samples_frequence)
{
    UVC_CHECK(stream == STREAM_UAC_SPK || stream == STREAM_UAC_MIC, "Invalid stream", ESP_ERR_INVALID_ARG);
    UVC_CHECK(ch_num != 0, "ch_num can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(bit_resolution != 0, "bit_resolution can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(samples_frequence != 0, "samples_frequence can't 0", ESP_ERR_INVALID_ARG);
    UVC_CHECK(s_usb_dev.enabled[stream], "uac stream not config", ESP_ERR_INVALID_STATE);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_usb_dev.ifc[stream] && !s_usb_dev.ifc[stream]->not_found, "uac interface not found", ESP_ERR_INVALID_STATE);
    if ((xEventGroupGetBits(s_usb_dev.event_group_hdl) & s_usb_dev.ifc[stream]->evt_bit)) {
        ESP_LOGE(TAG, "%s stream running, please suspend before frame_size_reset", s_usb_dev.ifc[stream]->name);
        return ESP_ERR_INVALID_STATE;
    }
    _uac_internal_stream_t uac_stream = (stream == STREAM_UAC_SPK) ? UAC_SPK : UAC_MIC;
    bool frame_found = false;
    uint8_t frame_index = 0;
    UVC_ENTER_CRITICAL();
    size_t frame_num = s_usb_dev.uac->frame_num[uac_stream];
    uac_frame_size_t *frame_size = s_usb_dev.uac->frame_size[uac_stream];
    for (size_t i = 0; i < frame_num; i++) {
        if (frame_size[i].ch_num == ch_num && frame_size[i].bit_resolution == bit_resolution
                && (frame_size[i].samples_frequence == samples_frequence || (samples_frequence >= frame_size[i].samples_frequence_min
                                                                             && samples_frequence <= frame_size[i].samples_frequence_max))) {
            frame_index = i + 1;
            frame_found = true;
            break;
        }
    }
    if (frame_found && s_usb_dev.uac->ch_num[uac_stream] == ch_num
            && s_usb_dev.uac->bit_resolution[uac_stream] == bit_resolution
            && s_usb_dev.uac->samples_frequence[uac_stream] == samples_frequence) {
        UVC_EXIT_CRITICAL();
        ESP_LOGW(TAG, "audio frame size not changed");
        return ESP_OK;
    }
    UVC_EXIT_CRITICAL();

    if (frame_found) {
        //Add a delay to avoid the situation that the device is not ready
        vTaskDelay(pdMS_TO_TICKS(ACTIVE_DEBOUNCE_TIME_MS));
        UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
        if (uac_stream == UAC_MIC) {
            UVC_ENTER_CRITICAL();
            s_usb_dev.uac->frame_index[UAC_MIC] = frame_index;
            //for mic support a range of samples_frequence
            s_usb_dev.uac->frame_size[UAC_MIC][frame_index - 1].samples_frequence = samples_frequence;
            s_usb_dev.uac->samples_frequence[UAC_MIC] = samples_frequence;
            UVC_EXIT_CRITICAL();
            //change users configuration
            s_usb_dev.uac_cfg.mic_ch_num = ch_num;
            s_usb_dev.uac_cfg.mic_bit_resolution = bit_resolution;
            s_usb_dev.uac_cfg.mic_samples_frequence = samples_frequence;
        } else {
            UVC_ENTER_CRITICAL();
            s_usb_dev.uac->frame_index[UAC_SPK] = frame_index;
            //for spk support a range of samples_frequence
            s_usb_dev.uac->frame_size[UAC_SPK][frame_index - 1].samples_frequence = samples_frequence;
            s_usb_dev.uac->samples_frequence[UAC_SPK] = samples_frequence;
            UVC_EXIT_CRITICAL();
            //change users configuration
            s_usb_dev.uac_cfg.spk_ch_num = ch_num;
            s_usb_dev.uac_cfg.spk_bit_resolution = bit_resolution;
            s_usb_dev.uac_cfg.spk_samples_frequence = samples_frequence;
        }
    } else {
        ESP_LOGE(TAG, "%s frame size not found, ch_num = %u, bit_resolution = %u, samples_frequence = %"PRIu32, s_usb_dev.ifc[stream]->name, ch_num, bit_resolution, samples_frequence);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "%s frame size change, ch_num = %u, bit_resolution = %u, samples_frequence = %"PRIu32, s_usb_dev.ifc[stream]->name, ch_num, bit_resolution, samples_frequence);
    return ESP_OK;
}

esp_err_t uvc_frame_size_list_get(uvc_frame_size_t *frame_list, size_t *list_size, size_t *cur_index)
{
    UVC_CHECK(s_usb_dev.enabled[STREAM_UVC], "uvc stream not config", ESP_ERR_INVALID_STATE);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    if (!(s_usb_dev.uvc->vs_ifc && !s_usb_dev.uvc->vs_ifc->not_found)) {
        if (list_size) {
            *list_size = 0;
        }
        return ESP_ERR_INVALID_STATE;
    }
    UVC_ENTER_CRITICAL();
    uint8_t frame_num = s_usb_dev.uvc->frame_num;
    if (list_size) {
        *list_size = frame_num;
    }
    if (frame_list && frame_num) {
        memcpy(&frame_list[0], s_usb_dev.uvc->frame_size, sizeof(uvc_frame_size_t) * frame_num);
    }
    if (cur_index) {
        *cur_index = frame_num == 1 ? 0 : (s_usb_dev.uvc->frame_index - 1);
    }
    UVC_EXIT_CRITICAL();
    return ESP_OK;
}

esp_err_t uvc_frame_size_reset(uint16_t frame_width, uint16_t frame_height, uint32_t frame_interval)
{
    UVC_CHECK(s_usb_dev.enabled[STREAM_UVC], "uvc stream not config", ESP_ERR_INVALID_STATE);
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_usb_dev.uvc->vs_ifc && !s_usb_dev.uvc->vs_ifc->not_found, "uvc interface not found", ESP_ERR_INVALID_STATE);
    UVC_CHECK(s_usb_dev.uvc->frame_size != NULL, "uvc frame size list not found", ESP_ERR_INVALID_STATE);
    //Add a delay to avoid the situation that the device is not ready
    vTaskDelay(pdMS_TO_TICKS(ACTIVE_DEBOUNCE_TIME_MS));
    UVC_CHECK(_usb_device_get_state() == STATE_DEVICE_ACTIVE, "USB Device not active", ESP_ERR_INVALID_STATE);
    if ((xEventGroupGetBits(s_usb_dev.event_group_hdl) & s_usb_dev.ifc[STREAM_UVC]->evt_bit)) {
        ESP_LOGE(TAG, "%s stream running, please suspend before frame_size_reset", s_usb_dev.ifc[STREAM_UVC]->name);
        return ESP_ERR_INVALID_STATE;
    }
    int frame_found = -1;
    if (frame_width && frame_height) {
        bool frame_reset = false;
        UVC_ENTER_CRITICAL();
        size_t frame_num = s_usb_dev.uvc->frame_num;
        uvc_frame_size_t *frame_size = s_usb_dev.uvc->frame_size;
        for (int i = 0; i < frame_num; i++) {
            if ((frame_width == FRAME_RESOLUTION_ANY || frame_width == frame_size[i].width)
                    && (frame_height == FRAME_RESOLUTION_ANY || frame_height == frame_size[i].height)) {
                if (i + 1 != s_usb_dev.uvc->frame_index) {
                    //change current configuration
                    s_usb_dev.uvc->frame_index = i + 1;
                    s_usb_dev.uvc->frame_height = frame_size[i].height;
                    s_usb_dev.uvc->frame_width = frame_size[i].width;
                    //change user configuration
                    s_usb_dev.uvc_cfg.frame_height = frame_size[i].height;
                    s_usb_dev.uvc_cfg.frame_width = frame_size[i].width;
                    frame_reset = true;
                }
                frame_found = i;
                break;
            }
        }
        UVC_EXIT_CRITICAL();
        if (frame_found == -1) {
            ESP_LOGE(TAG, "frame size not found, width = %d, height = %d", frame_width, frame_height);
            return ESP_ERR_NOT_FOUND;
        } else if (frame_reset == false) {
            ESP_LOGW(TAG, "frame size not changed, width = %d, height = %d", frame_width, frame_height);
            return ESP_OK;
        }
    }
    uint32_t final_interval = frame_interval;
    if (frame_interval) {
        if (frame_found != -1) {
            final_interval = 0;
            UVC_ENTER_CRITICAL();
            uvc_frame_size_t *frame_size = s_usb_dev.uvc->frame_size;
            if (frame_size[frame_found].interval_step) {
                // continues interval
                if (frame_interval >= frame_size[frame_found].interval_min && frame_interval <= frame_size[frame_found].interval_max) {
                    for (uint32_t i = frame_size[frame_found].interval_min; i < frame_size[frame_found].interval_max; i += frame_size[frame_found].interval_step) {
                        if (frame_interval >= i && frame_interval < (i + frame_size[frame_found].interval_step)) {
                            final_interval = i;
                        }
                    }
                }
            } else {
                // fixed interval
                if (frame_interval == frame_size[frame_found].interval_min) {
                    final_interval = frame_size[frame_found].interval_min;
                } else if (frame_interval == frame_size[frame_found].interval_max) {
                    final_interval = frame_size[frame_found].interval_max;
                } else if (frame_interval == frame_size[frame_found].interval) {
                    final_interval = frame_size[frame_found].interval;
                }
            }
            UVC_EXIT_CRITICAL();
            if (final_interval == 0) {
                UVC_ENTER_CRITICAL();
                final_interval = frame_size[frame_found].interval;
                UVC_EXIT_CRITICAL();
                ESP_LOGW(TAG, "frame interval %" PRIu32 " not support, using = %" PRIu32, frame_interval, final_interval);
            }
        } else {
            ESP_LOGW(TAG, "frame interval force to %" PRIu32, final_interval);
        }
        // else User should make sure the frame_interval is between frame_interval_min and frame_interval_max
        UVC_ENTER_CRITICAL();
        s_usb_dev.uvc->frame_interval = final_interval;
        UVC_EXIT_CRITICAL();
        //change user configuration
        s_usb_dev.uvc_cfg.frame_interval = final_interval;
    }
    ESP_LOGI(TAG, "UVC frame size reset, width = %d, height = %d, interval = %"PRIu32, frame_width, frame_height, final_interval);
    return ESP_OK;
}
