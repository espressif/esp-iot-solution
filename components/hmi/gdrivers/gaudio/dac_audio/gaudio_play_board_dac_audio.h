/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef GAUDIO_PLAY_BOARD_H
#define GAUDIO_PLAY_BOARD_H

static dac_audio_handle_t dac = NULL;

static bool gaudio_play_dac_audio_setup(uint32_t frequency, ArrayDataFormat format)
{
    dac = iot_dac_audio_create(0, frequency, 16, I2S_DAC_CHANNEL_RIGHT_EN, 1024, true);
    /* Return FALSE if any parameter invalid */
    return true;
}

static bool gaudio_play_dac_audio_play(const uint8_t *data, int length, TickType_t ticks_to_wait)
{
    if (iot_dac_audio_play(dac, data, length, ticks_to_wait) == 0) {
        return true;
    } else {
        return false;
    }
}

static void gaudio_play_dac_audio_start(void)
{
    /* Start the DAC Audio */
}

static void gaudio_play_dac_audio_stop(void)
{
    /* Stop the DAC Audio */
}

#endif /* GAUDIO_PLAY_BOARD_H */
