/*
 * Created by Oleg Gerasimov <ogerasimov@gmail.com>
 * 14.08.2016
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

#define GDISP_DRIVER_VMT          GDISPVMT_ST7789

/* Disp Includes */
#include "gdisp_lld_config.h"
#include "src/gdisp/gdisp_driver.h"
#include "board_ST7789.h"
#include "ST7789.h"

#ifndef GDISP_SCREEN_HEIGHT
#define GDISP_SCREEN_HEIGHT       CONFIG_UGFX_DRIVER_SCREEN_HEIGHT
#endif
#ifndef GDISP_SCREEN_WIDTH
#define GDISP_SCREEN_WIDTH        CONFIG_UGFX_DRIVER_SCREEN_WIDTH
#endif

#ifndef GDISP_INITIAL_CONTRAST
#define GDISP_INITIAL_CONTRAST    100
#endif
#ifndef GDISP_INITIAL_BACKLIGHT
#define GDISP_INITIAL_BACKLIGHT   100
#endif

// Define one of supported type, if not defined yet
#if !defined(ST7789_TYPE_R) && !defined(ST7789_TYPE_B)
// It seems all modern boards is 7735R
#define ST7789_TYPE_R TRUE
#endif
// Define one of supported color packing, if not defined yet
#if !defined(ST7789_COLOR_RGB) && !defined(ST7789_COLOR_BRG)
// It seems all modern boards is RGB
#define ST7789_COLOR_RGB FALSE
#endif

// Strange boars with shifted coords
#if !defined (ST7789_SHIFTED_COORDS)
#define ST7789_SHIFTED_COORDS FALSE
#endif

#if ST7789_COLOR_RGB
#define ST7789_MADCTRL_COLOR 0x00
#else
#define ST7789_MADCTRL_COLOR 0x08
#endif

#if ST7789_SHIFTED_COORDS
#define ST7789_COL_SHIFT 2
#define ST7789_ROW_SHIFT 1
#else
#define ST7789_COL_SHIFT 0
#define ST7789_ROW_SHIFT 0
#endif

#define WRAP_U16(c)                  (((c >> 8) & 0xff) | ((c << 8) & 0xff00))

// Some common routines and macros
#define dummy_read(g)                { volatile uint16_t dummy; dummy = read_data(g); (void) dummy; }
#define write_reg(g, reg, data)      { write_cmd(g, reg); write_data(g, data); }

// Serial write data for fast fill.
#ifndef write_data_repeat
#define write_data_repeat(g, data, count) { int i; for (i = 0; i < count; ++i) write_data (g, data); }
#endif

LLDSPEC bool_t gdisp_lld_init(GDisplay *g)
{
    // No private area for this controller
    g->priv = 0;

    // Initialise the board interface
    init_board(g);

    // Finish Init
    post_init_board(g);

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
}

LLDSPEC color_t gdisp_lld_read_color(GDisplay *g)
{
    /* Code here*/
    return 0;
}

LLDSPEC void gdisp_lld_read_stop(GDisplay *g)
{
    // setwritemode(g);
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
    write_data_byte_repeat (g, WRAP_U16(c), g->p.cx * g->p.cy);
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
            acquire_bus(g);
            // not implemented
            release_bus(g);
            set_backlight(g, 0);
            break;
        case powerSleep:
        case powerDeepSleep:
            // not implemented
            acquire_bus(g);
            release_bus(g);
            set_backlight(g, 0);
            break;
        case powerOn:
            acquire_bus(g);
            // not implemented
            release_bus(g);
            set_backlight(g, g->g.Backlight);
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
            write_cmd(g, ST7789_MADCTL);
            write_data_byte(g, 0x80 | ST7789_MADCTRL_COLOR);
            g->g.Height = GDISP_SCREEN_HEIGHT;
            g->g.Width = GDISP_SCREEN_WIDTH;
            release_bus(g);
            break;
        case GDISP_ROTATE_90:
            acquire_bus(g);
            write_cmd(g, ST7789_MADCTL);
            write_data_byte(g, 0x20 | ST7789_MADCTRL_COLOR);
            g->g.Height = GDISP_SCREEN_WIDTH;
            g->g.Width = GDISP_SCREEN_HEIGHT;
            release_bus(g);
            break;
        case GDISP_ROTATE_180:
            acquire_bus(g);
            write_cmd(g, ST7789_MADCTL);
            write_data_byte(g, 0x40 | ST7789_MADCTRL_COLOR);
            g->g.Height = GDISP_SCREEN_HEIGHT;
            g->g.Width = GDISP_SCREEN_WIDTH;
            release_bus(g);
            break;
        case GDISP_ROTATE_270:
            acquire_bus(g);
            write_cmd(g, ST7789_MADCTL);
            write_data_byte(g, 0xE0 | ST7789_MADCTRL_COLOR);
            g->g.Height = GDISP_SCREEN_WIDTH;
            g->g.Width = GDISP_SCREEN_HEIGHT;
            release_bus(g);
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
    default:
        return;
    }
}
#endif // GDISP_NEED_CONTROL && GDISP_HARDWARE_CONTROL

#endif /* GFX_USE_GDISP */

#endif /* CONFIG_UGFX_LCD_DRIVER_API_MODE */
