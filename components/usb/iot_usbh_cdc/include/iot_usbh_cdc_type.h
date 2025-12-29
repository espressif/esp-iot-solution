/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "usb/usb_types_ch9.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
#undef ESP_STATIC_ASSERT
#define ESP_STATIC_ASSERT _Static_assert
#endif

// For compatibility with older IDF versions
#ifndef ESP_RETURN_VOID_ON_ERROR
#define ESP_RETURN_VOID_ON_ERROR(x, log_tag, format, ...) do {                                  \
        esp_err_t err_rc_ = (x);                                                                \
        if (unlikely(err_rc_ != ESP_OK)) {                                                      \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
            return;                                                                             \
        }                                                                                       \
    } while(0)
#endif

#ifndef ESP_RETURN_VOID_ON_FALSE
#define ESP_RETURN_VOID_ON_FALSE(a, log_tag, format, ...) do {                                  \
        if (unlikely(!(a))) {                                                                   \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
            return;                                                                             \
        }                                                                                       \
    } while(0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t bmRequestType;
    uint8_t bNotificationCode;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint8_t Data[];
} USB_DESC_ATTR iot_cdc_notification_t;

#define USB_CDC_NOTIFY_NETWORK_CONNECTION        0x00
#define USB_CDC_NOTIFY_RESPONSE_AVAILABLE        0x01
#define USB_CDC_NOTIFY_SERIAL_STATE              0x20
#define USB_CDC_NOTIFY_SPEED_CHANGE              0x2A

typedef union {
    uint16_t full;       // Full 16-bit value
    struct {
        uint16_t dcd          : 1;  // bit0
        uint16_t dsr          : 1;  // bit1
        uint16_t break_det    : 1;  // bit2
        uint16_t ring         : 1;  // bit3
        uint16_t framing_err  : 1;  // bit4
        uint16_t parity_err   : 1;  // bit5
        uint16_t overrun_err  : 1;  // bit6
        uint16_t reserved     : 9;  // bit7~15
    };
} usb_cdc_serial_state_t;

// ---------------------- Communications Device Class Code ---------------------
#define USB_COMMUNICATIONS_DEVICE_CLASS 0x02

// ---------------------- Communications Interface Class Code ---------------------
#define USB_COMMUNICATIONS_INTERFACE_CLASS 0x02

// ---------------------- Communications Class Subclass Codes ---------------------
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_RESERVED 0x00
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_DIRECT_LINE_CONTROL_MODEL 0x01
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_ABSTRACT_CONTROL_MODEL 0x02
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_TELEPHONE_CONTROL_MODEL 0x03
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_MULTI_CHANNEL_CONTROL_MODEL 0x04
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_CAPI_CONTROL_MODEL 0x05
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL 0x06
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_ATM_NETWORKING_CONTROL_MODEL 0x07
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_WIRELESS_HANDSET_CONTROL_MODEL 0x08
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_DEVICE_MANAGEMENT 0x09
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_MOBILE_DIRECT_LINE_MODEL 0x0A
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_OBEX 0x0B
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_ETHERNET_EMULATION_MODEL 0x0C
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_NETWORK_CONTROL_MODEL 0x0D
#define USB_COMMUNICATIONS_INTERFACE_SUBCLASS_MOBILE_BROADBAND_INTERFACE_MODEL 0x0E

// ---------------------- Communications Class Protocol Codes ---------------------
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_NONE 0x00
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_AT_V250 0x01
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_AT_PCCA101 0x02
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_AT_PCCA101_ANNEX 0x03
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_AT_GSM 0x04
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_AT_3GPP 0x05
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_AT_TIA 0x06
#define USB_COMMUNICATIONS_INTERFACE_PROTOCOL_EEM 0x07

// ---------------------- Data Class Interface Codes ---------------------
#define USB_DATA_INTERFACE_CLASS 0x0A

// ---------------------- Data Class Subclass Codes ---------------------
#define USB_DATA_INTERFACE_SUBCLASS_RESERVED 0x00
#define USB_DATA_INTERFACE_SUBCLASS_NETWORK_TRANSFER_BLOCK 0x01
#define USB_DATA_INTERFACE_SUBCLASS_ISDN_BRI 0x30
#define USB_DATA_INTERFACE_SUBCLASS_HDLC 0x31
#define USB_DATA_INTERFACE_SUBCLASS_TRANSPARENT_TRANSFER 0x32
#define USB_DATA_INTERFACE_SUBCLASS_USE_COMMUNICATIONS_DESC 0xFE

// ---------------------- Class-Specific DescriptorType ---------------------
#define USB_CS_DESCRIPTOR_TYPE_INTERFACE 0x24
#define USB_CS_DESCRIPTOR_TYPE_ENDPOINT 0x25

// ---------------------- Class-Specific DescriptorSubtype ---------------------
#define USB_CS_DESCRIPTOR_SUBTYPE_HEADER 0x00
#define USB_CS_DESCRIPTOR_SUBTYPE_CALL_MANAGEMENT 0x01
#define USB_CS_DESCRIPTOR_SUBTYPE_ACM 0x02
#define USB_CS_DESCRIPTOR_SUBTYPE_DIRECT_LINE 0x03
#define USB_CS_DESCRIPTOR_SUBTYPE_TELEPHONE_RINGER 0x04
#define USB_CS_DESCRIPTOR_SUBTYPE_TELEPHONE_CALLING 0x05
#define USB_CS_DESCRIPTOR_SUBTYPE_UNION_FUNCTION 0x06
#define USB_CS_DESCRIPTOR_SUBTYPE_COUNTRY_SPECIFIC 0x07
#define USB_CS_DESCRIPTOR_SUBTYPE_TELEPHONE_OPERATIONAL 0x08
#define USB_CS_DESCRIPTOR_SUBTYPE_USB_TERMINAL 0x09
#define USB_CS_DESCRIPTOR_SUBTYPE_NETWORK_CHANNEL_TERMINAL 0x0A
#define USB_CS_DESCRIPTOR_SUBTYPE_PROTOCOL_UNIT 0x0B
#define USB_CS_DESCRIPTOR_SUBTYPE_EXTENSION_UNIT 0x0C
#define USB_CS_DESCRIPTOR_SUBTYPE_MULTI_CHANNEL 0x0D
#define USB_CS_DESCRIPTOR_SUBTYPE_CAPI_CONTROL 0x0E
#define USB_CS_DESCRIPTOR_SUBTYPE_ETHERNET_NETWORKING 0x0F
#define USB_CS_DESCRIPTOR_SUBTYPE_ATM_NETWORKING 0x10
#define USB_CS_DESCRIPTOR_SUBTYPE_WIRELESS_HANDSET_CONTROL 0x11

// ----------------- Header Functional Descriptor -------------------

/**
 * @brief Size of a Header Functional Descriptor in bytes
 */
#define USB_CDC_HEADER_FUNC_DESC_SIZE        5

/**
 * @brief Structure representing a Header Functional Descriptor
 *
 */
typedef union {
    struct {
        uint8_t bFunctionLength;                    /**< Size of the descriptor in bytes */
        uint8_t bDescriptorType;            /**< CS_INTERFACE descriptor type */
        uint8_t bDescriptorSubtype;         /**< Header functional descriptor subtype */
        uint16_t bcdCDC;                   /**< USB Class Definitions for Communications Devices Specification release number in binary-coded decimal. */
    } USB_DESC_ATTR;                        /**< USB descriptor attributes */
    uint8_t val[USB_CDC_HEADER_FUNC_DESC_SIZE];          /**< Descriptor value */
} usb_cdc_header_func_desc_t;
ESP_STATIC_ASSERT(sizeof(usb_cdc_header_func_desc_t) == USB_CDC_HEADER_FUNC_DESC_SIZE, "Size of usb_cdc_header_func_desc_t incorrect");

// ----------------- Union Functional Descriptor -------------------
/**
 * @brief Size of a Union Functional Descriptor in bytes
 */
#define USB_CDC_UNION_FUNC_DESC_SIZE         6
/**
 * @brief Structure representing a Union Functional Descriptor
 *
 */
typedef union {
    struct {
        uint8_t bFunctionLength;            /**< Size of the descriptor in bytes */
        uint8_t bDescriptorType;            /**< CS_INTERFACE descriptor type */
        uint8_t bDescriptorSubtype;         /**< Union functional descriptor subtype */
        uint8_t bControlInterface;          /**< The interface number of the Communications or Data Class interface, designated as the controlling interface for the union. */
        uint8_t bSubordinateInterface0;          /**< Interface number of first subordinate interface in the union.  */
        uint8_t bSubordinateInterface1;          /**< Interface number of second subordinate interface in the union.  */
        // bSubordinateInterfaceN-1         Interface number of N-1 subordinate interface in the union.
    } USB_DESC_ATTR;                        /**< USB descriptor attributes */
    uint8_t val[USB_CDC_UNION_FUNC_DESC_SIZE];          /**< Descriptor value */
} usb_cdc_union_func_desc_t;
ESP_STATIC_ASSERT(sizeof(usb_cdc_union_func_desc_t) == USB_CDC_UNION_FUNC_DESC_SIZE, "Size of usb_cdc_union_func_desc_t incorrect");

// ----------------- Ethernet Networking Functional Descriptor -------------------
/**
 * @brief Size of an Ethernet Networking Functional Descriptor in bytes
 */
#define USB_CDC_ETHERNET_NETWORKING_FUNC_DESC_SIZE  13

typedef union {
    struct {
        uint8_t bFunctionLength;            /**< Size of the descriptor in bytes */
        uint8_t bDescriptorType;            /**< CS_INTERFACE descriptor type */
        uint8_t bDescriptorSubtype;         /**< Ethernet networking functional descriptor subtype */
        uint8_t iMACAddress;                /**< Index of string descriptor containing MAC address */
        uint32_t bmEthernetStatistics;      /**< Bitmap of supported Ethernet statistics */
        uint16_t wMaxSegmentSize;           /**< Maximum segment size for Ethernet packets */
        uint16_t wNumberMCFilters;          /**< Number of multicast filters supported */
        uint8_t bNumberPowerFilters;        /**< Contains the number of pattern filters that are available for causing wake-up of the host. */
    } USB_DESC_ATTR;                        /**< USB descriptor attributes */
    uint8_t val[USB_CDC_ETHERNET_NETWORKING_FUNC_DESC_SIZE];  /**< Descriptor value */
} usb_cdc_ethernet_networking_func_desc_t;

// --------------- Interface Association Descriptor -----------------
/**
 * @brief Size of an Interface Association Descriptor in bytes.
 * An IAD must appear before the sequence of interface descriptors to which it is associated and must be immediately adjacent to them.
 */
#define USB_INTERFACE_ASSOCIATION_DESC_SIZE  8
typedef union {
    struct {
        uint8_t bFunctionLength;            /**< Size of the descriptor in bytes */
        uint8_t bDescriptorType;            /**< fixed to 0x0B（IAD） */
        uint8_t bFirstInterface;          /**< The interface number of the first interface in the association */
        uint8_t bInterfaceCount;           /**< The number of contiguous interfaces in the association */
        uint8_t bFunctionClass;           /**< The class code for the interfaces in the association */
        uint8_t bFunctionSubClass;        /**< The subclass code for the interfaces in the association */
        uint8_t bFunctionProtocol;        /**< The protocol code for the interfaces in the association */
        uint8_t iFunction;                /**< Index of string descriptor describing the function of the interfaces in the association */
    } USB_DESC_ATTR;                        /**< USB descriptor attributes */
    uint8_t val[USB_INTERFACE_ASSOCIATION_DESC_SIZE];  /**< Descriptor value */
} usb_IAD_desc_t;

// ---------------------- Device Qualifier Descriptor ---------------------
/**
 * @brief Size of a Device Qualifier Descriptor in bytes
 */
#define USB_DEVICE_QUALIFIER_DESC_SIZE 10
typedef union {
    struct {
        uint8_t bLength;                   /**< Size of the descriptor in bytes */
        uint8_t bDescriptorType;           /**< fixed to 0x06 (Device Qualifier) */
        uint16_t bcdUSB;                   /**< USB Specification Release Number in binary-coded decimal */
        uint8_t bDeviceClass;              /**< Device class code */
        uint8_t bDeviceSubClass;           /**< Device subclass code */
        uint8_t bDeviceProtocol;           /**< Device protocol code */
        uint8_t bMaxPacketSize0;           /**< Maximum packet size for endpoint zero */
        uint8_t bNumConfigurations;         /**< Number of possible configurations */
        uint8_t bReserved;                 /**< Reserved for future use, must be zero */
    } USB_DESC_ATTR;                        /**< USB descriptor attributes */
    uint8_t val[USB_DEVICE_QUALIFIER_DESC_SIZE];  /**< Descriptor value */
} usb_device_qualifier_desc_t;

/**
 * @brief USB CDC Request Codes
 *
 * @see Table 19, USB CDC specification rev. 1.2
 */
typedef enum {
    CDC_REQ_SEND_ENCAPSULATED_COMMAND = 0x00,
    CDC_REQ_GET_ENCAPSULATED_RESPONSE = 0x01,
    CDC_REQ_SET_COMM_FEATURE = 0x02,
    CDC_REQ_GET_COMM_FEATURE = 0x03,
    CDC_REQ_CLEAR_COMM_FEATURE = 0x04,
    CDC_REQ_SET_AUX_LINE_STATE = 0x10,
    CDC_REQ_SET_HOOK_STATE = 0x11,
    CDC_REQ_PULSE_SETUP = 0x12,
    CDC_REQ_SEND_PULSE = 0x13,
    CDC_REQ_SET_PULSE_TIME = 0x14,
    CDC_REQ_RING_AUX_JACK = 0x15,
    CDC_REQ_SET_LINE_CODING = 0x20,
    CDC_REQ_GET_LINE_CODING = 0x21,
    CDC_REQ_SET_CONTROL_LINE_STATE = 0x22,
    CDC_REQ_SEND_BREAK = 0x23,
    CDC_REQ_SET_RINGER_PARMS = 0x30,
    CDC_REQ_GET_RINGER_PARMS = 0x31,
    CDC_REQ_SET_OPERATION_PARMS = 0x32,
    CDC_REQ_GET_OPERATION_PARMS = 0x33,
    CDC_REQ_SET_LINE_PARMS = 0x34,
    CDC_REQ_GET_LINE_PARMS = 0x35,
    CDC_REQ_DIAL_DIGITS = 0x36,
    CDC_REQ_SET_UNIT_PARAMETER = 0x37,
    CDC_REQ_GET_UNIT_PARAMETER = 0x38,
    CDC_REQ_CLEAR_UNIT_PARAMETER = 0x39,
    CDC_REQ_GET_PROFILE = 0x3A,
    CDC_REQ_SET_ETHERNET_MULTICAST_FILTERS = 0x40,
    CDC_REQ_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x41,
    CDC_REQ_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x42,
    CDC_REQ_SET_ETHERNET_PACKET_FILTER = 0x43,
    CDC_REQ_GET_ETHERNET_STATISTIC = 0x44,
    CDC_REQ_SET_ATM_DATA_FORMAT = 0x50,
    CDC_REQ_GET_ATM_DEVICE_STATISTICS = 0x51,
    CDC_REQ_SET_ATM_DEFAULT_VC = 0x52,
    CDC_REQ_GET_ATM_VC_STATISTICS = 0x53,
    CDC_REQ_GET_NTB_PARAMETERS = 0x80,
    CDC_REQ_GET_NET_ADDRESS = 0x81,
    CDC_REQ_SET_NET_ADDRESS = 0x82,
    CDC_REQ_GET_NTB_FORMAT = 0x83,
    CDC_REQ_SET_NTB_FORMAT = 0x84,
    CDC_REQ_GET_NTB_INPUT_SIZE = 0x85,
    CDC_REQ_SET_NTB_INPUT_SIZE = 0x86,
    CDC_REQ_GET_MAX_DATAGRAM_SIZE = 0x87,
    CDC_REQ_SET_MAX_DATAGRAM_SIZE = 0x88,
    CDC_REQ_GET_CRC_MODE = 0x89,
    CDC_REQ_SET_CRC_MODE = 0x8A
} iot_cdc_request_code_t;

#ifdef __cplusplus
}
#endif
