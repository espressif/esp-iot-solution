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

/* Disp Includes */
#include "lcd_adapter.h"
#include "ILI9341.h"

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
    (void) state;
}

static GFXINLINE void set_backlight(GDisplay *g, uint8_t percent)
{
    (void) g;
    (void) percent;
}

static GFXINLINE void acquire_bus(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void release_bus(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void write_index(GDisplay *g, uint16_t index)
{
    board_lcd_write_cmd(index);
}

static GFXINLINE void write_data(GDisplay *g, uint16_t data)
{
    board_lcd_write_data(data);
}

static GFXINLINE void write_data_byte_repeat(GDisplay *g, uint16_t data, int point_num)
{
    board_lcd_write_data_byte_repeat(data, point_num);
}

static GFXINLINE void write_cmddata(GDisplay *g, uint8_t cmd, uint32_t data)
{
    board_lcd_write_cmddata(cmd, data);
}

static GFXINLINE void blit_area(GDisplay *g)
{
    const uint16_t	*buffer;
    buffer = (const uint16_t *)g->p.ptr;
    buffer += g->p.y1 * g->p.x2 + g->p.x1;	// The buffer start position
    board_lcd_blit_area(g->p.x, g->p.y, buffer, g->p.cx, g->p.cy);
}

static GFXINLINE void setreadmode(GDisplay *g)
{
    (void) g;
}

static GFXINLINE void setwritemode(GDisplay *g)
{
    (void) g;
}

static GFXINLINE uint16_t read_data(GDisplay *g)
{
    (void) g;
    return 0;
}

static GFXINLINE void set_viewport(GDisplay *g)
{
    write_cmddata(g, ILI9341_COLADDRSET_REG, MAKEWORD( ((g->p.x) >> 8), (g->p.x) & 0xFF, ((g->p.x + g->p.cx - 1) >> 8), (g->p.x + g->p.cx - 1) & 0xFF));
    write_cmddata(g, ILI9341_PAGEADDRSET_REG, MAKEWORD( ((g->p.y) >> 8), (g->p.y) & 0xFF, ((g->p.y + g->p.cy - 1) >> 8), (g->p.y + g->p.cy - 1) & 0xFF));
    write_index (g, ILI9341_MEMORYWRITE_REG);
}

#endif /* _GDISP_LLD_BOARD_H */

#endif /* CONFIG_UGFX_LCD_DRIVER_API_MODE */
