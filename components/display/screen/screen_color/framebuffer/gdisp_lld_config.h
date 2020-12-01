/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _FB_GDISP_LLD_CONFIG_H_
#define _FB_GDISP_LLD_CONFIG_H_

/* uGFX Config Includes */
#include "sdkconfig.h"

#if GFX_USE_GDISP

#if CONFIG_UGFX_LCD_DRIVER_FRAMEBUFFER_MODE

#define GDISP_HARDWARE_DRAWPIXEL        TRUE
#define GDISP_HARDWARE_BITFILLS         TRUE
#define GDISP_HARDWARE_FILLS            TRUE
#define GDISP_HARDWARE_PIXELREAD        TRUE
#define GDISP_HARDWARE_CONTROL          TRUE

// Uncomment this if your frame buffer device requires flushing
#define GDISP_HARDWARE_FLUSH            TRUE
#define GDISP_LLD_PIXELFORMAT           GDISP_PIXELFORMAT_RGB565

// Any other support comes from the board file
//#include

#ifndef GDISP_LLD_PIXELFORMAT
#error "GDISP FrameBuffer: You must specify a GDISP_LLD_PIXELFORMAT in your board_framebuffer.h or your makefile"
#endif

// This driver currently only supports unpacked formats with more than 8 bits per pixel
//  that is, we only support GRAY_SCALE with 8 bits per pixel or any unpacked TRUE_COLOR format.
// Note: At the time this file is included we have not calculated all our color
//          definitions so we need to do this by hand.
#if (GDISP_LLD_PIXELFORMAT & 0x4000) && (GDISP_LLD_PIXELFORMAT & 0xFF) != 8
#error "GDISP FrameBuffer: This driver does not support the specified GDISP_LLD_PIXELFORMAT"
#endif

#endif /* CONFIG_UGFX_LCD_DRIVER_FRAMEBUFFER_MODE */

#endif /* GFX_USE_GDISP */

#endif /* _FB_GDISP_LLD_CONFIG_H_ */
