/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include "libuvc_def.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_types_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UAC_VERSION_1       0x0100
#define UAC_VERSION_2       0x0200
#define UAC_FORMAT_TYPEI    0x0001

typedef enum {
    CS_INTERFACE_DESC = 0x24,
    CS_ENDPOINT_DESC  = 0x25,
} descriptor_types_t;

typedef enum {
    VIDEO_SUBCLASS_UNDEFINED            = 0x00,
    VIDEO_SUBCLASS_CONTROL              = 0x01,
    VIDEO_SUBCLASS_STREAMING            = 0x02,
    VIDEO_SUBCLASS_INTERFACE_COLLECTION = 0x03,
} video_subclass_type_t;

typedef enum {
    AUDIO_SUBCLASS_UNDEFINED      = 0x00,
    AUDIO_SUBCLASS_CONTROL        = 0x01,
    AUDIO_SUBCLASS_STREAMING      = 0x02,
    AUDIO_SUBCLASS_MIDI_STREAMING = 0x03,
} audio_subclass_type_t;

typedef enum {
    AUDIO_FUNC_PROTOCOL_CODE_UNDEF       = 0x00,
    AUDIO_FUNC_PROTOCOL_CODE_V2          = 0x20,
} audio_function_protocol_code_t;

typedef enum {
    AUDIO_TERM_TYPE_USB_UNDEFINED       = 0x0100,
    AUDIO_TERM_TYPE_USB_STREAMING       = 0x0101,
    AUDIO_TERM_TYPE_USB_VENDOR_SPEC     = 0x01FF,
} audio_terminal_type_t;

typedef enum {
    AUDIO_TERM_TYPE_IN_UNDEFINED        = 0x0200,
    AUDIO_TERM_TYPE_IN_GENERIC_MIC      = 0x0201,
    AUDIO_TERM_TYPE_IN_DESKTOP_MIC      = 0x0202,
    AUDIO_TERM_TYPE_IN_PERSONAL_MIC     = 0x0203,
    AUDIO_TERM_TYPE_IN_OMNI_MIC         = 0x0204,
    AUDIO_TERM_TYPE_IN_ARRAY_MIC        = 0x0205,
    AUDIO_TERM_TYPE_IN_PROC_ARRAY_MIC   = 0x0206,
} audio_terminal_input_type_t;

typedef enum {
    AUDIO_TERM_TYPE_OUT_UNDEFINED               = 0x0300,
    AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER         = 0x0301,
    AUDIO_TERM_TYPE_OUT_HEADPHONES              = 0x0302,
    AUDIO_TERM_TYPE_OUT_HEAD_MNT_DISP_AUIDO     = 0x0303,
    AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER         = 0x0304,
    AUDIO_TERM_TYPE_OUT_ROOM_SPEAKER            = 0x0305,
    AUDIO_TERM_TYPE_OUT_COMMUNICATION_SPEAKER   = 0x0306,
    AUDIO_TERM_TYPE_OUT_LOW_FRQ_EFFECTS_SPEAKER = 0x0307,
    AUDIO_TERM_TYPE_HEADSET                     = 0x0402,
} audio_terminal_output_type_t;

typedef enum {
    AUDIO_FEATURE_CONTROL_MUTE              = 0x0001,
    AUDIO_FEATURE_CONTROL_VOLUME            = 0x0002,
    AUDIO_FEATURE_CONTROL_BASS              = 0x0004,
    AUDIO_FEATURE_CONTROL_MID               = 0x0008,
    AUDIO_FEATURE_CONTROL_TREBLE            = 0x0010,
    AUDIO_FEATURE_CONTROL_GRAPHIC_EQUALIZER = 0x0020,
    AUDIO_FEATURE_CONTROL_AUTOMATIC_GAIN    = 0x0040,
    AUDIO_FEATURE_CONTROL_DEALY             = 0x0080,
} audio_feature_unit_pos_t;

typedef enum {
    AUDIO_EP_CONTROL_UNDEF        = 0x00,
    AUDIO_EP_CONTROL_SAMPLING_FEQ = 0x01,
    AUDIO_EP_CONTROL_PITCH        = 0x02,
} audio_ep_ctrl_pos_t;

typedef enum {
    VIDEO_CS_ITF_VC_UNDEFINED       = 0x00,
    VIDEO_CS_ITF_VC_HEADER          = 0x01,
    VIDEO_CS_ITF_VC_INPUT_TERMINAL  = 0x02,
    VIDEO_CS_ITF_VC_OUTPUT_TERMINAL = 0x03,
    VIDEO_CS_ITF_VC_SELECTOR_UNIT   = 0x04,
    VIDEO_CS_ITF_VC_PROCESSING_UNIT = 0x05,
    VIDEO_CS_ITF_VC_EXTENSION_UNIT  = 0x06,
    VIDEO_CS_ITF_VC_ENCODING_UNIT   = 0x07,
    VIDEO_CS_ITF_VC_MAX,
} video_cs_vc_interface_subtype_t;

typedef enum {
    VIDEO_CS_ITF_VS_UNDEFINED             = 0x00,
    VIDEO_CS_ITF_VS_INPUT_HEADER          = 0x01,
    VIDEO_CS_ITF_VS_OUTPUT_HEADER         = 0x02,
    VIDEO_CS_ITF_VS_STILL_IMAGE_FRAME     = 0x03,
    VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED   = 0x04,
    VIDEO_CS_ITF_VS_FRAME_UNCOMPRESSED    = 0x05,
    VIDEO_CS_ITF_VS_FORMAT_MJPEG          = 0x06,
    VIDEO_CS_ITF_VS_FRAME_MJPEG           = 0x07,
    VIDEO_CS_ITF_VS_FORMAT_MPEG2TS        = 0x0A,
    VIDEO_CS_ITF_VS_FORMAT_DV             = 0x0C,
    VIDEO_CS_ITF_VS_COLORFORMAT           = 0x0D,
    VIDEO_CS_ITF_VS_FORMAT_FRAME_BASED    = 0x10,
    VIDEO_CS_ITF_VS_FRAME_FRAME_BASED     = 0x11,
    VIDEO_CS_ITF_VS_FORMAT_STREAM_BASED   = 0x12,
    VIDEO_CS_ITF_VS_FORMAT_H264           = 0x13,
    VIDEO_CS_ITF_VS_FRAME_H264            = 0x14,
    VIDEO_CS_ITF_VS_FORMAT_H264_SIMULCAST = 0x15,
    VIDEO_CS_ITF_VS_FORMAT_VP8            = 0x16,
    VIDEO_CS_ITF_VS_FRAME_VP8             = 0x17,
    VIDEO_CS_ITF_VS_FORMAT_VP8_SIMULCAST  = 0x18,
} video_cs_vs_interface_subtype_t;

typedef enum {
    AUDIO_CS_AC_INTERFACE_AC_DESCRIPTOR_UNDEF   = 0x00,
    AUDIO_CS_AC_INTERFACE_HEADER                = 0x01,
    AUDIO_CS_AC_INTERFACE_INPUT_TERMINAL        = 0x02,
    AUDIO_CS_AC_INTERFACE_OUTPUT_TERMINAL       = 0x03,
    AUDIO_CS_AC_INTERFACE_MIXER_UNIT            = 0x04,
    AUDIO_CS_AC_INTERFACE_SELECTOR_UNIT         = 0x05,
    AUDIO_CS_AC_INTERFACE_FEATURE_UNIT          = 0x06,
    AUDIO_CS_AC_INTERFACE_EFFECT_UNIT           = 0x07,
    AUDIO_CS_AC_INTERFACE_PROCESSING_UNIT       = 0x08,
    AUDIO_CS_AC_INTERFACE_EXTENSION_UNIT        = 0x09,
    AUDIO_CS_AC_INTERFACE_CLOCK_SOURCE          = 0x0A,
    AUDIO_CS_AC_INTERFACE_CLOCK_SELECTOR        = 0x0B,
    AUDIO_CS_AC_INTERFACE_CLOCK_MULTIPLIER      = 0x0C,
    AUDIO_CS_AC_INTERFACE_SAMPLE_RATE_CONVERTER = 0x0D,
} audio_cs_ac_interface_subtype_t;

typedef enum {
    AUDIO_CS_AS_INTERFACE_AS_DESCRIPTOR_UNDEF   = 0x00,
    AUDIO_CS_AS_INTERFACE_AS_GENERAL            = 0x01,
    AUDIO_CS_AS_INTERFACE_FORMAT_TYPE           = 0x02,
    AUDIO_CS_AS_INTERFACE_ENCODER               = 0x03,
    AUDIO_CS_AS_INTERFACE_DECODER               = 0x04,
} audio_cs_as_interface_subtype_t;

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
} USB_DESC_ATTR vs_format_mjpeg_desc_t;

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
} USB_DESC_ATTR vs_frame_mjpeg_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bFormatIndex;
    uint8_t  bNumFrameDescriptors;
    uint8_t  guidFormat[16];
    uint8_t  bBitsPerPixel;
    uint8_t  bDefaultFrameIndex;
    uint8_t  bAspectRatioX;
    uint8_t  bAspectRatioY;
    uint8_t  bmInterlaceFlags;
    uint8_t  bCopyProtect;
    uint8_t  bVariableSize;
} USB_DESC_ATTR vs_format_frame_based_desc_t;

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
    uint32_t dwDefaultFrameInterval;
    uint8_t  bFrameIntervalType;
    uint32_t dwBytesPerLine;
    union {
        uint32_t dwFrameInterval;
        struct {
            uint32_t dwMinFrameInterval;
            uint32_t dwMaxFrameInterval;
            uint32_t dwFrameIntervalStep;
        };
    };
} USB_DESC_ATTR vs_frame_frame_based_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdADC;
    uint16_t wTotalLength;
    uint8_t bInCollection;
    uint8_t baInterfaceNr[];
} USB_DESC_ATTR ac_interface_header_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bTerminalID;
    uint16_t wTerminalType;
    uint8_t bAssocTerminal;
    uint8_t bNrChannels;
    uint16_t wChannelConfig;
    uint8_t iChannelNames;
    uint8_t iTerminal;
} USB_DESC_ATTR ac_interface_input_terminal_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bTerminalID;
    uint16_t wTerminalType;
    uint8_t bAssocTerminal;
    uint8_t bSourceID;
    uint8_t iTerminal;
} USB_DESC_ATTR ac_interface_output_terminal_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bUnitID;
    uint8_t bSourceID;
    uint8_t bControlSize;
    uint8_t bmaControls[2];
    uint8_t iFeature;
} USB_DESC_ATTR ac_interface_feature_unit_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bTerminalLink;
    uint8_t bDelay;
    uint16_t wFormatTag;
} USB_DESC_ATTR as_interface_header_desc_t;

/// AUDIO Type I Format
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bFormatType;
    uint8_t bNrChannels;
    uint8_t bSubframeSize;
    uint8_t bBitResolution;
    uint8_t bSamFreqType;
    uint8_t tSamFreq[3];
} USB_DESC_ATTR as_interface_type_I_format_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bmAttributes;
    uint8_t bLockDelayUnits;
    uint16_t wLockDelay;
} USB_DESC_ATTR as_cs_ep_desc_t;

#ifdef SUPPORT_UAC_V2
/* UAC v2.0 is incompatible with UAC v1.0 */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint16_t bcdADC;
    uint8_t bCategory;
    uint16_t wTotalLength;
    uint8_t bmControls;
} USB_DESC_ATTR ac2_interface_header_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint16_t wTerminalType;
    uint8_t bAssocTerminal;
    uint8_t bCSourceID;
    uint8_t bNrChannels;
    uint32_t bmChannelConfig;
    uint16_t bmControls;
    uint8_t iTerminal;
} USB_DESC_ATTR ac2_interface_input_terminal_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bUnitID;
    uint8_t bSourceID;
    struct TU_ATTR_PACKED {
        uint32_t bmaControls;
    } controls[2];
} USB_DESC_ATTR ac2_interface_feature_unit_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bTerminalLink;
    uint8_t bmControls;
    uint8_t bFormatType;
    uint32_t bmFormats;
    uint8_t bNrChannels;
    uint32_t bmChannelConfig;
    uint8_t iChannelNames;
} USB_DESC_ATTR as2_interface_header_desc_t;

/// AUDIO Type I Format
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bFormatType;
    uint8_t bSubslotSize;
    uint8_t bBitResolution;
} USB_DESC_ATTR as2_interface_type_I_format_desc_t;
#endif

void parse_ac_header_desc(const uint8_t *buff, uint16_t *bcdADC, uint8_t *intf_num);
void parse_ac_input_desc(const uint8_t *buff, uint8_t *terminal_idx, uint16_t *terminal_type);
void parse_ac_output_desc(const uint8_t *buff, uint8_t *terminal_idx, uint16_t *terminal_type);
void parse_ac_feature_desc(const uint8_t *buff, uint8_t *source_idx, uint8_t *feature_unit_idx, uint8_t *volume_ch, uint8_t *mute_ch);
void parse_as_general_desc(const uint8_t *buff, uint8_t *source_idx, uint16_t *format_tag);
void parse_as_type_desc(const uint8_t *buff, uint8_t *channel_num, uint8_t *bit_resolution, uint8_t *freq_type, const uint8_t **pp_samfreq);

void print_cfg_desc(const uint8_t *buff);
void print_assoc_desc(const uint8_t *buff);
void print_intf_desc(const uint8_t *buff);
void parse_ep_desc(const uint8_t *buff, uint16_t *ep_mps, uint8_t *ep_addr, uint8_t *ep_attr);
void parse_vs_format_mjpeg_desc(const uint8_t *buff, uint8_t *format_idx, uint8_t *frame_num, enum uvc_frame_format *fmt);
void parse_vs_frame_mjpeg_desc(const uint8_t *buff, uint8_t *frame_idx, uint16_t *width, uint16_t *height, uint8_t *interval_type, const uint32_t **pp_interval, uint32_t *dflt_interval);
void parse_vs_format_frame_based_desc(const uint8_t *buff, uint8_t *format_idx, uint8_t *frame_num, enum uvc_frame_format *fmt);
void parse_vs_frame_frame_based_desc(const uint8_t *buff, uint8_t *frame_idx, uint16_t *width, uint16_t *height, uint8_t *interval_type, const uint32_t **pp_interval, uint32_t *dflt_interval);
void print_uvc_header_desc(const uint8_t *buff, uint8_t sub_class);
void print_device_descriptor(const uint8_t *buff);
void print_ep_desc(const uint8_t *buff);
#ifdef __cplusplus
}
#endif
