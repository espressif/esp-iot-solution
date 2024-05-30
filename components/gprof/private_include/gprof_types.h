/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPROF_MAGIC         "gmon"      /*!< GProf header magic */
#define GPROF_VERSION       1           /*!< GProf header version */
#define HISTFRACTION        2           /*!< GProf histogram fraction */
#define HASHFRACTION        2           /*!< GProf hash fraction */
#define ARCDENSITY          3           /*!< GProf arc hash density */
#define MINARCS             50          /*!< GProf min arc density value */
#define MAXARCS             (1 << 20)   /*!< GProf max arc density value */
#define NARCS_PER_WRITEV    32          /*!< GProf arc density write vector size */

#define GPROF_TAG_TIME_HIST 0           /*!< GProf time histogram tag */
#define GPROF_TAG_CG_ARC    1           /*!< GProf call graphic arc tag */

typedef uint16_t    histcnt_t;          /*!< GProf time histogram count type */
typedef uintptr_t   from_t;             /*!< GProf address type within caller's body */

/** @brief GProf state */
typedef enum esp_gprof_state {
    ESP_GPROF_STATE_NONE = 0,           /*!< GProf is not initialized */
    ESP_GPROF_STATE_START,              /*!< GProf is initialized and started */
    ESP_GPROF_STATE_SAVED,              /*!< GProf is stopped and data is saved */
} esp_gprof_state_t;

#pragma pack(4)
/** @brief GProf header description */
typedef struct ghdr {
    uint8_t         magic[4];           /*!< magic */
    uint32_t        version;            /*!< version number */
    uint8_t         spare[12];          /*!< reserved */
} ghdr_t;

/** @brief GProf time histogram header description */
typedef struct hist_hdr {
    uintptr_t       low_pc;             /*!< low address of target program */
    uintptr_t       high_pc;            /*!< high address of target program */

    uint32_t        hist_size;          /*!< histogram size */
    uint32_t        prof_rate;          /*!< sample Rate */

    uint8_t         dimen[15];          /*!< histogram sample description */
    uint8_t         dimen_abbrev;       /*!< histogram sample description abbrev */
} hist_hdr_t;

/** @brief GProf call graphic header description */
typedef struct cg_hdr {
    uintptr_t       from_pc;            /*!< address within caller's body */
    uintptr_t       self_pc;            /*!< address within callee's body */
    uint32_t        count;              /*!< number of arc traversals */
} cg_hdr_t;
#pragma pack()

/** @brief GProf call graphic to address */
typedef struct to {
    uintptr_t       self_pc;            /*!< address within callee's body */
    uint32_t        count;              /*!< call count */
    from_t          from;               /*!< address within caller's body */
} to_t;

/** @brief GProf object */
typedef struct esp_gprof {
    histcnt_t       *histcnt;           /*!< histogram counter buffer */
    size_t          histcnt_bs;         /*!< histogram counter buffer size by byte */
    size_t          histcnt_num;        /*!< histogram counter number */

    from_t          *from;              /*!< address buffer within caller's body */
    size_t          from_bs;            /*!< address buffer size within caller's body */
    size_t          from_num;           /*!< address number within caller's body */

    to_t            *to;                /*!< address buffer within callee's body */
    size_t          to_bs;              /*!< address buffer size within callee's body */
    size_t          to_num;             /*!< address number within callee's body */
    size_t          to_ind;             /*!< address index within callee's body */

    uintptr_t       low_pc;             /*!< low address of target program */
    uintptr_t       high_pc;            /*!< high address of target program */
    size_t          text_size;          /*!< program size */

    uint8_t         hashfraction;       /*!< histogram fraction */
    uint8_t         log_hashfraction;   /*!< histogram fraction bit */

    esp_gprof_state_t state;            /*!< current state */
    uint32_t        saved_size;         /*!< saved data size */
    TaskHandle_t    task_handle;        /*!< task handle */
    const esp_partition_t *part;        /*!< partition pointer */
} esp_gprof_t;

/** @brief GProf saved data header description */
typedef struct esp_gprof_hdr {
    uint32_t size;                      /*!< saved data size */
    uint32_t reserved[3];               /*!< reserved */
} esp_gprof_hdr_t;

#ifdef __cplusplus
}
#endif
