/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

#if GFX_USE_GDISP

#define GDISP_DRIVER_VMT          GDISPVMT_TEMPLATE

/* Disp Includes */
#include "gdisp_lld_config.h"
#include "src/gdisp/gdisp_driver.h"
#include "board_template.h"
#include "template.h"

#ifndef GDISP_SCREEN_HEIGHT
#define GDISP_SCREEN_HEIGHT       64
#endif
#ifndef GDISP_SCREEN_WIDTH
#define GDISP_SCREEN_WIDTH        128
#endif
#ifndef GDISP_INITIAL_CONTRAST
#define GDISP_INITIAL_CONTRAST    51
#endif
#ifndef GDISP_INITIAL_BACKLIGHT
#define GDISP_INITIAL_BACKLIGHT   100
#endif

#define GDISP_FLG_NEEDFLUSH       (GDISP_FLG_DRIVER << 0)

#ifndef TEMPLATE_LCD_BIAS
#define TEMPLATE_LCD_BIAS         TEMPLATE_LCD_BIAS_7
#endif
#ifndef TEMPLATE_ADC
#define TEMPLATE_ADC              TEMPLATE_ADC_NORMAL
#endif
#ifndef TEMPLATE_COM_SCAN
#define TEMPLATE_COM_SCAN         TEMPLATE_COM_SCAN_INC
#endif
#ifndef TEMPLATE_PAGE_ORDER
#define TEMPLATE_PAGE_ORDER       0,1,2,3,4,5,6,7
#endif

// Some common routines and macros
#define RAM(g)                           ((uint8_t *)g->priv)
#define write_cmd2(g, cmd1, cmd2)        { write_cmd(g, cmd1); write_cmd(g, cmd2); }
#define write_cmd3(g, cmd1, cmd2, cmd3)  { write_cmd(g, cmd1); write_cmd(g, cmd2); write_cmd(g, cmd3); }

// Some common routines and macros
#define delay(us)           gfxSleepMicroseconds(us)
#define delay_ms(ms)        gfxSleepMilliseconds(ms)

#define xyaddr(x, y)        ((x) + ((y) >> 3)*GDISP_SCREEN_WIDTH)
#define xybit(y)            (1 << ((y) & 7))

/**
 * As this controller can't update on a pixel boundary we need to maintain the
 * the entire display surface in memory so that we can do the necessary bit
 * operations. Fortunately it is a small display in monochrome.
 * 64 * 128 / 8 = 1024 bytes.
 */
LLDSPEC bool_t gdisp_lld_init(GDisplay *g)
{
    // The private area is the display surface.
    g->priv = gfxAlloc(GDISP_SCREEN_HEIGHT * GDISP_SCREEN_WIDTH / 8);
    if (!g->priv) {
        return FALSE;
    }

    // Initialise the board interface
    init_board(g);

    // Finish Init
    post_init_board(g);

    // Release the bus
    release_bus(g);

    // Initialise the GDISP structure
    g->g.Width = GDISP_SCREEN_WIDTH;
    g->g.Height = GDISP_SCREEN_HEIGHT;
    g->g.Orientation = GDISP_ROTATE_0;
    g->g.Powermode = powerOn;
    g->g.Backlight = GDISP_INITIAL_BACKLIGHT;
    g->g.Contrast = GDISP_INITIAL_CONTRAST;

    return TRUE;
}

#if GDISP_HARDWARE_FLUSH
LLDSPEC void gdisp_lld_flush(GDisplay *g)
{
    unsigned p;

    // Don't flush if we don't need it.
    if (!(g->flags & GDISP_FLG_NEEDFLUSH)) {
        return;
    }

    acquire_bus(g);
    uint8_t pagemap[8] = {TEMPLATE_PAGE_ORDER};
    for (p = 0; p < 8; p++) {
        write_cmd(g, TEMPLATE_PAGE | pagemap[p]);
        write_cmd(g, TEMPLATE_COLUMN_MSB | 0);
        write_cmd(g, TEMPLATE_COLUMN_LSB | 0);
        write_cmd(g, TEMPLATE_RMW);
        write_data(g, RAM(g) + (p * GDISP_SCREEN_WIDTH), GDISP_SCREEN_WIDTH);
    }
    release_bus(g);

    g->flags &= ~GDISP_FLG_NEEDFLUSH;
}
#endif

#if GDISP_HARDWARE_DRAWPIXEL
LLDSPEC void gdisp_lld_draw_pixel(GDisplay *g)
{
    coord_t x, y;

    switch (g->g.Orientation) {
    default:
    case GDISP_ROTATE_0:
        x = g->p.x;
        y = g->p.y;
        break;
    case GDISP_ROTATE_90:
        x = g->p.y;
        y = GDISP_SCREEN_HEIGHT - 1 - g->p.x;
        break;
    case GDISP_ROTATE_180:
        x = GDISP_SCREEN_WIDTH - 1 - g->p.x;
        y = GDISP_SCREEN_HEIGHT - 1 - g->p.y;
        break;
    case GDISP_ROTATE_270:
        x = GDISP_SCREEN_HEIGHT - 1 - g->p.y;
        y = g->p.x;
        break;
    }
    if (gdispColor2Native(g->p.color) != Black) {
        RAM(g)[xyaddr(x, y)] |= xybit(y);
    } else {
        RAM(g)[xyaddr(x, y)] &= ~xybit(y);
    }
    g->flags |= GDISP_FLG_NEEDFLUSH;
}
#endif

#if GDISP_HARDWARE_PIXELREAD
LLDSPEC color_t gdisp_lld_get_pixel_color(GDisplay *g)
{
    coord_t x, y;

    switch (g->g.Orientation) {
    default:
    case GDISP_ROTATE_0:
        x = g->p.x;
        y = g->p.y;
        break;
    case GDISP_ROTATE_90:
        x = g->p.y;
        y = GDISP_SCREEN_HEIGHT - 1 - g->p.x;
        break;
    case GDISP_ROTATE_180:
        x = GDISP_SCREEN_WIDTH - 1 - g->p.x;
        y = GDISP_SCREEN_HEIGHT - 1 - g->p.y;
        break;
    case GDISP_ROTATE_270:
        x = GDISP_SCREEN_HEIGHT - 1 - g->p.y;
        x = g->p.x;
        break;
    }
    return (RAM(g)[xyaddr(x, y)] & xybit(y)) ? White : Black;
}
#endif

#if GDISP_NEED_CONTROL && GDISP_HARDWARE_CONTROL
LLDSPEC void gdisp_lld_control(GDisplay *g)
{
    switch (g->p.x) {
    case GDISP_CONTROL_POWER:
        if (g->g.Powermode == (powermode_t)g->p.ptr) {
            return;
        }
        switch ((powermode_t)g->p.ptr) {
        case powerOff:
        case powerSleep:
        case powerDeepSleep:
            acquire_bus(g);
            write_cmd(g, TEMPLATE_DISPLAY_OFF);
            release_bus(g);
            break;
        case powerOn:
            acquire_bus(g);
            write_cmd(g, TEMPLATE_DISPLAY_ON);
            release_bus(g);
            break;
        default:
            return;
        }
        g->g.Powermode = (powermode_t)g->p.ptr;
        return;

    case GDISP_CONTROL_ORIENTATION:
        if (g->g.Orientation == (orientation_t)g->p.ptr) {
            return;
        }
        switch ((orientation_t)g->p.ptr) {
        // Rotation is handled by the drawing routines
        case GDISP_ROTATE_0:
        case GDISP_ROTATE_180:
            g->g.Height = GDISP_SCREEN_HEIGHT;
            g->g.Width = GDISP_SCREEN_WIDTH;
            break;
        case GDISP_ROTATE_90:
        case GDISP_ROTATE_270:
            g->g.Height = GDISP_SCREEN_WIDTH;
            g->g.Width = GDISP_SCREEN_HEIGHT;
            break;
        default:
            return;
        }
        g->g.Orientation = (orientation_t)g->p.ptr;
        return;

    case GDISP_CONTROL_CONTRAST:
        if ((unsigned)g->p.ptr > 100) {
            g->p.ptr = (void *)100;
        }
        acquire_bus(g);
        write_cmd2(g, TEMPLATE_CONTRAST, ((((unsigned)g->p.ptr) << 6) / 101) & 0x3F);
        release_bus(g);
        g->g.Contrast = (unsigned)g->p.ptr;
        return;
    }
}
#endif // GDISP_NEED_CONTROL

#endif // GFX_USE_GDISP
