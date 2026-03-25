/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <sdkconfig.h>
#include <esp_log.h>
#include <esp_local_ctrl.h>
#include <protocomm_security.h>
#include <esp_random.h>
#include <nvs.h>
#include <mdns.h>

#include "esp_weaver.h"
#include "esp_weaver_priv.h"

static const char *TAG = "esp_weaver_local_ctrl";

/* PoP storage for SEC1 */
static char *s_local_ctrl_pop = NULL;

/* SEC1 security params (must persist while local ctrl is running) */
static protocomm_security1_params_t *s_sec1_params = NULL;

/* NVS namespace and key for PoP persistence */
#define ESP_WEAVER_NVS_LOCAL_CTRL_NAMESPACE "local_ctrl"
#define ESP_WEAVER_NVS_LOCAL_CTRL_POP       "pop"

/* PoP length: 8 hex chars + null terminator (matches RainMaker) */
#define ESP_WEAVER_POP_LEN  9

/* Property names */
#define PROP_NAME_CONFIG    "config"
#define PROP_NAME_PARAMS    "params"

/* Custom property types */
enum property_types {
    PROP_TYPE_NODE_CONFIG = 1,
    PROP_TYPE_NODE_PARAMS,
};

/* Custom property flags */
enum property_flags {
    PROP_FLAG_READONLY = (1 << 0)
};

/* Minimum JSON length for non-empty object (length of "{}") */
#define EMPTY_JSON_OBJECT_LEN    2

static esp_err_t get_property_values(size_t props_count,
                                     const esp_local_ctrl_prop_t props[],
                                     esp_local_ctrl_prop_val_t prop_values[],
                                     void *usr_ctx)
{
    esp_err_t ret = ESP_OK;
    size_t i;
    for (i = 0; i < props_count && ret == ESP_OK; i++) {
        ESP_LOGD(TAG, "(%d) Reading property: %s", (int)i, props[i].name);
        switch (props[i].type) {
        case PROP_TYPE_NODE_CONFIG: {
            char *node_config = esp_weaver_get_node_config();
            if (!node_config) {
                ESP_LOGE(TAG, "Failed to allocate memory for %s", props[i].name);
                prop_values[i].size = 0;
                prop_values[i].data = NULL;
                prop_values[i].free_fn = NULL;
                ret = ESP_ERR_NO_MEM;
            } else {
                ESP_LOGD(TAG, "Node config: %s", node_config);
                prop_values[i].size = strlen(node_config);
                prop_values[i].data = node_config;
                prop_values[i].free_fn = free;
            }
            break;
        }
        case PROP_TYPE_NODE_PARAMS: {
            char *node_params = esp_weaver_get_node_params();
            if (!node_params) {
                ESP_LOGE(TAG, "Failed to allocate memory for %s", props[i].name);
                prop_values[i].size = 0;
                prop_values[i].data = NULL;
                prop_values[i].free_fn = NULL;
                ret = ESP_ERR_NO_MEM;
            } else {
                ESP_LOGD(TAG, "Params requested, size: %d", (int)strlen(node_params));
                prop_values[i].size = strlen(node_params);
                prop_values[i].data = node_params;
                prop_values[i].free_fn = free;
            }
            break;
        }
        default:
            prop_values[i].size = 0;
            prop_values[i].data = NULL;
            prop_values[i].free_fn = NULL;
            break;
        }
    }
    if (ret != ESP_OK) {
        for (size_t j = 0; j < i; j++) {
            if (prop_values[j].free_fn) {
                prop_values[j].free_fn(prop_values[j].data);
                prop_values[j].free_fn = NULL;
                prop_values[j].data = NULL;
                prop_values[j].size = 0;
            }
        }
    }
    return ret;
}

static esp_err_t set_property_values(size_t props_count,
                                     const esp_local_ctrl_prop_t props[],
                                     const esp_local_ctrl_prop_val_t prop_values[],
                                     void *usr_ctx)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGD(TAG, "set_property_values called, props_count: %d", (int)props_count);

    for (size_t i = 0; i < props_count; i++) {
        if (props[i].flags & PROP_FLAG_READONLY) {
            ESP_LOGE(TAG, "%s is read-only", props[i].name);
            return ESP_ERR_INVALID_ARG;
        }
    }

    for (size_t i = 0; i < props_count && ret == ESP_OK; i++) {
        switch (props[i].type) {
        case PROP_TYPE_NODE_PARAMS:
            if (prop_values[i].data && prop_values[i].size > 0) {
                ret = esp_weaver_handle_set_params((const char *)prop_values[i].data, prop_values[i].size,
                                                   ESP_WEAVER_REQ_SRC_LOCAL);
            }
            break;
        default:
            break;
        }
    }
    return ret;
}

static char *esp_weaver_local_ctrl_get_nvs(const char *key)
{
    char *val = NULL;
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(CONFIG_ESP_WEAVER_NVS_PARTITION_NAME,
                                            ESP_WEAVER_NVS_LOCAL_CTRL_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return NULL;
    }
    size_t len = 0;
    if ((err = nvs_get_blob(handle, key, NULL, &len)) == ESP_OK) {
        val = calloc(1, len + 1);
        if (val) {
            if (nvs_get_blob(handle, key, val, &len) != ESP_OK) {
                free(val);
                val = NULL;
            }
        }
    }
    nvs_close(handle);
    return val;
}

static esp_err_t esp_weaver_local_ctrl_set_nvs(const char *key, const char *val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(CONFIG_ESP_WEAVER_NVS_PARTITION_NAME,
                                            ESP_WEAVER_NVS_LOCAL_CTRL_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, key, val, strlen(val));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t esp_weaver_local_ctrl_set_pop(const char *pop)
{
    if (s_local_ctrl_pop) {
        free(s_local_ctrl_pop);
        s_local_ctrl_pop = NULL;
    }

    if (!pop) {
        ESP_LOGI(TAG, "Custom PoP cleared. Will use NVS or generate new one.");
        return ESP_OK;
    }

    if (strlen(pop) == 0) {
        ESP_LOGE(TAG, "Empty PoP string not allowed. Use NULL to clear.");
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(pop) > ESP_WEAVER_POP_LEN - 1) {
        ESP_LOGE(TAG, "PoP string too long (max %d chars)", ESP_WEAVER_POP_LEN - 1);
        return ESP_ERR_INVALID_ARG;
    }

    s_local_ctrl_pop = strdup(pop);
    if (!s_local_ctrl_pop) {
        ESP_LOGE(TAG, "Failed to allocate memory for PoP");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Custom PoP set for local control");
    return ESP_OK;
}

const char *esp_weaver_local_ctrl_get_pop(void)
{
    if (s_local_ctrl_pop) {
        return s_local_ctrl_pop;
    }

    char *pop = esp_weaver_local_ctrl_get_nvs(ESP_WEAVER_NVS_LOCAL_CTRL_POP);
    if (pop) {
        s_local_ctrl_pop = pop;
        ESP_LOGI(TAG, "PoP read from NVS");
        return s_local_ctrl_pop;
    }

    ESP_LOGI(TAG, "No PoP in NVS. Generating a new one.");
    s_local_ctrl_pop = calloc(1, ESP_WEAVER_POP_LEN);
    if (!s_local_ctrl_pop) {
        ESP_LOGE(TAG, "Failed to allocate memory for PoP");
        return NULL;
    }

    uint8_t random_bytes[4] = {0};
    esp_fill_random(random_bytes, sizeof(random_bytes));
    snprintf(s_local_ctrl_pop, ESP_WEAVER_POP_LEN, "%02x%02x%02x%02x",
             random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3]);

    esp_err_t err = esp_weaver_local_ctrl_set_nvs(ESP_WEAVER_NVS_LOCAL_CTRL_POP, s_local_ctrl_pop);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to store PoP to NVS: %s. PoP will change on reboot.", esp_err_to_name(err));
    }
    return s_local_ctrl_pop;
}

esp_err_t esp_weaver_local_ctrl_start(void)
{
    esp_weaver_state_t state = esp_weaver_get_state();

    if (state == ESP_WEAVER_STATE_DEINIT) {
        ESP_LOGE(TAG, "Weaver not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (state >= ESP_WEAVER_STATE_LOCAL_CTRL_STARTING) {
        ESP_LOGW(TAG, "Local control already started or starting");
        return ESP_ERR_INVALID_STATE;
    }

    int sec_ver = CONFIG_ESP_WEAVER_LOCAL_CTRL_SECURITY_VERSION;

    esp_weaver_set_state(ESP_WEAVER_STATE_LOCAL_CTRL_STARTING);

    const char *node_id = esp_weaver_get_node_id();
    ESP_LOGI(TAG, "Starting local control with HTTP transport and security version: %d", sec_ver);

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s (0x%x)", esp_err_to_name(err), err);
        esp_weaver_set_state(ESP_WEAVER_STATE_INIT_DONE);
        return err;
    }
    err = mdns_hostname_set(node_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set mDNS hostname '%s': %s", node_id, esp_err_to_name(err));
    }

    httpd_config_t http_conf = HTTPD_DEFAULT_CONFIG();
    http_conf.server_port = CONFIG_ESP_WEAVER_LOCAL_CTRL_HTTP_PORT;
    http_conf.ctrl_port = CONFIG_ESP_WEAVER_LOCAL_CTRL_HTTP_CTRL_PORT;
    http_conf.stack_size = CONFIG_ESP_WEAVER_LOCAL_CTRL_STACK_SIZE;

    esp_local_ctrl_config_t config = {
        .transport = ESP_LOCAL_CTRL_TRANSPORT_HTTPD,
        .transport_config = {
            .httpd = &http_conf,
        },
        .proto_sec = {
            .version = PROTOCOM_SEC0,
            .custom_handle = NULL,
            .sec_params = NULL,
        },
        .handlers = {
            .get_prop_values = get_property_values,
            .set_prop_values = set_property_values,
            .usr_ctx = NULL,
            .usr_ctx_free_fn = NULL,
        },
        .max_properties = CONFIG_ESP_WEAVER_LOCAL_CTRL_MAX_PROPERTIES,
    };

    free(s_sec1_params);
    s_sec1_params = NULL;

    if (sec_ver == 1) {
        const char *pop_str = esp_weaver_local_ctrl_get_pop();
        if (pop_str) {
            ESP_LOGI(TAG, "PoP for local control: %s", pop_str);
            s_sec1_params = calloc(1, sizeof(protocomm_security1_params_t));
            if (!s_sec1_params) {
                ESP_LOGE(TAG, "Failed to allocate security params");
                mdns_free();
                esp_weaver_set_state(ESP_WEAVER_STATE_INIT_DONE);
                return ESP_ERR_NO_MEM;
            }
            s_sec1_params->data = (const uint8_t *)pop_str;
            s_sec1_params->len = strlen(pop_str);
        } else {
            ESP_LOGE(TAG, "SEC1 requested but no PoP available. Cannot start local control with authentication.");
            ESP_LOGE(TAG, "Please ensure NVS is initialized and accessible.");
            mdns_free();
            esp_weaver_set_state(ESP_WEAVER_STATE_INIT_DONE);
            return ESP_ERR_NOT_FOUND;
        }
        config.proto_sec.version = PROTOCOM_SEC1;
        config.proto_sec.sec_params = s_sec1_params;
    }

    err = esp_local_ctrl_start(&config);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start local control: %s", esp_err_to_name(err));
        free(s_sec1_params);
        s_sec1_params = NULL;
        mdns_free();
        esp_weaver_set_state(ESP_WEAVER_STATE_INIT_DONE);
        return err;
    }

    mdns_service_instance_name_set("_esp_local_ctrl", "_tcp", node_id);
    mdns_service_txt_item_set("_esp_local_ctrl", "_tcp", "node_id", node_id);

    esp_local_ctrl_prop_t node_config = {
        .name = PROP_NAME_CONFIG,
        .type = PROP_TYPE_NODE_CONFIG,
        .size = 0,
        .flags = PROP_FLAG_READONLY,
        .ctx = NULL,
        .ctx_free_fn = NULL,
    };

    esp_local_ctrl_prop_t node_params = {
        .name = PROP_NAME_PARAMS,
        .type = PROP_TYPE_NODE_PARAMS,
        .size = 0,
        .flags = 0,
        .ctx = NULL,
        .ctx_free_fn = NULL,
    };

    err = esp_local_ctrl_add_property(&node_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add config property: %s", esp_err_to_name(err));
        esp_local_ctrl_stop();
        free(s_sec1_params);
        s_sec1_params = NULL;
        mdns_free();
        esp_weaver_set_state(ESP_WEAVER_STATE_INIT_DONE);
        return err;
    }
    err = esp_local_ctrl_add_property(&node_params);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add params property: %s", esp_err_to_name(err));
        esp_local_ctrl_stop();
        free(s_sec1_params);
        s_sec1_params = NULL;
        mdns_free();
        esp_weaver_set_state(ESP_WEAVER_STATE_INIT_DONE);
        return err;
    }

    esp_weaver_set_state(ESP_WEAVER_STATE_LOCAL_CTRL_STARTED);
    ESP_LOGI(TAG, "Local control started on port %d, node_id: %s",
             CONFIG_ESP_WEAVER_LOCAL_CTRL_HTTP_PORT, node_id);

    return ESP_OK;
}

esp_err_t esp_weaver_local_ctrl_set_txt(const char *key, const char *value)
{
    if (esp_weaver_get_state() != ESP_WEAVER_STATE_LOCAL_CTRL_STARTED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    return mdns_service_txt_item_set("_esp_local_ctrl", "_tcp", key, value);
}

esp_err_t esp_weaver_local_ctrl_stop(void)
{
    if (esp_weaver_get_state() < ESP_WEAVER_STATE_LOCAL_CTRL_STARTING) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_local_ctrl_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop local control: %s", esp_err_to_name(err));
    }

    free(s_sec1_params);
    s_sec1_params = NULL;
    /* s_local_ctrl_pop is intentionally NOT freed here.
     * It is released in esp_weaver_node_deinit() via esp_weaver_local_ctrl_set_pop(NULL). */
    mdns_free();
    esp_weaver_set_state(ESP_WEAVER_STATE_INIT_DONE);
    ESP_LOGI(TAG, "Local control stopped");
    return err;
}

esp_err_t esp_weaver_local_ctrl_send_params(void)
{
    if (esp_weaver_get_state() != ESP_WEAVER_STATE_LOCAL_CTRL_STARTED) {
        return ESP_ERR_INVALID_STATE;
    }

    char *params_json = esp_weaver_get_changed_node_params();
    if (!params_json) {
        ESP_LOGE(TAG, "Failed to generate changed params JSON");
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    size_t json_len = strlen(params_json);

    /* Skip sending if no parameters changed (JSON is just "{}") */
    if (json_len > EMPTY_JSON_OBJECT_LEN) {
        ESP_LOGD(TAG, "Reporting params: %s", params_json);
        err = esp_local_ctrl_send_data((uint8_t *)params_json, json_len);
    }

    free(params_json);
    return err;
}
