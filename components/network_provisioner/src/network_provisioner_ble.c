/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include "network_provisioner_priv.h"

#if CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_ROLE_CENTRAL && \
    CONFIG_BT_NIMBLE_ROLE_OBSERVER && CONFIG_BT_NIMBLE_GATT_CLIENT

static const char *TAG = "net_prov_ble";

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <protocomm_ext_nimble.h>

_Static_assert(sizeof(ble_addr_t) <= NETWORK_PROVISIONER_BLE_CONNECT_CFG_MAX,
               "BLE connect cfg blob is smaller than ble_addr_t");

static void addr_from_nimble(const ble_addr_t *in, network_provisioner_ble_device_t *out)
{
    for (int i = 0; i < 6; i++) {
        out->addr[i] = in->val[5 - i];
    }
    out->addr_type = in->type;
}

static void addr_to_nimble(const uint8_t addr[6], uint8_t addr_type, ble_addr_t *out)
{
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 6; i++) {
        out->val[i] = addr[5 - i];
    }
    out->type = addr_type;
}

/* nimble_init returns before host sync. start_scan before ble_hs_id_ctx exists
 * faults in ble_hs_id_infer_auto. */
static esp_err_t wait_host_ready(void)
{
    for (int i = 0; i < 100; i++) {
        if (ble_hs_synced()) {
            if (ble_hs_util_ensure_addr(0) == 0) {
                uint8_t own_addr_type = 0;
                if (ble_hs_id_infer_auto(0, &own_addr_type) == 0) {
                    return ESP_OK;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGE(TAG, "NimBLE host not ready");
    return ESP_ERR_TIMEOUT;
}

static bool name_matches(const uint8_t *name, size_t name_len, const char *filter, bool exact)
{
    if (!filter || !filter[0]) {
        return true;
    }
    if (!name || name_len == 0) {
        return false;
    }
    size_t filter_len = strlen(filter);
    if (exact) {
        return name_len == filter_len && memcmp(name, filter, filter_len) == 0;
    }
    return name_len >= filter_len && memcmp(name, filter, filter_len) == 0;
}

static bool require_ble(const network_provisioner_t *np)
{
    return np && np->pc &&
           protocomm_ext_get_transport_method(np->pc) == PROTOCOMM_EXT_TRANSPORT_METHOD_BLE;
}

esp_err_t network_provisioner_ble_scan(network_provisioner_t *np, uint32_t timeout_ms,
                                       const char *name, bool name_exact,
                                       const uint8_t *addr,
                                       network_provisioner_ble_device_t *out,
                                       size_t capacity, size_t *out_count)
{
    if (!np || !np->pc || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = 0;
    if (!require_ble(np)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!out || capacity == 0 || timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_ext_transport_handle_t th = protocomm_ext_get_transport_handle(np->pc);
    if (!th) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = wait_host_ready();
    if (err != ESP_OK) {
        return err;
    }

    protocomm_ext_nimble_scan_config_t cfg = {
        .scan_timeout_ms = timeout_ms,
    };
    err = esp_protocomm_ext_nimble_start_scan(th, &cfg);
    esp_protocomm_ext_nimble_stop_scan(th);
    if (err != ESP_OK) {
        return err;
    }

    int count = esp_protocomm_ext_nimble_get_scanned_device_count(th);
    size_t listed = 0;
    for (int i = 0; i < count && listed < capacity; i++) {
        protocomm_ext_nimble_scanned_device_info_t info = {0};
        if (esp_protocomm_ext_nimble_get_scanned_device_info(th, i, &info) != ESP_OK) {
            continue;
        }
        uint8_t copied_addr[6];
        for (int b = 0; b < 6; b++) {
            copied_addr[b] = info.addr.val[5 - b];
        }
        bool match = name_matches(info.name, info.name_len, name, name_exact);
        if (match && addr) {
            match = memcmp(copied_addr, addr, 6) == 0;
        }
        if (!match) {
            free(info.name);
            free(info.mfg_data);
            continue;
        }

        network_provisioner_ble_device_t *dev = &out[listed];
        memset(dev, 0, sizeof(*dev));
        addr_from_nimble(&info.addr, dev);
        if (info.name && info.name_len > 0) {
            size_t n = info.name_len;
            if (n >= sizeof(dev->name)) {
                n = sizeof(dev->name) - 1;
            }
            memcpy(dev->name, info.name, n);
        }
        if (info.mfg_data && info.mfg_data_len > 0) {
            size_t n = info.mfg_data_len;
            if (n > NETWORK_PROVISIONER_BLE_MFG_MAX) {
                n = NETWORK_PROVISIONER_BLE_MFG_MAX;
            }
            memcpy(dev->mfg_data, info.mfg_data, n);
            dev->mfg_data_len = (uint8_t)n;
        }
        listed++;
        free(info.name);
        free(info.mfg_data);
    }
    *out_count = listed;
    return ESP_OK;
}

esp_err_t network_provisioner_ble_connect_cfg(const uint8_t addr[6], uint8_t addr_type,
                                              void *out, size_t out_size)
{
    if (!addr || !out || out_size < sizeof(ble_addr_t)) {
        return ESP_ERR_INVALID_ARG;
    }
    addr_to_nimble(addr, addr_type, (ble_addr_t *)out);
    return ESP_OK;
}

esp_err_t network_provisioner_ble_has_char(network_provisioner_t *np,
                                           const uint8_t svc_uuid128[16],
                                           uint16_t chr_uuid16)
{
    if (!np || !np->pc || !svc_uuid128) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!require_ble(np)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    protocomm_ext_transport_handle_t th = protocomm_ext_get_transport_handle(np->pc);
    if (!th) {
        return ESP_ERR_INVALID_STATE;
    }

    ble_uuid128_t svc;
    ble_uuid128_t chr;
    memcpy(svc.value, svc_uuid128, 16);
    svc.u.type = BLE_UUID_TYPE_128;
    memcpy(chr.value, svc_uuid128, 16);
    chr.u.type = BLE_UUID_TYPE_128;
    chr.value[12] = (uint8_t)(chr_uuid16 & 0xff);
    chr.value[13] = (uint8_t)((chr_uuid16 >> 8) & 0xff);
    return esp_protocomm_ext_nimble_find_chr_uuid(th, &svc.u, &chr.u);
}

#else

esp_err_t network_provisioner_ble_scan(network_provisioner_t *np, uint32_t timeout_ms,
                                       const char *name, bool name_exact,
                                       const uint8_t *addr,
                                       network_provisioner_ble_device_t *out,
                                       size_t capacity, size_t *out_count)
{
    (void)np;
    (void)timeout_ms;
    (void)name;
    (void)name_exact;
    (void)addr;
    (void)out;
    (void)capacity;
    if (out_count) {
        *out_count = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t network_provisioner_ble_connect_cfg(const uint8_t addr[6], uint8_t addr_type,
                                              void *out, size_t out_size)
{
    (void)addr;
    (void)addr_type;
    (void)out;
    (void)out_size;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t network_provisioner_ble_has_char(network_provisioner_t *np,
                                           const uint8_t svc_uuid128[16],
                                           uint16_t chr_uuid16)
{
    (void)np;
    (void)svc_uuid128;
    (void)chr_uuid16;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
