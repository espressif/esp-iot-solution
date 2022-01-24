/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 XMOS LIMITED
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
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_DFU_DEVICE_H_
#define _TUSB_DFU_DEVICE_H_

#include "dfu.h"

#ifdef __cplusplus
  extern "C" {
#endif

//--------------------------------------------------------------------+
// Class Driver Default Configure & Validation
//--------------------------------------------------------------------+

#if !defined(CFG_TUD_DFU_XFER_BUFSIZE)
#define CFG_TUD_DFU_XFER_BUFSIZE    512
#endif

#define TU_U32_BYTE3(_u32)    ((uint8_t) ((((uint32_t) _u32) >> 24) & 0x000000ff)) // MSB
#define TU_U32_BYTE2(_u32)    ((uint8_t) ((((uint32_t) _u32) >> 16) & 0x000000ff))
#define TU_U32_BYTE1(_u32)    ((uint8_t) ((((uint32_t) _u32) >>  8) & 0x000000ff))
#define TU_U32_BYTE0(_u32)    ((uint8_t) (((uint32_t)  _u32)        & 0x000000ff)) // LSB

// Length of template descriptor: 9 bytes + number of alternatives * 9
#define TUD_DFU_DESC_LEN(_alt_count)    (9 + (_alt_count) * 9)

// Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
// Note: Alternate count must be numberic or macro, string index is increased by one for each Alt interface
#define TUD_DFU_DESCRIPTOR(_itfnum, _alt_count, _stridx, _attr, _timeout, _xfer_size) \
  TU_XSTRCAT(_TUD_DFU_ALT_,_alt_count)(_itfnum, 0, _stridx), \
  /* Function */ \
  9, DFU_DESC_FUNCTIONAL, _attr, U16_TO_U8S_LE(_timeout), U16_TO_U8S_LE(_xfer_size), U16_TO_U8S_LE(0x0101)

#define _TUD_DFU_ALT(_itfnum, _alt, _stridx) \
  /* Interface */ \
  9, TUSB_DESC_INTERFACE, _itfnum, _alt, 0, TUD_DFU_APP_CLASS, TUD_DFU_APP_SUBCLASS, DFU_PROTOCOL_DFU, _stridx

#define _TUD_DFU_ALT_1(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx)

#define _TUD_DFU_ALT_2(_itfnum, _alt_count, _stridx) \
  _TUD_DFU_ALT(_itfnum, _alt_count, _stridx),      \
  _TUD_DFU_ALT_1(_itfnum, _alt_count+1, _stridx+1)

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Must be called when the application is done with flashing started by
// tud_dfu_download_cb() and tud_dfu_manifest_cb().
// status is DFU_STATUS_OK if successful, any other error status will cause state to enter dfuError
void tud_dfu_finish_flashing(uint8_t status);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.

// Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
// Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
// During this period, USB host won't try to communicate with us.
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state);

// Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
// This callback could be returned before flashing op is complete (async).
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_download_cb (uint8_t alt, uint16_t block_num, uint8_t const *data, uint16_t length);

// Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
// Application can do checksum, or actual flashing if buffered entire image previously.
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_manifest_cb(uint8_t alt);

// Invoked when received DFU_UPLOAD request
// Application must populate data with up to length bytes and
// Return the number of written bytes
TU_ATTR_WEAK uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length);

// Invoked when a DFU_DETACH request is received
TU_ATTR_WEAK void tud_dfu_detach_cb(void);

// Invoked when the Host has terminated a download or upload transfer
TU_ATTR_WEAK void tud_dfu_abort_cb(uint8_t alt);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void     dfu_moded_init(void);
void     dfu_moded_reset(uint8_t rhport);
uint16_t dfu_moded_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool     dfu_moded_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_MODE_DEVICE_H_ */
