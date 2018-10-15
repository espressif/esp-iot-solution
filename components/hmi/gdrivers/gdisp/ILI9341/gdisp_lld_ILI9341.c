/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/* uGFX Config Includes */
#include "sdkconfig.h"

#if CONFIG_UGFX_LCD_DRIVER_API_MODE

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

#if GFX_USE_GDISP

#if defined(GDISP_SCREEN_HEIGHT) || defined(GDISP_SCREEN_HEIGHT)
#if GFX_COMPILER_WARNING_TYPE == GFX_COMPILER_WARNING_DIRECT
#warning "GDISP: This low level driver does not support setting a screen size. It is being ignored."
#elif GFX_COMPILER_WARNING_TYPE == GFX_COMPILER_WARNING_MACRO
COMPILER_WARNING("GDISP: This low level driver does not support setting a screen size. It is being ignored.")
#endif
#undef GDISP_SCREEN_WIDTH
#undef GDISP_SCREEN_HEIGHT
#endif

#define GDISP_DRIVER_VMT              GDISPVMT_ILI9341

/* Disp Includes */
#include "gdisp_lld_config.h"
#include "src/gdisp/gdisp_driver.h"
#include "board_ILI9341.h"
#include "ILI9341.h"

#ifndef GDISP_SCREEN_HEIGHT
#define GDISP_SCREEN_HEIGHT           CONFIG_UGFX_DRIVER_SCREEN_HEIGHT
#endif
#ifndef GDISP_SCREEN_WIDTH
#define GDISP_SCREEN_WIDTH            CONFIG_UGFX_DRIVER_SCREEN_WIDTH
#endif
#ifndef GDISP_INITIAL_CONTRAST
#define GDISP_INITIAL_CONTRAST        100
#endif
#ifndef GDISP_INITIAL_BACKLIGHT
#define GDISP_INITIAL_BACKLIGHT       100
#endif

#define WRAP_U16(c)   (((c >> 8) & 0xff) | ((c << 8) & 0xff00))

// Some common routines and macros
#define dummy_read(g)                { volatile uint16_t dummy; dummy = read_data(g); (void) dummy; }
#define write_reg(g, reg, data)      { write_index(g, reg); write_data(g, data); }
#define write_data16(g, data)        { write_data(g, data >> 8); write_data(g, (uint8_t)data); }
#define delay(us)                    gfxSleepMicroseconds(us)
#define delayms(ms)                  gfxSleepMilliseconds(ms)

LLDSPEC bool_t gdisp_lld_init(GDisplay *g)
{
    // No private area for this controller
    g->priv = 0;

    // Initialise the board interface
    init_board(g);

    // Finish Init
    post_init_board(g);

    // Release the bus
    release_bus(g);
    /* Turn on the back-light */
    set_backlight(g, GDISP_INITIAL_BACKLIGHT);

    /* Initialise the GDISP structure */
    g->g.Width = GDISP_SCREEN_WIDTH;
    g->g.Height = GDISP_SCREEN_HEIGHT;
    g->g.Orientation = GDISP_ROTATE_0;
    g->g.Powermode = powerOn;
    g->g.Backlight = GDISP_INITIAL_BACKLIGHT;
    g->g.Contrast = GDISP_INITIAL_CONTRAST;
    return TRUE;
}

#if GDISP_HARDWARE_STREAM_WRITE
LLDSPEC void gdisp_lld_write_start(GDisplay *g)
{
    acquire_bus(g);
    set_viewport(g);
    write_index(g, 0x2C);
}

LLDSPEC void gdisp_lld_write_color(GDisplay *g)
{
    LLDCOLOR_TYPE c;
    c = gdispColor2Native(g->p.color);
    write_data(g, WRAP_U16(c) );
}

LLDSPEC void gdisp_lld_write_stop(GDisplay *g)
{
    release_bus(g);
}
#endif // GDISP_HARDWARE_STREAM_WRITE

#if GDISP_HARDWARE_STREAM_READ
LLDSPEC void gdisp_lld_read_start(GDisplay *g)
{
    acquire_bus(g);
    set_viewport(g);
    write_index(g, 0x2E);
    setreadmode(g);
    dummy_read(g);
}

LLDSPEC color_t gdisp_lld_read_color(GDisplay *g)
{
    uint16_t data;
    data = read_data(g);
    return gdispNative2Color(data);
}

LLDSPEC void gdisp_lld_read_stop(GDisplay *g)
{
    setwritemode(g);
    release_bus(g);
}
#endif // GDISP_HARDWARE_STREAM_READ

LLDSPEC void gdisp_lld_draw_pixel(GDisplay *g)
{
    set_viewport(g);
    gdisp_lld_write_color (g);
}

#if GDISP_HARDWARE_FILLS
LLDSPEC void gdisp_lld_fill_area(GDisplay *g)
{
    LLDCOLOR_TYPE c = gdispColor2Native(g->p.color);

    acquire_bus(g);
    set_viewport (g);
    write_data_byte_repeat(g, WRAP_U16(c), g->p.cx * g->p.cy);
    release_bus(g);
}
#endif // GDISP_HARDWARE_FILLS

#if GDISP_HARDWARE_BITFILLS
LLDSPEC void gdisp_lld_blit_area (GDisplay *g)
{
    set_viewport(g);
    blit_area(g);
}
#endif // GDISP_HARDWARE_BITFILLS

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
            write_reg(g, 0x0010, 0x0001);    /* enter sleep mode */
            release_bus(g);
            break;
        case powerOn:
            acquire_bus(g);
            write_reg(g, 0x0010, 0x0000);    /* leave sleep mode */
            release_bus(g);
            break;
        default:
            return;
        }
        g->g.Powermode = (powermode_t)g->p.ptr;
        return;

    case GDISP_CONTROL_ORIENTATION:
        switch ((orientation_t)g->p.ptr) {
        case GDISP_ROTATE_0:
            acquire_bus(g);
            write_reg(g, 0x36, 0x88);    /* Invert X and Y axes */
            release_bus(g);
            g->g.Height = GDISP_SCREEN_HEIGHT;
            g->g.Width = GDISP_SCREEN_WIDTH;
            break;
        case GDISP_ROTATE_90:
            acquire_bus(g);
            write_reg(g, 0x36, 0x28);    /* X and Y axes non-inverted */
            release_bus(g);
            g->g.Height = GDISP_SCREEN_WIDTH;
            g->g.Width = GDISP_SCREEN_HEIGHT;
            break;
        case GDISP_ROTATE_180:
            acquire_bus(g);
            write_reg(g, 0x36, 0x48);    /* Invert X and Y axes */
            release_bus(g);
            g->g.Height = GDISP_SCREEN_HEIGHT;
            g->g.Width = GDISP_SCREEN_WIDTH;
            break;
        case GDISP_ROTATE_270:
            acquire_bus(g);
            write_reg(g, 0x36, 0xE8);        /* X and Y axes non-inverted */
            release_bus(g);
            g->g.Height = GDISP_SCREEN_WIDTH;
            g->g.Width = GDISP_SCREEN_HEIGHT;
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
        set_backlight(g, (unsigned)g->p.ptr);
        g->g.Backlight = (unsigned)g->p.ptr;
        return;

    case GDISP_CONTROL_CONTRAST:
    default:
        return;
    }
}
#endif // GDISP_NEED_CONTROL && GDISP_HARDWARE_CONTROL

#endif /* GFX_USE_GDISP */

#endif /* CONFIG_UGFX_LCD_DRIVER_API_MODE */
