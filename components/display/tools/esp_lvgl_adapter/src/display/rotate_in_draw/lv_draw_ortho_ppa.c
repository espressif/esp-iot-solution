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
#include <stdio.h>

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
/* cut() needs at most 7 over every size up to 1300 x 1300 (test/main.c). */
#define MAX_PIECES 8

static ppa_client_handle_t g_client;
static uint32_t g_queued;            /* tickets handed out */
static volatile uint32_t g_done;     /* counted by the PPA's interrupt */
/* Fills run on the PPA's other engine (blend), with a client and a count of
 * their own: the two engines finish in no order relative to each other. */
static ppa_client_handle_t g_fill_client;
static uint32_t g_fill_queued;
static volatile uint32_t g_fill_done;
static bool g_stuck;

/* The last transactions handed to the PPA, so a stall can say which one never
 * finished: nothing else on a board can. Room for every one that can be
 * pending on both engines, and the last few finished before them. */
#define TRAIL 48
typedef struct {
    const void * in, * out;
    uint32_t ticket;
    uint16_t in_w, in_h, bw, bh, bx, by;
    uint16_t out_w, out_h, ox, oy;
    uint8_t kind;     /* 0 a tile turned, 1 a video frame, 2 a fill */
    uint8_t rot;      /* LVGL's rotation */
    uint8_t bits;
    uint8_t bypass;   /* bypass_bits() */
    uint16_t left_in, left_out;   /* bits the last 16x16 macro block leaves, as read and as turned */
} trail_t;
static trail_t g_trail[TRAIL];
static uint32_t g_trail_n;
static uint8_t g_stuck_engine;       /* 1 scale-rotate-mirror, 2 fill */
static uint32_t g_stuck_done, g_stuck_queued;

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

/* The driver's test (ppa_srm.c), for a block it reads as w_in x h_in: how
 * many bits its last macro block leaves the DMA, and whether the block is more
 * than one unit -- the two things that decide whether it bypasses the
 * macro-block order, which keeps a small last block from leaving the DMA short
 * of its FIFO and never finishing. */
#define FIFO_BITS (12 * 128)   /* dma2d_fifo_depth_bits */
static uint32_t leftover(uint32_t w_in, uint32_t h_in, uint32_t bits, uint32_t mb, uint32_t sx_int, uint32_t sx_frag,
                         uint32_t sy_int, uint32_t sy_frag, bool * several)
{
    const uint32_t FRAG = 16;            /* PPA_LL_SRM_SCALING_FRAG_MAX: 4 bits */
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
    *several = w_out > wdiv || h_in > mb;
    return w_left * h_left * bits;
}

/* Whether the driver bypasses: strictly below the FIFO's depth, in v5.5 and
 * on master alike. Whether it is needed, as this file judges it: below TWICE
 * the depth. A Tab5's own log (silicon v1.0, ESP-IDF v5.5.6) showed a block
 * whose last macro block leaves exactly 1536 bits -- 464x70 turned 90 --
 * never finish, right after an identical one had: most likely a race at the
 * exact depth, which the drivers' strict test leaves out, and nothing says
 * the race stops one bit above it. A margin of a whole FIFO costs little,
 * since a bypass given where it is not needed does no harm: Espressif's own
 * esp_lvgl_adapter ships an ESP-IDF patch that gives it to every transaction
 * ("Temporary fix for the PPA hang issue", 0001-bugfix-ppa-...patch). */
static bool bypass(uint32_t w_in, uint32_t h_in, uint32_t bits, uint32_t mb, uint32_t sx_int, uint32_t sx_frag,
                   uint32_t sy_int, uint32_t sy_frag, bool needed)
{
    bool several;
    const uint32_t left = leftover(w_in, h_in, bits, mb, sx_int, sx_frag, sy_int, sy_frag, &several);
    return several && (needed ? left < 2 * FIFO_BITS : left < FIFO_BITS);
}

/* Bits 0-3 of trail_t.bypass for a block: bit 0 whether the driver gives the
 * bypass, bit 1 whether it is needed, at 16x16 macro blocks (silicon below
 * v3.0); bits 2 and 3 the same at 32x32. The driver sizes the block as it
 * reads it; the need, at 90 and 270, is sized as it comes out turned (the fix
 * on master, 469aa16). */
static uint8_t bypass_bits(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits, float scale_x,
                           float scale_y)
{
    const bool quarter = rot == LV_DISPLAY_ROTATION_90 || rot == LV_DISPLAY_ROTATION_270;
    /* As the driver quantizes them. */
    const uint32_t xi = (uint32_t)scale_x, xf = (uint32_t)(scale_x * 16) & 15;
    const uint32_t yi = (uint32_t)scale_y, yf = (uint32_t)(scale_y * 16) & 15;
    uint8_t r = 0;
    for (uint32_t mb = 16, sh = 0; mb <= 32; mb += 16, sh += 2) {
        if (bypass(w, h, bits, mb, xi, xf, yi, yf, false)) {
            r |= (uint8_t)(1u << sh);
        }
        if (quarter ? bypass(h, w, bits, mb, yi, yf, xi, xf, true) : bypass(w, h, bits, mb, xi, xf, yi, yf, true)) {
            r |= (uint8_t)(2u << sh);
        }
    }
    return r;
}

bool lv_draw_ortho_ppa_dig734(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits,
                              float scale_x, float scale_y)
{
    /* Needed and not given, at either macro-block size: the hang. */
    const uint8_t b = bypass_bits(w, h, rot, bits, scale_x, scale_y);
    return (((b >> 1) & ~b) & 0x5u) != 0;
}

/* What is not sent: a block that needs the bypass and is not given it, at
 * either macro-block size -- the DIG-734 hang. */
static bool unsafe(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits, float scale_x, float scale_y)
{
    return lv_draw_ortho_ppa_dig734(w, h, rot, bits, scale_x, scale_y);
}

typedef struct {
    uint32_t x, y, w, h;
} piece_t;

/* Cut a block into pieces none of which needs a bypass it is not given. A cut
 * falls on the 8-pixel grid and leaves at least a macro block on each side:
 * no sliver, which is the shape the PPA has been tried on least -- an earlier
 * cut made strips of one pixel, a third of the time. A block that cannot be
 * cut so (one under 32 pixels, mostly) is the CPU's, and a small one costs it
 * little. Over every size up to 1300 x 1300, at 90 and 180, 16 and 24 bits:
 * at most 7 pieces, and at most 0.7% of sizes, 0.03% of their pixels, left to
 * the CPU (test/main.c). Two levels try every cut on the grid, the third only
 * the first, so a block costs at most a few thousand tests of the predicate,
 * each a handful of instructions. False if it will not cut. */
#define SIDE 16
#define STEP 8
static bool cut(uint32_t x, uint32_t y, uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits,
                piece_t * out, int * n, int depth)
{
    if (!unsafe(w, h, rot, bits, 1.0f, 1.0f)) {
        if (*n == MAX_PIECES) {
            return false;
        }
        out[(*n)++] = (piece_t) {
            x, y, w, h
        };
        return true;
    }
    if (depth >= 3) {
        return false;
    }
    for (int dim = 0; dim < 2; dim++) {
        const uint32_t size = dim == 0 ? w : h;
        for (uint32_t c = SIDE; c + SIDE <= size; c += STEP) {
            const int mark = *n;
            const bool ok = dim == 0
                            ? cut(x, y, c, h, rot, bits, out, n, depth + 1) && cut(x + c, y, w - c, h, rot, bits, out, n, depth + 1)
                            : cut(x, y, w, c, rot, bits, out, n, depth + 1) && cut(x, y + c, w, h - c, rot, bits, out, n, depth + 1);
            if (ok) {
                return true;
            }
            *n = mark;
            if (depth >= 1) {
                break;
            }
        }
    }
    return false;
}

int lv_draw_ortho_ppa_cut(uint32_t w, uint32_t h, lv_display_rotation_t rot, uint32_t bits, uint32_t (*pieces)[4],
                          int max)
{
    piece_t p[MAX_PIECES];
    int n = 0;
    if (!cut(0, 0, w, h, rot, bits, p, &n, 0)) {
        return 0;
    }
    for (int i = 0; i < n && i < max; i++) {
        pieces[i][0] = p[i].x;
        pieces[i][1] = p[i].y;
        pieces[i][2] = p[i].w;
        pieces[i][3] = p[i].h;
    }
    return n;
}

/* One more transaction on its way: kept for a stall's report. */
static void note(uint8_t kind, uint32_t ticket, const void * in, uint32_t in_w, uint32_t in_h, uint32_t bx,
                 uint32_t by, uint32_t bw, uint32_t bh, const void * out, uint32_t out_w, uint32_t out_h, uint32_t ox,
                 uint32_t oy, lv_display_rotation_t rot, uint32_t bits, float scale_x, float scale_y)
{
    trail_t * t = &g_trail[g_trail_n++ % TRAIL];
    t->in = in;
    t->out = out;
    t->ticket = ticket;
    t->in_w = (uint16_t)in_w;
    t->in_h = (uint16_t)in_h;
    t->bx = (uint16_t)bx;
    t->by = (uint16_t)by;
    t->bw = (uint16_t)bw;
    t->bh = (uint16_t)bh;
    t->out_w = (uint16_t)out_w;
    t->out_h = (uint16_t)out_h;
    t->ox = (uint16_t)ox;
    t->oy = (uint16_t)oy;
    t->kind = kind;
    t->rot = (uint8_t)rot;
    t->bits = (uint8_t)bits;
    t->bypass = 0;
    t->left_in = t->left_out = 0;
    if (kind != 2) {
        const bool quarter = rot == LV_DISPLAY_ROTATION_90 || rot == LV_DISPLAY_ROTATION_270;
        const uint32_t xi = (uint32_t)scale_x, xf = (uint32_t)(scale_x * 16) & 15;
        const uint32_t yi = (uint32_t)scale_y, yf = (uint32_t)(scale_y * 16) & 15;
        bool several;
        t->bypass = bypass_bits(bw, bh, rot, bits, scale_x, scale_y);
        t->left_in = (uint16_t)leftover(bw, bh, bits, 16, xi, xf, yi, yf, &several);
        t->left_out = (uint16_t)(quarter ? leftover(bh, bw, bits, 16, yi, yf, xi, xf, &several) : t->left_in);
    }
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
static void log_stall_line(const char * line, void * user)
{
    LV_UNUSED(user);
    LV_LOG_ERROR("PPA: %s", line);
}

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
            g_stuck_engine = done == &g_fill_done ? 2 : 1;
            g_stuck_done = *done;
            g_stuck_queued = done == &g_fill_done ? g_fill_queued : g_queued;
            g_stuck = true;
            lv_draw_ortho_ppa_describe_stall(log_stall_line, NULL);
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
    if (dst->stride == 0 || src->stride != src->w * px || dst->stride != dst->w * px) {
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
    /* buffer_size is rounded DOWN to whole cache lines. Do not still claim
     * the dummy/padded last row when it no longer fits: at 720 RGB565 pixels
     * per row, a cache buffer 1281 rows high loses 32 bytes here. The IDF
     * driver rejects that pic_w/pic_h even though the requested 1280 rows fit. */
    const uint32_t out_h = size / dst->stride;
    uintptr_t a = (uintptr_t)dst->buf + (uintptr_t)dy * dst->stride;
    uintptr_t b = a + (uintptr_t)rows * dst->stride;
    if (b > (uintptr_t)dst->buf + size) {
        return 0;
    }

    /* Cut first: a block left to the CPU needs nothing written back. */
    piece_t pieces[MAX_PIECES];
    int n = 0;
    if (!cut(0, 0, bw, bh, rot, px * 8, pieces, &n, 0)) {
        return 0;
    }
    if (g_queued - g_done + (uint32_t)n > PENDING) {
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

    ppa_srm_oper_config_t op;
    lv_memzero(&op, sizeof(op));
    op.in.buffer = src->buf;
    op.in.pic_w = src->w;
    op.in.pic_h = src->h;
    op.in.srm_cm = cm;
    op.out.buffer = dst->buf;
    op.out.buffer_size = size;
    op.out.pic_w = dst->w;
    op.out.pic_h = out_h;
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
        note(0, g_queued, src->buf, src->w, src->h, op.in.block_offset_x, op.in.block_offset_y, p->w, p->h, dst->buf,
             dst->w, out_h, op.out.block_offset_x, op.out.block_offset_y, rot, px * 8, 1.0f, 1.0f);
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
    ++g_fill_queued;
    note(2, g_fill_queued, NULL, 0, 0, 0, 0, op.fill_block_w, op.fill_block_h, dst->buf, dst->w, dst->h, op.out.block_offset_x,
         op.out.block_offset_y, LV_DISPLAY_ROTATION_0, px * 8, 1.0f, 1.0f);
    return wait_on(&g_fill_done, g_fill_queued);
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
    g_trail_n = 0;
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
    const lv_display_rotation_t rot = op.rotation_angle == PPA_SRM_ROTATION_ANGLE_90 ? LV_DISPLAY_ROTATION_90
                                      : op.rotation_angle == PPA_SRM_ROTATION_ANGLE_180 ? LV_DISPLAY_ROTATION_180
                                      : op.rotation_angle == PPA_SRM_ROTATION_ANGLE_270 ? LV_DISPLAY_ROTATION_270
                                      : LV_DISPLAY_ROTATION_0;
    const uint32_t bits = op.out.srm_cm == PPA_SRM_COLOR_MODE_RGB565 ? 16 : op.out.srm_cm == PPA_SRM_COLOR_MODE_RGB888 ? 24 : 32;
    piece_t pieces[MAX_PIECES];
    int n = 0;
    if (unsafe(op.in.block_w, op.in.block_h, rot, bits, op.scale_x, op.scale_y)) {
        if (op.scale_x == 1.0f && op.scale_y == 1.0f && !op.mirror_x && !op.mirror_y &&
                cut(0, 0, op.in.block_w, op.in.block_h, rot, bits, pieces, &n, 0)) {
            /* pieces placed below */
        } else {
            /* Scaled or mirrored: a row or column off the frame's edge is
             * cheaper than placing pieces through a scale. */
            n = 0;
            bool safe = false;
            for (uint32_t k = 1; k <= 3 && !safe; k++) {
                if (op.in.block_h > k && !unsafe(op.in.block_w, op.in.block_h - k, rot, bits, op.scale_x,
                                                 op.scale_y)) {
                    op.in.block_h -= k;
                    safe = true;
                } else if (op.in.block_w > k && !unsafe(op.in.block_w - k, op.in.block_h, rot, bits, op.scale_x,
                                                        op.scale_y)) {
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
        } else if (rot == LV_DISPLAY_ROTATION_180) {
            ox = bw - p->x - p->w;
            oy = bh - p->y - p->h;
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
        note(1, g_queued, op.in.buffer, op.in.pic_w, op.in.pic_h, op.in.block_offset_x, op.in.block_offset_y,
             op.in.block_w, op.in.block_h, op.out.buffer, op.out.pic_w, op.out.pic_h, op.out.block_offset_x,
             op.out.block_offset_y, rot, bits, op.scale_x, op.scale_y);
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

static void say_one(const trail_t * t, const char * what, lv_draw_ortho_ppa_line_cb_t line, void * user)
{
    char buf[260];
    static const char * const KIND[] = {"tile", "video", "fill"};
    static const int DEG[] = {0, 90, 180, 270};
    if (t->kind == 2) {
        snprintf(buf, sizeof(buf), "%s #%u: fill %ux%u at %u,%u of %ux%u (%p), %u bits", what, (unsigned)t->ticket,
                 t->bw, t->bh, t->ox, t->oy, t->out_w, t->out_h, t->out, t->bits);
    } else {
        snprintf(buf, sizeof(buf), "%s #%u: %s %ux%u at %u,%u of %ux%u (%p) turned %d into %u,%u of %ux%u (%p), "
                 "%u bits, last macro block %u/%u bits read/turned of a %u-bit FIFO, bypass given/needed %c%c "
                 "at 16x16, %c%c at 32x32", what, (unsigned)t->ticket, KIND[t->kind], t->bw, t->bh, t->bx, t->by,
                 t->in_w, t->in_h, t->in, DEG[t->rot & 3], t->ox, t->oy, t->out_w, t->out_h, t->out, t->bits,
                 t->left_in, t->left_out, FIFO_BITS, t->bypass & 1 ? 'y' : 'n', t->bypass & 2 ? 'y' : 'n',
                 t->bypass & 4 ? 'y' : 'n', t->bypass & 8 ? 'y' : 'n');
    }
    line(buf, user);
}

void lv_draw_ortho_ppa_describe_stall(lv_draw_ortho_ppa_line_cb_t line, void * user)
{
    if (line == NULL || g_stuck_engine == 0) {
        return;
    }
    const uint8_t want = g_stuck_engine == 2 ? 2 : 0xFF;   /* 0xFF: either turn kind */
    char buf[120];
    snprintf(buf, sizeof(buf), "the %s engine: %u handed to it, %u finished", g_stuck_engine == 2 ? "fill" : "turning",
             (unsigned)g_stuck_queued, (unsigned)g_stuck_done);
    line(buf, user);
    const uint32_t first = g_trail_n > TRAIL ? g_trail_n - TRAIL : 0;
    const trail_t * last_done = NULL;
    int said = 0;
    for (uint32_t i = first; i < g_trail_n; i++) {
        const trail_t * t = &g_trail[i % TRAIL];
        if (want == 2 ? t->kind != 2 : t->kind == 2) {
            continue;
        }
        if ((int32_t)(t->ticket - g_stuck_done) <= 0) {
            last_done = t;
        } else if (said < 4) {
            say_one(t, said == 0 ? "never finished" : "queued after it", line, user);
            said++;
        }
    }
    if (last_done) {
        say_one(last_done, "last finished", line, user);
    }
}
