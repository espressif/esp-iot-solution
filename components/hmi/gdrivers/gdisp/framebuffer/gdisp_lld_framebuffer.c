/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/* uGFX Config Includes  */
#include "sdkconfig.h"

#if CONFIG_UGFX_LCD_DRIVER_FRAMEBUFFER_MODE

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

#if GFX_USE_GDISP

#define GDISP_DRIVER_VMT            GDISPVMT_FRAMEBUFFER

/* Disp Includes */
#include "gdisp_lld_config.h"
#include "src/gdisp/gdisp_driver.h"
#include "lcd_adapter.h"
#include "board_framebuffer.h"

typedef struct fbPriv {
    fbInfo_t fbi;            // Display information
} fbPriv_t;

#define PIXIL_POS(g, x, y)        ((y) * ((fbPriv_t *)(g)->priv)->fbi.linelen + (x) * sizeof(LLDCOLOR_TYPE))
#define PIXEL_ADDR(g, pos)        ((LLDCOLOR_TYPE *)(((char *)((fbPriv_t *)(g)->priv)->fbi.pixels)+pos))

LLDSPEC bool_t gdisp_lld_init(GDisplay *g)
{
    // Initialize the private structure
    if (!(g->priv = gfxAlloc(sizeof(fbPriv_t)))) {
        gfxHalt("GDISP Framebuffer: Failed to allocate private memory");
    }
    ((fbPriv_t *)g->priv)->fbi.pixels = 0;
    ((fbPriv_t *)g->priv)->fbi.linelen = 0;

    // Initialize the GDISP structure
    g->g.Orientation = GDISP_ROTATE_0;
    g->g.Powermode = powerOn;
    g->board = 0;                            // preinitialize
    board_init(g, &((fbPriv_t *)g->priv)->fbi);

    return TRUE;
}

#if GDISP_HARDWARE_FLUSH
LLDSPEC void gdisp_lld_flush(GDisplay *g)
{
    board_flush(g);
}
#endif //GDISP_HARDWARE_FLUSH

LLDSPEC void gdisp_lld_draw_pixel(GDisplay *g)
{
    PIXEL_ADDR(g, PIXIL_POS(g, g->p.x, g->p.y))[0] = gdispColor2Native(g->p.color);
}

LLDSPEC color_t gdisp_lld_get_pixel_color(GDisplay *g)
{
    LLDCOLOR_TYPE color;
    color = PIXEL_ADDR(g, PIXIL_POS(g, g->p.x, g->p.y))[0];
    return gdispNative2Color(color);
}

#if GDISP_HARDWARE_FILLS
LLDSPEC void gdisp_lld_fill_area(GDisplay *g)
{
    LLDCOLOR_TYPE *pointer;
    LLDCOLOR_TYPE c = gdispColor2Native(g->p.color);
    for (uint16_t i = 0; i < g->p.cy; ++i) {
        pointer = PIXEL_ADDR(g, PIXIL_POS(g, g->p.x, g->p.y + i));
        for (uint16_t j = 0; j < g->p.cx; ++j) {
            *pointer++ = c;
        }
    }
}
#endif // GDISP_HARDWARE_FILLS

#if GDISP_HARDWARE_BITFILLS
LLDSPEC void gdisp_lld_blit_area(GDisplay *g)
{
    const color_t *buffer;
    LLDCOLOR_TYPE *pointer, *pointer1;
    buffer = (const color_t *)g->p.ptr + g->p.y1 * g->p.x2 + g->p.x1; // The buffer start position
    for (uint16_t i = 0; i < g->p.cy; ++i) {
        pointer = PIXEL_ADDR(g, PIXIL_POS(g, g->p.x, g->p.y + i));
        pointer1 = buffer + i * g->p.x2;
        for (uint16_t j = 0; j < g->p.cx; ++j) {
            *pointer++ = *pointer1++;
        }
    }
}
#endif // GDISP_HARDWARE_BITFILLS

#if GDISP_NEED_CONTROL
LLDSPEC void gdisp_lld_control(GDisplay *g)
{
    coord_t tmp;
    switch (g->p.x) {
    case GDISP_CONTROL_POWER:
        if (g->g.Powermode == (powermode_t)g->p.ptr) {
            return;
        }
        switch ((powermode_t)g->p.ptr) {
        case powerOff: case powerOn: case powerSleep: case powerDeepSleep:
            board_power(g, (powermode_t)g->p.ptr);
            break;
        default:
            return;
        }
        g->g.Powermode = (powermode_t)g->p.ptr;
        return;

    case GDISP_CONTROL_ORIENTATION:
        switch ((orientation_t)g->p.ptr) {
        case GDISP_ROTATE_0:
            board_lcd_set_orientation(0);
            break;
        case GDISP_ROTATE_90:
            board_lcd_set_orientation(90);
            tmp = g->g.Width;
            g->g.Width = g->g.Height;
            g->g.Height = tmp;
            ((fbPriv_t *)g->priv)->fbi.linelen = g->g.Width * sizeof(LLDCOLOR_TYPE); // bytes per row
            break;
        case GDISP_ROTATE_180:
            board_lcd_set_orientation(180);
            break;
        case GDISP_ROTATE_270:
            board_lcd_set_orientation(270);
            tmp = g->g.Width;
            g->g.Width = g->g.Height;
            g->g.Height = tmp;
            ((fbPriv_t *)g->priv)->fbi.linelen = g->g.Width * sizeof(LLDCOLOR_TYPE); // bytes per row
            break;
        default:
            return;
        }
        g->g.Orientation = (orientation_t)g->p.ptr;
        return;

    case GDISP_CONTROL_BACKLIGHT:
        if ((unsigned)g->p.ptr > 100) {
            g->p.ptr = (void *)100;
        }
        board_backlight(g, (unsigned)g->p.ptr);
        g->g.Backlight = (unsigned)g->p.ptr;
        return;

    case GDISP_CONTROL_CONTRAST:
        if ((unsigned)g->p.ptr > 100) {
            g->p.ptr = (void *)100;
        }
        board_contrast(g, (unsigned)g->p.ptr);
        g->g.Contrast = (unsigned)g->p.ptr;
        return;
    }
}
#endif // GDISP_NEED_CONTROL

#endif /* GFX_USE_GDISP */

#endif /* CONFIG_UGFX_LCD_DRIVER_FRAMEBUFFER_MODE */
