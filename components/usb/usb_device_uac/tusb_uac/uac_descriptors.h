/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Jerzy Kasenbreg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "uac_config.h"
#include "sdkconfig.h"

#if !CONFIG_USB_DEVICE_UAC_AS_PART
enum {
    ITF_NUM_AUDIO_CONTROL = 0,
#if SPEAK_CHANNEL_NUM
    ITF_NUM_AUDIO_STREAMING_SPK,
#endif // SPEAK_CHANNEL_NUM
#if MIC_CHANNEL_NUM
    ITF_NUM_AUDIO_STREAMING_MIC,
#endif // MIC_CHANNEL_NUM
    ITF_NUM_TOTAL
};
#endif

// Unit numbers are arbitrary selected
#define UAC2_ENTITY_CLOCK                0x04
// Speaker path
#define UAC2_ENTITY_SPK_INPUT_TERMINAL   0x01
#define UAC2_ENTITY_SPK_FEATURE_UNIT     0x02
#define UAC2_ENTITY_SPK_OUTPUT_TERMINAL  0x03
// Microphone path
#define UAC2_ENTITY_MIC_INPUT_TERMINAL   0x11
#define UAC2_ENTITY_MIC_FEATURE_TERMINAL 0x12
#define UAC2_ENTITY_MIC_OUTPUT_TERMINAL  0x13

#if SPEAK_CHANNEL_NUM && MIC_CHANNEL_NUM
#define NUM_INTERFACES 3
#elif SPEAK_CHANNEL_NUM || MIC_CHANNEL_NUM
#define NUM_INTERFACES 2
#endif

#if SPEAK_CHANNEL_NUM && MIC_CHANNEL_NUM
#define TUD_AUDIO_DESC_CS_AC_TOTAL_LEN ( \
    TUD_AUDIO_DESC_CLK_SRC_LEN\
    +TUD_AUDIO_DESC_SPK_FEATURE_UNIT_N_CHANNEL_LEN\
    +TUD_AUDIO_DESC_INPUT_TERM_LEN\
    +TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
    +TUD_AUDIO_DESC_INPUT_TERM_LEN\
    +TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
    +TUD_AUDIO_DESC_MIC_FEATURE_UNIT_N_CHANNEL_LEN)

#define TUD_AUDIO_DEVICE_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
    + TUD_AUDIO_DESC_STD_AC_LEN\
    + TUD_AUDIO_DESC_CS_AC_LEN\
    + TUD_AUDIO_DESC_CS_AC_TOTAL_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    + TUD_AUDIO_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN\
    /* Interface 2, Alternate 0 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    /* Interface 2, Alternate 1 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    + TUD_AUDIO_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_FB_EP_LEN)

#define TUD_AUDIO_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epfb)  TUD_AUDIO_MIC_SPEAK_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epfb)

#elif MIC_CHANNEL_NUM
#define TUD_AUDIO_DESC_CS_AC_TOTAL_LEN ( \
    TUD_AUDIO_DESC_CLK_SRC_LEN\
    +TUD_AUDIO_DESC_INPUT_TERM_LEN\
    +TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
    +TUD_AUDIO_DESC_MIC_FEATURE_UNIT_N_CHANNEL_LEN)

#define TUD_AUDIO_DEVICE_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
    + TUD_AUDIO_DESC_STD_AC_LEN\
    + TUD_AUDIO_DESC_CS_AC_LEN\
    + TUD_AUDIO_DESC_CS_AC_TOTAL_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    + TUD_AUDIO_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epfb)  TUD_AUDIO_MIC_DESCRIPTOR(_itfnum, _stridx, _epin)

#elif SPEAK_CHANNEL_NUM
#define TUD_AUDIO_DESC_CS_AC_TOTAL_LEN ( \
    TUD_AUDIO_DESC_CLK_SRC_LEN\
    +TUD_AUDIO_DESC_SPK_FEATURE_UNIT_N_CHANNEL_LEN\
    +TUD_AUDIO_DESC_INPUT_TERM_LEN\
    +TUD_AUDIO_DESC_OUTPUT_TERM_LEN)

#define TUD_AUDIO_DEVICE_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
    + TUD_AUDIO_DESC_STD_AC_LEN\
    + TUD_AUDIO_DESC_CS_AC_LEN\
    + TUD_AUDIO_DESC_CS_AC_TOTAL_LEN\
    /* Interface 1, Alternate 0 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    /* Interface 1, Alternate 1 */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    + TUD_AUDIO_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_FB_EP_LEN)

#define TUD_AUDIO_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epfb)  TUD_AUDIO_SPEAK_DESCRIPTOR(_itfnum, _stridx, _epout, _epfb)

#endif

/**
 * @brief UAC 2.0 Stereo Microphone + Speaker
 *
 */
#if SPEAK_CHANNEL_NUM && MIC_CHANNEL_NUM
#define TUD_AUDIO_MIC_SPEAK_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epfb) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO_DESC_IAD(/*_firstitfs*/ _itfnum, /*_nitfs*/ NUM_INTERFACES, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_HEADSET, /*_totallen*/ TUD_AUDIO_DESC_CS_AC_TOTAL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),\
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ SPEAK_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Feature Unit Descriptor(4.7.2.8) */\
    TUD_AUDIO_DESC_FEATURE_UNIT_N_CHANNEL(/*_length*/ TUD_AUDIO_DESC_SPK_FEATURE_UNIT_N_CHANNEL_LEN,/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, INPUT_CTRL),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ MIC_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_FRONT_CENTER, /*_idxchannelnames*/ 0x00, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_FEATURE_UNIT_N_CHANNEL(/*_length*/ TUD_AUDIO_DESC_MIC_FEATURE_UNIT_N_CHANNEL_LEN, /*_unitid*/ UAC2_ENTITY_MIC_FEATURE_TERMINAL, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00, MIC_CTRL),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 1, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx + 1),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 1, /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ _stridx + 1),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ SPEAK_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT, /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(/*_ep*/ _epfb, /*_epsize*/ 4, /*_interval*/TUD_OPT_HIGH_SPEED ? 4 : 1),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 2, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx + 2),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 2, /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx + 2),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ MIC_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_FRONT_CENTER, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN, /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)
#endif

/**
 * @brief UAC 2.0 Stereo Microphone
 *
 */
#if MIC_CHANNEL_NUM
#define TUD_AUDIO_MIC_DESCRIPTOR(_itfnum, _stridx, _epin) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO_DESC_IAD(/*_firstitfs*/ _itfnum, /*_nitfs*/ NUM_INTERFACES, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_MICROPHONE, /*_totallen*/ TUD_AUDIO_DESC_CS_AC_TOTAL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 1, /*_ctrl*/ 1, /*_assocTerm*/ UAC2_ENTITY_MIC_INPUT_TERMINAL,  /*_stridx*/ 0x00),\
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ MIC_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_FRONT_CENTER, /*_idxchannelnames*/ 0x00, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_srcid*/ UAC2_ENTITY_MIC_FEATURE_TERMINAL, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_FEATURE_UNIT_N_CHANNEL(/*_length*/ TUD_AUDIO_DESC_MIC_FEATURE_UNIT_N_CHANNEL_LEN, /*_unitid*/ UAC2_ENTITY_MIC_FEATURE_TERMINAL, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00, MIC_CTRL),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 1, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx + 1),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 1, /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx + 1),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ MIC_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_FRONT_CENTER, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN, /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)
#endif

/**
 * @brief UAC 2.0 Stereo Speaker
 *
 */
#if SPEAK_CHANNEL_NUM
#define TUD_AUDIO_SPEAK_DESCRIPTOR(_itfnum, _stridx, _epout, _epfb) \
    /* Standard Interface Association Descriptor (IAD) */\
    TUD_AUDIO_DESC_IAD(/*_firstitfs*/ _itfnum, /*_nitfs*/ NUM_INTERFACES, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_DESKTOP_SPEAKER, /*_totallen*/ TUD_AUDIO_DESC_CS_AC_TOTAL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  /*_stridx*/ 0x00),\
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ SPEAK_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), /*_stridx*/ 0x00),\
    /* Feature Unit Descriptor(4.7.2.8) */\
    TUD_AUDIO_DESC_FEATURE_UNIT_N_CHANNEL(/*_length*/ TUD_AUDIO_DESC_SPK_FEATURE_UNIT_N_CHANNEL_LEN, /*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_stridx*/ 0x00, INPUT_CTRL),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 1, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx + 1),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ _itfnum + 1, /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ _stridx + 1),\
    /* Class-Specific AS Interface Descriptor(4.9.2) */\
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ SPEAK_CHANNEL_NUM, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
    TUD_AUDIO_DESC_TYPE_I_FORMAT(CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ _epout, /*_attr*/ (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT, /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
    TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(/*_ep*/ _epfb, /*_epsize*/ 4, /*_interval*/TUD_OPT_HIGH_SPEED ? 4 : 1)
#endif

#ifdef __cplusplus
}
#endif
