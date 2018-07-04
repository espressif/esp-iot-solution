/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _GDISP_LLD_BOARD_H
#define _GDISP_LLD_BOARD_H

/* uGFX Config Includes */
#include "sdkconfig.h"

#if CONFIG_UGFX_LCD_DRIVER_API_MODE

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

/* LCD Includes */
#include "lcd_adapter.h"
#include "SSD1306.h"

// #define SSD1306_PAGE_PREFIX    0x40

static GFXINLINE void init_board(GDisplay *g)
{
    board_lcd_init();
}

static GFXINLINE void post_init_board(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void setpin_reset(GDisplay *g, bool_t state)
{
    (void) g;
    (void) state;
}

static GFXINLINE void acquire_bus(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void release_bus(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void write_cmd(GDisplay *g, uint8_t cmd)
{
    board_lcd_write_cmd(cmd);
}

static GFXINLINE void write_data(GDisplay *g, uint8_t *data, uint16_t length)
{
    board_lcd_write_datas(data, length);
}

#endif /* CONFIG_UGFX_LCD_DRIVER_API_MODE */

#endif /* _GDISP_LLD_BOARD_H */
