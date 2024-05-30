/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_err.h"
#include "usb/usb_types_stack.h"
#include "libuvc_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_RESOLUTION_ANY              __UINT16_MAX__        /*!< any uvc frame resolution */
#define UAC_FREQUENCY_ANY                 __UINT32_MAX__        /*!< any uac sample frequency */
#define UAC_BITS_ANY                      __UINT16_MAX__        /*!< any uac bit resolution */
#define UAC_CH_ANY                        0                     /*!< any uac channel number */
#define FPS2INTERVAL(fps)                 (10000000ul / fps)    /*!< convert fps to uvc frame interval */
#define FRAME_INTERVAL_FPS_5              FPS2INTERVAL(5)       /*!< 5 fps */
#define FRAME_INTERVAL_FPS_10             FPS2INTERVAL(10)      /*!< 10 fps */
#define FRAME_INTERVAL_FPS_15             FPS2INTERVAL(15)      /*!< 15 fps */
#define FRAME_INTERVAL_FPS_20             FPS2INTERVAL(20)      /*!< 20 fps */
#define FRAME_INTERVAL_FPS_30             FPS2INTERVAL(25)      /*!< 25 fps */
#define FLAG_UVC_SUSPEND_AFTER_START      (1 << 0)              /*!< suspend uvc after usb_streaming_start */
#define FLAG_UAC_SPK_SUSPEND_AFTER_START  (1 << 1)              /*!< suspend uac speaker after usb_streaming_start */
#define FLAG_UAC_MIC_SUSPEND_AFTER_START  (1 << 2)              /*!< suspend uac microphone after usb_streaming_start */

/**
 * @brief UVC stream usb transfer type, most camera using isochronous mode,
 * bulk mode can also be support for higher bandwidth
 */
typedef enum {
    UVC_XFER_ISOC = 0,  /*!< Isochronous Transfer Mode */
    UVC_XFER_BULK,      /*!< Bulk Transfer Mode */
    UVC_XFER_UNKNOWN,   /*!< Unknown Mode */
} uvc_xfer_t;

/**
 * @brief UVC stream format type, default using MJPEG format,
 */
/** @cond **/
typedef enum {
    UVC_FORMAT_MJPEG = 0,    /*!< Default MJPEG format */
    UVC_FORMAT_FRAME_BASED,  /*!< Frame-based format */
    UVC_FORMAT_MAX,         /*!< Unknown format */
} uvc_format_t;
/** @endcond **/

/**
 * @brief Stream id, used for control
 *
 */
typedef enum {
    STREAM_UVC = 0,     /*!< usb video stream */
    STREAM_UAC_SPK,     /*!< usb audio speaker stream */
    STREAM_UAC_MIC,     /*!< usb audio microphone stream */
    STREAM_MAX,         /*!< max stream id */
} usb_stream_t;

/**
 * @brief USB device connection status
 *
 */
typedef enum {
    STREAM_CONNECTED = 0,
    STREAM_DISCONNECTED,
} usb_stream_state_t;

/**
 * @brief Stream control type, which also depends on if device support
 *
 */
typedef enum {
    CTRL_NONE = 0,     /*!< None */
    CTRL_SUSPEND,      /*!< streaming suspend control. ctrl_data NULL */
    CTRL_RESUME,       /*!< streaming resume control. ctrl_data NULL */
    CTRL_UAC_MUTE,     /*!< mute control. ctrl_data (false/true) */
    CTRL_UAC_VOLUME,   /*!< volume control. ctrl_data (0~100) */
    CTRL_MAX,          /*!< max type value */
} stream_ctrl_t;

/**
 * @brief UVC configurations, for params with (optional) label, users do not need to specify manually,
 * unless there is a problem with descriptors, or users want to skip the get and process descriptors steps
 */
typedef struct uvc_config {
    uint16_t frame_width;           /*!< Picture width, set FRAME_RESOLUTION_ANY for any resolution */
    uint16_t frame_height;          /*!< Picture height, set FRAME_RESOLUTION_ANY for any resolution */
    uint32_t frame_interval;        /*!< Frame interval in 100-ns units, 666666 ~ 15 Fps*/
    uint32_t xfer_buffer_size;      /*!< Transfer buffer size, using double buffer here, must larger than one frame size */
    uint8_t *xfer_buffer_a;         /*!< Buffer a for usb payload */
    uint8_t *xfer_buffer_b;         /*!< Buffer b for usb payload */
    uint32_t frame_buffer_size;     /*!< Frame buffer size, must larger than one frame size */
    uint8_t *frame_buffer;          /*!< Buffer for one frame */
    uvc_frame_callback_t frame_cb;  /*!< callback function to handle incoming frame */
    void *frame_cb_arg;             /*!< callback function arg */
    uvc_format_t format;            /*!< (optional) UVC stream format, default using MJPEG */
    /*!< Optional configs, Users need to specify parameters manually when they want to
    skip the get and process descriptors steps (used to speed up startup)*/
    uvc_xfer_t xfer_type;           /*!< (optional) UVC stream transfer type, UVC_XFER_ISOC or UVC_XFER_BULK */
    uint8_t format_index;           /*!< (optional) Format index */
    uint8_t frame_index;            /*!< (optional) Frame index, to choose resolution */
    uint16_t interface;             /*!< (optional) UVC stream interface number */
    uint16_t interface_alt;         /*!< (optional) UVC stream alternate interface, to choose MPS (Max Packet Size), bulk fix to 0*/
    uint8_t ep_addr;                /*!< (optional) endpoint address of selected alternate interface*/
    uint32_t ep_mps;                /*!< (optional) MPS of selected interface_alt */
    uint32_t flags;                 /*!< (optional) flags to control the driver behavers */
} uvc_config_t;

/**
 * @brief mic frame type
 * */
typedef struct {
    void *data;                 /*!< mic data */
    uint32_t data_bytes;        /*!< mic data size */
    uint16_t bit_resolution;    /*!< bit resolution in buffer */
    uint32_t samples_frequence; /*!< mic sample frequency */
} mic_frame_t;

/**
 * @brief uvc frame type
 * */
typedef struct {
    uint16_t width;             /*!< frame width */
    uint16_t height;            /*!< frame height */
    uint32_t interval;          /*!< frame interval */
    uint32_t interval_min;      /*!< frame min interval */
    uint32_t interval_max;      /*!< frame max interval */
    uint32_t interval_step;     /*!< frame interval step */
} uvc_frame_size_t;

/**
 * @brief uac frame type
 * */
typedef struct {
    uint8_t ch_num;                 /*!< channel numbers */
    uint16_t bit_resolution;        /*!< bit resolution in buffer */
    uint32_t samples_frequence;     /*!< sample frequency */
    uint32_t samples_frequence_min; /*!< min sample frequency */
    uint32_t samples_frequence_max; /*!< max sample frequency */
} uac_frame_size_t;

/**
 * @brief user callback function to handle incoming mic frames
 *
 */
typedef void(*mic_callback_t)(mic_frame_t *frame, void *user_ptr);

/**
 * @brief user callback function to handle usb device connection status
 *
 */
typedef void(*state_callback_t)(usb_stream_state_t state, void *user_ptr);

/**
 * @brief UAC configurations, for params with (optional) label, users do not need to specify manually,
 * unless there is a problem with descriptor parse, or a problem with the device descriptor
 */
typedef struct {
    uint8_t spk_ch_num;                  /*!< speaker channel numbers, UAC_CH_ANY for any channel number  */
    uint8_t mic_ch_num;                  /*!< microphone channel numbers, UAC_CH_ANY for any channel number */
    uint16_t mic_bit_resolution;         /*!< microphone resolution(bits), UAC_BITS_ANY for any bit resolution */
    uint32_t mic_samples_frequence;      /*!< microphone frequency(Hz), UAC_FREQUENCY_ANY for any frequency */
    uint16_t spk_bit_resolution;         /*!< speaker resolution(bits), UAC_BITS_ANY for any */
    uint32_t spk_samples_frequence;      /*!< speaker frequency(Hz), UAC_FREQUENCY_ANY for any frequency */
    uint32_t spk_buf_size;               /*!< size of speaker send buffer, should be a multiple of spk_ep_mps */
    uint32_t mic_buf_size;               /*!< mic receive buffer size, 0 if not use */
    mic_callback_t mic_cb;               /*!< mic callback, can not block in here!, NULL if not use */
    void *mic_cb_arg;                    /*!< mic callback args, NULL if not use */
    /*!< Optional configs, Users need to specify parameters manually when they want to
    skip the get and process descriptors steps (used to speed up startup)*/
    uint16_t mic_interface;              /*!< (optional) microphone stream interface number, set 0 if not use */
    uint8_t  mic_ep_addr;                /*!< (optional) microphone interface endpoint address */
    uint32_t mic_ep_mps;                 /*!< (optional) microphone interface endpoint mps */
    uint16_t spk_interface;              /*!< (optional) speaker stream interface number, set 0 if not use */
    uint8_t  spk_ep_addr;                /*!< (optional) speaker interface endpoint address */
    uint32_t spk_ep_mps;                 /*!< (optional) speaker interface endpoint mps */
    uint16_t ac_interface;               /*!< (optional) audio control interface number, set 0 if not use */
    uint8_t  mic_fu_id;                  /*!< (optional) microphone feature unit id, set 0 if not use */
    uint8_t  spk_fu_id;                  /*!< (optional) speaker feature unit id, set 0 if not use */
    uint32_t flags;                      /*!< (optional) flags to control the driver behavers */
} uac_config_t;

/**
 * @brief Config UVC streaming with user defined parameters.For normal use, user only need to specify
 * no-optional parameters, and set optional parameters to 0 (the driver will find the correct value from the device descriptors).
 * For quick start mode, user should specify all parameters manually to skip get and process descriptors steps.
 *
 * @param config parameters defined in uvc_config_t
 * @return esp_err_t
 *      ESP_ERR_INVALID_STATE USB streaming is running, user need to stop streaming first
 *      ESP_ERR_INVALID_ARG Invalid argument
 *      ESP_OK Success
 */
esp_err_t uvc_streaming_config(const uvc_config_t *config);

/**
 * @brief Config UAC streaming with user defined parameters.For normal use, user only need to specify
 * no-optional parameters, and set optional parameters to 0 (the driver will find the correct value from the device descriptors).
 * For quick start mode, user should specify all parameters manually to skip get and process descriptors steps.
 *
 * @param config parameters defined in uvc_config_t
 * @return esp_err_t
 *      ESP_ERR_INVALID_STATE USB streaming is running, user need to stop streaming first
 *      ESP_ERR_INVALID_ARG Invalid argument
 *      ESP_OK Success
 */
esp_err_t uac_streaming_config(const uac_config_t *config);

/**
 * @brief Start usb streaming with pre-configs, usb driver will create internal tasks
 * to handle usb data from stream pipe, and run user's callback after new frame ready.
 *
 * @return
 *         ESP_ERR_INVALID_STATE streaming not configured, or streaming running already
 *         ESP_FAIL start failed
 *         ESP_OK start succeed
 */
esp_err_t usb_streaming_start(void);

/**
 * @brief Stop current usb streaming, internal tasks will be delete, related resource will be free
 *
 * @return
 *         ESP_ERR_INVALID_STATE streaming not started
 *         ESP_ERR_TIMEOUT stop wait timeout
 *         ESP_OK stop succeed
 */
esp_err_t usb_streaming_stop(void);

/**
 * @brief Wait for USB device connection
 *
 * @param timeout_ms timeout in ms
 * @return esp_err_t
 *      ESP_ERR_INVALID_STATE: usb streaming not started
 *      ESP_ERR_TIMEOUT: timeout
 *      ESP_OK: device connected
 */
esp_err_t usb_streaming_connect_wait(size_t timeout_ms);

/**
 * @brief This function registers a callback for USB streaming, please note that only one callback
 *  can be registered, the later registered callback will overwrite the previous one.
 *
 * @param cb A pointer to a function that will be called when the USB streaming state changes.
 * @param user_ptr user_ptr is a void pointer.
 *
 * @return esp_err_t
 *    - ESP_OK Success
 *    - ESP_ERR_INVALID_STATE USB streaming is running, callback need register before start
 */
esp_err_t usb_streaming_state_register(state_callback_t cb, void *user_ptr);

/**
 * @brief Control USB streaming with specific stream and control type
 * @param stream stream type defined in usb_stream_t
 * @param ctrl_type control type defined in stream_ctrl_t
 * @param ctrl_value control value
 * @return
 *         ESP_ERR_INVALID_ARG invalid arg
 *         ESP_ERR_INVALID_STATE driver not configured or not started
 *         ESP_ERR_NOT_SUPPORTED current device not support this control type
 *         ESP_FAIL control failed
 *         ESP_OK succeed
 */
esp_err_t usb_streaming_control(usb_stream_t stream, stream_ctrl_t ctrl_type, void *ctrl_value);

/**
 * @brief Write data to the speaker buffer, will be send out when USB device is ready
 *
 * @param data The data to be written.
 * @param data_bytes The size of the data to be written.
 * @param timeout_ms The timeout value for writing data to the buffer.
 *
 * @return
 *         ESP_ERR_INVALID_STATE spk stream not config
 *         ESP_ERR_NOT_FOUND spk interface not found
 *         ESP_ERR_TIMEOUT spk ringbuf full
 *         ESP_OK succeed
 */
esp_err_t uac_spk_streaming_write(void *data, size_t data_bytes, size_t timeout_ms);

/**
 * @brief Read data from internal mic buffer, the actual size will be returned
 *
 * @param buf pointer to the buffer to store the received data
 * @param buf_size The size of the data buffer.
 * @param data_bytes The actual size read from buffer
 * @param timeout_ms The timeout value for the read operation.
 *
 * @return
 *         ESP_ERR_INVALID_ARG parameter error
 *         ESP_ERR_INVALID_STATE mic stream not config
 *         ESP_ERR_NOT_FOUND mic interface not found
 *         ESP_TIMEOUT timeout
 *         ESP_OK succeed
 */
esp_err_t uac_mic_streaming_read(void *buf, size_t buf_size, size_t *data_bytes, size_t timeout_ms);

/**
 * @brief Get the audio frame size list of current stream, the list contains audio channel number, bit resolution and samples frequency.
 * IF list_size equals 1 and the samples_frequence equals 0, which means the frequency can be set to any value between samples_frequence_min
 * and samples_frequence_max.
 *
 *
 * @param stream the stream type
 * @param frame_list the output frame list, NULL to only get the list size
 * @param list_size frame list size
 * @param cur_index current frame index
 * @return esp_err_t
 *      - ESP_ERR_INVALID_ARG Parameter error
 *      - ESP_ERR_INVALID_STATE USB device not active
 *      - ESP_OK Success
 */
esp_err_t uac_frame_size_list_get(usb_stream_t stream, uac_frame_size_t *frame_list, size_t *list_size, size_t *cur_index);

/**
 * @brief Reset audio channel number, bit resolution and samples frequency, please reset when the streaming
 * in suspend state. The new configs will be effective after streaming resume.
 *
 * @param stream stream type
 * @param ch_num audio channel numbers
 * @param bit_resolution audio bit resolution
 * @param samples_frequence audio samples frequency
 * @return esp_err_t
 *       - ESP_ERR_INVALID_ARG Parameter error
 *       - ESP_ERR_INVALID_STATE USB device not active
 *       - ESP_ERR_NOT_FOUND frequency not found
 *       - ESP_OK Success
 *       - ESP_FAIL Reset failed
 */
esp_err_t uac_frame_size_reset(usb_stream_t stream, uint8_t ch_num, uint16_t bit_resolution, uint32_t samples_frequence);

/**
 * @brief Get the frame size list of current connected camera
 *
 * @param frame_list the frame size list, can be NULL if only want to get list size
 * @param list_size the list size
 * @param cur_index current frame index
 * @return esp_err_t
 *       ESP_ERR_INVALID_ARG parameter error
 *       ESP_ERR_INVALID_STATE uvc stream not config or not active
 *       ESP_OK succeed
 */
esp_err_t uvc_frame_size_list_get(uvc_frame_size_t *frame_list, size_t *list_size, size_t *cur_index);

/**
 * @brief Reset the expected frame size and frame interval, please reset when uvc streaming
 * in suspend state.The new configs will be effective after streaming resume.
 *
 * Note: frame_width and frame_height can be set to 0 at the same time, which means
 * no change on frame size.
 *
 * @param frame_width frame width, FRAME_RESOLUTION_ANY means any width
 * @param frame_height frame height, FRAME_RESOLUTION_ANY means any height
 * @param frame_interval frame interval, 0 means no change
 * @return esp_err_t
 */
esp_err_t uvc_frame_size_reset(uint16_t frame_width, uint16_t frame_height, uint32_t frame_interval);

#ifdef __cplusplus
}
#endif
