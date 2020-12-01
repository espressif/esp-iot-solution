/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _FB_GDISP_LLD_BOARD_H
#define _FB_GDISP_LLD_BOARD_H

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

/* Disp Includes */
#include "gdisp_lld_config.h"
#include "lcd_adapter.h"

/* uGFX Include */
#include "sdkconfig.h"
#include "esp_log.h"

typedef struct fbInfo {
    void *pixels;           // The pixel buffer
    coord_t linelen;        // The number of bytes per display line
} fbInfo_t;

static uint16_t *p_frame = NULL;

static void board_init(GDisplay *g, fbInfo_t *fbi)
{
    // TODO: Set the details of the frame buffer
    g->g.Width = CONFIG_UGFX_DRIVER_SCREEN_WIDTH;
    g->g.Height = CONFIG_UGFX_DRIVER_SCREEN_HEIGHT;
    g->g.Backlight = 255;
    g->g.Contrast = 255;
    fbi->linelen = g->g.Width * sizeof(LLDCOLOR_TYPE);              // bytes per row
    fbi->pixels = (void *) malloc(g->g.Width * g->g.Height * 2);    // pointer to the memory frame buffer
    p_frame = fbi->pixels;
    if (fbi->pixels == NULL) {
        ESP_LOGE("framebuffer", "fbi->pixels malloc error\n");
        ESP_LOGE("framebuffer", "%s:%d (%s)- assert failed!\n", __FILE__, __LINE__, __FUNCTION__);
        abort();
    }
    board_lcd_init();
}

#if GDISP_HARDWARE_FLUSH
static void board_flush(GDisplay *g)
{
    // TODO: Can be an empty function if your hardware doesn't support this
    (void) g;
    board_lcd_flush(0, 0, p_frame, g->g.Width, g->g.Height);
}
#endif //GDISP_HARDWARE_FLUSH

#if GDISP_NEED_CONTROL
static void board_backlight(GDisplay *g, uint8_t percent)
{
    // TODO: Can be an empty function if your hardware doesn't support this
    (void) g;
    (void) percent;
}

static void board_contrast(GDisplay *g, uint8_t percent)
{
    // TODO: Can be an empty function if your hardware doesn't support this
    (void) g;
    (void) percent;
}

static void board_power(GDisplay *g, powermode_t pwr)
{
    // TODO: Can be an empty function if your hardware doesn't support this
    (void) g;
    (void) pwr;
}
#endif //GDISP_NEED_CONTROL

#endif /* _FB_GDISP_LLD_BOARD_H*/
