/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/** Handle on an open UVC device.
 * only one device supported
 */
typedef void* uvc_device_handle_t;

/** UVC error types, based on libusb errors
 * @ingroup diag
 */
typedef enum uvc_error {
    /** Success (no error) */
    UVC_SUCCESS = 0,
    /** Input/output error */
    UVC_ERROR_IO = -1,
    /** Invalid parameter */
    UVC_ERROR_INVALID_PARAM = -2,
    /** Access denied */
    UVC_ERROR_ACCESS = -3,
    /** No such device */
    UVC_ERROR_NO_DEVICE = -4,
    /** Entity not found */
    UVC_ERROR_NOT_FOUND = -5,
    /** Resource busy */
    UVC_ERROR_BUSY = -6,
    /** Operation timed out */
    UVC_ERROR_TIMEOUT = -7,
    /** Overflow */
    UVC_ERROR_OVERFLOW = -8,
    /** Pipe error */
    UVC_ERROR_PIPE = -9,
    /** System call interrupted */
    UVC_ERROR_INTERRUPTED = -10,
    /** Insufficient memory */
    UVC_ERROR_NO_MEM = -11,
    /** Operation not supported */
    UVC_ERROR_NOT_SUPPORTED = -12,
    /** Device is not UVC-compliant */
    UVC_ERROR_INVALID_DEVICE = -50,
    /** Mode not supported */
    UVC_ERROR_INVALID_MODE = -51,
    /** Resource has a callback (can't use polling and async) */
    UVC_ERROR_CALLBACK_EXISTS = -52,
    /** Undefined error */
    UVC_ERROR_OTHER = -99
} uvc_error_t;

/** Color coding of stream, transport-independent
 * @ingroup streaming
 */
enum uvc_frame_format {
    UVC_FRAME_FORMAT_UNKNOWN = 0,
    /** Any supported format */
    UVC_FRAME_FORMAT_ANY = 0,
    UVC_FRAME_FORMAT_UNCOMPRESSED,
    UVC_FRAME_FORMAT_COMPRESSED,
    /** YUYV/YUV2/YUV422: YUV encoding with one luminance value per pixel and
     * one UV (chrominance) pair for every two pixels.
     */
    UVC_FRAME_FORMAT_YUYV,
    UVC_FRAME_FORMAT_UYVY,
    /** 24-bit RGB */
    UVC_FRAME_FORMAT_RGB,
    UVC_FRAME_FORMAT_BGR,
    /** Motion-JPEG (or JPEG) encoded images */
    UVC_FRAME_FORMAT_MJPEG,
    UVC_FRAME_FORMAT_H264,
    /** Greyscale images */
    UVC_FRAME_FORMAT_GRAY8,
    UVC_FRAME_FORMAT_GRAY16,
    /* Raw colour mosaic images */
    UVC_FRAME_FORMAT_BY8,
    UVC_FRAME_FORMAT_BA81,
    UVC_FRAME_FORMAT_SGRBG8,
    UVC_FRAME_FORMAT_SGBRG8,
    UVC_FRAME_FORMAT_SRGGB8,
    UVC_FRAME_FORMAT_SBGGR8,
    /** YUV420: NV12 */
    UVC_FRAME_FORMAT_NV12,
    /** YUV: P010 */
    UVC_FRAME_FORMAT_P010,
    /** Number of formats understood */
    UVC_FRAME_FORMAT_COUNT,
};

/** Converts an unaligned four-byte little-endian integer into an int32 */
#define DW_TO_INT(p) ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))
/** Converts an unaligned two-byte little-endian integer into an int16 */
#define SW_TO_SHORT(p) ((p)[0] | ((p)[1] << 8))
/** Converts an int16 into an unaligned two-byte little-endian integer */
#define SHORT_TO_SW(s, p) \
    (p)[0] = (s); \
    (p)[1] = (s) >> 8;
/** Converts an int32 into an unaligned four-byte little-endian integer */
#define INT_TO_DW(i, p) \
    (p)[0] = (i); \
    (p)[1] = (i) >> 8; \
    (p)[2] = (i) >> 16; \
    (p)[3] = (i) >> 24;

/** Streaming mode, includes all information needed to select stream
 * @ingroup streaming
 */
typedef struct uvc_stream_ctrl {
    uint16_t bmHint;
    uint8_t bFormatIndex;
    uint8_t bFrameIndex;
    uint32_t dwFrameInterval;
    uint16_t wKeyFrameRate;
    uint16_t wPFrameRate;
    uint16_t wCompQuality;
    uint16_t wCompWindowSize;
    uint16_t wDelay;
    uint32_t dwMaxVideoFrameSize;
    uint32_t dwMaxPayloadTransferSize;
    uint32_t dwClockFrequency;
    uint8_t bmFramingInfo;
    uint8_t bPreferredVersion;
    uint8_t bMinVersion;
    uint8_t bMaxVersion;
    uint8_t bInterfaceNumber;
} uvc_stream_ctrl_t;

/** UVC request code (A.8) */
enum uvc_req_code {
    UVC_RC_UNDEFINED = 0x00,
    UVC_SET_CUR = 0x01,
    UVC_GET_CUR = 0x81,
    UVC_GET_MIN = 0x82,
    UVC_GET_MAX = 0x83,
    UVC_GET_RES = 0x84,
    UVC_GET_LEN = 0x85,
    UVC_GET_INFO = 0x86,
    UVC_GET_DEF = 0x87
};

/** VideoStreaming interface control selector (A.9.7) */
enum uvc_vs_ctrl_selector {
    UVC_VS_CONTROL_UNDEFINED = 0x00,
    UVC_VS_PROBE_CONTROL = 0x01,
    UVC_VS_COMMIT_CONTROL = 0x02,
    UVC_VS_STILL_PROBE_CONTROL = 0x03,
    UVC_VS_STILL_COMMIT_CONTROL = 0x04,
    UVC_VS_STILL_IMAGE_TRIGGER_CONTROL = 0x05,
    UVC_VS_STREAM_ERROR_CODE_CONTROL = 0x06,
    UVC_VS_GENERATE_KEY_FRAME_CONTROL = 0x07,
    UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL = 0x08,
    UVC_VS_SYNC_DELAY_CONTROL = 0x09
};

/** An image frame received from the UVC device
 * @ingroup streaming
 */
typedef struct uvc_frame {
    /** Image data for this frame */
    void *data;
    /** Size of image data buffer */
    size_t data_bytes;
    /** Width of image in pixels */
    uint32_t width;
    /** Height of image in pixels */
    uint32_t height;
    /** Pixel data format */
    enum uvc_frame_format frame_format;
    /** Number of bytes per horizontal line (undefined for compressed format) */
    size_t step;
    /** Frame number (may skip, but is strictly monotonically increasing) */
    uint32_t sequence;
    /** Estimate of system time when the device started capturing the image */
    struct timeval capture_time;
    /** Estimate of system time when the device finished receiving the image */
    struct timespec capture_time_finished;
    /** Handle on the device that produced the image.
     * @warning You must not call any uvc_* functions during a callback. */
    uvc_device_handle_t *source;
    /** Is the data buffer owned by the library?
     * If 1, the data buffer can be arbitrarily reallocated by frame conversion
     * functions.
     * If 0, the data buffer will not be reallocated or freed by the library.
     * Set this field to zero if you are supplying the buffer.
     */
    uint8_t library_owns_data;
    /** Metadata for this frame if available */
    void *metadata;
    /** Size of metadata buffer */
    size_t metadata_bytes;
} uvc_frame_t;

/** A callback function to handle incoming assembled UVC frames
 * @ingroup streaming
 */
typedef void(*uvc_frame_callback_t)(struct uvc_frame *frame, void *user_ptr);
