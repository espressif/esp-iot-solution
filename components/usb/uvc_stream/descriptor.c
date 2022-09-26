// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
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
// limitations under the License

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"

typedef enum {
    CS_INTERFACE_DESC = 0x24,
    CS_ENDPOINT_DESC = 0x25,
} descriptor_types_t;

typedef enum {
    SC_VIDEOCONTROL = 1,
    SC_VIDEOSTREAMING = 2,
} interface_sub_class_t;

static interface_sub_class_t interface_sub_class = SC_VIDEOCONTROL;

typedef enum {
    VC_HEADER = 0x01,
    VC_INPUT_TERMINAL = 0x02,
    VC_OUTPUT_TERMINAL = 0x03,
    VC_SELECTOR_UNIT = 0x04,
    VC_PROCESSING_UNIT = 0x05,
    VS_FORMAT_MJPEG = 0x06,
    VS_FRAME_MJPEG = 0x07,
    VS_STILL_FRAME = 0x03,
    VS_COLORFORMAT = 0x0D,
} descriptor_subtypes_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
} desc_header_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} USB_DESC_ATTR ifc_assoc_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint16_t bcdUVC;
    uint16_t wTotalLength;
    uint32_t dwClockFrequency;
    uint8_t  bFunctionProtocol;
    uint8_t  bInCollection;
    uint8_t  baInterfaceNr;
} USB_DESC_ATTR vc_interface_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bNumFormats;
    uint16_t wTotalLength;
    uint8_t  bEndpointAddress;
    uint8_t  bFunctionProtocol;
    uint8_t  bmInfo;
    uint8_t  bTerminalLink;
    uint8_t  bStillCaptureMethod;
    uint8_t  bTriggerSupport;
    uint8_t  bTriggerUsage;
    uint8_t  bControlSize;
    uint8_t  bmaControls;
} USB_DESC_ATTR vs_interface_desc_t;


typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint16_t wMaxTransferSize;
} USB_DESC_ATTR class_specific_endpoint_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bFormatIndex;
    uint8_t  bNumFrameDescriptors;
    uint8_t  bmFlags;
    uint8_t  bDefaultFrameIndex;
    uint8_t  bAspectRatioX;
    uint8_t  bAspectRatioY;
    uint8_t  bmInterlaceFlags;
    uint8_t  bCopyProtect;
} USB_DESC_ATTR vs_format_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bFrameIndex;
    uint8_t  bmCapabilities;
    uint16_t wWidth;
    uint16_t wHeigh;
    uint32_t dwMinBitRate;
    uint32_t dwMaxBitRate;
    uint32_t dwMaxVideoFrameBufSize;
    uint32_t dwDefaultFrameInterval;
    uint8_t  bFrameIntervalType;
    union {
        uint32_t dwFrameInterval;
        struct {
            uint32_t dwMinFrameInterval;
            uint32_t dwMaxFrameInterval;
            uint32_t dwFrameIntervalStep;
        };
    };
} USB_DESC_ATTR vs_frame_desc_t;


static void print_class_header_desc(const uint8_t *buff)
{
    if (interface_sub_class == SC_VIDEOCONTROL) {
        const vc_interface_desc_t *desc = (const vc_interface_desc_t *) buff;
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
        printf("\t*** Class-specific VC Interface Descriptor ***\n");
        printf("\tbLength 0x%x\n", desc->bLength);
        printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
        printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
#endif
        printf("\tbcdUVC %x\n", desc->bcdUVC);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
        printf("\twTotalLength %u\n", desc->wTotalLength);
        printf("\tdwClockFrequency %u\n", desc->dwClockFrequency);
        printf("\tbFunctionProtocol %u\n", desc->bFunctionProtocol);
        printf("\tbInCollection %u\n", desc->bInCollection);
        printf("\tbaInterfaceNr %u\n", desc->baInterfaceNr);
#endif
    } else if (interface_sub_class == SC_VIDEOSTREAMING) {
        const vs_interface_desc_t *desc = (const vs_interface_desc_t *) buff;
        printf("\t*** Class-specific VS Interface Descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
        printf("\tbLength 0x%x\n", desc->bLength);
        printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
        printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
#endif
        printf("\tbNumFormats %x\n", desc->bNumFormats);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
        printf("\twTotalLength %u\n", desc->wTotalLength);
        printf("\tbEndpointAddress %u\n", desc->bEndpointAddress);
        printf("\tbFunctionProtocol %u\n", desc->bFunctionProtocol);
        printf("\tbmInfo 0x%x\n", desc->bmInfo);
        printf("\tbTerminalLink %u\n", desc->bTerminalLink);
        printf("\tbStillCaptureMethod %u\n", desc->bStillCaptureMethod);
        printf("\tbTriggerSupport %u\n", desc->bTriggerSupport);
        printf("\tbTriggerUsage %u\n", desc->bTriggerUsage);
        printf("\tbControlSize %u\n", desc->bControlSize);
        printf("\tbmaControls 0x%x\n", desc->bmaControls);
#endif
    }
}

static void print_vs_format_mjpeg_desc(const uint8_t *buff)
{
    const vs_format_desc_t *desc = (const vs_format_desc_t *) buff;
    printf("\t*** VS Format MJPEG Descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType 0x%x\n", desc->bDescriptorSubType);
#endif
    printf("\tbFormatIndex 0x%x\n", desc->bFormatIndex);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbNumFrameDescriptors %u\n", desc->bNumFrameDescriptors);
    printf("\tbmFlags 0x%x\n", desc->bmFlags);
#endif
    printf("\tbDefaultFrameIndex %u\n", desc->bDefaultFrameIndex);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbAspectRatioX %u\n", desc->bAspectRatioX);
    printf("\tbAspectRatioY %u\n", desc->bAspectRatioY);
    printf("\tbmInterlaceFlags 0x%x\n", desc->bmInterlaceFlags);
    printf("\tbCopyProtect %u\n", desc->bCopyProtect);
#endif
}

static void print_vs_frame_mjpeg_desc(const uint8_t *buff)
{
    // Copy to local buffer due to potential misalignment issues.
    uint32_t raw_desc[25];
    uint32_t desc_size = ((const vs_frame_desc_t *)buff)->bLength;
    memcpy(raw_desc, buff, desc_size);

    const vs_frame_desc_t *desc = (const vs_frame_desc_t *) raw_desc;
    printf("\t*** VS MJPEG Frame Descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType 0x%x\n", desc->bDescriptorSubType);
#endif
    printf("\tbFrameIndex 0x%x\n", desc->bFrameIndex);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbmCapabilities 0x%x\n", desc->bmCapabilities);
#endif
    printf("\twWidth %u\n", desc->wWidth);
    printf("\twHeigh %u\n", desc->wHeigh);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tdwMinBitRate %lu\n", desc->dwMinBitRate);
    printf("\tdwMaxBitRate %lu\n", desc->dwMaxBitRate);
#endif
    printf("\tdwMaxVideoFrameBufSize %lu\n", desc->dwMaxVideoFrameBufSize);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tdwDefaultFrameInterval %lu\n", desc->dwDefaultFrameInterval);
    printf("\tbFrameIntervalType %lu\n", desc->bFrameIntervalType);
#endif

    if (desc->bFrameIntervalType == 0) {
        // Continuous Frame Intervals
        printf("\tdwMinFrameInterval %lu\n",  desc->dwMinFrameInterval);
        printf("\tdwMaxFrameInterval %lu\n",  desc->dwMaxFrameInterval);
        printf("\tdwFrameIntervalStep %lu\n", desc->dwFrameIntervalStep);
    } else {
        // Discrete Frame Intervals
        size_t num_of_intervals = (desc->bLength - 26) / 4;
        uint32_t *interval = (uint32_t *)&desc->dwFrameInterval;
        for (int i = 0; i < num_of_intervals; ++i) {
            printf("\tFrameInterval[%d] %lu\n", i, interval[i]);
        }
    }
}


static void print_class_specific_desc(const uint8_t *buff)
{
    desc_header_t *header = (desc_header_t *)buff;

    switch (header->bDescriptorSubtype) {
    case VC_HEADER:
        print_class_header_desc(buff);
        break;
    case VS_FORMAT_MJPEG:
        if (interface_sub_class == SC_VIDEOCONTROL) {
            printf("\t*** Extension Unit Descriptor unsupported, skipping... ***\n");;
            interface_sub_class = SC_VIDEOSTREAMING;
            return;
        }
        print_vs_format_mjpeg_desc(buff);
        interface_sub_class = SC_VIDEOCONTROL;
        break;
    case VS_FRAME_MJPEG:
        print_vs_frame_mjpeg_desc(buff);
        break;
    default:
        break;
    }
}

void _print_uvc_class_descriptors_cb(const usb_standard_desc_t *desc)
{
    const uint8_t *buff = (uint8_t *)desc;
    desc_header_t *header = (desc_header_t *)desc;

    switch (header->bDescriptorType) {
    case CS_INTERFACE_DESC:
        print_class_specific_desc(buff);
        break;
    default:
        break;
    }
}
