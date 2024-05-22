/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2023 Espressif
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for Video
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = CONFIG_TUSB_VID,
    .idProduct          = CONFIG_TUSB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#if CFG_TUD_CAM1_VIDEO_STREAMING_BULK

#if CONFIG_UVC_CAM1_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_MJPEG_BULK_LEN(4))
#elif CONFIG_FORMAT_H264_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_FRAME_BASED_BULK_LEN(4))
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN)
#elif CONFIG_FORMAT_H264_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_FRAME_BASED_BULK_LEN)
#else
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_UNCOMPR_BULK_LEN)
#endif
#endif // CONFIG_UVC_CAM1_MULTI_FRAMESIZE

#else

#if CONFIG_UVC_CAM1_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_MJPEG_LEN(4))
#elif CONFIG_FORMAT_H264_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_FRAME_BASED_LEN(4))
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MJPEG_LEN)
#elif CONFIG_FORMAT_H264_CAM1
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_FRAME_BASED_LEN)
#else
#define TUD_CAM1_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_UNCOMPR_LEN)
#endif
#endif // CONFIG_UVC_CAM1_MULTI_FRAMESIZE

#endif // CFG_TUD_CAM1_VIDEO_STREAMING_BULK

#if CONFIG_UVC_SUPPORT_TWO_CAM
#if CFG_TUD_CAM2_VIDEO_STREAMING_BULK

#if CONFIG_UVC_CAM2_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_MJPEG_BULK_LEN(4))
#elif CONFIG_FORMAT_H264_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_FRAME_BASED_BULK_LEN(4))
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MJPEG_BULK_LEN)
#elif CONFIG_FORMAT_H264_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_FRAME_BASED_BULK_LEN)
#else
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_UNCOMPR_BULK_LEN)
#endif
#endif // CONFIG_UVC_CAM2_MULTI_FRAMESIZE

#else

#if CONFIG_UVC_CAM2_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_MJPEG_LEN(4))
#elif CONFIG_FORMAT_H264_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MULTI_FRAME_BASED_LEN(4))
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_MJPEG_LEN)
#elif CONFIG_FORMAT_H264_CAM2
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_FRAME_BASED_LEN)
#else
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    (TUD_VIDEO_CAPTURE_DESC_UNCOMPR_LEN)
#endif
#endif // CONFIG_UVC_CAM2_MULTI_FRAMESIZE

#endif // CFG_TUD_CAM2_VIDEO_STREAMING_BULK
#else
#define TUD_CAM2_VIDEO_CAPTURE_DESC_LEN    0
#endif

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CAM1_VIDEO_CAPTURE_DESC_LEN + TUD_CAM2_VIDEO_CAPTURE_DESC_LEN)
#define EPNUM_CAM1_VIDEO_IN    0x81
#if CONFIG_UVC_SUPPORT_TWO_CAM
#define EPNUM_CAM2_VIDEO_IN    0x82
#endif

uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 500),
    // IAD for Video Control
#if CFG_TUD_CAM1_VIDEO_STREAMING_BULK
#if CONFIG_UVC_CAM1_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_MJPEG_BULK(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_H264_BULK(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG_BULK(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                            UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT, UVC_CAM1_FRAME_RATE,
                                            CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_H264_BULK(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                           UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT, UVC_CAM1_FRAME_RATE,
                                           CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#else
    TUD_VIDEO_CAPTURE_DESCRIPTOR_UNCOMPR_BULK(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                              UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT, UVC_CAM1_FRAME_RATE,
                                              CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#endif // CONFIG_UVC_CAM1_MULTI_FRAMESIZE
#else
#if CONFIG_UVC_CAM1_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_MJPEG(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_H264(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                       UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT, UVC_CAM1_FRAME_RATE,
                                       CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM1
    TUD_VIDEO_CAPTURE_DESCRIPTOR_H264(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                      UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT, UVC_CAM1_FRAME_RATE,
                                      CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#else
    TUD_VIDEO_CAPTURE_DESCRIPTOR_UNCOMPR(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM1_VIDEO_IN,
                                         UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT, UVC_CAM1_FRAME_RATE,
                                         CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#endif // CONFIG_UVC_CAM1_MULTI_FRAMESIZE
#endif // CFG_TUD_CAM1_VIDEO_STREAMING_BULK

#if CONFIG_UVC_SUPPORT_TWO_CAM
#if CFG_TUD_CAM2_VIDEO_STREAMING_BULK
#if CONFIG_UVC_CAM2_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_MJPEG_BULK(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN, CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_H264_BULK(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN, CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG_BULK(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN,
                                            UVC_CAM2_FRAME_WIDTH, UVC_CAM2_FRAME_HEIGHT, UVC_CAM2_FRAME_RATE,
                                            CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_H264_BULK(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN,
                                           UVC_CAM2_FRAME_WIDTH, UVC_CAM2_FRAME_HEIGHT, UVC_CAM2_FRAME_RATE,
                                           CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#else
    TUD_VIDEO_CAPTURE_DESCRIPTOR_UNCOMPR_BULK(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN,
                                              UVC_CAM2_FRAME_WIDTH, UVC_CAM2_FRAME_HEIGHT, UVC_CAM2_FRAME_RATE,
                                              CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#endif // CONFIG_UVC_CAM2_MULTI_FRAMESIZE
#else
#if CONFIG_UVC_CAM2_MULTI_FRAMESIZE
#if CONFIG_FORMAT_MJPEG_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_MJPEG(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN, CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MULTI_H264(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN, CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#else
#if CONFIG_FORMAT_MJPEG_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_MJPEG(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN,
                                       UVC_CAM2_FRAME_WIDTH, UVC_CAM2_FRAME_HEIGHT, UVC_CAM2_FRAME_RATE,
                                       CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#elif CONFIG_FORMAT_H264_CAM2
    TUD_VIDEO_CAPTURE_DESCRIPTOR_H264(4, ITF_NUM_VIDEO_CONTROL_2, EPNUM_CAM2_VIDEO_IN,
                                      UVC_CAM2_FRAME_WIDTH, UVC_CAM2_FRAME_HEIGHT, UVC_CAM2_FRAME_RATE,
                                      CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#else
    TUD_VIDEO_CAPTURE_DESCRIPTOR_UNCOMPR(4, ITF_NUM_VIDEO_CONTROL, EPNUM_CAM2_VIDEO_IN,
                                         UVC_CAM2_FRAME_WIDTH, UVC_CAM2_FRAME_HEIGHT, UVC_CAM2_FRAME_RATE,
                                         CFG_TUD_CAM2_VIDEO_STREAMING_EP_BUFSIZE),
#endif
#endif // CONFIG_UVC_CAM2_MULTI_FRAMESIZE
#endif // CFG_TUD_CAM2_VIDEO_STREAMING_BULK
#endif

};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations

    return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
    CONFIG_TUSB_MANUFACTURER,      // 1: Manufacturer
    CONFIG_TUSB_PRODUCT,           // 2: Product
    CONFIG_TUSB_SERIAL_NUM,        // 3: Serials, should use chip ID
    "UVC CAM1",               // 4: UVC Interface
#if CONFIG_UVC_SUPPORT_TWO_CAM
    "UVC CAM2",               // 5: UVC Interface
#endif
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }

        const char *str = string_desc_arr[index];

        // Cap at max char
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}
