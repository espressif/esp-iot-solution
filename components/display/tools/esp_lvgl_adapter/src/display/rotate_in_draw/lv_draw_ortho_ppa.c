/*
 * SPDX-FileCopyrightText: 2026 youkorr
 *
 * SPDX-License-Identifier: MIT
 *
 * From https://github.com/youkorr/ppa-alternative- (components/lvgl_ortho).
 */

#include "lv_draw_ortho_ppa.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_rom_sys.h"

/* The PPA's output must start on, and its size be a multiple of, the L2
 * cache line (ESP-IDF's ppa.rst, "Buffer Alignment"). */
#ifdef CONFIG_CACHE_L2_CACHE_LINE_SIZE
#define LINE CONFIG_CACHE_L2_CACHE_LINE_SIZE
#else
#define LINE 128
#endif

/* Two tiles of up to four parts each, every part possibly cut in pieces for
 * DIG-734, and a read-back: what can be queued at once. A full queue is
 * declined and the CPU does that turn. */
#define PENDING 32
#define MAX_PIECES 24

static ppa_client_handle_t g_client;
static uint32_t g_queued;            /* tickets handed out */
static volatile uint32_t g_done;     /* counted by the PPA's interrupt */
/* Fills run on the PPA's other engine (blend), with a client and a count of
 * their own: the two engines finish in no order relative to each other. */
static ppa_client_handle_t g_fill_client;
static uint32_t g_fill_queued;
static volatile uint32_t g_fill_done;
static bool g_stuck;

bool lv_draw_ortho_ppa_stuck(void)
{
    return g_stuck;
}

void lv_draw_ortho_ppa_mark_stuck(void)
{
    g_stuck = true;
}

/* ------------------------------------------------------------------------ */
/* DIG-734                                                                  */
/* ------------------------------------------------------------------------ */

/* The driver's test (ppa_srm.c), for a block it reads as w_in x h_in: whether
 * it bypasses the macro-block order, which is what keeps a small last block
 * from leaving the DMA short of its FIFO and never finishing. */
static bool bypass(uint32_t w_in, uint32_t h_in, uint32_t bits, uint32_t mb, uint32_t sx_int, uint32_t sx_frag,
                   uint32_t sy_int, uint32_t sy_frag)
{
    const uint32_t FRAG = 16;            /* PPA_LL_SRM_SCALING_FRAG_MAX: 4 bits */
    const uint32_t FIFO = 12 * 128;      /* dma2d_fifo_depth_bits */
    const uint32_t w_out = w_in * sx_int + w_in * sx_frag / FRAG;
    const uint32_t wdiv = bits >= 24 ? 32 : 64;
    uint32_t w_left = w_out % wdiv;
    if (w_left == 0) {
        w_left = wdiv;
    }
    uint32_t h_in_left = h_in % mb;
    if (h_in_left == 0) {
        h_in_left = mb;
    }
    const uint32_t h_left = h_in_left * sy_int + h_in_left * sy_frag / FRAG;
    return (w_out > wdiv || h_in > mb) && w_left * h_left * bits < FIFO;
}

bool lv_draw_ortho_ppa_dig734(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits,
                              float scale_x, float scale_y)
{
    if (rot != LV_DISPLAY_ROTATION_90 && rot != LV_DISPLAY_ROTATION_270) {
        return false;
    }
    /* As the driver quantizes them. */
    const uint32_t xi = (uint32_t)scale_x, xf = (uint32_t)(scale_x * 16) & 15;
    const uint32_t yi = (uint32_t)scale_y, yf = (uint32_t)(scale_y * 16) & 15;
    /* 16x16 macro blocks below silicon v3.0, 32x32 selectable on it: both. */
    for (uint32_t mb = 16; mb <= 32; mb += 16) {
        /* What the block needs (master: sized after the turn) against what
         * v5.5.6 gives it (sized before). Asking for the bypass when it is not
         * needed is harmless; withholding it when it is, is the hang. */
        const bool needed = bypass(h, w, bits, mb, yi, yf, xi, xf);
        const bool given = bypass(w, h, bits, mb, xi, xf, yi, yf);
        if (needed && !given) {
            return true;
        }
    }
    return false;
}

typedef struct {
    uint32_t x, y, w, h;
} piece_t;

/* Cut a block into pieces each safe from DIG-734: first where the macro
 * blocks fall, then anywhere. Every size up to 1300 x 1300 was checked to cut
 * into at most 24. False if it will not (the CPU turns that block). */
static bool cut(uint32_t x, uint32_t y, uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits,
                piece_t * out, int * n, int depth)
{
    if (!lv_draw_ortho_ppa_dig734(w, h, rot, bits, 1.0f, 1.0f)) {
        if (*n == MAX_PIECES) {
            return false;
        }
        out[(*n)++] = (piece_t) {
            x, y, w, h
        };
        return true;
    }
    if (depth > 6) {
        return false;
    }
    for (int dim = 0; dim < 2; dim++) {
        const uint32_t size = dim == 0 ? w : h;
        const uint32_t units[3] = {64, 16, 2};
        uint32_t cuts[6];
        int nc = 0;
        for (int u = 0; u < 3; u++) {
            cuts[nc++] = size % units[u] ? size - size % units[u] : size - units[u];
        }
        cuts[nc++] = size / 2;
        cuts[nc++] = 1;
        cuts[nc++] = size - 1;
        for (int i = 0; i < nc; i++) {
            const uint32_t c = cuts[i];
            if (c == 0 || c >= size) {
                continue;
            }
            const int mark = *n;
            const bool ok = dim == 0
                            ? cut(x, y, c, h, rot, bits, out, n, depth + 1) && cut(x + c, y, w - c, h, rot, bits, out, n, depth + 1)
                            : cut(x, y, w, c, rot, bits, out, n, depth + 1) && cut(x, y + c, w, h - c, rot, bits, out, n, depth + 1);
            if (ok) {
                return true;
            }
            *n = mark;
        }
    }
    return false;
}

static bool on_done(ppa_client_handle_t client, ppa_event_data_t * event, void * user)
{
    (void)event;
    (void)user;
    if (client == g_fill_client) {
        g_fill_done++;
    } else {
        g_done++;
    }
    return false;
}

/* Until `*done` reaches `ticket`, bounded the same way for both engines. */
static bool wait_on(volatile uint32_t * done, uint32_t ticket)
{
    uint32_t seen = *done, idle_us = 0;
    while ((int32_t)(*done - ticket) < 0) {
        if (g_stuck) {
            return false;
        }
        esp_rom_delay_us(10);
        if (*done != seen) {
            seen = *done;
            idle_us = 0;
        } else if ((idle_us += 10) >= LV_DRAW_ORTHO_PPA_TIMEOUT_MS * 1000u) {
            g_stuck = true;
            return false;
        }
    }
    return true;
}

/* As in lv_draw_ortho_video.c: LVGL's 90 and the PPA's 90 are the same turn. */
static ppa_srm_rotation_angle_t ppa_angle(lv_display_rotation_t r)
{
    switch (r) {
    case LV_DISPLAY_ROTATION_90:
        return PPA_SRM_ROTATION_ANGLE_90;
    case LV_DISPLAY_ROTATION_180:
        return PPA_SRM_ROTATION_ANGLE_180;
    case LV_DISPLAY_ROTATION_270:
        return PPA_SRM_ROTATION_ANGLE_270;
    default:
        return PPA_SRM_ROTATION_ANGLE_0;
    }
}

static uint32_t turn(const lv_draw_ortho_pic_t * src, const lv_area_t * block, const lv_draw_ortho_pic_t * dst,
                     int32_t dx, int32_t dy, lv_display_rotation_t rot, lv_color_format_t cf)
{
    const ppa_srm_color_mode_t cm = cf == LV_COLOR_FORMAT_RGB565 ? PPA_SRM_COLOR_MODE_RGB565
                                    : cf == LV_COLOR_FORMAT_RGB888 ? PPA_SRM_COLOR_MODE_RGB888
                                    : (ppa_srm_color_mode_t) - 1;
    if ((int)cm == -1 || g_client == NULL || g_stuck) {
        return 0;
    }
    const uint32_t px = lv_color_format_get_size(cf);
    /* The PPA counts a row as pic_w pixels: no padding in a stride. */
    if (src->stride != src->w * px || dst->stride != dst->w * px) {
        return 0;
    }
    if (((uintptr_t)dst->buf % LINE) != 0) {
        return 0;
    }

    const bool across = rot == LV_DISPLAY_ROTATION_90 || rot == LV_DISPLAY_ROTATION_270;
    const uint32_t bw = (uint32_t)lv_area_get_width(block), bh = (uint32_t)lv_area_get_height(block);
    const uint32_t rows = across ? bw : bh;
    /* The driver invalidates whole rows of the output from dy, rounded to the
     * line: they have to lie inside the buffer it is told about. */
    const uint32_t size = (dst->stride * dst->h) & ~(uint32_t)(LINE - 1);
    uintptr_t a = (uintptr_t)dst->buf + (uintptr_t)dy * dst->stride;
    uintptr_t b = a + (uintptr_t)rows * dst->stride;
    if (b > (uintptr_t)dst->buf + size) {
        return 0;
    }

    /* Write back and drop the output rows first. The CPU has drawn right up to
     * this block's edges, and a cache line it shares with the block holds its
     * pixels too: the driver's invalidate would throw those away, and a later
     * write-back would put stale ones over the PPA's. lv_draw_ortho_video.c
     * does the same for the same reason. */
    a &= ~(uintptr_t)(LINE - 1);
    b = (b + LINE - 1) & ~(uintptr_t)(LINE - 1);
    if (esp_cache_msync((void *)a, b - a, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE) != ESP_OK) {
        return 0;
    }

    piece_t pieces[MAX_PIECES];
    int n = 0;
    if (!cut(0, 0, bw, bh, rot, px * 8, pieces, &n, 0)) {
        return 0;
    }
    if (g_queued - g_done + (uint32_t)n > PENDING) {
        return 0;
    }

    ppa_srm_oper_config_t op;
    lv_memzero(&op, sizeof(op));
    op.in.buffer = src->buf;
    op.in.pic_w = src->w;
    op.in.pic_h = src->h;
    op.in.srm_cm = cm;
    op.out.buffer = dst->buf;
    op.out.buffer_size = size;
    op.out.pic_w = dst->w;
    op.out.pic_h = dst->h;
    op.out.srm_cm = cm;
    op.rotation_angle = ppa_angle(rot);
    op.scale_x = 1.0f;
    op.scale_y = 1.0f;
    op.mode = PPA_TRANS_MODE_NON_BLOCKING;
    for (int i = 0; i < n; i++) {
        const piece_t * p = &pieces[i];
        /* Where the piece lands inside the block's output: the PPA turns
         * counter-clockwise on the screen (see p4fake.c and
         * lv_draw_ortho_video.c), so its 90 sends input column x to output
         * row bw - 1 - x. */
        uint32_t ox, oy;
        switch (rot) {
        case LV_DISPLAY_ROTATION_90:
            ox = p->y;
            oy = bw - p->x - p->w;
            break;
        case LV_DISPLAY_ROTATION_180:
            ox = bw - p->x - p->w;
            oy = bh - p->y - p->h;
            break;
        case LV_DISPLAY_ROTATION_270:
            ox = bh - p->y - p->h;
            oy = p->x;
            break;
        default:
            ox = p->x;
            oy = p->y;
            break;
        }
        op.in.block_w = p->w;
        op.in.block_h = p->h;
        op.in.block_offset_x = (uint32_t)block->x1 + p->x;
        op.in.block_offset_y = (uint32_t)block->y1 + p->y;
        op.out.block_offset_x = (uint32_t)dx + ox;
        op.out.block_offset_y = (uint32_t)dy + oy;
        /* A piece refused after others were queued: those finish, and the
         * CPU then turns the whole block over them -- the same pixels. */
        if (ppa_do_scale_rotate_mirror(g_client, &op) != ESP_OK) {
            return 0;
        }
        ++g_queued;
    }
    return g_queued;
}

/* The PPA finishes its transactions in the order they were queued, so a
 * ticket is done once that many have finished. Bounded: a transaction that
 * makes no progress for LV_DRAW_ORTHO_PPA_TIMEOUT_MS is a stuck PPA. */
static bool wait(uint32_t ticket)
{
    return wait_on(&g_done, ticket);
}

/* A plain opaque rectangle into the frame buffer, by the PPA's fill. The
 * driver writes back and invalidates the rows itself (ppa_fill.c). The colour
 * is given with its low bits already cleared to RGB565's (or RGB888's)
 * precision, so whether the PPA truncates or rounds when it narrows it, the
 * pixel is the one LVGL's lv_color_to_u16() would have written. Waited for
 * before returning: what follows in the frame buffer is drawn over it. */
static bool fill(const lv_draw_ortho_pic_t * dst, const lv_area_t * rect, lv_color_t color, lv_color_format_t cf)
{
    if (g_fill_client == NULL || g_stuck) {
        return false;
    }
    const uint32_t px = lv_color_format_get_size(cf);
    const ppa_fill_color_mode_t cm = cf == LV_COLOR_FORMAT_RGB565 ? PPA_FILL_COLOR_MODE_RGB565
                                     : cf == LV_COLOR_FORMAT_RGB888 ? PPA_FILL_COLOR_MODE_RGB888
                                     : (ppa_fill_color_mode_t) - 1;
    if ((int)cm == -1 || dst->stride != dst->w * px || ((uintptr_t)dst->buf % LINE) != 0) {
        return false;
    }
    const uint32_t size = (dst->stride * dst->h) & ~(uint32_t)(LINE - 1);
    if (dst->stride * dst->h > size) {
        return false;    /* the driver wants the whole picture in buffer_size */
    }

    ppa_fill_oper_config_t op;
    lv_memzero(&op, sizeof(op));
    op.out.buffer = dst->buf;
    op.out.buffer_size = size;
    op.out.pic_w = dst->w;
    op.out.pic_h = dst->h;
    op.out.block_offset_x = (uint32_t)rect->x1;
    op.out.block_offset_y = (uint32_t)rect->y1;
    op.out.fill_cm = cm;
    op.fill_block_w = (uint32_t)lv_area_get_width(rect);
    op.fill_block_h = (uint32_t)lv_area_get_height(rect);
    const uint32_t r = cf == LV_COLOR_FORMAT_RGB565 ? (color.red & 0xF8u) : color.red;
    const uint32_t g = cf == LV_COLOR_FORMAT_RGB565 ? (color.green & 0xFCu) : color.green;
    const uint32_t b = cf == LV_COLOR_FORMAT_RGB565 ? (color.blue & 0xF8u) : color.blue;
    op.fill_argb_color.val = 0xFF000000u | (r << 16) | (g << 8) | b;
    op.mode = PPA_TRANS_MODE_NON_BLOCKING;
    if (ppa_do_fill(g_fill_client, &op) != ESP_OK) {
        return false;
    }
    return wait_on(&g_fill_done, ++g_fill_queued);
}

static esp_err_t ensure_client(void)
{
    if (g_client) {
        return ESP_OK;
    }
    ppa_client_config_t cfg;
    lv_memzero(&cfg, sizeof(cfg));
    cfg.oper_type = PPA_OPERATION_SRM;
    cfg.max_pending_trans_num = PENDING;
    esp_err_t err = ppa_register_client(&cfg, &g_client);
    if (err != ESP_OK) {
        return err;
    }
    ppa_event_callbacks_t cbs = {.on_trans_done = on_done};
    err = ppa_client_register_event_callbacks(g_client, &cbs);
    if (err != ESP_OK) {
        ppa_unregister_client(g_client);
        g_client = NULL;
        return err;
    }
    g_queued = g_done = 0;
    /* The fill client: optional, the turns work without it. */
    lv_memzero(&cfg, sizeof(cfg));
    cfg.oper_type = PPA_OPERATION_FILL;
    cfg.max_pending_trans_num = 1;
    if (ppa_register_client(&cfg, &g_fill_client) == ESP_OK) {
        ppa_event_callbacks_t fcbs = {.on_trans_done = on_done};
        if (ppa_client_register_event_callbacks(g_fill_client, &fcbs) != ESP_OK) {
            ppa_unregister_client(g_fill_client);
            g_fill_client = NULL;
        }
    }
    g_fill_queued = g_fill_done = 0;
    return ESP_OK;
}

esp_err_t lv_draw_ortho_ppa_init(void)
{
    const esp_err_t err = ensure_client();
    if (err != ESP_OK) {
        return err;
    }
    if (!g_stuck) {
        lv_draw_ortho_set_turner(turn, wait);
        lv_draw_ortho_set_filler(fill);
    }
    return ESP_OK;
}

esp_err_t lv_draw_ortho_ppa_srm(const ppa_srm_oper_config_t * in)
{
    if (g_stuck) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = ensure_client();
    if (err != ESP_OK) {
        return err;
    }
    ppa_srm_oper_config_t op = *in;
    op.mode = PPA_TRANS_MODE_NON_BLOCKING;
    const bool quarter = op.rotation_angle == PPA_SRM_ROTATION_ANGLE_90 ||
                         op.rotation_angle == PPA_SRM_ROTATION_ANGLE_270;
    const lv_display_rotation_t rot = op.rotation_angle == PPA_SRM_ROTATION_ANGLE_90 ? LV_DISPLAY_ROTATION_90
                                      : op.rotation_angle == PPA_SRM_ROTATION_ANGLE_270 ? LV_DISPLAY_ROTATION_270
                                      : LV_DISPLAY_ROTATION_0;
    const uint32_t bits = op.out.srm_cm == PPA_SRM_COLOR_MODE_RGB565 ? 16 : op.out.srm_cm == PPA_SRM_COLOR_MODE_RGB888 ? 24 : 32;
    piece_t pieces[MAX_PIECES];
    int n = 0;
    if (quarter && lv_draw_ortho_ppa_dig734(op.in.block_w, op.in.block_h, rot, bits, op.scale_x, op.scale_y)) {
        if (op.scale_x == 1.0f && op.scale_y == 1.0f && !op.mirror_x && !op.mirror_y &&
                cut(0, 0, op.in.block_w, op.in.block_h, rot, bits, pieces, &n, 0)) {
            /* pieces placed below */
        } else {
            /* Scaled or mirrored: a row or column off the frame's edge is
             * cheaper than placing pieces through a scale. */
            n = 0;
            bool safe = false;
            for (uint32_t k = 1; k <= 3 && !safe; k++) {
                if (op.in.block_h > k && !lv_draw_ortho_ppa_dig734(op.in.block_w, op.in.block_h - k, rot, bits,
                                                                   op.scale_x, op.scale_y)) {
                    op.in.block_h -= k;
                    safe = true;
                } else if (op.in.block_w > k && !lv_draw_ortho_ppa_dig734(op.in.block_w - k, op.in.block_h, rot, bits,
                                                                          op.scale_x, op.scale_y)) {
                    op.in.block_w -= k;
                    safe = true;
                }
            }
            if (!safe) {
                return ESP_ERR_NOT_SUPPORTED;
            }
        }
    }
    if (n == 0) pieces[n++] = (piece_t) {
        0, 0, op.in.block_w, op.in.block_h
    };
    const uint32_t bw = op.in.block_w, bh = op.in.block_h, bx = op.in.block_offset_x, by = op.in.block_offset_y;
    const uint32_t dx = op.out.block_offset_x, dy = op.out.block_offset_y;
    for (int i = 0; i < n; i++) {
        const piece_t * p = &pieces[i];
        uint32_t ox = p->x, oy = p->y;
        if (rot == LV_DISPLAY_ROTATION_90) {
            ox = p->y;
            oy = bw - p->x - p->w;
        } else if (rot == LV_DISPLAY_ROTATION_270) {
            ox = bh - p->y - p->h;
            oy = p->x;
        }
        op.in.block_w = p->w;
        op.in.block_h = p->h;
        op.in.block_offset_x = bx + p->x;
        op.in.block_offset_y = by + p->y;
        if (n > 1) {
            op.out.block_offset_x = dx + ox;
            op.out.block_offset_y = dy + oy;
        }
        err = ppa_do_scale_rotate_mirror(g_client, &op);
        if (err != ESP_OK) {
            /* What was queued still has to finish before the caller's buffer
             * is touched again. */
            wait(g_queued);
            return err;
        }
        ++g_queued;
    }
    return wait(g_queued) ? ESP_OK : ESP_ERR_TIMEOUT;
}

void lv_draw_ortho_ppa_deinit(void)
{
    lv_draw_ortho_set_turner(NULL, NULL);
    lv_draw_ortho_set_filler(NULL);
    if (g_fill_client && wait_on(&g_fill_done, g_fill_queued)) {
        ppa_unregister_client(g_fill_client);
        g_fill_client = NULL;
    }
    if (g_client && wait(g_queued)) {
        ppa_unregister_client(g_client);
        g_client = NULL;
    }
}
