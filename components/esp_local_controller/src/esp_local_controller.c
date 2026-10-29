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

#include "constants.pb-c.h"
#include "esp_local_ctrl.pb-c.h"
#include "esp_local_controller.h"
#include "esp_local_controller_priv.h"

static const char *TAG = "local_ctrl";

static void version_clear(esp_local_controller_version_t *ver)
{
    if (!ver) {
        return;
    }
    free(ver->ver);
    ver->ver = NULL;
    ver->sec_ver = -1;
    ver->sec_patch_ver = -1;
}

static void version_invalidate(esp_local_controller_t *ctrl)
{
    if (!ctrl) {
        return;
    }
    free(ctrl->version_json);
    ctrl->version_json = NULL;
    version_clear(&ctrl->version);
    ctrl->version_valid = false;
}

static esp_err_t check_status(Status st, const char *what)
{
    if (st != STATUS__Success) {
        ESP_LOGE(TAG, "%s status=%d", what, (int)st);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * Locate the `{...}` object value for key "local_ctrl" and return a pointer to
 * the opening brace. *out_len includes both braces. Skips JSON string contents
 * when matching braces so embedded `{`/`}` in strings do not confuse the scan.
 */
static const char *find_local_ctrl_object(const char *json, size_t *out_len)
{
    const char *key = strstr(json, "\"local_ctrl\"");
    if (!key) {
        return NULL;
    }
    const char *p = key + strlen("\"local_ctrl\"");
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

static esp_err_t parse_int_field(const char *json, size_t json_len, const char *key,
                                 int *out, bool *found)
{
    *found = false;
    char keyed[64];
    int n = snprintf(keyed, sizeof(keyed), "\"%s\"", key);
    if (n <= 0 || (size_t)n >= sizeof(keyed)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Search only within [json, json + json_len). */
    const char *limit = json + json_len;
    const char *sec_key = NULL;
    for (const char *cursor = json; cursor + (size_t)n <= limit; cursor++) {
        if (memcmp(cursor, keyed, (size_t)n) == 0) {
            sec_key = cursor;
            break;
        }
    }
    if (!sec_key) {
        return ESP_OK;
    }

    const char *p = sec_key + (size_t)n;
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
    if (p >= limit || (!isdigit((unsigned char) * p) && *p != '-')) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p || end > limit) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *out = (int)v;
    *found = true;
    return ESP_OK;
}

static esp_err_t parse_ver_string(const char *json, size_t json_len, char **out_ver)
{
    *out_ver = NULL;
    const char *key_lit = "\"ver\"";
    size_t key_len = strlen(key_lit);
    const char *limit = json + json_len;
    const char *key = NULL;
    for (const char *cursor = json; cursor + key_len <= limit; cursor++) {
        if (memcmp(cursor, key_lit, key_len) == 0) {
            key = cursor;
            break;
        }
    }
    if (!key) {
        return ESP_OK;
    }

    const char *p = key + key_len;
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
    if (p >= limit || *p != '"') {
        return ESP_ERR_INVALID_RESPONSE;
    }
    p++;
    const char *end = p;
    while (end < limit && *end != '"') {
        if (*end == '\\' && end + 1 < limit) {
            end += 2;
            continue;
        }
        end++;
    }
    if (end >= limit || end < p) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    size_t len = (size_t)(end - p);
    char *copy = malloc(len + 1);
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, p, len);
    copy[len] = '\0';
    *out_ver = copy;
    return ESP_OK;
}

esp_err_t esp_local_controller_parse_version(const char *json, esp_local_controller_version_t *out)
{
    if (!json || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->sec_ver = -1;
    out->sec_patch_ver = -1;

    size_t obj_len = 0;
    const char *obj = find_local_ctrl_object(json, &obj_len);
    if (!obj || obj_len < 2) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    esp_err_t err = parse_ver_string(obj, obj_len, &out->ver);
    if (err != ESP_OK) {
        return err;
    }

    bool found = false;
    err = parse_int_field(obj, obj_len, "sec_ver", &out->sec_ver, &found);
    if (err != ESP_OK) {
        version_clear(out);
        return err;
    }
    if (!found || out->sec_ver < 0 || out->sec_ver > 2) {
        version_clear(out);
        return ESP_ERR_INVALID_RESPONSE;
    }

    found = false;
    err = parse_int_field(obj, obj_len, "sec_patch_ver", &out->sec_patch_ver, &found);
    if (err != ESP_OK) {
        version_clear(out);
        return err;
    }
    if (!found) {
        out->sec_patch_ver = -1;
    }

    return ESP_OK;
}

esp_local_controller_t *esp_local_controller_create(protocomm_ext_t *pc)
{
    if (!pc) {
        return NULL;
    }
    esp_local_controller_t *ctrl = calloc(1, sizeof(*ctrl));
    if (!ctrl) {
        return NULL;
    }
    ctrl->pc = pc;
    ctrl->version.sec_ver = -1;
    ctrl->version.sec_patch_ver = -1;

    /* BLE name→UUID (no-op on HTTP / Console). Matches IDF esp_local_ctrl. */
    if (protocomm_ext_set_config_endpoint(pc, ESP_LOCAL_CONTROLLER_EP_VERSION,
                                          ESP_LOCAL_CONTROLLER_EP_VERSION_UUID) != ESP_OK ||
            protocomm_ext_set_config_endpoint(pc, ESP_LOCAL_CONTROLLER_EP_SESSION,
                                              ESP_LOCAL_CONTROLLER_EP_SESSION_UUID) != ESP_OK ||
            protocomm_ext_set_config_endpoint(pc, ESP_LOCAL_CONTROLLER_EP_CONTROL,
                                              ESP_LOCAL_CONTROLLER_EP_CONTROL_UUID) != ESP_OK) {
        free(ctrl);
        return NULL;
    }

    return ctrl;
}

void esp_local_controller_delete(esp_local_controller_t *ctrl)
{
    if (!ctrl) {
        return;
    }
    version_invalidate(ctrl);
    free(ctrl);
}

protocomm_ext_t *esp_local_controller_get_protocomm(esp_local_controller_t *ctrl)
{
    return ctrl ? ctrl->pc : NULL;
}

esp_err_t esp_local_controller_send_ep(esp_local_controller_t *ctrl, const char *ep_name,
                                       const uint8_t *req, size_t req_len,
                                       uint8_t **resp, size_t *resp_len)
{
    if (!ctrl || !ctrl->pc || !ep_name || !req || !req_len || !resp || !resp_len) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ctrl->session_started) {
        return ESP_ERR_INVALID_STATE;
    }
    return protocomm_ext_send_data(ctrl->pc, ep_name, req, req_len, resp, resp_len);
}

static esp_err_t fetch_version_locked(esp_local_controller_t *ctrl)
{
    uint8_t *ver = NULL;
    size_t ver_len = 0;
    esp_err_t err = protocomm_ext_get_version_capabilities(ctrl->pc, ESP_LOCAL_CONTROLLER_EP_VERSION,
                                                           &ver, &ver_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get_version failed: %s", esp_err_to_name(err));
        version_invalidate(ctrl);
        return err;
    }

    free(ctrl->version_json);
    ctrl->version_json = calloc(1, ver_len + 1);
    if (!ctrl->version_json) {
        free(ver);
        version_invalidate(ctrl);
        return ESP_ERR_NO_MEM;
    }
    memcpy(ctrl->version_json, ver, ver_len);
    free(ver);

    version_clear(&ctrl->version);
    err = esp_local_controller_parse_version(ctrl->version_json, &ctrl->version);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "parse_version failed: %s", esp_err_to_name(err));
        version_invalidate(ctrl);
        return err;
    }
    ctrl->version_valid = true;
    ESP_LOGI(TAG, "local_ctrl ver=%s sec_ver=%d sec_patch_ver=%d",
             ctrl->version.ver ? ctrl->version.ver : "?",
             ctrl->version.sec_ver, ctrl->version.sec_patch_ver);
    return ESP_OK;
}

esp_err_t esp_local_controller_fetch_version(esp_local_controller_t *ctrl,
                                             const void *transport_connect_cfg)
{
    if (!ctrl || !ctrl->pc) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = protocomm_ext_open_session(ctrl->pc, transport_connect_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open_session failed: %s", esp_err_to_name(err));
        version_invalidate(ctrl);
        return err;
    }

    err = fetch_version_locked(ctrl);
    if (err != ESP_OK) {
        protocomm_ext_close_session(ctrl->pc);
        return err;
    }
    return ESP_OK;
}

esp_err_t esp_local_controller_establish_security(esp_local_controller_t *ctrl)
{
    if (!ctrl || !ctrl->pc) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = protocomm_ext_security_init(ctrl->pc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "security_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = protocomm_ext_establish_security(ctrl->pc, ESP_LOCAL_CONTROLLER_EP_SESSION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "establish_security failed: %s", esp_err_to_name(err));
        return err;
    }

    ctrl->session_started = true;
    return ESP_OK;
}

esp_err_t esp_local_controller_start_session(esp_local_controller_t *ctrl,
                                             const void *transport_connect_cfg)
{
    if (!ctrl || !ctrl->pc) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Prerequisite: protocomm_ext_set_security() already configured when the
     * peer scheme is known. Same order as the recommended split API:
     * fetch_version (open + plaintext) → establish_security.
     */
    esp_err_t err = esp_local_controller_fetch_version(ctrl, transport_connect_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_local_controller_establish_security(ctrl);
    if (err != ESP_OK) {
        version_invalidate(ctrl);
        ctrl->session_started = false;
        protocomm_ext_close_session(ctrl->pc);
        return err;
    }

    return ESP_OK;
}

esp_err_t esp_local_controller_stop_session(esp_local_controller_t *ctrl)
{
    if (!ctrl || !ctrl->pc) {
        return ESP_ERR_INVALID_ARG;
    }
    ctrl->session_started = false;
    return protocomm_ext_close_session(ctrl->pc);
}

esp_err_t esp_local_controller_get_version(esp_local_controller_t *ctrl,
                                           esp_local_controller_version_t *out)
{
    if (!ctrl || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ctrl->version_valid) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(out, 0, sizeof(*out));
    out->sec_ver = ctrl->version.sec_ver;
    out->sec_patch_ver = ctrl->version.sec_patch_ver;
    if (ctrl->version.ver) {
        out->ver = strdup(ctrl->version.ver);
        if (!out->ver) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

void esp_local_controller_version_free(esp_local_controller_version_t *ver)
{
    version_clear(ver);
}

void esp_local_controller_props_free(esp_local_controller_prop_t *props, size_t count)
{
    if (!props) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(props[i].name);
        free(props[i].value);
    }
    free(props);
}

static esp_err_t pack_and_send(esp_local_controller_t *ctrl, LocalCtrlMessage *msg,
                               uint8_t **resp_raw, size_t *resp_len)
{
    size_t packed_len = local_ctrl_message__get_packed_size(msg);
    if (packed_len == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *packed = malloc(packed_len);
    if (!packed) {
        return ESP_ERR_NO_MEM;
    }
    local_ctrl_message__pack(msg, packed);

    esp_err_t err = esp_local_controller_send_ep(ctrl, ESP_LOCAL_CONTROLLER_EP_CONTROL,
                                                 packed, packed_len, resp_raw, resp_len);
    free(packed);
    return err;
}

esp_err_t esp_local_controller_get_property_count(esp_local_controller_t *ctrl, uint32_t *count)
{
    if (!ctrl || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    LocalCtrlMessage msg = LOCAL_CTRL_MESSAGE__INIT;
    CmdGetPropertyCount cmd = CMD_GET_PROPERTY_COUNT__INIT;
    msg.msg = LOCAL_CTRL_MSG_TYPE__TypeCmdGetPropertyCount;
    msg.payload_case = LOCAL_CTRL_MESSAGE__PAYLOAD_CMD_GET_PROP_COUNT;
    msg.cmd_get_prop_count = &cmd;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(ctrl, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    LocalCtrlMessage *resp = local_ctrl_message__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != LOCAL_CTRL_MSG_TYPE__TypeRespGetPropertyCount ||
            !resp->resp_get_prop_count) {
        local_ctrl_message__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_status(resp->resp_get_prop_count->status, "GetPropertyCount");
    if (err == ESP_OK) {
        uint32_t n = resp->resp_get_prop_count->count;
        if (n > ESP_LOCAL_CONTROLLER_MAX_PROPERTIES) {
            ESP_LOGE(TAG, "property count %u exceeds max %u",
                     (unsigned)n, (unsigned)ESP_LOCAL_CONTROLLER_MAX_PROPERTIES);
            err = ESP_ERR_INVALID_SIZE;
        } else {
            *count = n;
        }
    }
    local_ctrl_message__free_unpacked(resp, NULL);
    return err;
}

esp_err_t esp_local_controller_get_property_values(esp_local_controller_t *ctrl,
                                                   const uint32_t *indices,
                                                   size_t index_count,
                                                   esp_local_controller_prop_t **out_props,
                                                   size_t *out_count)
{
    if (!ctrl || !indices || index_count == 0 || !out_props || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (index_count > ESP_LOCAL_CONTROLLER_MAX_PROPERTIES) {
        return ESP_ERR_INVALID_SIZE;
    }
    *out_props = NULL;
    *out_count = 0;

    LocalCtrlMessage msg = LOCAL_CTRL_MESSAGE__INIT;
    CmdGetPropertyValues cmd = CMD_GET_PROPERTY_VALUES__INIT;
    msg.msg = LOCAL_CTRL_MSG_TYPE__TypeCmdGetPropertyValues;
    msg.payload_case = LOCAL_CTRL_MESSAGE__PAYLOAD_CMD_GET_PROP_VALS;
    msg.cmd_get_prop_vals = &cmd;
    cmd.n_indices = index_count;
    cmd.indices = (uint32_t *)indices;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(ctrl, &msg, &resp_raw, &resp_len);
    if (err != ESP_OK) {
        return err;
    }

    LocalCtrlMessage *resp = local_ctrl_message__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != LOCAL_CTRL_MSG_TYPE__TypeRespGetPropertyValues ||
            !resp->resp_get_prop_vals) {
        local_ctrl_message__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_status(resp->resp_get_prop_vals->status, "GetPropertyValues");
    if (err != ESP_OK) {
        local_ctrl_message__free_unpacked(resp, NULL);
        return err;
    }

    size_t n = resp->resp_get_prop_vals->n_props;
    if (n > ESP_LOCAL_CONTROLLER_MAX_PROPERTIES) {
        ESP_LOGE(TAG, "n_props %zu exceeds max %u",
                 n, (unsigned)ESP_LOCAL_CONTROLLER_MAX_PROPERTIES);
        local_ctrl_message__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_SIZE;
    }
    /* calloc(0) may return NULL; treat an empty property list as success. */
    if (n == 0) {
        local_ctrl_message__free_unpacked(resp, NULL);
        return ESP_OK;
    }
    esp_local_controller_prop_t *props = calloc(n, sizeof(*props));
    if (!props) {
        local_ctrl_message__free_unpacked(resp, NULL);
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < n; i++) {
        PropertyInfo *pi = resp->resp_get_prop_vals->props[i];
        if (!pi) {
            esp_local_controller_props_free(props, n);
            local_ctrl_message__free_unpacked(resp, NULL);
            return ESP_ERR_INVALID_STATE;
        }
        if (pi->status != STATUS__Success) {
            ESP_LOGE(TAG, "property[%zu] status=%d", i, (int)pi->status);
            esp_local_controller_props_free(props, n);
            local_ctrl_message__free_unpacked(resp, NULL);
            return ESP_FAIL;
        }
        props[i].type = pi->type;
        props[i].flags = pi->flags;
        if (pi->name) {
            props[i].name = strdup(pi->name);
            if (!props[i].name) {
                esp_local_controller_props_free(props, n);
                local_ctrl_message__free_unpacked(resp, NULL);
                return ESP_ERR_NO_MEM;
            }
        }
        if (pi->value.len && pi->value.data) {
            props[i].value = malloc(pi->value.len);
            if (!props[i].value) {
                esp_local_controller_props_free(props, n);
                local_ctrl_message__free_unpacked(resp, NULL);
                return ESP_ERR_NO_MEM;
            }
            memcpy(props[i].value, pi->value.data, pi->value.len);
            props[i].value_len = pi->value.len;
        }
    }

    local_ctrl_message__free_unpacked(resp, NULL);
    *out_props = props;
    *out_count = n;
    return ESP_OK;
}

esp_err_t esp_local_controller_set_property_values(esp_local_controller_t *ctrl,
                                                   const esp_local_controller_prop_set_t *props,
                                                   size_t prop_count)
{
    if (!ctrl || !props || prop_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (prop_count > ESP_LOCAL_CONTROLLER_MAX_PROPERTIES) {
        return ESP_ERR_INVALID_SIZE;
    }

    PropertyValue **entries = calloc(prop_count, sizeof(*entries));
    PropertyValue *storage = calloc(prop_count, sizeof(*storage));
    if (!entries || !storage) {
        free(entries);
        free(storage);
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < prop_count; i++) {
        property_value__init(&storage[i]);
        storage[i].index = props[i].index;
        if (props[i].value_len && !props[i].value) {
            free(entries);
            free(storage);
            return ESP_ERR_INVALID_ARG;
        }
        storage[i].value.data = (uint8_t *)props[i].value;
        storage[i].value.len = props[i].value_len;
        entries[i] = &storage[i];
    }

    LocalCtrlMessage msg = LOCAL_CTRL_MESSAGE__INIT;
    CmdSetPropertyValues cmd = CMD_SET_PROPERTY_VALUES__INIT;
    msg.msg = LOCAL_CTRL_MSG_TYPE__TypeCmdSetPropertyValues;
    msg.payload_case = LOCAL_CTRL_MESSAGE__PAYLOAD_CMD_SET_PROP_VALS;
    msg.cmd_set_prop_vals = &cmd;
    cmd.n_props = prop_count;
    cmd.props = entries;

    uint8_t *resp_raw = NULL;
    size_t resp_len = 0;
    esp_err_t err = pack_and_send(ctrl, &msg, &resp_raw, &resp_len);
    free(entries);
    free(storage);
    if (err != ESP_OK) {
        return err;
    }

    LocalCtrlMessage *resp = local_ctrl_message__unpack(NULL, resp_len, resp_raw);
    free(resp_raw);
    if (!resp || resp->msg != LOCAL_CTRL_MSG_TYPE__TypeRespSetPropertyValues ||
            !resp->resp_set_prop_vals) {
        local_ctrl_message__free_unpacked(resp, NULL);
        return ESP_ERR_INVALID_STATE;
    }
    err = check_status(resp->resp_set_prop_vals->status, "SetPropertyValues");
    local_ctrl_message__free_unpacked(resp, NULL);
    return err;
}
