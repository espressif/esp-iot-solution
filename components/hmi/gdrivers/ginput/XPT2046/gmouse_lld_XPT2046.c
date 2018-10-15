/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/* uGFX Config Includes */
#include "sdkconfig.h"

#ifdef CONFIG_UGFX_GUI_ENABLE

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

#if (GFX_USE_GINPUT && GINPUT_NEED_MOUSE)

#define GMOUSE_DRIVER_VMT    GMOUSEVMT_XPT2046

/* Input Includes */
#include "src/ginput/ginput_driver_mouse.h"
// Get the hardware interface
#include "gmouse_lld_XPT2046.h"

#define CMD_X                0xD1
#define CMD_Y                0x91
#define CMD_ENABLE_IRQ       0x80

static bool_t touch_get_xyz(GMouse *m, GMouseReading *pdr)
{
    // No buttons
    pdr->buttons = 0;
    pdr->z = 0;

    if (getpin_pressed(m)) {
        pdr->z = 1;                        // Set to Z_MAX as we are pressed

        aquire_bus(m);
        read_value(m, CMD_X);              // Dummy read - disable PenIRQ
        pdr->x = read_value(m, CMD_X);     // Read X-Value

        read_value(m, CMD_Y);              // Dummy read - disable PenIRQ
        pdr->y = read_value(m, CMD_Y);     // Read Y-Value

        read_value(m, CMD_ENABLE_IRQ);     // Enable IRQ

        release_bus(m);
    }
    return TRUE;
}

const GMouseVMT const GMOUSE_DRIVER_VMT[1] = {{
        {
            GDRIVER_TYPE_TOUCH,
            GMOUSE_VFLG_TOUCH | GMOUSE_VFLG_CALIBRATE | GMOUSE_VFLG_CAL_TEST |
            GMOUSE_VFLG_ONLY_DOWN | GMOUSE_VFLG_POORUPDOWN,
            sizeof(GMouse) + GMOUSE_BOARD_DATA_SIZE,
            _gmouseInitDriver,
            _gmousePostInitDriver,
            _gmouseDeInitDriver
        },
        1,                // z_max - (currently?) not supported
        0,                // z_min - (currently?) not supported
        1,                // z_touchon
        0,                // z_touchoff
        {
            // pen_jitter
            GMOUSE_PEN_CALIBRATE_ERROR,            // calibrate
            GMOUSE_PEN_CLICK_ERROR,                // click
            GMOUSE_PEN_MOVE_ERROR                  // move
        },
        {
            // finger_jitter
            GMOUSE_FINGER_CALIBRATE_ERROR,        // calibrate
            GMOUSE_FINGER_CLICK_ERROR,            // click
            GMOUSE_FINGER_MOVE_ERROR              // move
        },
        touch_init_board,     // init
        0,                    // deinit
        touch_get_xyz,        // get
        touch_save_calibration,                // calsave
        touch_load_calibration                 // calload
    }
};

#endif /* GFX_USE_GINPUT && GINPUT_NEED_MOUSE */

#endif /* CONFIG_UGFX_GUI_ENABLE */
