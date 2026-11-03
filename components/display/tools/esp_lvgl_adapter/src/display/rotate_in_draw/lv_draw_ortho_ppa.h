/*
 * SPDX-FileCopyrightText: 2026 youkorr
 *
 * SPDX-License-Identifier: MIT
 *
 * From https://github.com/youkorr/ppa-alternative- (components/lvgl_ortho).
 */

/**
 * @file lv_draw_ortho_ppa.h
 *
 * lv_draw_ortho's turns done by the ESP32-P4's PPA instead of its CPU.
 *
 * lv_draw_ortho draws every widget upright into a tile in internal RAM and
 * puts the tile into the frame buffer turned. Simulated on a P4
 * (test/p4sim), the drawing reads and writes no PSRAM at all; the turn, done
 * by the CPU, is what costs -- five instructions a pixel and every frame-buffer
 * line through the cache. The PPA turns a block by DMA while the CPU goes on:
 * with this, the CPU draws the next tile while the PPA turns the last one.
 *
 * RGB565 and RGB888 only; anything else, a buffer the PPA cannot write, or a
 * full queue, and that turn is the CPU's as before.
 *
 * Two faults of the P4's PPA are kept away from, both found in ESP-IDF's
 * issues and neither handled by LVGL's own PPA unit nor by Espressif's ports:
 *
 *  - DIG-734 (esp-idf #19023, #19096, #18768): at 90 and 270, a block whose
 *    last macro block leaves the DMA fewer bits than its FIFO holds never
 *    finishes. The driver works around it, but up to v5.5.6 -- what ESPHome
 *    builds with -- it sizes that last block before the turn instead of after,
 *    so a sixth of all block sizes hang for ever (fixed on master by 469aa16).
 *    Such a block is cut here into pieces that are safe either way.
 *  - No wait is unbounded: a transaction that has not finished in
 *    LV_DRAW_ORTHO_PPA_TIMEOUT_MS marks the PPA stuck, and lv_draw_ortho and
 *    the video window go on without it. The driver's own blocking mode waits
 *    with portMAX_DELAY, which is a frozen screen.
 */
#ifndef LV_DRAW_ORTHO_PPA_H
#define LV_DRAW_ORTHO_PPA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_draw_ortho.h"
#include "esp_err.h"
#include "driver/ppa.h"

/** Register a PPA client and hand lv_draw_ortho's turns to it. */
esp_err_t lv_draw_ortho_ppa_init(void);

/** Hand the turns back to the CPU and release the PPA. */
void lv_draw_ortho_ppa_deinit(void);

/** One scale-rotate-mirror through the same client and the same guards: cut
 *  for DIG-734 when it is 1:1 and unmirrored, trimmed by up to three source
 *  rows or columns otherwise (the frame's edge, never its middle), and waited
 *  for with a bound. ESP_ERR_TIMEOUT marks the PPA stuck. `op->mode` is
 *  ignored. For lv_draw_ortho_video.c. */
esp_err_t lv_draw_ortho_ppa_srm(const ppa_srm_oper_config_t * op);

/** How long a PPA transaction may take before the PPA is taken for stuck. A
 *  whole 1280x720 turn takes 15 to 45 ms on a P4. */
#ifndef LV_DRAW_ORTHO_PPA_TIMEOUT_MS
#define LV_DRAW_ORTHO_PPA_TIMEOUT_MS 250
#endif

/** True once a PPA transaction has not finished in time: it is not used again
 *  until a reset, since nothing public can unstick it. */
bool lv_draw_ortho_ppa_stuck(void);
void lv_draw_ortho_ppa_mark_stuck(void);

/** Whether the PPA driver of ESP-IDF v5.5.6 and earlier can hang on this
 *  scale-rotate-mirror: `w` x `h` the INPUT block, `rot` LVGL's rotation, `bits`
 *  the output's bits a pixel, scales as given to the driver. DIG-734. */
bool lv_draw_ortho_ppa_dig734(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits,
                              float scale_x, float scale_y);

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_ORTHO_PPA_H */
