/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_esl_ap_init(void);
esp_err_t app_esl_ap_start(void);
/** True after a successful app_esl_ap_start() (idempotent). Independent of phone/NUS. */
bool app_esl_ap_is_started(void);
esp_err_t app_esl_ap_start_scan(void);
esp_err_t app_esl_ap_stop_scan(void);
esp_err_t app_esl_ap_unassociate_tag(uint8_t esl_id, uint8_t group_id);
esp_err_t app_esl_ap_provision_tag(const uint8_t addr[6], uint8_t addr_type,
                                   uint8_t esl_id, uint8_t group_id);
bool app_esl_ap_is_provisioning(void);
/** True while provision / image update / unassociate / recovery TX / sync owns the ACL pipeline. */
bool app_esl_ap_pipeline_busy(void);
bool app_esl_ap_can_queue_image_update(uint8_t esl_id, uint8_t group_id);
void app_esl_ap_request_image_update(uint8_t esl_id);
esp_err_t app_esl_ap_request_image_update_id(uint8_t esl_id, uint8_t group_id, uint32_t image_id);

/** Post recovery timer work onto app_esl_ap_task (from esp_timer; non-blocking). */
bool app_esl_ap_post_recovery_connect_timeout(void);
bool app_esl_ap_post_recovery_tick(void);

#ifdef __cplusplus
}
#endif
