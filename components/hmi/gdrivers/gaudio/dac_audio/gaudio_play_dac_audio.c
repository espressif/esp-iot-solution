/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "iot_dac_audio.h"

/* uGFX Includes */
#include "gos_freertos_priv.h"
#include "gfx.h"

#if GFX_USE_GAUDIO && GAUDIO_NEED_PLAY

/* Include the driver defines */
#include "src/gaudio/gaudio_driver_play.h"

/* Include the board interface */
#include "gaudio_play_board_dac_audio.h"

static GTimer playTimer;
static GDataBuffer *pplay;
static ArrayDataFormat playfmt;
static size_t playlen;
static uint8_t *pdata;
static bool isinit = false;

static void gaudio_play(void *param)
{
    unsigned len;
    (void)param;

    while (pplay != NULL) {

        if (gaudio_play_dac_audio_play(pdata, playlen, portMAX_DELAY) == true) {
            playlen -= playlen;
            // Have we finished the buffer
            while (!playlen) {
                gfxSystemLock();
                gaudioPlayReleaseDataBlockI(pplay);

                // Get a new data buffer
                if (!(pplay = gaudioPlayGetDataBlockI())) {
                    // We should really only do the play-done when the audio
                    // has really finished playing. Unfortunately there seems
                    // to be no documented way of determining this.
                    gaudioPlayDoneI();
                    gfxSystemUnlock();
                    gtimerStop(&playTimer);
                    return;
                }

                // Set up ready for the new buffer
                playlen = pplay->len;
                pdata = (uint8_t *)(pplay + 1);
                gfxSystemUnlock();
            }
        } else {
            gaudio_play_dac_audio_play(pdata, playlen, portMAX_DELAY);
        }
    }
}

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

bool_t gaudio_play_lld_init(uint16_t channel, uint32_t frequency, ArrayDataFormat format)
{
    (void)channel;
    if (!isinit) {
        /* Initialise the timer */
        gtimerInit(&playTimer);

        if (format != ARRAY_DATA_8BITUNSIGNED) {
            return FALSE;
        }

        playfmt = format;
        isinit = true;
        return gaudio_play_dac_audio_setup(frequency, format);
    }
    return true;
}

void gaudio_play_lld_start(void)
{
    gfxSystemLock();
    // Get a new data buffer
    if (pplay || !(pplay = gaudioPlayGetDataBlockI())) {
        gfxSystemUnlock(); // Nothing to do
        return;
    }

    // Set up ready for the new buffer
    playlen = pplay->len;
    if (gfxSampleFormatBits(playfmt) > 8) {
        playlen >>= 1;
    }
    pdata = (uint8_t *)(pplay + 1);
    gfxSystemUnlock();

    // Start the playing by starting the timer and executing gaudio_play immediately just to get things started
    // We really should set the timer to be equivalent to half the available data but that is just too hard to calculate.
    // gtimerStart(&playTimer, gaudio_play, 0, TRUE, 5);
    gaudio_play(0);
}

void gaudio_play_lld_stop(void)
{
    // Stop the timer interrupt
    gtimerStop(&playTimer);

    // We may need to clean up the remaining buffer.
    gfxSystemLock();
    if (pplay) {
        gaudioPlayReleaseDataBlockI(pplay);
        pplay = 0;
        gaudioPlayDoneI();
    }
    gfxSystemUnlock();
}

bool_t gaudio_play_lld_set_volume(uint8_t vol)
{
    (void)vol;
    return FALSE;
}

#endif /* GFX_USE_GAUDIO && GAUDIO_NEED_PLAY */
