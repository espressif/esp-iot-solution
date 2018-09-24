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
#include "i2s_lcd_com.h"
#include "NT35510.h"

// Static inline functions
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
}

static GFXINLINE void acquire_bus(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void release_bus(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void acquire_sem()
{
    /* Code here*/
}

static GFXINLINE void release_sem()
{
    /* Code here*/
}

static GFXINLINE void write_cmd(GDisplay *g, uint16_t cmd)
{
    board_lcd_write_cmd(cmd);
}

static GFXINLINE void write_data(GDisplay *g, uint16_t data)
{
    board_lcd_write_data(data);
}

static GFXINLINE void write_data_repeats(GDisplay *g, uint16_t data, uint16_t x, uint16_t y)
{
    board_lcd_write_datas(data, x, y);
}

static GFXINLINE void write_data_byte(GDisplay *g, uint8_t data)
{
    board_lcd_write_data_byte(data);
}

static GFXINLINE void blit_area(GDisplay *g)
{
    const uint16_t	*buffer;
    buffer = (const uint16_t *)g->p.ptr;
    buffer += g->p.y1 * g->p.x2 + g->p.x1;	// The buffer start position
    board_lcd_blit_area(g->p.x, g->p.y, buffer, g->p.cx, g->p.cy);
}

static GFXINLINE void set_backlight(GDisplay *g, uint16_t data)
{
    (void) g;
}

static GFXINLINE void set_viewport(GDisplay *g)
{
    board_lcd_write_reg((uint16_t)(NT35510_CASET), (uint16_t)(g->p.x) >> 8);
    board_lcd_write_reg((uint16_t)(NT35510_CASET + 1), (uint16_t)(g->p.x) & 0xff);
    board_lcd_write_reg((uint16_t)(NT35510_CASET + 2), (uint16_t)(g->p.x + g->p.cx - 1) >> 8);
    board_lcd_write_reg((uint16_t)(NT35510_CASET + 3), (uint16_t)(g->p.x + g->p.cx - 1) & 0xff);
    board_lcd_write_reg((uint16_t)(NT35510_RASET), (uint16_t)(g->p.y) >> 8);
    board_lcd_write_reg((uint16_t)(NT35510_RASET + 1), (uint16_t)(g->p.y) & 0xff);
    board_lcd_write_reg((uint16_t)(NT35510_RASET + 2), (uint16_t)(g->p.y + g->p.cy - 1) >> 8);
    board_lcd_write_reg((uint16_t)(NT35510_RASET + 3), (uint16_t)(g->p.y + g->p.cy - 1) & 0xff);
    board_lcd_write_cmd((uint16_t)NT35510_RAMWR);
}

#endif /* CONFIG_UGFX_LCD_DRIVER_API_MODE */

#endif /* _GDISP_LLD_BOARD_H*/
