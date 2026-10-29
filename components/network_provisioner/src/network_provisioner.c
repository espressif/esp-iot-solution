/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

#include "network_provisioner.h"
#include "network_provisioner_priv.h"

static const char *TAG = "net_prov";

network_provisioner_t *network_provisioner_create(protocomm_ext_t *pc)
{
    if (!pc) {
        return NULL;
    }

    network_provisioner_t *np = calloc(1, sizeof(*np));
    if (!np) {
        return NULL;
    }
    np->pc = pc;

    /* BLE name→UUID (no-op on HTTP / Console). Matches wifi_prov manager.c. */
    if (protocomm_ext_set_config_endpoint(pc, NETWORK_PROVISIONER_EP_CTRL,
                                          NETWORK_PROVISIONER_EP_CTRL_UUID) != ESP_OK ||
            protocomm_ext_set_config_endpoint(pc, NETWORK_PROVISIONER_EP_SCAN,
                                              NETWORK_PROVISIONER_EP_SCAN_UUID) != ESP_OK ||
            protocomm_ext_set_config_endpoint(pc, NETWORK_PROVISIONER_EP_SESSION,
                                              NETWORK_PROVISIONER_EP_SESSION_UUID) != ESP_OK ||
            protocomm_ext_set_config_endpoint(pc, NETWORK_PROVISIONER_EP_CONFIG,
                                              NETWORK_PROVISIONER_EP_CONFIG_UUID) != ESP_OK ||
            protocomm_ext_set_config_endpoint(pc, NETWORK_PROVISIONER_EP_PROTO_VER,
                                              NETWORK_PROVISIONER_EP_PROTO_VER_UUID) != ESP_OK) {
        free(np);
        return NULL;
    }

    return np;
}

void network_provisioner_delete(network_provisioner_t *np)
{
    if (!np) {
        return;
    }
    protocomm_ext_t *owned = np->owns_pc ? np->pc : NULL;
    free(np->version_json);
    free(np);
    if (owned) {
        protocomm_ext_delete(owned);
        /* Host task must leave before the next NimBLE init. */
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

network_provisioner_t *network_provisioner_create_ble(void)
{
    protocomm_ext_config_data_t cfg = {
        .transport_method = PROTOCOMM_EXT_TRANSPORT_METHOD_BLE,
        .transport_data = NULL,
        .security_data = NULL,
    };
    /* Placeholder only. set_security() replaces it after proto-ver is known.
     * Sec0 can be compiled out; Sec1 accepts NULL params. Sec2-only builds
     * need some params so protocomm_ext_new() can finish. */
#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_0
    cfg.security_method = PROTOCOMM_EXT_SECURITY_METHOD_NONE;
#elif CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_1
    cfg.security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1;
#elif CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_2
    static const protocomm_ext_security2_params_t sec2_placeholder = {
        .username = "placeholder",
        .username_len = sizeof("placeholder") - 1,
        .password = "placeholder",
        .password_len = sizeof("placeholder") - 1,
    };
    cfg.security_method = PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2;
    cfg.security_data = &sec2_placeholder;
#else
    ESP_LOGE(TAG, "No protocomm security version is enabled");
    return NULL;
#endif
    protocomm_ext_t *pc = protocomm_ext_new(&cfg);
    if (!pc) {
        /* new() may have started NimBLE before failing. Let the host task exit. */
        vTaskDelay(pdMS_TO_TICKS(300));
        return NULL;
    }
    network_provisioner_t *np = network_provisioner_create(pc);
    if (!np) {
        protocomm_ext_delete(pc);
        vTaskDelay(pdMS_TO_TICKS(300));
        return NULL;
    }
    np->owns_pc = true;
    return np;
}

static esp_err_t cred_len(const char *src, size_t *out_len)
{
    size_t n = src ? strlen(src) : 0;
    if (n > 64) {
        return ESP_ERR_INVALID_SIZE;
    }
    *out_len = n;
    return ESP_OK;
}

esp_err_t network_provisioner_set_security(network_provisioner_t *np,
                                           const network_provisioner_security_t *sec)
{
    if (!np || !np->pc || !sec) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sec->sec_ver < 0 || sec->sec_ver > 2) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (sec->sec_ver == 0) {
        return protocomm_ext_set_security(np->pc, PROTOCOMM_EXT_SECURITY_METHOD_NONE, NULL);
    }

    if (sec->sec_ver == 1) {
        if (sec->no_pop || !sec->pop || !sec->pop[0]) {
            return protocomm_ext_set_security(np->pc, PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, NULL);
        }
        size_t pop_len = 0;
        esp_err_t err = cred_len(sec->pop, &pop_len);
        if (err != ESP_OK) {
            return err;
        }
        protocomm_ext_security1_params_t sec1 = {
            .data = (const uint8_t *)sec->pop,
            .len = (uint16_t)pop_len,
        };
        return protocomm_ext_set_security(np->pc, PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1, &sec1);
    }

    if (!sec->username || !sec->username[0] || !sec->pop) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t user_len = 0;
    size_t pass_len = 0;
    esp_err_t err = cred_len(sec->username, &user_len);
    if (err != ESP_OK) {
        return err;
    }
    err = cred_len(sec->pop, &pass_len);
    if (err != ESP_OK) {
        return err;
    }
    protocomm_ext_security2_params_t sec2 = {
        .username = sec->username,
        .username_len = (uint16_t)user_len,
        .password = sec->pop,
        .password_len = (uint16_t)pass_len,
    };
    return protocomm_ext_set_security(np->pc, PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2, &sec2);
}

esp_err_t network_provisioner_add_endpoint(network_provisioner_t *np,
                                           const char *name, uint16_t uuid)
{
    if (!np || !np->pc || !name || !name[0]) {
        return ESP_ERR_INVALID_ARG;
    }
    return protocomm_ext_set_config_endpoint(np->pc, name, uuid);
}

esp_err_t network_provisioner_send(network_provisioner_t *np, const char *ep_name,
                                   const uint8_t *req, size_t req_len,
                                   uint8_t **resp, size_t *resp_len)
{
    return network_provisioner_send_ep(np, ep_name, req, req_len, resp, resp_len);
}

esp_err_t network_provisioner_get_version(network_provisioner_t *np,
                                          const char **version_json)
{
    if (!np || !version_json) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!np->version_json) {
        return ESP_ERR_INVALID_STATE;
    }
    *version_json = np->version_json;
    return ESP_OK;
}

protocomm_ext_t *network_provisioner_get_protocomm(network_provisioner_t *np)
{
    return np ? np->pc : NULL;
}

esp_err_t network_provisioner_send_ep(network_provisioner_t *np, const char *ep_name,
                                      const uint8_t *req, size_t req_len,
                                      uint8_t **resp, size_t *resp_len)
{
    if (!np || !np->pc || !ep_name || !req || !req_len || !resp || !resp_len) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!np->session_started) {
        return ESP_ERR_INVALID_STATE;
    }
    return protocomm_ext_send_data(np->pc, ep_name, req, req_len, resp, resp_len);
}

/**
 * Locate the `{...}` object value for key "prov". *out_len includes both braces.
 * Skips JSON string contents when matching braces.
 */
static const char *find_prov_object(const char *json, size_t *out_len)
{
    const char *key = strstr(json, "\"prov\"");
    if (!key) {
        return NULL;
    }
    const char *p = key + strlen("\"prov\"");
    while (*p && isspace((unsigned char) * p)) {
        p++;
    }
    if (*p != ':') {
        return NULL;
    }
    p++;
    while (*p && isspace((unsigned char) * p)) {
        p++;
    }
    if (*p != '{') {
        return NULL;
    }

    const char *start = p;
    int depth = 0;
    for (; *p; p++) {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) {
                    p++;
                }
                p++;
            }
            if (!*p) {
                return NULL;
            }
            continue;
        }
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0) {
                *out_len = (size_t)(p - start + 1);
                return start;
            }
        }
    }
    return NULL;
}

static bool find_json_cap_array(const char *json, size_t json_len,
                                const char **out_start, const char **out_end)
{
    const char *key_lit = "\"cap\"";
    size_t key_len = strlen(key_lit);
    const char *limit = json + json_len;
    const char *cap_key = NULL;
    for (const char *cursor = json; cursor + key_len <= limit; cursor++) {
        if (memcmp(cursor, key_lit, key_len) == 0) {
            cap_key = cursor;
            break;
        }
    }
    if (!cap_key) {
        return false;
    }
    const char *lbr = NULL;
    for (const char *p = cap_key + key_len; p < limit; p++) {
        if (*p == '[') {
            lbr = p;
            break;
        }
    }
    if (!lbr) {
        return false;
    }
    const char *rbr = NULL;
    for (const char *p = lbr + 1; p < limit; p++) {
        if (*p == ']') {
            rbr = p;
            break;
        }
    }
    if (!rbr || rbr <= lbr) {
        return false;
    }
    *out_start = lbr;
    *out_end = rbr;
    return true;
}

static bool cap_array_has_token(const char *arr_start, const char *arr_end, const char *token)
{
    size_t tlen = strlen(token);
    for (const char *p = arr_start; p < arr_end; p++) {
        if (*p != '"') {
            continue;
        }
        if ((size_t)(arr_end - (p + 1)) < tlen + 1) {
            break;
        }
        if (strncmp(p + 1, token, tlen) == 0 && p[1 + tlen] == '"') {
            return true;
        }
    }
    return false;
}

static esp_err_t parse_sec_ver_field(const char *json, size_t json_len,
                                     int *out_sec_ver, bool *found)
{
    *found = false;
    const char *key_lit = "\"sec_ver\"";
    size_t key_len = strlen(key_lit);
    const char *limit = json + json_len;
    const char *sec_key = NULL;
    for (const char *cursor = json; cursor + key_len <= limit; cursor++) {
        if (memcmp(cursor, key_lit, key_len) == 0) {
            sec_key = cursor;
            break;
        }
    }
    if (!sec_key) {
        return ESP_OK;
    }

    const char *p = sec_key + key_len;
    while (p < limit && *p != ':') {
        p++;
    }
    if (p >= limit || *p != ':') {
        return ESP_ERR_INVALID_RESPONSE;
    }
    p++;
    while (p < limit && isspace((unsigned char) * p)) {
        p++;
    }
    if (p >= limit || !isdigit((unsigned char) * p)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p || end > limit || v < 0 || v > 2) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *out_sec_ver = (int)v;
    *found = true;
    return ESP_OK;
}

esp_err_t network_provisioner_parse_capabilities(const char *json,
                                                 network_provisioner_capabilities_t *caps)
{
    if (!json || !caps) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(caps, 0, sizeof(*caps));
    caps->sec_ver = -1;

    size_t prov_len = 0;
    const char *prov = find_prov_object(json, &prov_len);
    if (!prov) {
        /* Bare version string (very old peers): assume Sec1 + Wi-Fi. */
        caps->wifi_prov = true;
        caps->wifi_scan = true;
        caps->sec_ver = 1;
        return ESP_OK;
    }

    const char *arr_s = NULL;
    const char *arr_e = NULL;
    bool has_cap = find_json_cap_array(prov, prov_len, &arr_s, &arr_e);

    if (has_cap) {
        caps->wifi_prov = cap_array_has_token(arr_s, arr_e, "wifi_prov");
        caps->wifi_scan = cap_array_has_token(arr_s, arr_e, "wifi_scan");
        caps->thread_prov = cap_array_has_token(arr_s, arr_e, "thread_prov");
        caps->thread_scan = cap_array_has_token(arr_s, arr_e, "thread_scan");
        caps->no_sec = cap_array_has_token(arr_s, arr_e, "no_sec");
        caps->no_pop = cap_array_has_token(arr_s, arr_e, "no_pop");
        /*
         * IDF in-tree wifi_provisioning advertises "wifi_scan" but not "wifi_prov"
         * (network_provisioning adds the latter). Treat scan-capable Wi-Fi peers
         * without Thread caps as wifi_prov.
         */
        if (caps->wifi_scan && !caps->wifi_prov &&
                !caps->thread_prov && !caps->thread_scan) {
            caps->wifi_prov = true;
        }
    }

    bool sec_found = false;
    esp_err_t err = parse_sec_ver_field(prov, prov_len, &caps->sec_ver, &sec_found);
    if (err != ESP_OK) {
        return err;
    }
    /* Structured proto-ver must advertise sec_ver explicitly (no silent Sec0). */
    if (!sec_found) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

static esp_err_t fetch_version_locked(network_provisioner_t *np)
{
    uint8_t *ver = NULL;
    size_t ver_len = 0;
    esp_err_t err = protocomm_ext_get_version_capabilities(np->pc, NETWORK_PROVISIONER_EP_PROTO_VER,
                                                           &ver, &ver_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get_version failed: %s", esp_err_to_name(err));
        return err;
    }

    free(np->version_json);
    np->version_json = calloc(1, ver_len + 1);
    if (!np->version_json) {
        free(ver);
        return ESP_ERR_NO_MEM;
    }
    memcpy(np->version_json, ver, ver_len);
    free(ver);

    err = network_provisioner_parse_capabilities(np->version_json, &np->caps);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "parse_capabilities failed: %s", esp_err_to_name(err));
        np->caps_valid = false;
        return err;
    }
    np->caps_valid = true;
    ESP_LOGI(TAG, "capabilities: sec_ver=%d no_sec=%d no_pop=%d wifi_prov=%d wifi_scan=%d "
             "thread_prov=%d thread_scan=%d",
             np->caps.sec_ver, np->caps.no_sec, np->caps.no_pop,
             np->caps.wifi_prov, np->caps.wifi_scan, np->caps.thread_prov, np->caps.thread_scan);
    return ESP_OK;
}

esp_err_t network_provisioner_fetch_capabilities(network_provisioner_t *np,
                                                 const void *transport_connect_cfg)
{
    if (!np || !np->pc) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = protocomm_ext_open_session(np->pc, transport_connect_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open_session failed: %s", esp_err_to_name(err));
        return err;
    }

    err = fetch_version_locked(np);
    if (err != ESP_OK) {
        protocomm_ext_close_session(np->pc);
        return err;
    }
    return ESP_OK;
}

esp_err_t network_provisioner_establish_security(network_provisioner_t *np)
{
    if (!np || !np->pc) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Sec2 client keygen is CPU-heavy. Prefer calling protocomm_ext_security_init()
     * before SoftAP/HTTP keep-alive when the scheme is known in advance.
     */
    esp_err_t err = protocomm_ext_security_init(np->pc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "security_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = protocomm_ext_establish_security(np->pc, NETWORK_PROVISIONER_EP_SESSION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "establish_security failed: %s", esp_err_to_name(err));
        return err;
    }

    np->session_started = true;
    return ESP_OK;
}

esp_err_t network_provisioner_start_session(network_provisioner_t *np,
                                            const void *transport_connect_cfg)
{
    if (!np || !np->pc) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * When security is already configured (Sec2 especially), finish local keygen
     * before SoftAP HTTP keep-alive so the peer does not idle-kick the STA.
     */
    esp_err_t err = protocomm_ext_security_init(np->pc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "security_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = network_provisioner_fetch_capabilities(np, transport_connect_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = protocomm_ext_establish_security(np->pc, NETWORK_PROVISIONER_EP_SESSION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "establish_security failed: %s", esp_err_to_name(err));
        protocomm_ext_close_session(np->pc);
        return err;
    }

    np->session_started = true;
    return ESP_OK;
}

esp_err_t network_provisioner_stop_session(network_provisioner_t *np)
{
    if (!np || !np->pc) {
        return ESP_ERR_INVALID_ARG;
    }
    np->session_started = false;
    return protocomm_ext_close_session(np->pc);
}

esp_err_t network_provisioner_get_capabilities(network_provisioner_t *np,
                                               network_provisioner_capabilities_t *caps)
{
    if (!np || !caps) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!np->caps_valid) {
        return ESP_ERR_INVALID_STATE;
    }
    *caps = np->caps;
    return ESP_OK;
}

static esp_err_t poll_deadline_sleep(uint32_t *elapsed_ms, uint32_t timeout_ms, uint32_t interval_ms)
{
    uint32_t remaining = (timeout_ms > *elapsed_ms) ? (timeout_ms - *elapsed_ms) : 0;
    if (remaining == 0) {
        return ESP_ERR_TIMEOUT;
    }
    uint32_t sleep_ms = interval_ms < remaining ? interval_ms : remaining;
    vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    *elapsed_ms += sleep_ms;
    return ESP_OK;
}

esp_err_t network_provisioner_provision_wifi(network_provisioner_t *np,
                                             const network_provisioner_wifi_creds_t *creds,
                                             network_provisioner_wifi_status_t *out_status)
{
    if (!np || !creds || !creds->ssid || creds->ssid_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t timeout_ms = creds->poll_timeout_ms ? creds->poll_timeout_ms
                          : CONFIG_NETWORK_PROVISIONER_DEFAULT_POLL_TIMEOUT_MS;
    uint32_t interval_ms = creds->poll_interval_ms ? creds->poll_interval_ms
                           : CONFIG_NETWORK_PROVISIONER_DEFAULT_POLL_INTERVAL_MS;
    if (interval_ms == 0 || timeout_ms == 0 || interval_ms > timeout_ms) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = network_provisioner_wifi_set_config(
                        np, creds->ssid, creds->ssid_len,
                        creds->passphrase, creds->passphrase_len,
                        creds->bssid, creds->channel);
    if (err != ESP_OK) {
        return err;
    }

    err = network_provisioner_wifi_apply_config(np);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t elapsed = 0;
    network_provisioner_wifi_status_t status = {0};
    while (elapsed <= timeout_ms) {
        err = network_provisioner_wifi_get_status(np, &status);
        if (err != ESP_OK) {
            return err;
        }
        if (status.state == NETWORK_PROVISIONER_WIFI_STATE_CONNECTED) {
            if (out_status) {
                *out_status = status;
            }
            return ESP_OK;
        }
        if (status.state == NETWORK_PROVISIONER_WIFI_STATE_CONNECTION_FAILED) {
            if (out_status) {
                *out_status = status;
            }
            return ESP_FAIL;
        }
        if (elapsed >= timeout_ms) {
            break;
        }
        err = poll_deadline_sleep(&elapsed, timeout_ms, interval_ms);
        if (err != ESP_OK) {
            break;
        }
    }

    if (out_status) {
        *out_status = status;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t network_provisioner_provision_thread(network_provisioner_t *np,
                                               const network_provisioner_thread_creds_t *creds,
                                               network_provisioner_thread_status_t *out_status)
{
    if (!np || !creds || !creds->dataset || creds->dataset_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t timeout_ms = creds->poll_timeout_ms ? creds->poll_timeout_ms
                          : CONFIG_NETWORK_PROVISIONER_DEFAULT_POLL_TIMEOUT_MS;
    uint32_t interval_ms = creds->poll_interval_ms ? creds->poll_interval_ms
                           : CONFIG_NETWORK_PROVISIONER_DEFAULT_POLL_INTERVAL_MS;
    if (interval_ms == 0 || timeout_ms == 0 || interval_ms > timeout_ms) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = network_provisioner_thread_set_config(np, creds->dataset, creds->dataset_len);
    if (err != ESP_OK) {
        return err;
    }

    err = network_provisioner_thread_apply_config(np);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t elapsed = 0;
    network_provisioner_thread_status_t status = {0};
    while (elapsed <= timeout_ms) {
        err = network_provisioner_thread_get_status(np, &status);
        if (err != ESP_OK) {
            return err;
        }
        if (status.state == NETWORK_PROVISIONER_THREAD_STATE_ATTACHED) {
            if (out_status) {
                *out_status = status;
            }
            return ESP_OK;
        }
        if (status.state == NETWORK_PROVISIONER_THREAD_STATE_ATTACHING_FAILED) {
            if (out_status) {
                *out_status = status;
            }
            return ESP_FAIL;
        }
        if (elapsed >= timeout_ms) {
            break;
        }
        err = poll_deadline_sleep(&elapsed, timeout_ms, interval_ms);
        if (err != ESP_OK) {
            break;
        }
    }

    if (out_status) {
        *out_status = status;
    }
    return ESP_ERR_TIMEOUT;
}
