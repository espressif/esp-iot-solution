/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

#pragma once

#include "sdkconfig.h"
#include "uac_descriptors.h"
#include "tusb_config_uac.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

#ifdef CONFIG_TINYUSB_RHPORT_HS
#   define CFG_TUSB_RHPORT1_MODE    OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED
#   define CONFIG_USB_HS            1
#else
#   define CFG_TUSB_RHPORT0_MODE    OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED
#   define CONFIG_USB_HS            0
#endif

// // RHPort max operational speed can defined by board.mk
// #ifndef BOARD_TUD_MAX_SPEED
// #define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
// #endif

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#endif

#ifndef ESP_PLATFORM
#define ESP_PLATFORM 1
#endif

// Espressif IDF requires "freertos/" prefix in include path
#if TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3, OPT_MCU_ESP32P4)
#define CFG_TUSB_OS_INC_PATH    freertos/
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// #ifndef USB_OTG_HS_PERIPH_BASE
// #define USB_OTG_HS_PERIPH_BASE 1
// #endif

// Enable Device stack
#define CFG_TUD_ENABLED       1

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

#define USB_VID                      CONFIG_TUSB_VID
#define USB_PID                      CONFIG_TUSB_PID
#define USB_MANUFACTURER             CONFIG_TUSB_MANUFACTURER
#define USB_PRODUCT                  CONFIG_TUSB_PRODUCT

//------------- CLASS -------------//
/*!< Vendor Class */
#define CFG_TUD_VENDOR               1
#define VENDOR_BUF_SIZE              CONFIG_USB_HS ? 512 : 64
#define CFG_TUD_VENDOR_RX_BUFSIZE    VENDOR_BUF_SIZE * 10
#define CFG_TUD_VENDOR_TX_BUFSIZE    VENDOR_BUF_SIZE
#ifndef CFG_TUD_VENDOR_EPSIZE
#define CFG_TUD_VENDOR_EPSIZE        VENDOR_BUF_SIZE
#endif

/*!< HID Class */
#define CFG_TUD_HID                  1
#define CFG_TUD_HID_EP_BUFSIZE       CONFIG_USB_HS ? 512 : 64

#ifdef __cplusplus
}
#endif
