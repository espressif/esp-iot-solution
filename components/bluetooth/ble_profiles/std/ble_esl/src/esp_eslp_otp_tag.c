/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief ESL Profile Tag OTP Object Server (image slots)
 *
 *  Pattern follows examples/bluetooth/ble_profiles/ble_otp/ble_otp_server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_esl.h"
#include "esp_eslp_tag.h"
#include "esp_otp.h"
#include "esp_eslp_otp_priv.h"

static const char *TAG = "ble_eslp_otp_tag";

typedef enum {
    ESLP_IMG_EMPTY = 0,
    ESLP_IMG_WRITING,
    ESLP_IMG_VALID,
} eslp_img_state_t;

typedef struct {
    uint32_t current_size;
    eslp_img_state_t state;
    uint8_t *buffer;
} eslp_image_obj_t;

typedef struct {
    bool active;
    uint8_t op_code;
    uint8_t image_index;
    uint8_t write_mode;
    uint32_t offset;
    uint32_t length;
    uint32_t progress;
    esp_ble_otp_transfer_info_t transfer_info;
} eslp_otp_xfer_t;

static bool s_inited;
static uint8_t s_current_index;
static eslp_image_obj_t s_images[CONFIG_BLE_ESL_MAX_IMAGES];
static eslp_otp_xfer_t s_xfer;

static void eslp_otp_tag_post_image_written(uint8_t image_index, uint32_t size, esp_err_t status)
{
    esp_ble_eslp_image_written_t ev = {
        .image_index = image_index,
        .size = size,
        .status = status,
    };
    esp_err_t ret = esp_event_post(BLE_ESLP_EVENTS, BLE_ESLP_EVENT_IMAGE_WRITTEN, &ev, sizeof(ev), portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "post IMAGE_WRITTEN failed: %s", esp_err_to_name(ret));
    }
}

static void eslp_apply_object_to_ots(uint8_t index)
{
    if (index >= CONFIG_BLE_ESL_MAX_IMAGES) {
        return;
    }

    eslp_image_obj_t *obj = &s_images[index];
    esp_ble_ots_size_t obj_size = {
        .allocated_size = CONFIG_BLE_ESL_MAX_IMAGE_SIZE,
        .current_size = obj->current_size,
    };
    esp_ble_ots_id_t obj_id = {0};
    esp_ble_ots_prop_t obj_prop = {0};
    uint16_t obj_type = 0xFFFF;
    char name[16];

    obj_prop.read_prop = 1;
    obj_prop.write_prop = 1;
    obj_prop.truncate_prop = 1;
    obj_prop.delete_prop = 0; /* Image objects are not deletable */
    eslp_otp_image_index_to_id(index, &obj_id);
    snprintf(name, sizeof(name), "esl_img_%u", index);

    (void)esp_ble_ots_set_name((const uint8_t *)name, strlen(name));
    (void)esp_ble_ots_set_type(&obj_type);
    (void)esp_ble_ots_set_size(&obj_size);
    (void)esp_ble_ots_set_id(&obj_id);
    (void)esp_ble_ots_set_prop(&obj_prop);
}

static bool eslp_select_object(uint8_t index)
{
    if (index >= CONFIG_BLE_ESL_MAX_IMAGES || !s_images[index].buffer) {
        return false;
    }
    s_current_index = index;
    eslp_apply_object_to_ots(index);
    return true;
}

static void eslp_send_next_read_chunk(void)
{
    if (!s_xfer.active || s_xfer.op_code != BLE_OTS_OACP_READ) {
        return;
    }
    if (s_xfer.progress >= s_xfer.length) {
        (void)esp_ble_otp_server_disconnect_transfer_channel(&s_xfer.transfer_info);
        return;
    }

    eslp_image_obj_t *obj = &s_images[s_xfer.image_index];
    uint32_t remaining = s_xfer.length - s_xfer.progress;
    uint32_t chunk = s_xfer.transfer_info.mtu ? s_xfer.transfer_info.mtu : remaining;
    if (chunk > remaining) {
        chunk = remaining;
    }

    const uint8_t *data = &obj->buffer[s_xfer.offset + s_xfer.progress];
    if (esp_ble_otp_server_send_data(&s_xfer.transfer_info, data, (uint16_t)chunk) == ESP_OK) {
        s_xfer.progress += chunk;
    }
}

static esp_err_t eslp_oacp_cb(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx)
{
    (void)ctx;
    eslp_image_obj_t *obj = &s_images[s_current_index];

    switch (op_code) {
    case BLE_OTS_OACP_READ: {
        if (param_len < sizeof(uint32_t) * 2) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        uint32_t offset = 0;
        uint32_t length = 0;
        memcpy(&offset, parameter, sizeof(offset));
        memcpy(&length, parameter + sizeof(offset), sizeof(length));
        if (offset > obj->current_size) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        if (length == 0) {
            length = obj->current_size - offset;
        }
        if (length > obj->current_size - offset) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        s_xfer.active = true;
        s_xfer.op_code = BLE_OTS_OACP_READ;
        s_xfer.image_index = s_current_index;
        s_xfer.offset = offset;
        s_xfer.length = length;
        s_xfer.progress = 0;
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    }
    case BLE_OTS_OACP_WRITE: {
        /* OTS Write params: Offset(4) + Length(4) + Mode(1) */
        if (param_len < sizeof(uint32_t) * 2 + 1) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        uint32_t offset = 0;
        uint32_t length = 0;
        uint8_t mode = 0;
        memcpy(&offset, parameter, sizeof(offset));
        memcpy(&length, parameter + sizeof(offset), sizeof(length));
        mode = parameter[sizeof(offset) + sizeof(length)];

        if (mode == BLE_OTP_WRITE_MODE_APPEND || mode == BLE_OTP_WRITE_MODE_PATCH) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_NOT_SUPPORT, NULL, 0);
            break;
        }
        if (mode != BLE_OTP_WRITE_MODE_OVERWRITE && mode != BLE_OTP_WRITE_MODE_TRUNCATE) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        if (offset > obj->current_size ||
                length == 0 ||
                length > CONFIG_BLE_ESL_MAX_IMAGE_SIZE - offset) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }

        obj->state = ESLP_IMG_WRITING;
        s_xfer.active = true;
        s_xfer.op_code = BLE_OTS_OACP_WRITE;
        s_xfer.image_index = s_current_index;
        s_xfer.write_mode = mode;
        s_xfer.offset = offset;
        s_xfer.length = length;
        s_xfer.progress = 0;
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    }
    case BLE_OTS_OACP_ABORT:
        obj->state = (obj->current_size > 0) ? ESLP_IMG_VALID : ESLP_IMG_EMPTY;
        memset(&s_xfer, 0, sizeof(s_xfer));
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    default:
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_NOT_SUPPORT, NULL, 0);
        break;
    }
    return ESP_OK;
}

static esp_err_t eslp_olcp_cb(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx)
{
    (void)ctx;

    if (op_code == BLE_OTS_OLCP_REQ_NUM_OF_OBJ) {
        uint32_t count = CONFIG_BLE_ESL_MAX_IMAGES;
        esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS,
                                              (const uint8_t *)&count, sizeof(count));
        return ESP_OK;
    }

    switch (op_code) {
    case BLE_OTS_OLCP_FIRST:
        if (eslp_select_object(0)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_NO_OBJECT, NULL, 0);
        }
        break;
    case BLE_OTS_OLCP_LAST:
        if (eslp_select_object(CONFIG_BLE_ESL_MAX_IMAGES - 1)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_NO_OBJECT, NULL, 0);
        }
        break;
    case BLE_OTS_OLCP_NEXT:
        if (s_current_index + 1 < CONFIG_BLE_ESL_MAX_IMAGES &&
                eslp_select_object(s_current_index + 1)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_OUT_OF_BOUNDS, NULL, 0);
        }
        break;
    case BLE_OTS_OLCP_PREVIOUS:
        if (s_current_index > 0 && eslp_select_object(s_current_index - 1)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_OUT_OF_BOUNDS, NULL, 0);
        }
        break;
    case BLE_OTS_OLCP_GO_TO: {
        if (param_len < sizeof(esp_ble_ots_id_t)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        esp_ble_ots_id_t obj_id = {0};
        memcpy(obj_id.id, parameter, sizeof(obj_id.id));
        int index = eslp_otp_id_to_image_index(&obj_id);
        if (index >= 0 && index < CONFIG_BLE_ESL_MAX_IMAGES && eslp_select_object((uint8_t)index)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_OBJECT_ID_NOT_FOUND, NULL, 0);
        }
        break;
    }
    default:
        esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_NOT_SUPPORT, NULL, 0);
        break;
    }
    return ESP_OK;
}

static void eslp_otp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_OTP_EVENTS || !event_data || !s_inited) {
        return;
    }

    esp_ble_otp_event_data_t *evt = event_data;

    switch (id) {
    case BLE_OTP_EVENT_TRANSFER_CHANNEL_CONNECTED:
        s_xfer.transfer_info = evt->transfer_channel_connected.transfer_info;
        if (s_xfer.active && s_xfer.op_code == BLE_OTS_OACP_READ) {
            eslp_send_next_read_chunk();
        }
        break;
    case BLE_OTP_EVENT_TRANSFER_DATA_RECEIVED:
        if (s_xfer.active && s_xfer.op_code == BLE_OTS_OACP_WRITE) {
            eslp_image_obj_t *obj = &s_images[s_xfer.image_index];
            uint32_t write_pos = s_xfer.offset + s_xfer.progress;
            uint32_t copy_len = evt->transfer_data_received.data_len;
            if (copy_len > 0 && copy_len <= CONFIG_BLE_ESL_MAX_IMAGE_SIZE - write_pos) {
                memcpy(&obj->buffer[write_pos], evt->transfer_data_received.data, copy_len);
                s_xfer.progress += copy_len;
            }
        }
        break;
    case BLE_OTP_EVENT_TRANSFER_DATA_SENT:
        if (s_xfer.active && s_xfer.op_code == BLE_OTS_OACP_READ) {
            eslp_send_next_read_chunk();
        }
        break;
    case BLE_OTP_EVENT_TRANSFER_COMPLETE:
        if (s_xfer.active && s_xfer.op_code == BLE_OTS_OACP_WRITE) {
            eslp_image_obj_t *obj = &s_images[s_xfer.image_index];
            if (s_xfer.progress != s_xfer.length) {
                /* Incomplete transfer: keep previous size, do not mark new data VALID. */
                ESP_LOGW(TAG, "Write incomplete (%lu/%lu), not marking VALID",
                         (unsigned long)s_xfer.progress, (unsigned long)s_xfer.length);
                obj->state = (obj->current_size > 0) ? ESLP_IMG_VALID : ESLP_IMG_EMPTY;
                eslp_otp_tag_post_image_written(s_xfer.image_index, 0, ESP_FAIL);
            } else {
                uint32_t write_end = s_xfer.offset + s_xfer.length;
                if (s_xfer.write_mode == BLE_OTP_WRITE_MODE_TRUNCATE) {
                    obj->current_size = write_end;
                } else if (write_end > obj->current_size) {
                    /* Overwrite may grow Current Size up to Allocated Size. */
                    obj->current_size = write_end;
                }
                obj->state = ESLP_IMG_VALID;
                eslp_apply_object_to_ots(s_xfer.image_index);
                eslp_otp_tag_post_image_written(s_xfer.image_index, obj->current_size, ESP_OK);
            }
        }
        memset(&s_xfer, 0, sizeof(s_xfer));
        break;
    case BLE_OTP_EVENT_TRANSFER_ERROR:
        if (s_xfer.active && s_xfer.op_code == BLE_OTS_OACP_WRITE) {
            eslp_image_obj_t *obj = &s_images[s_xfer.image_index];
            obj->state = (obj->current_size > 0) ? ESLP_IMG_VALID : ESLP_IMG_EMPTY;
            eslp_otp_tag_post_image_written(s_xfer.image_index, 0, ESP_FAIL);
        }
        memset(&s_xfer, 0, sizeof(s_xfer));
        break;
    case BLE_OTP_EVENT_TRANSFER_CHANNEL_DISCONNECTED:
        memset(&s_xfer, 0, sizeof(s_xfer));
        break;
    default:
        break;
    }
}

esp_err_t esp_ble_eslp_get_image(uint8_t image_index, const uint8_t **data, uint32_t *len)
{
    if (!s_inited || !data || !len) {
        return (!data || !len) ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
    }
    if (image_index >= CONFIG_BLE_ESL_MAX_IMAGES || !s_images[image_index].buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_images[image_index].state != ESLP_IMG_VALID || s_images[image_index].current_size == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    *data = s_images[image_index].buffer;
    *len = s_images[image_index].current_size;
    return ESP_OK;
}

esp_err_t eslp_otp_tag_init(void)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(s_images, 0, sizeof(s_images));
    memset(&s_xfer, 0, sizeof(s_xfer));
    s_current_index = 0;

    for (uint8_t i = 0; i < CONFIG_BLE_ESL_MAX_IMAGES; i++) {
        s_images[i].buffer = calloc(1, CONFIG_BLE_ESL_MAX_IMAGE_SIZE);
        if (!s_images[i].buffer) {
            for (uint8_t j = 0; j < i; j++) {
                free(s_images[j].buffer);
                s_images[j].buffer = NULL;
            }
            return ESP_ERR_NO_MEM;
        }
        s_images[i].state = ESLP_IMG_EMPTY;
    }

    esp_err_t ret = esp_ble_conn_l2cap_coc_mem_init();
    if (ret != ESP_OK) {
        goto fail_bufs;
    }

    esp_ble_otp_config_t otp_cfg = {
        .role = BLE_OTP_ROLE_SERVER,
        .psm = BLE_OTP_PSM_DEFAULT,
        .l2cap_coc_mtu = BLE_OTP_L2CAP_COC_MTU_DEFAULT,
        .auto_discover_ots = false,
    };
    ret = esp_ble_otp_init(&otp_cfg);
    if (ret != ESP_OK) {
        goto fail_bufs;
    }

    esp_ble_ots_feature_t feature = {0};
    feature.oacp.read_op = 1;
    feature.oacp.write_op = 1;
    feature.oacp.truncation_op = 1;
    feature.oacp.abort_op = 1;
    feature.olcp.goto_op = 1;
    feature.olcp.req_num_op = 1;
    ret = esp_ble_otp_server_set_feature(&feature);
    if (ret != ESP_OK) {
        goto fail_otp;
    }

    ret = esp_ble_otp_server_register_oacp_callback(eslp_oacp_cb, NULL);
    if (ret != ESP_OK) {
        goto fail_otp;
    }
    ret = esp_ble_otp_server_register_olcp_callback(eslp_olcp_cb, NULL);
    if (ret != ESP_OK) {
        goto fail_otp;
    }

    ret = esp_event_handler_register(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, eslp_otp_event_handler, NULL);
    if (ret != ESP_OK) {
        goto fail_otp;
    }

    eslp_select_object(0);

    esp_ble_esl_image_info_t image_info = {
        .max_image_index = CONFIG_BLE_ESL_MAX_IMAGES - 1,
    };
    (void)esp_ble_esl_set_image_info(&image_info);

    s_inited = true;
    ESP_LOGI(TAG, "ESL OTP Object Server ready (%u images, %u bytes each)",
             CONFIG_BLE_ESL_MAX_IMAGES, CONFIG_BLE_ESL_MAX_IMAGE_SIZE);
    return ESP_OK;

fail_otp:
    (void)esp_ble_otp_deinit();
fail_bufs:
    for (uint8_t i = 0; i < CONFIG_BLE_ESL_MAX_IMAGES; i++) {
        free(s_images[i].buffer);
        s_images[i].buffer = NULL;
    }
    return ret;
}

esp_err_t eslp_otp_tag_deinit(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    (void)esp_event_handler_unregister(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, eslp_otp_event_handler);
    (void)esp_ble_otp_deinit();
    for (uint8_t i = 0; i < CONFIG_BLE_ESL_MAX_IMAGES; i++) {
        free(s_images[i].buffer);
        s_images[i].buffer = NULL;
    }
    memset(s_images, 0, sizeof(s_images));
    memset(&s_xfer, 0, sizeof(s_xfer));
    s_inited = false;
    return ESP_OK;
}

void eslp_otp_tag_clear_images(void)
{
    if (!s_inited) {
        return;
    }
    memset(&s_xfer, 0, sizeof(s_xfer));
    for (uint8_t i = 0; i < CONFIG_BLE_ESL_MAX_IMAGES; i++) {
        if (s_images[i].buffer) {
            memset(s_images[i].buffer, 0, CONFIG_BLE_ESL_MAX_IMAGE_SIZE);
        }
        s_images[i].current_size = 0;
        s_images[i].state = ESLP_IMG_EMPTY;
    }
    eslp_select_object(s_current_index < CONFIG_BLE_ESL_MAX_IMAGES ? s_current_index : 0);
}
