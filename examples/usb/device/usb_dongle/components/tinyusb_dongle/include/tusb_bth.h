/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "tinyusb.h"

static inline void hci_dump_buffer(const char *prefix, uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (!data || !len) {
        return;
    }

    if (prefix) {
        printf("%s: len %d\r\n", prefix, len);
    }

    for (i = 0; i < len; i++) {
        if (((i % 8) == 0) && (i != 0)) {
            printf("\r\n");
        }
        printf("%02x ", *(data + i));
    }

    printf("\r\n");
}

#ifdef HCI_DUMP_BUFFER
#define HCI_DUMP_BUFFER(_prefix, _data, _len) hci_dump_buffer(_prefix, _data, _len)
#else
#define HCI_DUMP_BUFFER(_prefix, _data, _len)
#endif

/* HCI message type definitions (for H4 messages) */
#define HCIT_TYPE_COMMAND   1
#define HCIT_TYPE_ACL_DATA  2
#define HCIT_TYPE_SCO_DATA  3
#define HCIT_TYPE_EVENT     4

typedef struct acl_data {
    bool is_new_pkt;
    uint16_t pkt_total_len;
    uint16_t pkt_cur_offset;
    uint8_t * pkt_val;
} acl_data_t;

/**
 * @brief Initialize BTH Device.
 */
void tusb_bth_init(void);

/**
 * @brief Deinitialization BTH Device.
 */
void tusb_bth_deinit(void);

#ifdef __cplusplus
}
#endif
