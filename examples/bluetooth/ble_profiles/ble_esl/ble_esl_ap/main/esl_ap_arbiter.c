/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pure-C per-group PAwR arbiter: single-flight, bounded queue, 4:2:1 weights
 * with aging up to RECOVERY. No GAP/controller dependency.
 */

#include "esl_ap_arbiter.h"

#include <string.h>
#include <stdint.h>

#ifdef ESL_AP_ARBITER_HOST_TEST
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#else
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#endif

typedef struct {
    esl_ap_arbiter_request_t req;
    esl_ap_arbiter_priority_t effective_priority;
    uint64_t enqueued_ms;
    uint64_t last_aged_ms;
    bool in_use;
    bool stale;
} esl_ap_arbiter_slot_t;

typedef struct {
    esl_ap_arbiter_slot_t queue[ESL_AP_ARBITER_QUEUE_DEPTH];
    uint8_t count;
    bool has_active;
    esl_ap_arbiter_slot_t active;
    uint64_t active_deadline_ms;
    /* Weighted fair queue deficit counters (recovery:app:health = 4:2:1). */
    int deficit[3];
    /* Next priority class to service in the DRR cycle (0=HEALTH..2=RECOVERY). */
    uint8_t drr_cursor;
} esl_ap_arbiter_group_t;

static esl_ap_arbiter_group_t s_groups[ESL_AP_ARBITER_MAX_GROUPS];
static esl_ap_arbiter_completion_cb_t s_completion_cb;
static esl_ap_arbiter_send_cb_t s_send_cb;
static portMUX_TYPE s_arbiter_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_inited;
static uint64_t s_now_ms;

#ifdef ESL_AP_ARBITER_HOST_TEST
static uint32_t s_completed_by_prio[3];
static uint32_t s_cancelled_count;
#endif

static const int s_weights[3] = {
    [ESL_AP_ARBITER_PRIO_HEALTH] = 1,
    [ESL_AP_ARBITER_PRIO_APP] = 2,
    [ESL_AP_ARBITER_PRIO_RECOVERY] = 4,
};

static void complete_request(const esl_ap_arbiter_request_t *req,
                             const uint8_t *data, uint8_t len, esp_err_t status)
{
#ifdef ESL_AP_ARBITER_HOST_TEST
    if (status == ESP_OK) {
        if (req->priority <= ESL_AP_ARBITER_PRIO_RECOVERY) {
            s_completed_by_prio[req->priority]++;
        }
    } else if (status == ESP_ERR_INVALID_STATE || status == ESP_ERR_INVALID_RESPONSE) {
        s_cancelled_count++;
    }
#endif
    if (s_completion_cb != NULL) {
        s_completion_cb(req, data, len, status);
    }
}

static void age_queued(esl_ap_arbiter_group_t *g)
{
    for (uint8_t i = 0; i < ESL_AP_ARBITER_QUEUE_DEPTH; i++) {
        esl_ap_arbiter_slot_t *s = &g->queue[i];
        if (!s->in_use) {
            continue;
        }
        while (s->effective_priority < ESL_AP_ARBITER_PRIO_RECOVERY &&
                s_now_ms >= s->last_aged_ms + ESL_AP_ARBITER_AGING_MS) {
            s->effective_priority =
                (esl_ap_arbiter_priority_t)(s->effective_priority + 1);
            s->last_aged_ms += ESL_AP_ARBITER_AGING_MS;
        }
    }
}

static int find_oldest_at_prio(esl_ap_arbiter_group_t *g, esl_ap_arbiter_priority_t prio)
{
    int best = -1;
    uint64_t best_t = UINT64_MAX;
    for (uint8_t i = 0; i < ESL_AP_ARBITER_QUEUE_DEPTH; i++) {
        esl_ap_arbiter_slot_t *s = &g->queue[i];
        if (!s->in_use || s->stale) {
            continue;
        }
        if (s->effective_priority != prio) {
            continue;
        }
        if (s->enqueued_ms < best_t) {
            best_t = s->enqueued_ms;
            best = (int)i;
        }
    }
    return best;
}

static bool prio_has_work(esl_ap_arbiter_group_t *g, esl_ap_arbiter_priority_t prio)
{
    return find_oldest_at_prio(g, prio) >= 0;
}

static int select_next_index(esl_ap_arbiter_group_t *g)
{
    /* Deficit round-robin: stay on a class until its quantum is consumed,
     * then advance. Quantum equals the class weight (4:2:1). */
    for (int tries = 0; tries < 3; tries++) {
        int p = g->drr_cursor % 3;
        if (!prio_has_work(g, (esl_ap_arbiter_priority_t)p)) {
            g->deficit[p] = 0;
            g->drr_cursor = (uint8_t)((p + 1) % 3);
            continue;
        }
        if (g->deficit[p] <= 0) {
            g->deficit[p] += s_weights[p];
        }
        if (g->deficit[p] <= 0) {
            g->drr_cursor = (uint8_t)((p + 1) % 3);
            continue;
        }
        int idx = find_oldest_at_prio(g, (esl_ap_arbiter_priority_t)p);
        if (idx < 0) {
            g->drr_cursor = (uint8_t)((p + 1) % 3);
            continue;
        }
        g->deficit[p] -= 1;
        if (g->deficit[p] <= 0) {
            g->drr_cursor = (uint8_t)((p + 1) % 3);
        }
        return idx;
    }

    /* Fallback: highest effective priority, FIFO. */
    for (int p = ESL_AP_ARBITER_PRIO_RECOVERY; p >= ESL_AP_ARBITER_PRIO_HEALTH; p--) {
        int idx = find_oldest_at_prio(g, (esl_ap_arbiter_priority_t)p);
        if (idx >= 0) {
            return idx;
        }
    }
    return -1;
}

/** Caller holds s_arbiter_lock. Returns true if *out_send should be passed to s_send_cb. */
static bool try_schedule_under_lock(uint8_t group_id, esl_ap_arbiter_request_t *out_send)
{
    if (group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return false;
    }
    esl_ap_arbiter_group_t *g = &s_groups[group_id];
    if (g->has_active) {
        return false;
    }

    age_queued(g);

    int idx = select_next_index(g);
    if (idx < 0) {
        return false;
    }

    g->active = g->queue[idx];
    g->queue[idx].in_use = false;
    g->count--;
    g->active.stale = false;
    uint32_t guard = g->active.req.guard_timeout_ms;
    if (guard == 0) {
        guard = 1000;
    }
    /* Deadline before has_active so tick cannot observe stale deadline=0. */
    g->active_deadline_ms = s_now_ms + guard;
    g->has_active = true;
    if (out_send != NULL) {
        *out_send = g->active.req;
    }
    return true;
}

static void try_schedule(uint8_t group_id)
{
    esl_ap_arbiter_request_t to_send;
    bool do_send;

    portENTER_CRITICAL(&s_arbiter_lock);
    do_send = try_schedule_under_lock(group_id, &to_send);
    portEXIT_CRITICAL(&s_arbiter_lock);

    if (do_send && s_send_cb != NULL) {
        s_send_cb(&to_send);
    }
}

esp_err_t esl_ap_arbiter_init(esl_ap_arbiter_completion_cb_t completion_cb)
{
    memset(s_groups, 0, sizeof(s_groups));
    s_completion_cb = completion_cb;
    /* Preserve send_cb across host_reset() so production wiring survives re-init. */
    s_inited = true;
    s_now_ms = 0;
#ifdef ESL_AP_ARBITER_HOST_TEST
    memset(s_completed_by_prio, 0, sizeof(s_completed_by_prio));
    s_cancelled_count = 0;
#endif
    return ESP_OK;
}

void esl_ap_arbiter_set_send_cb(esl_ap_arbiter_send_cb_t send_cb)
{
    s_send_cb = send_cb;
}

esp_err_t esl_ap_arbiter_submit(const esl_ap_arbiter_request_t *request)
{
    if (!s_inited || request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (request->group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return ESP_ERR_INVALID_ARG;
    }
    if (request->params_len > BLE_ESL_TLV_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (request->priority > ESL_AP_ARBITER_PRIO_RECOVERY) {
        return ESP_ERR_INVALID_ARG;
    }

    esl_ap_arbiter_request_t to_send;
    bool do_send = false;
    esp_err_t ret = ESP_OK;

    portENTER_CRITICAL(&s_arbiter_lock);
    esl_ap_arbiter_group_t *g = &s_groups[request->group_id];
    if (g->count >= ESL_AP_ARBITER_QUEUE_DEPTH) {
        ret = ESP_ERR_NO_MEM; /* busy / queue full */
    } else {
        int free_idx = -1;
        for (uint8_t i = 0; i < ESL_AP_ARBITER_QUEUE_DEPTH; i++) {
            if (!g->queue[i].in_use) {
                free_idx = (int)i;
                break;
            }
        }
        if (free_idx < 0) {
            ret = ESP_ERR_NO_MEM;
        } else {
            esl_ap_arbiter_slot_t *s = &g->queue[free_idx];
            memset(s, 0, sizeof(*s));
            s->req = *request;
            s->effective_priority = request->priority;
            s->enqueued_ms = s_now_ms;
            s->last_aged_ms = s_now_ms;
            s->in_use = true;
            s->stale = false;
            g->count++;
            do_send = try_schedule_under_lock(request->group_id, &to_send);
        }
    }
    portEXIT_CRITICAL(&s_arbiter_lock);

    if (ret != ESP_OK) {
        return ret;
    }
    if (do_send && s_send_cb != NULL) {
        s_send_cb(&to_send);
    }
    return ESP_OK;
}

esp_err_t esl_ap_arbiter_invalidate_generation(uint8_t group_id, uint16_t esl_addr,
                                               uint32_t before_generation)
{
    if (!s_inited || group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return ESP_ERR_INVALID_ARG;
    }

    esl_ap_arbiter_request_t cancelled[ESL_AP_ARBITER_QUEUE_DEPTH];
    uint8_t cancelled_n = 0;

    portENTER_CRITICAL(&s_arbiter_lock);
    esl_ap_arbiter_group_t *g = &s_groups[group_id];

    for (uint8_t i = 0; i < ESL_AP_ARBITER_QUEUE_DEPTH; i++) {
        esl_ap_arbiter_slot_t *s = &g->queue[i];
        if (!s->in_use) {
            continue;
        }
        if (s->req.esl_addr != esl_addr) {
            continue;
        }
        if (s->req.recovery_generation >= before_generation) {
            continue;
        }
        if (cancelled_n < ESL_AP_ARBITER_QUEUE_DEPTH) {
            cancelled[cancelled_n++] = s->req;
        }
        s->in_use = false;
        g->count--;
    }

    if (g->has_active &&
            g->active.req.esl_addr == esl_addr &&
            g->active.req.recovery_generation < before_generation) {
        /* Mark stale but keep slot until guard ends — no preemption. */
        g->active.stale = true;
    }
    portEXIT_CRITICAL(&s_arbiter_lock);

    for (uint8_t i = 0; i < cancelled_n; i++) {
        complete_request(&cancelled[i], NULL, 0, ESP_ERR_INVALID_STATE);
    }

    return ESP_OK;
}

bool esl_ap_arbiter_group_busy(uint8_t group_id, esl_ap_arbiter_priority_t *out_active_priority)
{
    if (group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return false;
    }
    bool busy = false;
    esl_ap_arbiter_priority_t prio = ESL_AP_ARBITER_PRIO_HEALTH;

    portENTER_CRITICAL(&s_arbiter_lock);
    esl_ap_arbiter_group_t *g = &s_groups[group_id];
    if (g->has_active) {
        busy = true;
        prio = g->active.req.priority;
    }
    portEXIT_CRITICAL(&s_arbiter_lock);

    if (busy && out_active_priority != NULL) {
        *out_active_priority = prio;
    }
    return busy;
}

esp_err_t esl_ap_arbiter_complete(uint8_t group_id, uint16_t esl_addr,
                                  uint8_t command_opcode, uint32_t request_id,
                                  esp_err_t status)
{
    if (!s_inited || group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return ESP_ERR_INVALID_ARG;
    }

    esl_ap_arbiter_request_t req_copy;
    bool was_stale = false;
    esp_err_t ret = ESP_OK;

    portENTER_CRITICAL(&s_arbiter_lock);
    esl_ap_arbiter_group_t *g = &s_groups[group_id];
    if (!g->has_active) {
        ret = ESP_ERR_INVALID_STATE;
    } else if (g->active.req.esl_addr != esl_addr ||
               g->active.req.command_opcode != command_opcode ||
               g->active.req.request_id != request_id) {
        ret = ESP_ERR_INVALID_RESPONSE;
    } else {
        req_copy = g->active.req;
        was_stale = g->active.stale;
        g->has_active = false;
        memset(&g->active, 0, sizeof(g->active));
    }
    portEXIT_CRITICAL(&s_arbiter_lock);

    if (ret != ESP_OK) {
        return ret;
    }

    if (was_stale) {
        complete_request(&req_copy, NULL, 0, ESP_ERR_INVALID_STATE);
    } else {
        complete_request(&req_copy, NULL, 0, status);
    }
    try_schedule(group_id);
    return ESP_OK;
}

esp_err_t esl_ap_arbiter_on_response(uint8_t group_id, uint16_t esl_addr,
                                     uint8_t command_opcode, uint32_t request_id,
                                     const uint8_t *data, uint8_t len)
{
    (void)data;
    (void)len;
    return esl_ap_arbiter_complete(group_id, esl_addr, command_opcode, request_id, ESP_OK);
}

bool esl_ap_arbiter_peek_active(uint8_t group_id, esl_ap_arbiter_request_t *out)
{
    if (!s_inited || group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return false;
    }
    bool has = false;

    portENTER_CRITICAL(&s_arbiter_lock);
    esl_ap_arbiter_group_t *g = &s_groups[group_id];
    if (g->has_active) {
        has = true;
        if (out != NULL) {
            *out = g->active.req;
        }
    }
    portEXIT_CRITICAL(&s_arbiter_lock);
    return has;
}

void esl_ap_arbiter_tick(uint64_t now_ms)
{
    if (!s_inited) {
        return;
    }

    for (uint8_t g = 0; g < ESL_AP_ARBITER_MAX_GROUPS; g++) {
        bool timed_out = false;
        bool idle = false;
        esl_ap_arbiter_request_t to_send;
        bool do_send = false;

        portENTER_CRITICAL(&s_arbiter_lock);
        s_now_ms = now_ms;
        esl_ap_arbiter_group_t *grp = &s_groups[g];
        age_queued(grp);
        if (grp->has_active && s_now_ms >= grp->active_deadline_ms) {
            timed_out = true;
        } else if (!grp->has_active) {
            idle = true;
            do_send = try_schedule_under_lock(g, &to_send);
        }
        portEXIT_CRITICAL(&s_arbiter_lock);

        if (timed_out) {
            /* Re-check under finish_active lock so a concurrent complete cannot
             * leave a freshly scheduled request timed out by a stale deadline. */
            esl_ap_arbiter_group_t *grp2 = &s_groups[g];
            esl_ap_arbiter_request_t req_copy;
            bool was_stale = false;
            bool do_finish = false;

            portENTER_CRITICAL(&s_arbiter_lock);
            if (grp2->has_active && s_now_ms >= grp2->active_deadline_ms) {
                req_copy = grp2->active.req;
                was_stale = grp2->active.stale;
                grp2->has_active = false;
                memset(&grp2->active, 0, sizeof(grp2->active));
                do_finish = true;
            }
            portEXIT_CRITICAL(&s_arbiter_lock);

            if (do_finish) {
                if (was_stale) {
                    complete_request(&req_copy, NULL, 0, ESP_ERR_INVALID_STATE);
                } else {
                    complete_request(&req_copy, NULL, 0, ESP_ERR_TIMEOUT);
                }
                try_schedule(g);
            }
        } else if (idle && do_send && s_send_cb != NULL) {
            s_send_cb(&to_send);
        }
    }
}

#ifdef ESL_AP_ARBITER_HOST_TEST
void esl_ap_arbiter_host_reset(void)
{
    (void)esl_ap_arbiter_init(s_completion_cb);
}

uint32_t esl_ap_arbiter_host_queue_depth(uint8_t group_id)
{
    if (group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return 0;
    }
    uint32_t count;
    portENTER_CRITICAL(&s_arbiter_lock);
    count = s_groups[group_id].count;
    portEXIT_CRITICAL(&s_arbiter_lock);
    return count;
}

bool esl_ap_arbiter_host_has_active(uint8_t group_id)
{
    if (group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return false;
    }
    bool has;
    portENTER_CRITICAL(&s_arbiter_lock);
    has = s_groups[group_id].has_active;
    portEXIT_CRITICAL(&s_arbiter_lock);
    return has;
}

esl_ap_arbiter_priority_t esl_ap_arbiter_host_active_priority(uint8_t group_id)
{
    esl_ap_arbiter_priority_t prio = ESL_AP_ARBITER_PRIO_HEALTH;
    if (group_id >= ESL_AP_ARBITER_MAX_GROUPS) {
        return prio;
    }
    portENTER_CRITICAL(&s_arbiter_lock);
    if (s_groups[group_id].has_active) {
        prio = s_groups[group_id].active.req.priority;
    }
    portEXIT_CRITICAL(&s_arbiter_lock);
    return prio;
}

uint32_t esl_ap_arbiter_host_completed_count(esl_ap_arbiter_priority_t prio)
{
    if (prio > ESL_AP_ARBITER_PRIO_RECOVERY) {
        return 0;
    }
    return s_completed_by_prio[prio];
}

uint32_t esl_ap_arbiter_host_cancelled_count(void)
{
    return s_cancelled_count;
}
#endif
