/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "usb_stream_descriptor.h"

void print_device_descriptor(const uint8_t *buff)
{
    if (buff == NULL) {
        return;
    }
#ifdef CONFIG_UVC_PRINT_DESC
    const usb_device_desc_t *devc_desc = (const usb_device_desc_t *)buff;
    printf("*** Device descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("bLength %d\n", devc_desc->bLength);
    printf("bDescriptorType %d\n", devc_desc->bDescriptorType);
#endif
    printf("bcdUSB %d.%d0\n", ((devc_desc->bcdUSB >> 8) & 0xF), ((devc_desc->bcdUSB >> 4) & 0xF));
    printf("bDeviceClass 0x%x\n", devc_desc->bDeviceClass);
    printf("bDeviceSubClass 0x%x\n", devc_desc->bDeviceSubClass);
    printf("bDeviceProtocol 0x%x\n", devc_desc->bDeviceProtocol);
    printf("bMaxPacketSize0 %d\n", devc_desc->bMaxPacketSize0);
    printf("idVendor 0x%x\n", devc_desc->idVendor);
    printf("idProduct 0x%x\n", devc_desc->idProduct);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("bcdDevice %d.%d0\n", ((devc_desc->bcdDevice >> 8) & 0xF), ((devc_desc->bcdDevice >> 4) & 0xF));
    printf("iManufacturer %d\n", devc_desc->iManufacturer);
    printf("iProduct %d\n", devc_desc->iProduct);
    printf("iSerialNumber %d\n", devc_desc->iSerialNumber);
#endif
    printf("bNumConfigurations %d\n", devc_desc->bNumConfigurations);
#endif
}

void print_uvc_header_desc(const uint8_t *buff, uint8_t sub_class)
{
    if (buff == NULL) {
        return;
    }
#ifdef CONFIG_UVC_PRINT_DESC
    if (sub_class == VIDEO_SUBCLASS_CONTROL) {
        const vc_interface_desc_t *desc = (const vc_interface_desc_t *) buff;
        printf("\t*** Class-specific VC Interface Descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
        printf("\tbLength 0x%x\n", desc->bLength);
        printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
        printf("\tbDescriptorSubType %u\n", desc->bDescriptorSubType);
#endif
        printf("\tbcdUVC %x\n", desc->bcdUVC);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
        printf("\twTotalLength %u\n", desc->wTotalLength);
        printf("\tdwClockFrequency %"PRIu32"\n", desc->dwClockFrequency);
        printf("\tbFunctionProtocol %u\n", desc->bFunctionProtocol);
        printf("\tbInCollection %u\n", desc->bInCollection);
        printf("\tbaInterfaceNr %u\n", desc->baInterfaceNr);
#endif
    } else if (sub_class == VIDEO_SUBCLASS_STREAMING) {
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
#endif
}

void parse_vs_format_mjpeg_desc(const uint8_t *buff, uint8_t *format_idx, uint8_t *frame_num)
{
    if (buff == NULL) {
        return;
    }
    const vs_format_desc_t *desc = (const vs_format_desc_t *) buff;
#ifdef CONFIG_UVC_PRINT_DESC
    printf("\t*** VS Format MJPEG Descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength 0x%x\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubType 0x%x\n", desc->bDescriptorSubType);
#endif
    printf("\tbFormatIndex 0x%x\n", desc->bFormatIndex);
    printf("\tbNumFrameDescriptors %u\n", desc->bNumFrameDescriptors);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbmFlags 0x%x\n", desc->bmFlags);
#endif
    printf("\tbDefaultFrameIndex %u\n", desc->bDefaultFrameIndex);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbAspectRatioX %u\n", desc->bAspectRatioX);
    printf("\tbAspectRatioY %u\n", desc->bAspectRatioY);
    printf("\tbmInterlaceFlags 0x%x\n", desc->bmInterlaceFlags);
    printf("\tbCopyProtect %u\n", desc->bCopyProtect);
#endif
#endif
    if (format_idx) {
        *format_idx = desc->bFormatIndex;
    }
    if (frame_num) {
        *frame_num = desc->bNumFrameDescriptors;
    }
}

void parse_vs_frame_mjpeg_desc(const uint8_t *buff, uint8_t *frame_idx, uint16_t *width, uint16_t *heigh)
{
    if (buff == NULL) {
        return;
    }
    // Copy to local buffer due to potential misalignment issues.
    uint32_t raw_desc[25];
    uint32_t desc_size = ((const vs_frame_desc_t *)buff)->bLength;
    memcpy(raw_desc, buff, desc_size);

    const vs_frame_desc_t *desc = (const vs_frame_desc_t *) raw_desc;
#ifdef CONFIG_UVC_PRINT_DESC
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
    printf("\tdwMinBitRate %"PRIu32"\n", desc->dwMinBitRate);
    printf("\tdwMaxBitRate %"PRIu32"\n", desc->dwMaxBitRate);
    printf("\tdwMaxVideoFrameBufSize %"PRIu32"\n", desc->dwMaxVideoFrameBufSize);
    printf("\tdwDefaultFrameInterval %"PRIu32"\n", desc->dwDefaultFrameInterval);
    printf("\tbFrameIntervalType %u\n", desc->bFrameIntervalType);
#endif

    if (desc->bFrameIntervalType == 0) {
        // Continuous Frame Intervals
        printf("\tdwMinFrameInterval %"PRIu32"\n",  desc->dwMinFrameInterval);
        printf("\tdwMaxFrameInterval %"PRIu32"\n",  desc->dwMaxFrameInterval);
        printf("\tdwFrameIntervalStep %"PRIu32"\n", desc->dwFrameIntervalStep);
    } else {
        // Discrete Frame Intervals
        size_t num_of_intervals = (desc->bLength - 26) / 4;
        uint32_t *interval = (uint32_t *)&desc->dwFrameInterval;
        for (int i = 0; i < num_of_intervals; ++i) {
            printf("\tFrameInterval[%d] %"PRIu32"\n", i, interval[i]);
        }
    }
#endif
    if (width) {
        *width = desc->wWidth;
    }
    if (heigh) {
        *heigh = desc->wHeigh;
    }
    if (frame_idx) {
        *frame_idx = desc->bFrameIndex;
    }
}

void print_ep_desc(const uint8_t *buff)
{
    if (buff == NULL) {
        return;
    }
#ifdef CONFIG_UVC_PRINT_DESC
    const usb_ep_desc_t *ep_desc = (const usb_ep_desc_t *)buff;
    const char *ep_type_str;
    int type = ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;
    switch (type) {
    case USB_BM_ATTRIBUTES_XFER_CONTROL:
        ep_type_str = "CTRL";
        break;
    case USB_BM_ATTRIBUTES_XFER_ISOC:
        ep_type_str = "ISOC";
        break;
    case USB_BM_ATTRIBUTES_XFER_BULK:
        ep_type_str = "BULK";
        break;
    case USB_BM_ATTRIBUTES_XFER_INT:
        ep_type_str = "INT";
        break;
    default:
        ep_type_str = NULL;
        break;
    }

    printf("\t\t*** Endpoint descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\t\tbLength %d\n", ep_desc->bLength);
    printf("\t\tbDescriptorType 0x%x\n", ep_desc->bDescriptorType);
#endif
    printf("\t\tbEndpointAddress 0x%x\tEP %d %s\n", ep_desc->bEndpointAddress,
           USB_EP_DESC_GET_EP_NUM(ep_desc),
           USB_EP_DESC_GET_EP_DIR(ep_desc) ? "IN" : "OUT");
    printf("\t\tbmAttributes 0x%x\t%s\n", ep_desc->bmAttributes, ep_type_str);
    printf("\t\twMaxPacketSize %d\n", ep_desc->wMaxPacketSize);
    printf("\t\tbInterval %d\n", ep_desc->bInterval);
#endif
}

void parse_ep_desc(const uint8_t *buff, uint16_t *ep_mps, uint8_t *ep_addr, uint8_t *ep_attr)
{
    if (buff == NULL) {
        return;
    }
    const usb_ep_desc_t *ep_desc = (const usb_ep_desc_t *)buff;
    if (ep_addr) {
        *ep_addr = ep_desc->bEndpointAddress;
    }
    if (ep_mps) {
        *ep_mps = ep_desc->wMaxPacketSize;
    }
    if (ep_attr) {
        *ep_attr = ep_desc->bmAttributes;
    }
}

void print_intf_desc(const uint8_t *buff)
{
    if (buff == NULL) {
        return;
    }
#ifdef CONFIG_UVC_PRINT_DESC
    const usb_intf_desc_t *intf_desc = (const usb_intf_desc_t *)buff;
    printf("\t*** Interface descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength %d\n", intf_desc->bLength);
    printf("\tbDescriptorType 0x%x\n", intf_desc->bDescriptorType);
#endif
    printf("\tbInterfaceNumber %d\n", intf_desc->bInterfaceNumber);
    printf("\tbAlternateSetting %d\n", intf_desc->bAlternateSetting);
    printf("\tbNumEndpoints %d\n", intf_desc->bNumEndpoints);
    printf("\tbInterfaceClass 0x%x (%s)\n", intf_desc->bInterfaceClass,
    intf_desc->bInterfaceClass==USB_CLASS_VIDEO?"Video":
    (intf_desc->bInterfaceClass==USB_CLASS_AUDIO?"Audio":"Unknown"));
    printf("\tbInterfaceSubClass 0x%x\n", intf_desc->bInterfaceSubClass);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbInterfaceProtocol 0x%x\n", intf_desc->bInterfaceProtocol);
    printf("\tiInterface %d\n", intf_desc->iInterface);
#endif
#endif
}

void print_assoc_desc(const uint8_t *buff)
{
    if (buff == NULL) {
        return;
    }
#ifdef CONFIG_UVC_PRINT_DESC
    const ifc_assoc_desc_t *assoc_desc = (const ifc_assoc_desc_t *)buff;
    printf("*** Interface Association Descriptor: ");
    if (assoc_desc->bFunctionClass == USB_CLASS_VIDEO) {
        printf("Video");
    } else if (assoc_desc->bFunctionClass == USB_CLASS_AUDIO) {
        printf("Audio");
    } else {
        printf("Unknown");
    }
    printf(" ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("bLength %d\n", assoc_desc->bLength);
    printf("bDescriptorType 0x%x\n", assoc_desc->bDescriptorType);
    printf("bFirstInterface %d\n", assoc_desc->bFirstInterface);
    printf("bInterfaceCount %d\n", assoc_desc->bInterfaceCount);
    printf("bFunctionClass 0x%x\n", assoc_desc->bFunctionClass);
    printf("bFunctionSubClass 0x%x\n", assoc_desc->bFunctionSubClass);
    printf("bFunctionProtocol 0x%x\n", assoc_desc->bFunctionProtocol);
    printf("iFunction %d\n", assoc_desc->iFunction);
#endif
#endif
}

void print_cfg_desc(const uint8_t *buff)
{
    if (buff == NULL) {
        return;
    }
#ifdef CONFIG_UVC_PRINT_DESC
    const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)buff;
    printf("*** Configuration descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("bLength %d\n", cfg_desc->bLength);
    printf("bDescriptorType 0x%x\n", cfg_desc->bDescriptorType);
#endif
    printf("wTotalLength %d\n", cfg_desc->wTotalLength);
    printf("bNumInterfaces %d\n", cfg_desc->bNumInterfaces);
    printf("bConfigurationValue %d\n", cfg_desc->bConfigurationValue);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("iConfiguration %d\n", cfg_desc->iConfiguration);
    printf("bmAttributes 0x%x\n", cfg_desc->bmAttributes);
    printf("bMaxPower %dmA\n", cfg_desc->bMaxPower * 2);
#endif
#endif
}

void parse_ac_header_desc(const uint8_t *buff, uint16_t *bcdADC, uint8_t *intf_num)
{
    if (buff == NULL) {
        return;
    }
    const ac_interface_header_desc_t *desc = (const ac_interface_header_desc_t *)buff;
#ifdef CONFIG_UVC_PRINT_DESC
    printf("\t*** Audio control header descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength %d\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubtype 0x%x\n", desc->bDescriptorSubtype);
#endif
    printf("\tbcdADC 0x%x\n", desc->bcdADC);
    printf("\twTotalLength %d\n", desc->wTotalLength);
    printf("\tbInCollection %d\n", desc->bInCollection);
    if (desc->bInCollection) {
        const uint8_t *p_intf = desc->baInterfaceNr;
        for (int i = 0; i < desc->bInCollection; ++i) {
            printf("\t\tInterface number[%d] = %d\n", i, p_intf[i]);
        }
    }
#endif
    if (bcdADC) {
        *bcdADC = desc->bcdADC;
    }
    if (intf_num) {
        *intf_num = desc->bInCollection;
    }
}

void parse_ac_input_desc(const uint8_t *buff, uint8_t *terminal_idx, uint16_t *terminal_type)
{
    if (buff == NULL) {
        return;
    }
    const ac_interface_input_terminal_desc_t *desc = (const ac_interface_input_terminal_desc_t *)buff;
#ifdef CONFIG_UVC_PRINT_DESC
    printf("\t*** Audio control input terminal descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength %d\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubtype 0x%x\n", desc->bDescriptorSubtype);
#endif
    printf("\tbTerminalID %d\n", desc->bTerminalID);
    printf("\twTerminalType 0x%x\n", desc->wTerminalType);
    printf("\tbAssocTerminal %d\n", desc->bAssocTerminal);
    printf("\tbNrChannels %d\n", desc->bNrChannels);
    printf("\twChannelConfig 0x%04x\n", desc->wChannelConfig);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tiChannelNames %d\n", desc->iChannelNames);
    printf("\tiTerminal %d\n", desc->iTerminal);
#endif
#endif
    if (terminal_idx) {
        *terminal_idx = desc->bTerminalID;
    }
    if (terminal_type) {
        *terminal_type = desc->wTerminalType;
    }
}

void parse_ac_output_desc(const uint8_t *buff, uint8_t *terminal_idx, uint16_t *terminal_type)
{
    if (buff == NULL) {
        return;
    }
    const ac_interface_output_terminal_desc_t *desc = (const ac_interface_output_terminal_desc_t *)buff;
#ifdef CONFIG_UVC_PRINT_DESC
    printf("\t*** Audio control output terminal descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength %d\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubtype 0x%x\n", desc->bDescriptorSubtype);
#endif
    printf("\tbTerminalID %d\n", desc->bTerminalID);
    printf("\twTerminalType 0x%x\n", desc->wTerminalType);
    printf("\tbAssocTerminal %d\n", desc->bAssocTerminal);
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbSourceID %d\n", desc->bSourceID);
    printf("\tiTerminal %d\n", desc->iTerminal);
#endif
#endif
    if (terminal_idx) {
        *terminal_idx = desc->bTerminalID;
    }
    if (terminal_type) {
        *terminal_type = desc->wTerminalType;
    }
}

void parse_ac_feature_desc(const uint8_t *buff, uint8_t *source_idx, uint8_t *feature_unit_idx, uint8_t *control_type, uint8_t *control_type1)
{
    if (buff == NULL) {
        return;
    }
    const ac_interface_feature_unit_desc_t *desc = (const ac_interface_feature_unit_desc_t *)buff;
#ifdef CONFIG_UVC_PRINT_DESC
    printf("\t*** Audio control feature unit descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength %d\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubtype 0x%x\n", desc->bDescriptorSubtype);
#endif
    printf("\tbUnitID %d\n", desc->bUnitID);
    printf("\tbSourceID %d\n", desc->bSourceID);
    printf("\tbControlSize %d\n", desc->bControlSize);
    /*we just print master and 1 channel */
    for (size_t i = 0; i < 2; i += desc->bControlSize) {
        printf("\tbmaControls[ch%d] 0x%x\n", i, desc->bmaControls[i]);
    }
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tiFeature %d\n", desc->iFeature);
#endif
#endif
    if (feature_unit_idx) {
        *feature_unit_idx = desc->bUnitID;
    }
    if (source_idx) {
        *source_idx = desc->bSourceID;
    }
    if (control_type) {
        *control_type = desc->bmaControls[0];
    }
    if (control_type1) {
        *control_type1 = desc->bmaControls[1];
    }
}

void parse_as_general_desc(const uint8_t *buff, uint8_t *source_idx, uint16_t *format_tag)
{
    if (buff == NULL) {
        return;
    }
    const as_interface_header_desc_t *desc = (const as_interface_header_desc_t *)buff;
#ifdef CONFIG_UVC_PRINT_DESC
    printf("\t*** Audio stream general descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength %d\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubtype 0x%x\n", desc->bDescriptorSubtype);
#endif
    printf("\tbTerminalLink %d\n", desc->bTerminalLink);
    printf("\tbDelay %d\n", desc->bDelay);
    printf("\twFormatTag %d\n", desc->wFormatTag);
#endif
    if (source_idx) {
        *source_idx = desc->bTerminalLink;
    }
    if (format_tag) {
        *format_tag = desc->wFormatTag;
    }
}

void parse_as_type_desc(const uint8_t *buff, uint8_t *channel_num, uint8_t *bit_resolution, uint8_t *freq_type, const uint8_t **pp_samfreq)
{
    if (buff == NULL) {
        return;
    }
    const as_interface_type_I_format_desc_t *desc = (const as_interface_type_I_format_desc_t *)buff;
#ifdef CONFIG_UVC_PRINT_DESC
    printf("\t*** Audio control header descriptor ***\n");
#ifdef CONFIG_UVC_PRINT_DESC_VERBOSE
    printf("\tbLength %d\n", desc->bLength);
    printf("\tbDescriptorType 0x%x\n", desc->bDescriptorType);
    printf("\tbDescriptorSubtype 0x%x\n", desc->bDescriptorSubtype);
#endif
    printf("\tbFormatType %d\n", desc->bFormatType);
    printf("\tbNrChannels %d\n", desc->bNrChannels);
    printf("\tbSubframeSize %d\n", desc->bSubframeSize);
    printf("\tbBitResolution %d\n", desc->bBitResolution);
    printf("\tbSamFreqType %d\n", desc->bSamFreqType);
    if (desc->bSamFreqType == 0) {
        // Continuous Frame Intervals
        const uint8_t *p_samfreq = desc->tSamFreq;
        uint32_t min_samfreq = (p_samfreq[2] << 16) + (p_samfreq[1] << 8) + p_samfreq[0];
        uint32_t max_samfreq = (p_samfreq[5] << 16) + (p_samfreq[4] << 8) + p_samfreq[3];
        printf("\ttLowerSamFreq %"PRIu32"\n", min_samfreq);
        printf("\ttUpperSamFreq %"PRIu32"\n", max_samfreq);
    } else {
        const uint8_t *p_samfreq = desc->tSamFreq;
        for (int i = 0; i < desc->bSamFreqType; ++i) {
            printf("\ttSamFreq[%d] %"PRIu32"\n", i, (uint32_t)((p_samfreq[3 * i + 2] << 16) + (p_samfreq[3 * i + 1] << 8) + p_samfreq[3 * i]));
        }
    }
#endif
    if (pp_samfreq) {
        *pp_samfreq = desc->tSamFreq;
    }
    if (channel_num) {
        *channel_num = desc->bNrChannels;
    }
    if (bit_resolution) {
        *bit_resolution = desc->bBitResolution;
    }
    if (freq_type) {
        *freq_type = desc->bSamFreqType;
    }
}
