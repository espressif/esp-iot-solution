/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _GDISP_LLD_TOGGLE_BOARD_H
#define _GDISP_LLD_TOGGLE_BOARD_H

// #error "GINPUT Toggle Pal Driver: You need to define your board definitions"

// The below are example values

#define GINPUT_TOGGLE_NUM_PORTS         2    // The total number of toggle inputs
#define GINPUT_TOGGLE_CONFIG_ENTRIES    1    // The total number of GToggleConfig entries

#define CLI_SW1_MASK 1
#define CLI_SW2_MASK 2

#define GINPUT_TOGGLE_DECLARE_STRUCTURE()                                         \
    const GToggleConfig GInputToggleConfigTable[GINPUT_TOGGLE_CONFIG_ENTRIES] = { \
        {0, /* touchpad 1 and touchpad 2 */                                       \
         CLI_SW1_MASK | CLI_SW2_MASK,                                             \
         0,                                                                       \
         0},                                                                      \
    }

#endif /* _GDISP_LLD_TOGGLE_BOARD_H */
