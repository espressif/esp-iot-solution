/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_flux_transport.h"
#include "flux_gatt_session.h"
#include "esp_gmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register a GMP link over an existing Flux session.
 * This optional adapter keeps esp_gmp core transport-agnostic.
 */
esp_err_t esp_gmp_flux_link_register(esp_gmp_link_t link, flux_session_t *flux);

/** Unregister a GMP link from the optional Flux adapter. */
void esp_gmp_flux_link_unregister(esp_gmp_link_t link);

/**
 * Fill @p out with Flux callbacks that forward RX to esp_gmp_input and release
 * completed GMP TX buffers. Optional user callbacks may be chained after GMP
 * processing. Chained session_complete_cb always receives data=NULL and size=0
 * (GMP already freed the TX frame); do not free or inspect the frame buffer.
 */
esp_err_t esp_gmp_flux_get_callbacks(esp_gmp_link_t link, flux_callbacks_t *out, const flux_callbacks_t *user_cbs);

/**
 * Preferred GATT integration path.
 * Fill @p out with gatt_session callbacks that forward RX/TX completion to GMP.
 * Optional user callbacks may be chained after GMP processing.
 * Chained session_complete_cb always receives data=NULL and size=0 (GMP already
 * freed the TX frame); do not free or inspect the frame buffer.
 */
esp_err_t esp_gmp_flux_get_gatt_callbacks(esp_gmp_link_t link, ble_session_callbacks_t *out, const ble_session_callbacks_t *user_cbs);

#ifdef __cplusplus
}
#endif
