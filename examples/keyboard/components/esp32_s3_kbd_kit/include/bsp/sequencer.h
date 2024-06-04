/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Maximum number of steps: 256
#ifndef SEQUENCER_STEPS
#    define SEQUENCER_STEPS 16
#endif

// Maximum number of tracks: 8
#ifndef SEQUENCER_TRACKS
#    define SEQUENCER_TRACKS 8
#endif

#ifndef SEQUENCER_TRACK_THROTTLE
#    define SEQUENCER_TRACK_THROTTLE 3
#endif

#ifndef SEQUENCER_PHASE_RELEASE_TIMEOUT
#    define SEQUENCER_PHASE_RELEASE_TIMEOUT 30
#endif

/**
 * Make sure that the items of this enumeration follow the powers of 2, separated by a ternary variant.
 * Check the implementation of `get_step_duration` for further explanation.
 */
typedef enum {
    SQ_RES_2, //
    SQ_RES_2T,
    SQ_RES_4,
    SQ_RES_4T,
    SQ_RES_8,
    SQ_RES_8T,
    SQ_RES_16,
    SQ_RES_16T,
    SQ_RES_32,
    SEQUENCER_RESOLUTIONS
} sequencer_resolution_t;
