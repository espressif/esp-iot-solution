/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_app_desc.h>
#include <json_generator.h>
#include <json_parser.h>
#include <nvs.h>
#include "esp_weaver.h"
#include "esp_weaver_standard_types.h"
#include "esp_weaver_priv.h"

static const char *TAG = "esp_weaver";

/* Parameter flags for tracking state changes */
#define ESP_WEAVER_PARAM_FLAG_VALUE_CHANGE  (1 << 0)
#define ESP_WEAVER_PARAMS_SIZE_MARGIN       50  /* To accommodate for changes in param values while creating JSON */
#define ESP_WEAVER_MAX_PARAM_VALUE_LEN      4096

static esp_err_t esp_weaver_param_store_value(esp_weaver_param_t *param);
static esp_err_t esp_weaver_param_read_value(esp_weaver_param_t *param, esp_weaver_param_val_t *val);

static inline void safe_free(void *ptr)
{
    if (ptr) {
        free(ptr);
    }
}

/* Internal parameter structure */
struct esp_weaver_param {
    char *name;
    char *type;
    char *ui_type;
    esp_weaver_param_val_t val;
    uint8_t props;
    uint8_t flags;
    bool has_bounds;
    esp_weaver_param_val_t min;
    esp_weaver_param_val_t max;
    esp_weaver_param_val_t step;
    esp_weaver_device_t *device;
    STAILQ_ENTRY(esp_weaver_param) next;
};

/* Internal device structure */
struct esp_weaver_device {
    char *name;
    char *type;
    void *priv_data;
    esp_weaver_device_bulk_write_cb_t bulk_write_cb;
    esp_weaver_param_t *primary_param;
    uint8_t param_count;
    STAILQ_HEAD(param_list, esp_weaver_param) params;
    STAILQ_ENTRY(esp_weaver_device) next;
};

/* Internal node structure */
struct esp_weaver_node {
    char *name;
    char *type;
    char *node_id;
    STAILQ_HEAD(device_list, esp_weaver_device) devices;
    esp_weaver_state_t state;
};

static esp_weaver_node_t *s_node = NULL;

esp_weaver_param_val_t esp_weaver_bool(bool bval)
{
    esp_weaver_param_val_t v = {.type = WEAVER_VAL_TYPE_BOOLEAN, .val = {.b = bval}};
    return v;
}

esp_weaver_param_val_t esp_weaver_int(int ival)
{
    esp_weaver_param_val_t v = {.type = WEAVER_VAL_TYPE_INTEGER, .val = {.i = ival}};
    return v;
}

esp_weaver_param_val_t esp_weaver_float(float fval)
{
    esp_weaver_param_val_t v = {.type = WEAVER_VAL_TYPE_FLOAT, .val = {.f = fval}};
    return v;
}

esp_weaver_param_val_t esp_weaver_str(const char *sval)
{
    esp_weaver_param_val_t v = {.type = WEAVER_VAL_TYPE_STRING, .val = {.s = (char *)sval}};
    return v;
}

esp_weaver_param_val_t esp_weaver_obj(const char *val)
{
    esp_weaver_param_val_t v = {.type = WEAVER_VAL_TYPE_OBJECT, .val = {.s = (char *)val}};
    return v;
}

esp_weaver_param_val_t esp_weaver_array(const char *val)
{
    esp_weaver_param_val_t v = {.type = WEAVER_VAL_TYPE_ARRAY, .val = {.s = (char *)val}};
    return v;
}

static void esp_weaver_val_free(esp_weaver_param_val_t *val)
{
    if (val && (val->type == WEAVER_VAL_TYPE_STRING || val->type == WEAVER_VAL_TYPE_OBJECT
                || val->type == WEAVER_VAL_TYPE_ARRAY) && val->val.s) {
        free(val->val.s);
        val->val.s = NULL;
        val->type = WEAVER_VAL_TYPE_INVALID;
    }
}

static esp_err_t esp_weaver_val_copy(esp_weaver_param_val_t *dst, const esp_weaver_param_val_t *src)
{
    if (!dst || !src) {
        return ESP_ERR_INVALID_ARG;
    }
    if (dst == src) {
        return ESP_OK;
    }

    if (src->type == WEAVER_VAL_TYPE_STRING || src->type == WEAVER_VAL_TYPE_OBJECT
            || src->type == WEAVER_VAL_TYPE_ARRAY) {
        char *new_s = NULL;
        if (src->val.s) {
            new_s = strdup(src->val.s);
            if (!new_s) {
                return ESP_ERR_NO_MEM;
            }
        }
        esp_weaver_val_free(dst);
        dst->type = src->type;
        dst->val.s = new_s;
    } else if (src->type == WEAVER_VAL_TYPE_BOOLEAN || src->type == WEAVER_VAL_TYPE_INTEGER
               || src->type == WEAVER_VAL_TYPE_FLOAT) {
        esp_weaver_val_free(dst);
        *dst = *src;
    } else {
        esp_weaver_val_free(dst);
        dst->type = src->type;
        dst->val.s = NULL;
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t esp_weaver_report_value(const esp_weaver_param_val_t *val, const char *key, json_gen_str_t *jptr)
{
    if (!key || !jptr) {
        return ESP_FAIL;
    }
    if (!val) {
        json_gen_obj_set_null(jptr, key);
        return ESP_OK;
    }
    switch (val->type) {
    case WEAVER_VAL_TYPE_BOOLEAN:
        json_gen_obj_set_bool(jptr, key, val->val.b);
        break;
    case WEAVER_VAL_TYPE_INTEGER:
        json_gen_obj_set_int(jptr, key, val->val.i);
        break;
    case WEAVER_VAL_TYPE_FLOAT:
        json_gen_obj_set_float(jptr, key, val->val.f);
        break;
    case WEAVER_VAL_TYPE_STRING:
        json_gen_obj_set_string(jptr, key, val->val.s ? val->val.s : "");
        break;
    case WEAVER_VAL_TYPE_OBJECT:
        json_gen_push_object_str(jptr, key, val->val.s ? val->val.s : "{}");
        break;
    case WEAVER_VAL_TYPE_ARRAY:
        json_gen_push_array_str(jptr, key, val->val.s ? val->val.s : "[]");
        break;
    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t esp_weaver_report_data_type(esp_weaver_val_type_t type, const char *data_type_key, json_gen_str_t *jptr)
{
    switch (type) {
    case WEAVER_VAL_TYPE_BOOLEAN:
        json_gen_obj_set_string(jptr, data_type_key, "bool");
        break;
    case WEAVER_VAL_TYPE_INTEGER:
        json_gen_obj_set_string(jptr, data_type_key, "int");
        break;
    case WEAVER_VAL_TYPE_FLOAT:
        json_gen_obj_set_string(jptr, data_type_key, "float");
        break;
    case WEAVER_VAL_TYPE_STRING:
        json_gen_obj_set_string(jptr, data_type_key, "string");
        break;
    case WEAVER_VAL_TYPE_OBJECT:
        json_gen_obj_set_string(jptr, data_type_key, "object");
        break;
    case WEAVER_VAL_TYPE_ARRAY:
        json_gen_obj_set_string(jptr, data_type_key, "array");
        break;
    default:
        json_gen_obj_set_string(jptr, data_type_key, "invalid");
        break;
    }
    return ESP_OK;
}

static esp_err_t esp_weaver_report_param_config(esp_weaver_param_t *param, json_gen_str_t *jptr)
{
    json_gen_start_object(jptr);
    if (param->name) {
        json_gen_obj_set_string(jptr, "name", param->name);
    }
    if (param->type) {
        json_gen_obj_set_string(jptr, "type", param->type);
    }
    esp_weaver_report_data_type(param->val.type, "data_type", jptr);
    json_gen_push_array(jptr, "properties");
    if (param->props & PROP_FLAG_READ) {
        json_gen_arr_set_string(jptr, "read");
    }
    if (param->props & PROP_FLAG_WRITE) {
        json_gen_arr_set_string(jptr, "write");
    }
    json_gen_pop_array(jptr);
    if (param->has_bounds) {
        json_gen_push_object(jptr, "bounds");
        esp_weaver_report_value(&param->min, "min", jptr);
        esp_weaver_report_value(&param->max, "max", jptr);
        bool report_step = (param->step.type == WEAVER_VAL_TYPE_INTEGER && param->step.val.i != 0)
                           || (param->step.type == WEAVER_VAL_TYPE_FLOAT && param->step.val.f != 0);
        if (report_step) {
            esp_weaver_report_value(&param->step, "step", jptr);
        }
        json_gen_pop_object(jptr);
    }
    if (param->ui_type) {
        json_gen_obj_set_string(jptr, "ui_type", param->ui_type);
    }
    json_gen_end_object(jptr);
    return ESP_OK;
}

static esp_err_t esp_weaver_report_info(json_gen_str_t *jptr)
{
    if (!s_node) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_app_desc_t *app_desc = esp_app_get_description();

    json_gen_obj_set_string(jptr, "node_id", s_node->node_id);
    json_gen_obj_set_string(jptr, "config_version", ESP_WEAVER_CONFIG_VERSION);
    json_gen_push_object(jptr, "info");
    json_gen_obj_set_string(jptr, "name", s_node->name);
    json_gen_obj_set_string(jptr, "fw_version", app_desc->version);
    json_gen_obj_set_string(jptr, "type", s_node->type);
    json_gen_obj_set_string(jptr, "model", app_desc->project_name);
    json_gen_obj_set_string(jptr, "project_name", app_desc->project_name);
    json_gen_obj_set_string(jptr, "platform", CONFIG_IDF_TARGET);
    json_gen_pop_object(jptr);
    return ESP_OK;
}

static esp_err_t esp_weaver_report_devices(json_gen_str_t *jptr)
{
    if (!s_node) {
        return ESP_ERR_INVALID_STATE;
    }
    if (STAILQ_EMPTY(&s_node->devices)) {
        return ESP_OK;
    }
    json_gen_push_array(jptr, "devices");
    esp_weaver_device_t *device;
    STAILQ_FOREACH(device, &s_node->devices, next) {
        json_gen_start_object(jptr);
        json_gen_obj_set_string(jptr, "name", device->name);
        if (device->type) {
            json_gen_obj_set_string(jptr, "type", device->type);
        }
        /* Use explicit primary param if set, otherwise fall back to first param */
        esp_weaver_param_t *primary = device->primary_param;
        if (!primary) {
            primary = STAILQ_FIRST(&device->params);
        }
        if (primary) {
            json_gen_obj_set_string(jptr, "primary", primary->name);
        }
        if (!STAILQ_EMPTY(&device->params)) {
            json_gen_push_array(jptr, "params");
            esp_weaver_param_t *param;
            STAILQ_FOREACH(param, &device->params, next) {
                esp_weaver_report_param_config(param, jptr);
            }
            json_gen_pop_array(jptr);
        }
        json_gen_end_object(jptr);
    }
    json_gen_pop_array(jptr);
    return ESP_OK;
}

static int esp_weaver_populate_node_config(char *buf, size_t buf_size)
{
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, buf_size, NULL, NULL);
    json_gen_start_object(&jstr);
    if (esp_weaver_report_info(&jstr) != ESP_OK) {
        return -1;
    }
    if (esp_weaver_report_devices(&jstr) != ESP_OK) {
        return -1;
    }
    if (json_gen_end_object(&jstr) < 0) {
        return -1;
    }
    return json_gen_str_end(&jstr);
}

char *esp_weaver_get_node_config(void)
{
    int req_size = esp_weaver_populate_node_config(NULL, 0);
    if (req_size < 0) {
        ESP_LOGE(TAG, "Failed to get required size for node config JSON");
        return NULL;
    }
    char *node_config = calloc(1, req_size);
    if (!node_config) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for node config", req_size);
        return NULL;
    }
    if (esp_weaver_populate_node_config(node_config, req_size) < 0) {
        free(node_config);
        ESP_LOGE(TAG, "Failed to generate node config JSON");
        return NULL;
    }
    return node_config;
}

static int esp_weaver_get_params_json(char *buf, size_t buf_size, uint8_t flags, bool reset_flags)
{
    if (!s_node) {
        return -1;
    }
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, buf_size, NULL, NULL);
    json_gen_start_object(&jstr);

    esp_weaver_device_t *dev;
    esp_weaver_param_t *param;
    STAILQ_FOREACH(dev, &s_node->devices, next) {
        bool device_added = false;
        STAILQ_FOREACH(param, &dev->params, next) {
            if (!flags || (param->flags & flags)) {
                if (!device_added) {
                    json_gen_push_object(&jstr, dev->name);
                    device_added = true;
                }
                esp_weaver_report_value(&param->val, param->name, &jstr);
            }
        }
        if (device_added) {
            json_gen_pop_object(&jstr);
        }
    }

    if (json_gen_end_object(&jstr) < 0) {
        return -1;
    }

    if (reset_flags && buf != NULL) {
        STAILQ_FOREACH(dev, &s_node->devices, next) {
            STAILQ_FOREACH(param, &dev->params, next) {
                param->flags &= ~flags;
            }
        }
    }

    return json_gen_str_end(&jstr);
}

char *esp_weaver_get_node_params(void)
{
    int req_size = esp_weaver_get_params_json(NULL, 0, 0, false);
    if (req_size < 0) {
        return NULL;
    }
    req_size += ESP_WEAVER_PARAMS_SIZE_MARGIN;
    char *params_json = calloc(1, req_size);
    if (!params_json) {
        return NULL;
    }
    if (esp_weaver_get_params_json(params_json, req_size, 0, false) < 0) {
        free(params_json);
        return NULL;
    }
    return params_json;
}

char *esp_weaver_get_changed_node_params(void)
{
    int req_size = esp_weaver_get_params_json(NULL, 0, ESP_WEAVER_PARAM_FLAG_VALUE_CHANGE, false);
    if (req_size < 0) {
        return NULL;
    }
    req_size += ESP_WEAVER_PARAMS_SIZE_MARGIN;
    char *params_json = calloc(1, req_size);
    if (!params_json) {
        return NULL;
    }
    if (esp_weaver_get_params_json(params_json, req_size, ESP_WEAVER_PARAM_FLAG_VALUE_CHANGE, true) < 0) {
        free(params_json);
        return NULL;
    }
    return params_json;
}

/* Process params for a single device */
static esp_err_t esp_weaver_device_set_params(const esp_weaver_device_t *device, jparse_ctx_t *jptr, esp_weaver_write_ctx_t *ctx)
{
    if (device->param_count == 0) {
        ESP_LOGD(TAG, "Device %s has no parameters, skipping", device->name);
        return ESP_OK;
    }

    esp_weaver_param_write_req_t *write_req = calloc(device->param_count, sizeof(esp_weaver_param_write_req_t));
    if (!write_req) {
        ESP_LOGE(TAG, "Could not allocate memory for set params");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = ESP_OK;
    esp_err_t cb_err = ESP_OK;

    uint8_t num_param = 0;
    esp_weaver_param_t *param;
    STAILQ_FOREACH(param, &device->params, next) {
        if (param->props & PROP_FLAG_WRITE) {
            bool found = false;
            write_req[num_param].val.type = param->val.type;

            switch (param->val.type) {
            case WEAVER_VAL_TYPE_BOOLEAN:
                if (json_obj_get_bool(jptr, param->name, &write_req[num_param].val.val.b) == 0) {
                    write_req[num_param].param = param;
                    found = true;
                }
                break;
            case WEAVER_VAL_TYPE_INTEGER:
                if (json_obj_get_int(jptr, param->name, &write_req[num_param].val.val.i) == 0) {
                    write_req[num_param].param = param;
                    found = true;
                }
                break;
            case WEAVER_VAL_TYPE_FLOAT:
                if (json_obj_get_float(jptr, param->name, &write_req[num_param].val.val.f) == 0) {
                    write_req[num_param].param = param;
                    found = true;
                }
                break;
            case WEAVER_VAL_TYPE_STRING: {
                int str_len = 0;
                if (json_obj_get_strlen(jptr, param->name, &str_len) == 0) {
                    if (str_len <= 0 || str_len > ESP_WEAVER_MAX_PARAM_VALUE_LEN) {
                        ESP_LOGW(TAG, "String param %s length %d out of range", param->name, str_len);
                        break;
                    }
                    str_len++;
                    write_req[num_param].val.val.s = calloc(1, str_len);
                    if (!write_req[num_param].val.val.s) {
                        ESP_LOGE(TAG, "Could not allocate memory for string param %s", param->name);
                        err = ESP_ERR_NO_MEM;
                        goto set_params_free;
                    }
                    if (json_obj_get_string(jptr, param->name, write_req[num_param].val.val.s, str_len) != 0) {
                        free(write_req[num_param].val.val.s);
                        write_req[num_param].val.val.s = NULL;
                        break;
                    }
                    write_req[num_param].param = param;
                    found = true;
                }
                break;
            }
            case WEAVER_VAL_TYPE_OBJECT: {
                int val_size = 0;
                if (json_obj_get_object_strlen(jptr, param->name, &val_size) == 0) {
                    if (val_size <= 0 || val_size > ESP_WEAVER_MAX_PARAM_VALUE_LEN) {
                        ESP_LOGW(TAG, "Object param %s length %d out of range", param->name, val_size);
                        break;
                    }
                    val_size++;
                    write_req[num_param].val.val.s = calloc(1, val_size);
                    if (!write_req[num_param].val.val.s) {
                        ESP_LOGE(TAG, "Could not allocate memory for object param %s", param->name);
                        err = ESP_ERR_NO_MEM;
                        goto set_params_free;
                    }
                    if (json_obj_get_object_str(jptr, param->name, write_req[num_param].val.val.s, val_size) != 0) {
                        free(write_req[num_param].val.val.s);
                        write_req[num_param].val.val.s = NULL;
                        break;
                    }
                    write_req[num_param].param = param;
                    write_req[num_param].val.type = WEAVER_VAL_TYPE_OBJECT;
                    found = true;
                }
                break;
            }
            case WEAVER_VAL_TYPE_ARRAY: {
                int val_size = 0;
                if (json_obj_get_array_strlen(jptr, param->name, &val_size) == 0) {
                    if (val_size <= 0 || val_size > ESP_WEAVER_MAX_PARAM_VALUE_LEN) {
                        ESP_LOGW(TAG, "Array param %s length %d out of range", param->name, val_size);
                        break;
                    }
                    val_size++;
                    write_req[num_param].val.val.s = calloc(1, val_size);
                    if (!write_req[num_param].val.val.s) {
                        ESP_LOGE(TAG, "Could not allocate memory for array param %s", param->name);
                        err = ESP_ERR_NO_MEM;
                        goto set_params_free;
                    }
                    if (json_obj_get_array_str(jptr, param->name, write_req[num_param].val.val.s, val_size) != 0) {
                        free(write_req[num_param].val.val.s);
                        write_req[num_param].val.val.s = NULL;
                        break;
                    }
                    write_req[num_param].param = param;
                    write_req[num_param].val.type = WEAVER_VAL_TYPE_ARRAY;
                    found = true;
                }
                break;
            }
            default:
                break;
            }
            if (found) {
                num_param++;
            }
        }
    }

    ESP_LOGD(TAG, "Found %d params in write request for %s", num_param, device->name);

    if (num_param > 0) {
        if (device->bulk_write_cb) {
            cb_err = device->bulk_write_cb(device, (const esp_weaver_param_write_req_t *)write_req,
                                           num_param, device->priv_data, ctx);
            if (cb_err != ESP_OK) {
                ESP_LOGE(TAG, "Bulk write callback failed for device %s: %s",
                         device->name, esp_err_to_name(cb_err));
                err = cb_err;
            }
        } else {
            ESP_LOGW(TAG, "No bulk write callback for device %s, updating params only", device->name);
            for (int i = 0; i < num_param; i++) {
                esp_weaver_param_update(write_req[i].param, write_req[i].val);
            }
        }
    }

set_params_free:
    for (int i = 0; i < num_param; i++) {
        if ((write_req[i].val.type == WEAVER_VAL_TYPE_STRING || write_req[i].val.type == WEAVER_VAL_TYPE_OBJECT
                || write_req[i].val.type == WEAVER_VAL_TYPE_ARRAY) && write_req[i].val.val.s) {
            free(write_req[i].val.val.s);
        }
    }
    free(write_req);
    return err;
}

esp_err_t esp_weaver_handle_set_params(const char *data, size_t data_len, esp_weaver_req_src_t src)
{
    if (!s_node) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGD(TAG, "Received params (%d bytes)", (int)data_len);

    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, data, data_len) != 0) {
        return ESP_FAIL;
    }

    esp_weaver_write_ctx_t ctx = {
        .src = src,
    };

    esp_err_t ret = ESP_OK;
    esp_weaver_device_t *device;
    STAILQ_FOREACH(device, &s_node->devices, next) {
        if (json_obj_get_object(&jctx, device->name) == 0) {
            esp_err_t r = esp_weaver_device_set_params(device, &jctx, &ctx);
            if (r != ESP_OK && ret == ESP_OK) {
                ret = r;
            }
            json_obj_leave_object(&jctx);
        }
    }

    json_parse_end(&jctx);
    return ret;
}

esp_weaver_state_t esp_weaver_get_state(void)
{
    return s_node ? s_node->state : ESP_WEAVER_STATE_DEINIT;
}

void esp_weaver_set_state(esp_weaver_state_t state)
{
    if (s_node) {
        s_node->state = state;
    }
}

esp_weaver_node_t *esp_weaver_node_init(const esp_weaver_config_t *config, const char *name, const char *type)
{
    if (s_node) {
        ESP_LOGE(TAG, "Already initialized");
        return NULL;
    }

    if (!config || !name || !type) {
        ESP_LOGE(TAG, "Config, name and type are mandatory.");
        return NULL;
    }

    s_node = calloc(1, sizeof(esp_weaver_node_t));
    if (!s_node) {
        ESP_LOGE(TAG, "Failed to allocate memory for node");
        return NULL;
    }

    s_node->name = strdup(name);
    s_node->type = strdup(type);

    if (!s_node->name || !s_node->type) {
        safe_free(s_node->name);
        safe_free(s_node->type);
        free(s_node);
        s_node = NULL;
        return NULL;
    }

    if (config->node_id && strlen(config->node_id) > 0) {
        s_node->node_id = strdup(config->node_id);
    } else {
        uint8_t mac[6] = {0};
        esp_err_t mac_err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char id_buf[13];
        if (mac_err == ESP_OK) {
            snprintf(id_buf, sizeof(id_buf), "%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            ESP_LOGE(TAG, "Failed to read MAC address: %s", esp_err_to_name(mac_err));
            snprintf(id_buf, sizeof(id_buf), "UNKNOWN");
        }
        s_node->node_id = strdup(id_buf);
    }

    if (!s_node->node_id) {
        safe_free(s_node->name);
        safe_free(s_node->type);
        free(s_node);
        s_node = NULL;
        return NULL;
    }

    STAILQ_INIT(&s_node->devices);
    s_node->state = ESP_WEAVER_STATE_INIT_DONE;
    ESP_LOGI(TAG, "Weaver initialized: %s (%s), node_id: %s", name, type, s_node->node_id);

    return s_node;
}

esp_err_t esp_weaver_param_delete(esp_weaver_param_t *param)
{
    if (!param) {
        return ESP_ERR_INVALID_ARG;
    }

    safe_free(param->name);
    safe_free(param->type);
    safe_free(param->ui_type);
    esp_weaver_val_free(&param->val);

    if (param->has_bounds) {
        esp_weaver_val_free(&param->min);
        esp_weaver_val_free(&param->max);
        esp_weaver_val_free(&param->step);
    }

    free(param);
    return ESP_OK;
}

esp_err_t esp_weaver_device_delete(esp_weaver_device_t *device)
{
    if (!device) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_node) {
        esp_weaver_device_t *d;
        STAILQ_FOREACH(d, &s_node->devices, next) {
            if (d == device) {
                ESP_LOGE(TAG, "Device %s is still in node. Remove it first with esp_weaver_node_remove_device()",
                         device->name);
                return ESP_ERR_INVALID_STATE;
            }
        }
    }

    esp_weaver_param_t *param;
    while ((param = STAILQ_FIRST(&device->params)) != NULL) {
        STAILQ_REMOVE_HEAD(&device->params, next);
        esp_weaver_param_delete(param);
    }

    safe_free(device->name);
    safe_free(device->type);
    free(device);

    return ESP_OK;
}

esp_err_t esp_weaver_node_deinit(const esp_weaver_node_t *node)
{
    if (!s_node) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!node || node != s_node) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_weaver_local_ctrl_stop();
    esp_weaver_local_ctrl_set_pop(NULL);

    esp_weaver_device_t *dev;
    while ((dev = STAILQ_FIRST(&s_node->devices)) != NULL) {
        STAILQ_REMOVE_HEAD(&s_node->devices, next);
        esp_weaver_device_delete(dev);
    }

    safe_free(s_node->name);
    safe_free(s_node->type);
    safe_free(s_node->node_id);
    free(s_node);
    s_node = NULL;

    ESP_LOGI(TAG, "Weaver deinitialized");
    return ESP_OK;
}

const char *esp_weaver_get_node_id(void)
{
    return s_node ? s_node->node_id : NULL;
}

esp_weaver_device_t *esp_weaver_device_create(const char *dev_name, const char *type, void *priv_data)
{
    if (!dev_name || !type) {
        ESP_LOGE(TAG, "Device name and type are mandatory.");
        return NULL;
    }

    esp_weaver_device_t *device = calloc(1, sizeof(esp_weaver_device_t));
    if (!device) {
        ESP_LOGE(TAG, "Failed to allocate memory for device.");
        return NULL;
    }

    device->name = strdup(dev_name);
    device->type = strdup(type);
    device->priv_data = priv_data;
    STAILQ_INIT(&device->params);

    if (!device->name || !device->type) {
        safe_free(device->name);
        safe_free(device->type);
        free(device);
        return NULL;
    }

    ESP_LOGI(TAG, "Device created: %s (%s)", dev_name, type);
    return device;
}

esp_err_t esp_weaver_device_add_param(esp_weaver_device_t *device, esp_weaver_param_t *param)
{
    if (!device || !param) {
        return ESP_ERR_INVALID_ARG;
    }

    if (esp_weaver_device_get_param_by_name(device, param->name) != NULL) {
        ESP_LOGE(TAG, "Param %s already exists in device %s", param->name, device->name);
        return ESP_ERR_INVALID_ARG;
    }

    if (device->param_count == UINT8_MAX) {
        ESP_LOGE(TAG, "Device %s already has maximum number of params (%d)", device->name, UINT8_MAX);
        return ESP_ERR_NO_MEM;
    }

    param->device = device;
    STAILQ_INSERT_TAIL(&device->params, param, next);
    device->param_count++;

    if (param->props & PROP_FLAG_PERSIST) {
        esp_weaver_param_val_t stored_val = {0};
        if (esp_weaver_param_read_value(param, &stored_val) == ESP_OK) {
            esp_weaver_val_free(&param->val);
            param->val = stored_val;
            if (device->bulk_write_cb
                    && !(param->type && strcmp(param->type, ESP_WEAVER_PARAM_NAME) == 0)) {
                esp_weaver_write_ctx_t ctx = {
                    .src = ESP_WEAVER_REQ_SRC_INIT,
                };
                esp_weaver_param_val_t val_copy = {0};
                if (esp_weaver_val_copy(&val_copy, &param->val) == ESP_OK) {
                    esp_weaver_param_write_req_t write_req = {
                        .param = param,
                        .val = val_copy,
                    };
                    device->bulk_write_cb(device, &write_req, 1, device->priv_data, &ctx);
                    esp_weaver_val_free(&val_copy);
                } else {
                    ESP_LOGE(TAG, "Failed to copy param value for init callback");
                }
            }
        } else {
            esp_weaver_param_store_value(param);
        }
    }

    ESP_LOGD(TAG, "Param added to device %s: %s", device->name, param->name);
    return ESP_OK;
}

esp_err_t esp_weaver_device_add_bulk_cb(esp_weaver_device_t *device, esp_weaver_device_bulk_write_cb_t write_cb)
{
    if (!device) {
        return ESP_ERR_INVALID_ARG;
    }

    device->bulk_write_cb = write_cb;
    return ESP_OK;
}

esp_err_t esp_weaver_device_assign_primary_param(esp_weaver_device_t *device, esp_weaver_param_t *param)
{
    if (!device || !param) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Verify the param belongs to this device */
    esp_weaver_param_t *p;
    STAILQ_FOREACH(p, &device->params, next) {
        if (p == param) {
            device->primary_param = param;
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Param %s not found in device %s", param->name, device->name);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_weaver_node_add_device(const esp_weaver_node_t *node, esp_weaver_device_t *device)
{
    if (!node || !device) {
        return ESP_ERR_INVALID_ARG;
    }

    if (node != s_node) {
        ESP_LOGE(TAG, "Invalid node handle");
        return ESP_ERR_INVALID_ARG;
    }

    esp_weaver_device_t *existing;
    STAILQ_FOREACH(existing, &s_node->devices, next) {
        if (strcmp(existing->name, device->name) == 0) {
            ESP_LOGE(TAG, "Device with name %s already exists", device->name);
            return ESP_ERR_INVALID_ARG;
        }
    }

    STAILQ_INSERT_TAIL(&s_node->devices, device, next);

    ESP_LOGI(TAG, "Device added to node: %s", device->name);
    return ESP_OK;
}

esp_err_t esp_weaver_node_remove_device(const esp_weaver_node_t *node, esp_weaver_device_t *device)
{
    if (!node || !device) {
        return ESP_ERR_INVALID_ARG;
    }

    if (node != s_node) {
        ESP_LOGE(TAG, "Invalid node handle");
        return ESP_ERR_INVALID_ARG;
    }

    esp_weaver_device_t *existing;
    STAILQ_FOREACH(existing, &s_node->devices, next) {
        if (existing == device) {
            STAILQ_REMOVE(&s_node->devices, device, esp_weaver_device, next);
            ESP_LOGI(TAG, "Device removed from node: %s", device->name);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Device %s not found in node", device->name);
    return ESP_ERR_NOT_FOUND;
}

const char *esp_weaver_device_get_name(const esp_weaver_device_t *device)
{
    return device ? device->name : NULL;
}

void *esp_weaver_device_get_priv_data(const esp_weaver_device_t *device)
{
    return device ? device->priv_data : NULL;
}

esp_weaver_param_t *esp_weaver_device_get_param_by_name(const esp_weaver_device_t *device, const char *param_name)
{
    if (!device || !param_name) {
        return NULL;
    }

    esp_weaver_param_t *param;
    STAILQ_FOREACH(param, &device->params, next) {
        if (strcmp(param->name, param_name) == 0) {
            return param;
        }
    }

    return NULL;
}

esp_weaver_param_t *esp_weaver_device_get_param_by_type(const esp_weaver_device_t *device, const char *param_type)
{
    if (!device || !param_type) {
        return NULL;
    }

    esp_weaver_param_t *param;
    STAILQ_FOREACH(param, &device->params, next) {
        if (param->type && strcmp(param->type, param_type) == 0) {
            return param;
        }
    }

    return NULL;
}

esp_weaver_param_t *esp_weaver_param_create(const char *param_name, const char *type, esp_weaver_param_val_t val, uint8_t properties)
{
    if (!param_name || !type) {
        ESP_LOGE(TAG, "Param name and type are mandatory.");
        return NULL;
    }

    esp_weaver_param_t *param = calloc(1, sizeof(esp_weaver_param_t));
    if (!param) {
        ESP_LOGE(TAG, "Failed to allocate memory for param.");
        return NULL;
    }

    param->name = strdup(param_name);
    param->type = strdup(type);
    param->props = properties;

    if (!param->name || !param->type) {
        safe_free(param->name);
        safe_free(param->type);
        free(param);
        return NULL;
    }

    if (esp_weaver_val_copy(&param->val, &val) != ESP_OK) {
        safe_free(param->name);
        safe_free(param->type);
        free(param);
        return NULL;
    }

    ESP_LOGD(TAG, "Param created: %s (%s)", param_name, type);
    return param;
}

esp_err_t esp_weaver_param_add_ui_type(esp_weaver_param_t *param, const char *ui_type)
{
    if (!param || !ui_type) {
        return ESP_ERR_INVALID_ARG;
    }

    safe_free(param->ui_type);
    param->ui_type = strdup(ui_type);

    return param->ui_type ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t esp_weaver_param_add_bounds(esp_weaver_param_t *param, esp_weaver_param_val_t min, esp_weaver_param_val_t max, esp_weaver_param_val_t step)
{
    if (!param) {
        return ESP_ERR_INVALID_ARG;
    }

    if (min.type != max.type) {
        ESP_LOGE(TAG, "Bounds type mismatch: min type=%d, max type=%d", min.type, max.type);
        return ESP_ERR_INVALID_ARG;
    }
    if (step.type != WEAVER_VAL_TYPE_INVALID && step.type != min.type) {
        ESP_LOGE(TAG, "Step type (%d) must match bounds type (%d)", step.type, min.type);
        return ESP_ERR_INVALID_ARG;
    }
    if (param->val.type != min.type) {
        ESP_LOGE(TAG, "Bounds type (%d) must match parameter type (%d)", min.type, param->val.type);
        return ESP_ERR_INVALID_ARG;
    }
    if (min.type == WEAVER_VAL_TYPE_INTEGER && min.val.i > max.val.i) {
        ESP_LOGE(TAG, "Invalid bounds: min (%d) > max (%d)", min.val.i, max.val.i);
        return ESP_ERR_INVALID_ARG;
    } else if (min.type == WEAVER_VAL_TYPE_FLOAT && min.val.f > max.val.f) {
        ESP_LOGE(TAG, "Invalid bounds: min (%f) > max (%f)", min.val.f, max.val.f);
        return ESP_ERR_INVALID_ARG;
    }

    esp_weaver_param_val_t new_min = {0};
    esp_weaver_param_val_t new_max = {0};
    esp_weaver_param_val_t new_step = {0};

    esp_err_t err = esp_weaver_val_copy(&new_min, &min);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_weaver_val_copy(&new_max, &max);
    if (err != ESP_OK) {
        esp_weaver_val_free(&new_min);
        return err;
    }

    err = esp_weaver_val_copy(&new_step, &step);
    if (err != ESP_OK) {
        esp_weaver_val_free(&new_min);
        esp_weaver_val_free(&new_max);
        return err;
    }

    if (param->has_bounds) {
        esp_weaver_val_free(&param->min);
        esp_weaver_val_free(&param->max);
        esp_weaver_val_free(&param->step);
    }
    param->min = new_min;
    param->max = new_max;
    param->step = new_step;
    param->has_bounds = true;
    return ESP_OK;
}

const char *esp_weaver_param_get_name(const esp_weaver_param_t *param)
{
    return param ? param->name : NULL;
}

const esp_weaver_param_val_t *esp_weaver_param_get_val(const esp_weaver_param_t *param)
{
    return param ? &param->val : NULL;
}

static esp_err_t esp_weaver_param_store_value(esp_weaver_param_t *param)
{
    if (!param || !param->device) {
        return ESP_FAIL;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(CONFIG_ESP_WEAVER_NVS_PARTITION_NAME,
                                            param->device->name, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    if (param->val.type == WEAVER_VAL_TYPE_STRING || param->val.type == WEAVER_VAL_TYPE_OBJECT
            || param->val.type == WEAVER_VAL_TYPE_ARRAY) {
        if (param->val.val.s) {
            err = nvs_set_blob(handle, param->name, param->val.val.s, strlen(param->val.val.s));
            if (err == ESP_OK) {
                err = nvs_commit(handle);
            }
        } else {
            err = nvs_erase_key(handle, param->name);
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                err = ESP_OK;
            } else if (err == ESP_OK) {
                err = nvs_commit(handle);
            }
        }
    } else {
        err = nvs_set_blob(handle, param->name, &param->val, sizeof(esp_weaver_param_val_t));
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
    }
    nvs_close(handle);
    return err;
}

static esp_err_t esp_weaver_param_read_value(esp_weaver_param_t *param, esp_weaver_param_val_t *val)
{
    if (!param || !param->device || !val) {
        return ESP_FAIL;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(CONFIG_ESP_WEAVER_NVS_PARTITION_NAME,
                                            param->device->name, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    if (param->val.type == WEAVER_VAL_TYPE_STRING || param->val.type == WEAVER_VAL_TYPE_OBJECT
            || param->val.type == WEAVER_VAL_TYPE_ARRAY) {
        size_t len = 0;
        if ((err = nvs_get_blob(handle, param->name, NULL, &len)) == ESP_OK) {
            ESP_LOGD(TAG, "nvs_get_blob: %s, len: %d", param->name, len);
            char *s_val = calloc(1, len + 1);
            if (!s_val) {
                err = ESP_ERR_NO_MEM;
            } else {
                err = nvs_get_blob(handle, param->name, s_val, &len);
                if (err != ESP_OK) {
                    free(s_val);
                } else {
                    s_val[len] = '\0';
                    val->type = param->val.type;
                    val->val.s = s_val;
                }
            }
        }
    } else {
        size_t len = sizeof(esp_weaver_param_val_t);
        err = nvs_get_blob(handle, param->name, val, &len);
    }
    nvs_close(handle);
    return err;
}

esp_err_t esp_weaver_param_update(esp_weaver_param_t *param, esp_weaver_param_val_t val)
{
    if (!param) {
        return ESP_ERR_INVALID_ARG;
    }

    if (param->val.type != val.type) {
        ESP_LOGE(TAG, "Type mismatch for %s: existing=%d, new=%d",
                 param->name, param->val.type, val.type);
        return ESP_ERR_INVALID_ARG;
    }

    if (param->has_bounds) {
        bool out_of_bounds = false;
        if (param->val.type == WEAVER_VAL_TYPE_INTEGER) {
            if (val.val.i < param->min.val.i || val.val.i > param->max.val.i) {
                out_of_bounds = true;
                ESP_LOGE(TAG, "Value %d out of bounds for %s: min=%d, max=%d",
                         val.val.i, param->name, param->min.val.i, param->max.val.i);
            }
        } else if (param->val.type == WEAVER_VAL_TYPE_FLOAT) {
            if (val.val.f < param->min.val.f || val.val.f > param->max.val.f) {
                out_of_bounds = true;
                ESP_LOGE(TAG, "Value %f out of bounds for %s: min=%f, max=%f",
                         val.val.f, param->name, param->min.val.f, param->max.val.f);
            }
        }
        if (out_of_bounds) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    esp_err_t err = esp_weaver_val_copy(&param->val, &val);
    if (err == ESP_OK) {
        param->flags |= ESP_WEAVER_PARAM_FLAG_VALUE_CHANGE;
        if (param->props & PROP_FLAG_PERSIST) {
            esp_weaver_param_store_value(param);
        }
    }

    return err;
}

esp_err_t esp_weaver_param_update_and_report(esp_weaver_param_t *param, esp_weaver_param_val_t val)
{
    esp_err_t err = esp_weaver_param_update(param, val);
    if (err == ESP_OK) {
        esp_err_t send_err = esp_weaver_local_ctrl_send_params();
        if (send_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send params: %s", esp_err_to_name(send_err));
        }
    }
    return err;
}
