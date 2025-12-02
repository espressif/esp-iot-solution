/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"

#include "gprof_types.h"
#include "gprof.h"

#define DEPTH_MAX   5           /*!< Function calls depth */

#ifdef __XTENSA__
/* Xtensa regitser value offset */
#define PC_OFF      1           /*!< Xtensa program counter offset */
#define RA_OFF      3           /*!< Xtensa return address offset */
#define SP_OFF      4           /*!< Xtensa stack pointer offset */

#define RA2PC(_a)   ((_a) - 0x40000000) /*!< Calculate program counter based on return address */

#define __XTENSA_HIST_BT_PC__
#else
/* RISC-V regitser value offset */
#define PC_OFF      0           /*!< RISC-V program counter offset */
#define RA_OFF      1           /*!< RISC-V return address offset */
#define SP_OFF      2           /*!< RISC-V stack pointer offset */
#endif

#define PC_PTR(_p)  (((uintptr_t **)(gp->task_handle))[0][PC_OFF])  /*!< Get program counter value */
#define RA_PTR(_p)  (((uintptr_t **)(gp->task_handle))[0][RA_OFF])  /*!< Get return address  value */
#define SP_PTR(_p)  (((uintptr_t **)(gp->task_handle))[0][SP_OFF])  /*!< Get stack pointer value */

/* General rounding functions */
#define ROUND_DOWN(x, y)        (((x) / (y)) * (y))
#define ROUND_UP(x, y)          ((((x) + (y) - 1) / (y)) * (y))

#define GMON_IN_TEXT(_p, _a)    (((_a) - (_p)->low_pc) <  (_p)->text_size)  /*!< Check of PC is in text space */
#define GMON_OUT_TEXT(_p, _a)   (((_a) - (_p)->low_pc) >= (_p)->text_size)  /*!< Check of PC is out of text space */

#define PARTITION_SUBTYPE       0x3a    /*!< GProf partition subtype value */

/*!< GProf global data */
static esp_gprof_t s_gprof;
static const char *TAG = "esp_gprof";

#ifdef __XTENSA_HIST_BT_PC__
/**
 * @brief Get PC from backtrace based on xtensa stack
 */
static inline uintptr_t hist_bt_pc(esp_gprof_t *gp)
{
    uintptr_t sp;
    uintptr_t pc = RA2PC(RA_PTR());

    if (GMON_IN_TEXT(gp, pc)) {
        return pc;
    }

    sp = SP_PTR();

    for (int i = 0; i < DEPTH_MAX; i++) {
        if (!esp_stack_ptr_is_sane(sp)) {
            return 0;
        }

        pc = RA2PC(*((uintptr_t *)(sp - 16)));

        if (GMON_IN_TEXT(gp, pc)) {
            break;
        }

        sp = *((uintptr_t *)(sp - 12));
    }

    return pc;
}
#endif

/**
 * @brief Save buffer data to partition
 */
static esp_err_t save_buf_data(esp_gprof_t *gp, void *buf, int n)
{
    esp_err_t err;
    size_t off = gp->saved_size + sizeof(esp_gprof_hdr_t);

    err = esp_partition_write(gp->part, off, buf, n);
    if (err != ESP_OK) {
        return err;
    }

    /* Update saved data size */
    gp->saved_size += n;

    return ESP_OK;
}

/**
 * @brief Save vector data to partition
 */
static esp_err_t save_iov_data(esp_gprof_t *gp, const struct iovec *iov, int n)
{
    esp_err_t err;

    for (int i = 0; i < n; i++) {
        err = save_buf_data(gp, iov[i].iov_base, iov[i].iov_len);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

/**
 * @brief Save GProf data description
 */
static esp_err_t save_gprof_desc(esp_gprof_t *gp)
{
    esp_gprof_hdr_t hdr = {
        .size = gp->saved_size
    };

    return esp_partition_write(gp->part, 0, &hdr, sizeof(hdr));;
}

/**
 * @brief Save GProf header description
 */
static esp_err_t save_ghdr(esp_gprof_t *gp)
{
    ghdr_t ghdr;

    memcpy(&ghdr.magic[0], GPROF_MAGIC, sizeof(ghdr.magic));
    ghdr.version = GPROF_VERSION;
    memset(ghdr.spare, '\0', sizeof(ghdr.spare));

    return save_buf_data(gp, &ghdr, sizeof(ghdr_t));
}

/**
 * @brief Save GProf time histogram
 */
static esp_err_t save_hist(esp_gprof_t *gp)
{
    uint8_t tag = GPROF_TAG_TIME_HIST;
    hist_hdr_t hdr;

    struct iovec iov[3] = {
        { &tag, sizeof(tag) },
        { &hdr, sizeof(hist_hdr_t) },
        { gp->histcnt, gp->histcnt_bs }
    };

    hdr.low_pc    = gp->low_pc;
    hdr.high_pc   = gp->high_pc;
    hdr.hist_size = gp->histcnt_num;
    hdr.prof_rate = CONFIG_FREERTOS_HZ;
    strncpy((char *)hdr.dimen, "seconds", sizeof(hdr.dimen));
    hdr.dimen_abbrev = 's';

    return save_iov_data(gp, iov, 3);
}

/**
 * @brief Save GProf call graphic
 */
static esp_err_t save_callgraph(esp_gprof_t *gp)
{
    esp_err_t err;
    cg_hdr_t hdr_buf[NARCS_PER_WRITEV];
    struct iovec iov[NARCS_PER_WRITEV * 2];
    int nfilled = 0;
    uint8_t tag = GPROF_TAG_CG_ARC;

    /* Initialize vector basic data */
    for (int i = 0; i < NARCS_PER_WRITEV; i++) {
        iov[2 * i].iov_base = &tag;
        iov[2 * i].iov_len = sizeof(tag);

        iov[2 * i + 1].iov_base = &hdr_buf[i];
        iov[2 * i + 1].iov_len = sizeof(cg_hdr_t);
    }

    for (int i = 0; i < gp->from_num; i++) {
        /* skip if no caller */
        if (gp->from[i] == 0) {
            continue;
        }

        uintptr_t from_pc = gp->low_pc + i * gp->hashfraction * sizeof(from_t);
        for (from_t index = gp->from[i]; index != 0; index = gp->to[index].from) {
            cg_hdr_t arc;

            /* Save callee information */
            arc.from_pc = from_pc;
            arc.self_pc = gp->to[index].self_pc;
            arc.count   = gp->to[index].count;
            memcpy(hdr_buf + nfilled, &arc, sizeof(cg_hdr_t));

            if (++nfilled == NARCS_PER_WRITEV) {
                err = save_iov_data(gp, iov, 2 * nfilled);
                if (err != ESP_OK) {
                    return err;
                }

                nfilled = 0;
            }
        }
    }

    /* Save rest data */
    if (nfilled > 0) {
        err = save_iov_data(gp, iov, 2 * nfilled);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

/**
 * @brief Dump GProf header
 */
static size_t dump_head(const uint8_t *buf)
{
    ghdr_t *ghdr = (ghdr_t *)buf;

    ESP_LOGI(TAG, "magic: %c%c%c%c", ghdr->magic[0], ghdr->magic[1], ghdr->magic[2], ghdr->magic[3]);
    ESP_LOGI(TAG, "version: %" PRIu32, ghdr->version);
    ESP_LOGI(TAG, "spare:");
    for (int i = 0; i < 12; i++) {
        ESP_LOGI(TAG, "%02x ", ghdr->spare[i]);
    }

    return sizeof(ghdr_t);
}

/**
 * @brief Dump time histogram
 */
static size_t dump_time_hist(const uint8_t *buf)
{
    histcnt_t *hcnt;
    hist_hdr_t *ghhdr = (hist_hdr_t *)buf;

    ESP_LOGI(TAG, "low pc: %p", (void *)ghhdr->low_pc);
    ESP_LOGI(TAG, "high pc: %p", (void *)ghhdr->high_pc);
    ESP_LOGI(TAG, "hist size: %" PRIu32, ghhdr->hist_size);
    ESP_LOGI(TAG, "prof rate: %" PRIu32, ghhdr->prof_rate);
    ESP_LOGI(TAG, "dimen:");
    for (int i = 0; i < 15; i++) {
        ESP_LOGI(TAG, "%02x(%c)", ghhdr->dimen[i], ghhdr->dimen[i]);
    }
    ESP_LOGI(TAG, "dimen abbrev: %02x(%c)", ghhdr->dimen_abbrev, ghhdr->dimen_abbrev);

    ESP_LOGI(TAG, "hist samples:");
    hcnt = (histcnt_t *)buf + sizeof(hist_hdr_t);
    for (int i = 0; i < ghhdr->hist_size; i += 8) {
        int n = MIN(ghhdr->hist_size - i, 8);

        ESP_LOGI(TAG, "0x%04x\t", i);
        for (int j = 0; j < n; j++) {
            ESP_LOGI(TAG, "%04x ", hcnt[i + j]);
        }
    }

    return sizeof(hist_hdr_t) + ghhdr->hist_size * sizeof(histcnt_t);
}

/**
 * @brief Dump call graphic arc
 */
static int dump_cg_arc(const uint8_t *buf)
{
    cg_hdr_t *arc = (cg_hdr_t *)buf;

    ESP_LOGI(TAG, "from_pc: %p", (void *)arc->from_pc);
    ESP_LOGI(TAG, "self_pc: %p", (void *)arc->self_pc);
    ESP_LOGI(TAG, "count: %" PRIu32, arc->count);

    return sizeof(cg_hdr_t);
}

/**
 * @brief GProf sample callback function.
 */
static void IRAM_ATTR hist_sample_cb(void)
{
    size_t i;
    uintptr_t pc;
    esp_gprof_t *gp = &s_gprof;

    /* Get current PC */
#ifdef __XTENSA__
    pc = PC_PTR();
#ifdef __XTENSA_HIST_BT_PC__
    if (GMON_OUT_TEXT(gp, pc)) {
        pc = hist_bt_pc(gp);
    }
#endif
#else
    pc = PC_PTR();
#endif

    /* Check if PC is valid and update time histogram */
    i = (pc - gp->low_pc) / HISTFRACTION / sizeof(histcnt_t);
    if (i < gp->histcnt_num) {
        gp->histcnt[i]++;
    }
}

/**
 * @brief Initialize GProf data and allocate memory resource.
 *
 * @return ESP_OK if success or other if failed.
 */
static int gprof_data_init(esp_gprof_t *gp, void *low_pc, void *high_pc)
{
    size_t n;
    uint8_t *cp;

    ESP_LOGD(TAG, "low_pc=%p, high_pc=%p", low_pc, high_pc);

    /* Initialize program information */

    gp->low_pc    = (uintptr_t)low_pc;
    gp->high_pc   = (uintptr_t)high_pc;
    gp->text_size = ROUND_UP(high_pc - low_pc, sizeof(from_t));

    /* Initialize time histogram information */

    gp->histcnt_bs  = ROUND_UP(gp->text_size / HISTFRACTION, sizeof(void *));
    gp->histcnt_num = gp->histcnt_bs / sizeof(histcnt_t);

    ESP_LOGD(TAG, "text_size=%zu, histcnt_num=%zu", gp->text_size, gp->histcnt_bs);

    /* Initialize caller information */

    gp->from_bs  = ROUND_UP(gp->text_size / HASHFRACTION, sizeof(void *));
    gp->from_num = gp->from_bs / sizeof(from_t);

    /* Initialize callee information */

    gp->to_num = gp->text_size * ARCDENSITY / 100;
    gp->to_num = MAX(gp->to_num, MINARCS);
    gp->to_num = MIN(gp->to_num, MAXARCS);
    gp->to_bs  = ROUND_UP(gp->to_num * sizeof(to_t), sizeof(void *));

    /* Allocate and set memory resource */

    n = gp->histcnt_bs + gp->from_bs + gp->to_bs;
    ESP_LOGD(TAG, "total buffer size=%zu", n);
    cp = heap_caps_calloc(1, n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!cp) {
        return -ESP_ERR_NO_MEM;
    }

    gp->to = (to_t *)cp;

    cp += gp->to_bs;
    gp->histcnt = (histcnt_t *)cp;

    cp += gp->histcnt_bs;
    gp->from = (from_t *)cp;

    /* Initialize hash configuration */

    gp->hashfraction     = HASHFRACTION;
    gp->log_hashfraction = ffs(gp->hashfraction * sizeof(from_t)) - 1;

    ESP_LOGD(TAG, "hashfraction=%d, log_hashfraction=%d",
             gp->hashfraction, gp->log_hashfraction);

    return ESP_OK;
}

/**
 * @brief Initialize GProf storage.
 *
 * @return ESP_OK if success or other if failed.
 */
static esp_err_t gprof_storage_init(esp_gprof_t *gp)
{
    esp_err_t err;

    /* Get partition pointer by given type "data" and subtype "0x3a" */
    gp->part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, PARTITION_SUBTYPE, NULL);
    if (!gp->part) {
        ESP_LOGE(TAG, "Failed to find partition");
        return ESP_FAIL;
    }

    /* Erase partition before saving data */
    err = esp_partition_erase_range(gp->part, 0, gp->part->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase partition");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Initialize GProf.
 *
 * @return ESP_OK if success or other if failed.
 */
esp_err_t esp_gprof_init(void)
{
    esp_err_t err;
    esp_gprof_t *gp = &s_gprof;
    extern uint32_t _gprof_text_start, _gprof_text_end;

    ESP_LOGI(TAG, "GProf version: %d.%d.%d", GPROF_VER_MAJOR, GPROF_VER_MINOR, GPROF_VER_PATCH);

    /* Check GProf state */
    if (gp->state != ESP_GPROF_STATE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Initialize GProf data and allocate memory resource */
    err = gprof_data_init(gp, &_gprof_text_start, &_gprof_text_end);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init data, erroris %s", esp_err_to_name(err));
        return err;
    }

    /* Initialize GProf storage */
    err = gprof_storage_init(gp);
    if (err != ESP_OK) {
        heap_caps_free(gp->to);
        ESP_LOGE(TAG, "Failed to init storage, error is %s", esp_err_to_name(err));
        return err;
    }

    /* Initialize GProf sample */
    err = esp_register_freertos_tick_hook(hist_sample_cb);
    if (err != ESP_OK) {
        heap_caps_free(gp->to);
        ESP_LOGE(TAG, "Failed to init histogram, error is %s", esp_err_to_name(err));
        return err;
    }

    /* Get current task handle */
    gp->task_handle = xTaskGetCurrentTaskHandle();

    /* Set state to be started */
    gp->state = ESP_GPROF_STATE_START;

    ESP_LOGD(TAG, "gprof starts");

    return ESP_OK;
}

/**
 * @brief Save the profiling data of your running code to the specific partition.
 *        Later, you can use esp_gprof_dump to printf them out, or use idf.py
 *        gprof to read it out for analysis.
 *
 * @return ESP_OK if success or other if failed.
 */
esp_err_t esp_gprof_save(void)
{
    esp_err_t err;
    esp_gprof_t *gp = &s_gprof;

    /* Check GProf state */
    if (gp->state != ESP_GPROF_STATE_START) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "gprof end");

    /* Disable sample */
    esp_deregister_freertos_tick_hook(hist_sample_cb);
    gp->task_handle = NULL;

    /* Save GProf header description */
    err = save_ghdr(gp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save gprof header description, error is %s", esp_err_to_name(err));
        return err;
    }

    /* Save GProf time histogram */
    err = save_hist(gp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save histogram, error is %s", esp_err_to_name(err));
        return err;
    }

    /* Save GProf call graphic */
    err = save_callgraph(gp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save callgraph, error is %s", esp_err_to_name(err));
        return err;
    }

    /* Save GProf data description */
    err = save_gprof_desc(gp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save gprof description, error is %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Save %" PRIu32 " bytes to %" PRIu32, gp->saved_size, gp->part->address + sizeof(esp_gprof_hdr_t));
    ESP_LOGI(TAG, "GProf data is saved successfully");

    /* Set state to be saved */
    gp->state = ESP_GPROF_STATE_SAVED;

    return ESP_OK;
}

/**
 * @brief Dump the profiling data of your running code from the specific partition.
 *
 * @return ESP_OK if success or other if failed.
 */
esp_err_t esp_gprof_dump(void)
{
    size_t n;
    const void *buf;
    esp_err_t err;
    const uint8_t *p;
    const uint8_t *end;
    esp_partition_mmap_handle_t mmap_handle;
    esp_gprof_t *gp = &s_gprof;

    /* Check GProf state */
    if (gp->state != ESP_GPROF_STATE_SAVED) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Map partition to be continuous memory block */
    err = esp_partition_mmap(gp->part, sizeof(esp_gprof_hdr_t), gp->saved_size,
                             ESP_PARTITION_MMAP_DATA, &buf, &mmap_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to map partition, error is %s", esp_err_to_name(err));
        return err;
    }

    p = buf;
    end = buf + gp->saved_size;

    /* Dump header data */
    n = dump_head(p);
    p += n;

    while (p < end) {
        const uint8_t *tag;

        tag = p;
        p += sizeof(uint8_t);

        /* Dump tag offset and value */
        ESP_LOGI(TAG, "offset: 0x%" PRIx32, (uint32_t)(p - (const uint8_t *)buf));
        ESP_LOGI(TAG, "tag: %02x", tag[0]);
        switch (tag[0]) {
        case GPROF_TAG_TIME_HIST:
            ESP_LOGI(TAG, "record: time histogram");
            n = dump_time_hist(p);
            p += n;
            break;
        case GPROF_TAG_CG_ARC:
            ESP_LOGI(TAG, "record: call-graph");
            n = dump_cg_arc(p);
            p += n;
            break;
        default:
            ESP_LOGI(TAG, "record=%d not support", tag[0]);
            err = ESP_FAIL;
            break;
        }
    }

    /* Free mapped flash */
    esp_partition_munmap(mmap_handle);

    return err;
}

/**
 * @brief De-initialize GProf.
 *
 * @return ESP_OK if success or other if failed.
 */
esp_err_t esp_gprof_deinit(void)
{
    esp_gprof_t *gp = &s_gprof;

    /* Check GProf state */
    if (gp->state == ESP_GPROF_STATE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Free memory resource */
    heap_caps_free(gp->to);
    gp->to = NULL;

    /* Set state to be none */
    gp->state = ESP_GPROF_STATE_NONE;

    return ESP_OK;
}

void IRAM_ATTR _mcount(uintptr_t from_pc)
{
    size_t i;
    uintptr_t frompc_off;
    to_t *top, *prev_top;
    from_t *frompc_index;
    from_t to_index;
    esp_gprof_t *gp = &s_gprof;
    uintptr_t self_pc = (uintptr_t)__builtin_return_address(0);

    /* Don't support multi-thread */

    if (gp->task_handle != xTaskGetCurrentTaskHandle()) {
        goto done;
    }

#ifdef __XTENSA__
    /* Get return address */
    from_pc = RA2PC(from_pc);
#endif

    /* Get and check return address offset */
    frompc_off = from_pc - gp->low_pc;
    if (frompc_off > gp->text_size) {
        ESP_EARLY_LOGV(TAG, "from_pc=%gp low_pc=%gp high_pc=%gp",
                       (void *)from_pc, (void *)gp->low_pc, (void *)gp->high_pc);
        goto done;
    }

    i = frompc_off >> gp->log_hashfraction;

    /* Initialize callee data */
    frompc_index = &gp->from[i];
    to_index = *frompc_index;
    if (to_index == 0) {
        if ((gp->to_ind + 1) >= gp->to_num) {
            goto overflow;
        }

        to_index = ++gp->to_ind;

        top = &gp->to[to_index];
        top->self_pc = self_pc;
        top->count  = 1;
        top->from   = 0;

        *frompc_index = to_index;

        goto done;
    }

    /* Update callee data */
    top = &gp->to[to_index];
    if (top->self_pc == self_pc) {
        top->count++;
        goto done;
    }

    while (1) {
        /* Update callee data if no caller */
        if (top->from == 0) {
            if ((gp->to_ind + 1) >= gp->to_num) {
                goto overflow;
            }

            to_index = ++gp->to_ind;

            top = &gp->to[to_index];
            top->self_pc = self_pc;
            top->count  = 1;
            top->from   = *frompc_index;

            *frompc_index = to_index;

            goto done;
        }

        /* Update callee data is caller address is the same */
        prev_top = top;
        top = &gp->to[top->from];
        if (top->self_pc == self_pc) {
            top->count++;

            to_index       = prev_top->from;
            prev_top->from = top->from;
            top->from      = *frompc_index;
            *frompc_index  = to_index;

            goto done;
        }
    }

overflow:
    abort();
done:
    return;
}
