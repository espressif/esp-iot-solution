/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Jerzy Kasenbreg
 * Copyright (c) 2021 Koji KITAYAMA
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

#ifndef _USB_DESCRIPTORS_H_
#define _USB_DESCRIPTORS_H_

#include "tusb.h"
#include "bsp/esp-bsp.h"

#define LCD_WIDTH    BSP_LCD_H_RES
#define LCD_HEIGHT   BSP_LCD_V_RES

enum {
    REPORT_ID_TOUCH = 1,
    REPORT_ID_MAX_COUNT,
    REPORT_ID_COUNT
};

enum {
    ITF_NUM_VENDOR = 0,
#if CFG_TUD_HID
    ITF_NUM_HID,
#endif
#if CFG_TUD_AUDIO
    ITF_NUM_AUDIO_CONTROL,
    ITF_NUM_AUDIO_STREAMING_SPK,
    ITF_NUM_AUDIO_STREAMING_MIC,
#endif
    ITF_NUM_TOTAL,
};

enum {
    EPNUM_DEFAULT = 0,
    EPNUM_VENDOR,
#if CFG_TUD_HID
    EPNUM_HID_DATA,
#endif
#if CFG_TUD_AUDIO
    EPNUM_AUDIO_OUT,
    EPNUM_AUDIO_IN,
    EPNUM_AUDIO_FB,
#endif
    EPNUM_TOTAL
};

#define TUD_HID_REPORT_DESC_TOUCH_SCREEN(report_id, width, height) \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_DIGITIZER        ),\
    /* USAGE (Touch Screen) */\
    HID_USAGE        ( 0x04                       ),\
    HID_COLLECTION   ( HID_COLLECTION_APPLICATION ),\
      /* Report ID if any */\
      HID_REPORT_ID ( report_id                 ) \
      /* Input */ \
      /* Finger */ \
      FINGER_USAGE(width, height) \
      FINGER_USAGE(width, height) \
      FINGER_USAGE(width, height) \
      FINGER_USAGE(width, height) \
      FINGER_USAGE(width, height) \
      /* Contact count */\
      HID_USAGE     ( 0x54                                   ),\
      HID_LOGICAL_MAX ( 127                                    ),\
      HID_REPORT_COUNT( 1                                    ),\
      HID_REPORT_SIZE ( 8                                    ),\
      HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_REPORT_ID ( report_id + 1             ) \
    HID_USAGE (0x55              ),\
    HID_REPORT_COUNT (1               ),\
    HID_LOGICAL_MAX (0x10              ),\
    HID_FEATURE ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_COLLECTION_END \

#define FINGER_USAGE(width, height) \
    HID_USAGE     ( 0x42                                   ),\
    HID_COLLECTION  ( HID_COLLECTION_LOGICAL                 ),\
    HID_USAGE     ( 0x42                                   ),\
    HID_LOGICAL_MIN ( 0x00                                 ),\
    HID_LOGICAL_MAX ( 0x01                                 ),\
    HID_REPORT_SIZE ( 1                                    ),\
    HID_REPORT_COUNT( 1                                    ),\
    HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_REPORT_COUNT( 7                                    ),\
    HID_INPUT      ( HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE ),\
    HID_REPORT_SIZE ( 8                                    ),\
    HID_USAGE     ( 0x51                                   ),\
    HID_REPORT_COUNT( 1                                    ),\
    HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP                 ),\
    HID_LOGICAL_MAX_N ( width, 2                           ),\
    HID_REPORT_SIZE ( 16                                    ),\
    HID_UNIT_EXPONENT ( 0x0e                                ),\
    /* Inch,EngLinear */\
    HID_UNIT      ( 0x13                                   ),\
    /* X */\
    HID_USAGE     ( 0x30                                   ),\
    HID_PHYSICAL_MIN ( 0                                   ),\
    HID_PHYSICAL_MAX_N ( width, 2                           ),\
    HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    /* Y */\
    HID_LOGICAL_MAX_N ( height, 2                           ),\
    HID_PHYSICAL_MAX_N ( height, 2                            ),\
    HID_USAGE     ( 0x31                                   ),\
    HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DIGITIZER               ),\
    /* Width */\
    HID_USAGE     ( 0x48                                   ),\
    /* Height */\
    HID_USAGE     ( 0x49                                   ),\
    HID_REPORT_COUNT( 2                                    ),\
    HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),\
    HID_COLLECTION_END, \

#endif
