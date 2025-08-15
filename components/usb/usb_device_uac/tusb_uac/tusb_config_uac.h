/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "uac_descriptors.h"

//------------- CLASS -------------//
// The number of audio interfaces
#define CFG_TUD_AUDIO             1

//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

// Beware of confusion!

// When using RX and TX it is out of the perspective of the device (us),
// so RX is the data received from the host (SPK) and TX is the data sent to the host (MIC).

// But IN and OUT are the perspective of the host,
// so OUT is the data received from the host (SPK) and IN is the data sent to the host (MIC).

// Enable feedback EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                             1

// Enable/disable conversion from 16.16 to 10.14 format on full-speed devices
#if CONFIG_UAC_SUPPORT_MACOS
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION              1
#endif

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                                TUD_AUDIO_DEVICE_DESC_LEN

// How many formats are used, need to adjust USB descriptor if changed
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS                               1

#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                         DEFAULT_SAMPLE_RATE
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX                           SPEAK_CHANNEL_NUM
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                           MIC_CHANNEL_NUM

// Sample type
#if SPK_FORMAT_PCM
// 16bit in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX          2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX                  16
#elif SPK_FORMAT_FLOAT
// 32bit in 32bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX          4
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX                  32
#endif

#if MIC_FORMAT_PCM
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX          2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX                  16
#elif MIC_FORMAT_FLOAT
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX          4
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX                  32
#endif

// EP and buffer size - for isochronous EP´s, the buffer and EP size are equal (different sizes would not make sense)
#define CFG_TUD_AUDIO_ENABLE_EP_IN                1

#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN    ((CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE / 1000 * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX) + 4)

#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ      CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN * (MIC_INTERVAL_MS + 1)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX         CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN  // Maximum EP IN size for all AS alternate settings used

//------------ SPK (RX / OUT) -------------//

// EP and buffer size - for isochronous EP´s, the buffer and EP size are equal (different sizes would not make sense)
#define CFG_TUD_AUDIO_ENABLE_EP_OUT               1

// +4 for audio feedback
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT   ((CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE / 1000 * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX) + 4)

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ     CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT * (SPK_INTERVAL_MS + 1)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX        CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT // Maximum EP IN size for all AS alternate settings used

// Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces - We restrict us here to have a constant number for all audio functions (which means this has to be the maximum number of AS interfaces an audio function has and a second audio function with less AS interfaces just wastes a few bytes)
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT             1

// Size of control request buffer
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ    64

#ifdef __cplusplus
}
#endif
