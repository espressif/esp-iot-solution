/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmp_flux.h"
#include "esp_gmp.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    esp_gmp_link_t link;
    flux_session_t *flux;
    flux_callbacks_t user_cbs;
    ble_session_callbacks_t user_gatt_cbs;
    bool closing;
} gmp_flux_hook_t;

static gmp_flux_hook_t s_hooks[CONFIG_ESP_GMP_MAX_LINKS];

static gmp_flux_hook_t *hook_find(esp_gmp_link_t link)
{
    for (int i = 0; i < CONFIG_ESP_GMP_MAX_LINKS; i++) {
        if (s_hooks[i].link == link) {
            return &s_hooks[i];
        }
    }
    return NULL;
}

static gmp_flux_hook_t *hook_find_by_flux(flux_session_t *flux)
{
    for (int i = 0; i < CONFIG_ESP_GMP_MAX_LINKS; i++) {
        if (s_hooks[i].link != NULL && s_hooks[i].flux == flux) {
            return &s_hooks[i];
        }
    }
    return NULL;
}

static gmp_flux_hook_t *hook_alloc(esp_gmp_link_t link)
{
    gmp_flux_hook_t *h = hook_find(link);
    if (h) {
        return h;
    }
    for (int i = 0; i < CONFIG_ESP_GMP_MAX_LINKS; i++) {
        if (s_hooks[i].link == NULL) {
            memset(&s_hooks[i], 0, sizeof(s_hooks[i]));
            s_hooks[i].link = link;
            return &s_hooks[i];
        }
    }
    return NULL;
}

static bool hook_has_in_flight(const gmp_flux_hook_t *hook)
{
    return hook && hook->flux && !flux_session_send_idle(hook->flux);
}

static bool gmp_flux_forward_to_gmp(flux_session_t *session, const uint8_t *data, uint32_t size)
{
    gmp_flux_hook_t *hook = hook_find_by_flux(session);
    if (!hook) {
        return false;
    }
    return esp_gmp_input(hook->link, data, size);
}

static void gmp_flux_tx_done_and_cleanup(flux_session_t *session, const uint8_t *data)
{
    gmp_flux_hook_t *hook = hook_find_by_flux(session);
    if (hook) {
        esp_gmp_link_t link = hook->link;

        esp_gmp_transport_tx_done(link, data);

        hook = hook_find_by_flux(session);
        if (hook && hook->closing && hook->flux == session && !hook_has_in_flight(hook)) {
            memset(hook, 0, sizeof(*hook));
        }
    }
}

static esp_err_t gmp_flux_send(void *ctx, const uint8_t *data, size_t len)
{
    flux_session_t *flux = (flux_session_t *)ctx;
    uint8_t window = (uint8_t)CONFIG_ESP_GMP_FLUX_WINDOW_SIZE;
    uint8_t threshold = (uint8_t)CONFIG_ESP_GMP_FLUX_WINDOW_THRESHOLD;

    return flux_session_send(flux, data, (uint32_t)len, window, threshold);
}

static size_t gmp_flux_max_payload(void *ctx)
{
    size_t cap = flux_session_max_message_bytes((const flux_session_t *)ctx);
    return cap > 10 ? cap - 10 : 0;
}

static bool gmp_flux_can_send(void *ctx)
{
    return flux_session_send_idle((flux_session_t *)ctx);
}

static const esp_gmp_transport_t s_gmp_flux_transport = {
    .send = gmp_flux_send,
    .max_payload = gmp_flux_max_payload,
    .can_send = gmp_flux_can_send,
};

esp_err_t esp_gmp_flux_link_register(esp_gmp_link_t link, flux_session_t *flux)
{
    if (!link || !flux) {
        return ESP_ERR_INVALID_ARG;
    }

    gmp_flux_hook_t *hook = hook_alloc(link);
    if (!hook) {
        return ESP_ERR_NO_MEM;
    }
    if (hook->flux && hook->flux != flux) {
        if (hook->closing && !hook_has_in_flight(hook)) {
            memset(hook, 0, sizeof(*hook));
            hook->link = link;
        } else {
            return ESP_ERR_INVALID_STATE;
        }
    }
    if (hook_has_in_flight(hook) && hook->flux == flux) {
        return ESP_ERR_INVALID_STATE;
    }
    hook->flux = flux;
    hook->closing = false;

    esp_err_t err = esp_gmp_link_register(link, &s_gmp_flux_transport, flux);
    if (err != ESP_OK) {
        hook->flux = NULL;
    }
    return err;
}

void esp_gmp_flux_link_unregister(esp_gmp_link_t link)
{
    gmp_flux_hook_t *hook = hook_find(link);
    if (hook) {
        hook->closing = true;
    }
    esp_gmp_link_unregister(link);
    if (hook && !hook_has_in_flight(hook)) {
        memset(hook, 0, sizeof(*hook));
    }
}

static void gmp_flux_data_received(flux_session_t *session, uint8_t stream_id, const uint8_t *data, uint32_t size)
{
    gmp_flux_hook_t *hook = hook_find_by_flux(session);
    bool taken = gmp_flux_forward_to_gmp(session, data, size);

    if (!taken && hook && hook->user_cbs.data_received_cb) {
        hook->user_cbs.data_received_cb(session, stream_id, data, size);
        return;
    }

    if (!taken) {
        free((void *)data);
    }
}

static void gmp_flux_session_complete(flux_session_t *session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size)
{
    gmp_flux_hook_t *hook = hook_find_by_flux(session);
    void (*user_complete)(flux_session_t *session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size) = NULL;
    esp_gmp_link_t link = hook ? hook->link : NULL;

    if (hook) {
        user_complete = hook->user_cbs.session_complete_cb;
    }
    gmp_flux_tx_done_and_cleanup(session, data);
    (void)size;

    if (user_complete) {
        /* GMP owns/releases the TX frame; do not forward a freed pointer. */
        user_complete(session, stream_id, status, NULL, 0);
    }

    if (link && status != ESP_OK) {
        esp_gmp_notify_link_event(link, ESP_GMP_LINK_EVENT_TRANSPORT_ERROR, status);
    }
}

static void gmp_flux_progress(flux_session_t *session, uint8_t stream_id, uint32_t transferred, uint32_t total)
{
    gmp_flux_hook_t *hook = hook_find_by_flux(session);
    if (hook && hook->user_cbs.progress_cb) {
        hook->user_cbs.progress_cb(session, stream_id, transferred, total);
    }
}

static void gmp_flux_error(flux_session_t *session, esp_err_t error)
{
    gmp_flux_hook_t *hook = hook_find_by_flux(session);
    esp_gmp_link_t link = hook ? hook->link : NULL;

    if (hook && hook->user_cbs.error_cb) {
        hook->user_cbs.error_cb(session, error);
    }
    if (link) {
        esp_gmp_notify_link_event(link, ESP_GMP_LINK_EVENT_TRANSPORT_ERROR, error);
    }
}

esp_err_t esp_gmp_flux_get_callbacks(esp_gmp_link_t link, flux_callbacks_t *out, const flux_callbacks_t *user_cbs)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    gmp_flux_hook_t *hook = hook_alloc(link);
    if (!hook) {
        return ESP_ERR_NO_MEM;
    }
    if (hook && user_cbs) {
        hook->user_cbs = *user_cbs;
    } else if (hook) {
        memset(&hook->user_cbs, 0, sizeof(hook->user_cbs));
    }

    out->session_complete_cb = gmp_flux_session_complete;
    out->data_received_cb = gmp_flux_data_received;
    out->progress_cb = gmp_flux_progress;
    out->error_cb = gmp_flux_error;
    out->arg = NULL;
    return ESP_OK;
}

static void gmp_flux_gatt_data_received(gatt_session_t *session, uint8_t stream_id, const uint8_t *data, uint32_t size)
{
    gmp_flux_hook_t *hook = hook_find((esp_gmp_link_t)session);
    bool taken = false;

    flux_session_t *flux = gatt_session_get_flux(session);
    if (flux) {
        taken = gmp_flux_forward_to_gmp(flux, data, size);
    }

    if (!taken && hook && hook->user_gatt_cbs.data_received_cb) {
        hook->user_gatt_cbs.data_received_cb(session, stream_id, data, size);
        return;
    }

    if (!taken) {
        free((void *)data);
    }
}

static void gmp_flux_gatt_session_complete(gatt_session_t *session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size)
{
    gmp_flux_hook_t *hook = hook_find((esp_gmp_link_t)session);
    void (*user_complete)(gatt_session_t *session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size) = NULL;
    esp_gmp_link_t link = hook ? hook->link : NULL;

    if (hook) {
        user_complete = hook->user_gatt_cbs.session_complete_cb;
    }

    flux_session_t *flux = gatt_session_get_flux(session);
    if (flux) {
        gmp_flux_tx_done_and_cleanup(flux, data);
    }
    (void)size;

    if (user_complete) {
        /* GMP owns/releases the TX frame; do not forward a freed pointer. */
        user_complete(session, stream_id, status, NULL, 0);
    }

    if (link && status != ESP_OK) {
        esp_gmp_notify_link_event(link, ESP_GMP_LINK_EVENT_TRANSPORT_ERROR, status);
    }
}

static void gmp_flux_gatt_progress(gatt_session_t *session, uint8_t stream_id, uint32_t transferred, uint32_t total)
{
    gmp_flux_hook_t *hook = hook_find((esp_gmp_link_t)session);
    if (hook && hook->user_gatt_cbs.progress_cb) {
        hook->user_gatt_cbs.progress_cb(session, stream_id, transferred, total);
    }
}

static void gmp_flux_gatt_error(gatt_session_t *session, esp_err_t error)
{
    gmp_flux_hook_t *hook = hook_find((esp_gmp_link_t)session);
    esp_gmp_link_t link = hook ? hook->link : NULL;

    if (hook && hook->user_gatt_cbs.error_cb) {
        hook->user_gatt_cbs.error_cb(session, error);
    }
    if (link) {
        esp_gmp_notify_link_event(link, ESP_GMP_LINK_EVENT_TRANSPORT_ERROR, error);
    }
}

static void gmp_flux_gatt_mtu_changed(gatt_session_t *session, uint16_t mtu)
{
    gmp_flux_hook_t *hook = hook_find((esp_gmp_link_t)session);
    esp_gmp_link_t link = hook ? hook->link : (esp_gmp_link_t)session;

    if (hook && hook->user_gatt_cbs.mtu_changed_cb) {
        hook->user_gatt_cbs.mtu_changed_cb(session, mtu);
    }
    if (link) {
        esp_gmp_notify_link_event(link, ESP_GMP_LINK_EVENT_MTU_CHANGED, ESP_OK);
    }
}

esp_err_t esp_gmp_flux_get_gatt_callbacks(esp_gmp_link_t link, ble_session_callbacks_t *out, const ble_session_callbacks_t *user_cbs)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    gmp_flux_hook_t *hook = hook_alloc(link);
    if (!hook) {
        return ESP_ERR_NO_MEM;
    }

    if (user_cbs) {
        hook->user_gatt_cbs = *user_cbs;
    } else {
        memset(&hook->user_gatt_cbs, 0, sizeof(hook->user_gatt_cbs));
    }

    out->session_complete_cb = gmp_flux_gatt_session_complete;
    out->data_received_cb = gmp_flux_gatt_data_received;
    out->progress_cb = gmp_flux_gatt_progress;
    out->error_cb = gmp_flux_gatt_error;
    out->mtu_changed_cb = gmp_flux_gatt_mtu_changed;
    out->arg = NULL;
    return ESP_OK;
}
