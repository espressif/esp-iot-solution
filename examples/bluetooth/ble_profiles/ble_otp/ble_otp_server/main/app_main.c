/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"
#include "esp_otp.h"

static const char *TAG = "ble_otp_server";

/*
 * Virtual object store for demo/testing only
 * ------------------------------------------
 * The structures below simulate OTS objects in RAM. They are not real files
 * or persistent storage. Use them only to run the OTP server example and
 * verify profile behavior. In a real product you would:
 *   - Back objects by files (e.g. flash/SD) or a database
 *   - Persist object list and metadata across reboots
 *   - Enforce security and size limits per your product requirements
 */
#define MAX_OBJECTS 3
#define MAX_OBJECT_SIZE 1024

/** Object lifecycle state (virtual store only) */
typedef enum {
    OBJ_IDLE,
    OBJ_WRITING,
    OBJ_VALID,
} object_state_t;

/** In-memory virtual object: simulates one OTS object for the example. Not persistent. */
typedef struct {
    uint64_t id;
    uint32_t alloc_size;
    uint32_t write_offset;
    uint32_t final_size;

    object_state_t state;
    bool deleted;   /* Logical delete: object still in list but skipped by OLCP */

    uint8_t  buffer[MAX_OBJECT_SIZE];
} otp_object_t;

/** Per-transfer context used by this example to track active OACP Read/Write. */
typedef struct {
    bool active;
    uint8_t op_code;
    uint32_t offset;
    uint32_t length;
    uint32_t progress;
    uint32_t expected_size;  /* intended object size for write; committed to obj->final_size only on transfer complete */
    esp_ble_otp_transfer_info_t transfer_info;
} app_transfer_ctx_t;

static otp_object_t s_objects[MAX_OBJECTS];   /* Virtual object pool (not persistent) */
static uint8_t s_current_object = 0;         /* Index of current object exposed to OTS */
static app_transfer_ctx_t s_transfer = {0};

/** Display names for the virtual objects (exposed as OTS Object Name). */
static const char *s_object_names[MAX_OBJECTS] = {
    "otp_object_0",
    "otp_object_1",
    "otp_object_2",
};

static void app_objects_init(void)
{
    for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
        otp_object_t *obj = &s_objects[i];
        obj->id = (uint64_t)(i + 1);
        obj->alloc_size = MAX_OBJECT_SIZE;
        obj->write_offset = 0;
        obj->final_size = 0;
        obj->state = OBJ_VALID;

        const char *payload = (i == 0) ? "Hello-OTP-Object-0 " :
                              (i == 1) ? "Hello-OTP-Object-1 " :
                              "Hello-OTP-Object-2 ";
        size_t payload_len = strlen(payload);
        if (payload_len > MAX_OBJECT_SIZE) {
            payload_len = MAX_OBJECT_SIZE;
        }
        memcpy(obj->buffer, payload, payload_len);
        obj->final_size = payload_len;
        obj->deleted = false;
    }
}

static uint8_t app_count_objects(void)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
        if (!s_objects[i].deleted) {
            n++;
        }
    }
    return n;
}

static int8_t app_first_non_deleted(void)
{
    for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
        if (!s_objects[i].deleted) {
            return (int8_t)i;
        }
    }
    return -1;
}

static int8_t app_next_non_deleted(uint8_t from)
{
    for (uint8_t i = from + 1; i < MAX_OBJECTS; i++) {
        if (!s_objects[i].deleted) {
            return (int8_t)i;
        }
    }
    return -1;
}

static int8_t app_prev_non_deleted(uint8_t from)
{
    for (int i = (int)from - 1; i >= 0; i--) {
        if (!s_objects[i].deleted) {
            return (int8_t)i;
        }
    }
    return -1;
}

static void app_apply_object_to_ots(uint8_t index)
{
    if (index >= MAX_OBJECTS) {
        return;
    }

    otp_object_t *obj = &s_objects[index];
    esp_ble_ots_size_t obj_size = {
        .allocated_size = obj->alloc_size,
        .current_size = obj->final_size,
    };
    esp_ble_ots_id_t obj_id = { .id = {0} };
    esp_ble_ots_prop_t obj_prop = {0};
    uint16_t obj_type = 0xFFFF;

    obj_prop.read_prop = 1;
    obj_prop.write_prop = 1;
    obj_prop.execute_prop = 1;
    obj_prop.delete_prop = 1;
    obj_id.id[0] = (uint8_t)(obj->id & 0xFF);
    obj_id.id[1] = (uint8_t)((obj->id >> 8) & 0xFF);
    obj_id.id[2] = (uint8_t)((obj->id >> 16) & 0xFF);
    obj_id.id[3] = (uint8_t)((obj->id >> 24) & 0xFF);
    obj_id.id[4] = (uint8_t)((obj->id >> 32) & 0xFF);
    obj_id.id[5] = (uint8_t)((obj->id >> 40) & 0xFF);

    ESP_ERROR_CHECK(esp_ble_ots_set_name((const uint8_t *)s_object_names[index],
                                         strlen(s_object_names[index])));
    ESP_ERROR_CHECK(esp_ble_ots_set_type(&obj_type));
    ESP_ERROR_CHECK(esp_ble_ots_set_size(&obj_size));
    ESP_ERROR_CHECK(esp_ble_ots_set_id(&obj_id));
    ESP_ERROR_CHECK(esp_ble_ots_set_prop(&obj_prop));
}

static bool app_select_object(uint8_t index)
{
    if (index >= MAX_OBJECTS) {
        return false;
    }
    s_current_object = index;
    app_apply_object_to_ots(index);
    return true;
}

static int app_find_object_by_id(const esp_ble_ots_id_t *id)
{
    if (!id) {
        return -1;
    }
    for (uint8_t i = 0; i < MAX_OBJECTS; i++) {
        esp_ble_ots_id_t obj_id = { .id = {0} };
        obj_id.id[0] = (uint8_t)(s_objects[i].id & 0xFF);
        obj_id.id[1] = (uint8_t)((s_objects[i].id >> 8) & 0xFF);
        obj_id.id[2] = (uint8_t)((s_objects[i].id >> 16) & 0xFF);
        obj_id.id[3] = (uint8_t)((s_objects[i].id >> 24) & 0xFF);
        obj_id.id[4] = (uint8_t)((s_objects[i].id >> 32) & 0xFF);
        obj_id.id[5] = (uint8_t)((s_objects[i].id >> 40) & 0xFF);
        if (memcmp(obj_id.id, id->id, sizeof(obj_id.id)) == 0) {
            return i;
        }
    }
    return -1;
}

static void app_send_next_read_chunk(void)
{
    if (!s_transfer.active || s_transfer.op_code != BLE_OTS_OACP_READ) {
        return;
    }
    if (s_transfer.progress >= s_transfer.length) {
        esp_ble_otp_server_disconnect_transfer_channel(&s_transfer.transfer_info);
        return;
    }

    otp_object_t *obj = &s_objects[s_current_object];
    uint32_t remaining = s_transfer.length - s_transfer.progress;
    uint32_t chunk = s_transfer.transfer_info.mtu ? s_transfer.transfer_info.mtu : remaining;
    if (chunk > remaining) {
        chunk = remaining;
    }

    const uint8_t *data = &obj->buffer[s_transfer.offset + s_transfer.progress];
    if (esp_ble_otp_server_send_data(&s_transfer.transfer_info, data, chunk) == ESP_OK) {
        s_transfer.progress += chunk;
    }
}

static esp_err_t app_oacp_cb(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx)
{
    (void)ctx;

    switch (op_code) {
    case BLE_OTS_OACP_READ: {
        /* Object not found / invalid if current object was deleted */
        if (s_objects[s_current_object].deleted) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_OBJECT, NULL, 0);
            break;
        }
        if (param_len < sizeof(uint32_t) * 2) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        uint32_t offset = 0;
        uint32_t length = 0;
        memcpy(&offset, parameter, sizeof(offset));
        memcpy(&length, parameter + sizeof(offset), sizeof(length));
        otp_object_t *obj = &s_objects[s_current_object];
        if (offset > obj->final_size) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        if (length == 0) {
            length = obj->final_size - offset;
        }
        if (length > obj->final_size - offset) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        s_transfer.active = true;
        s_transfer.op_code = BLE_OTS_OACP_READ;
        s_transfer.offset = offset;
        s_transfer.length = length;
        s_transfer.progress = 0;
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    }
    case BLE_OTS_OACP_WRITE: {
        /* Object not found / invalid if current object was deleted */
        if (s_objects[s_current_object].deleted) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_OBJECT, NULL, 0);
            break;
        }
        if (param_len < sizeof(uint32_t) * 2) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        uint32_t offset = 0;
        uint32_t length = 0;
        memcpy(&offset, parameter, sizeof(offset));
        memcpy(&length, parameter + sizeof(offset), sizeof(length));
        otp_object_t *obj = &s_objects[s_current_object];
        if (offset > obj->alloc_size || length > obj->alloc_size - offset) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        obj->state = OBJ_WRITING;
        obj->write_offset = offset;
        s_transfer.expected_size = offset + length;
        s_transfer.active = true;
        s_transfer.op_code = BLE_OTS_OACP_WRITE;
        s_transfer.offset = offset;
        s_transfer.length = length;
        s_transfer.progress = 0;
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    }
    case BLE_OTS_OACP_EXECUTE:
        if (s_objects[s_current_object].deleted) {
            esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_INVALID_OBJECT, NULL, 0);
            break;
        }
        s_objects[s_current_object].state = OBJ_VALID;
        app_apply_object_to_ots(s_current_object);
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    case BLE_OTS_OACP_ABORT:
        s_objects[s_current_object].state = OBJ_VALID;
        s_transfer.expected_size = 0;
        s_transfer.active = false;
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    case BLE_OTS_OACP_DELETE:
        s_objects[s_current_object].deleted = true;
        s_transfer.expected_size = 0;
        s_transfer.active = false;
        {
            int8_t next = app_first_non_deleted();
            if (next >= 0) {
                s_current_object = (uint8_t)next;
                app_apply_object_to_ots(s_current_object);
            }
        }
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_SUCCESS, NULL, 0);
        break;
    default:
        esp_ble_otp_server_send_oacp_response(op_code, BLE_OTS_OACP_RSP_NOT_SUPPORT, NULL, 0);
        break;
    }

    return ESP_OK;
}

static esp_err_t app_olcp_cb(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx)
{
    (void)ctx;

    if (op_code == BLE_OTS_OLCP_REQ_NUM_OF_OBJ) {
        uint32_t count = app_count_objects();
        esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS,
                                              (const uint8_t *)&count, sizeof(count));
        return ESP_OK;
    }

    switch (op_code) {
    case BLE_OTS_OLCP_FIRST: {
        int8_t idx = app_first_non_deleted();
        if (idx >= 0 && app_select_object((uint8_t)idx)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_NO_OBJECT, NULL, 0);
        }
        break;
    }
    case BLE_OTS_OLCP_LAST: {
        int8_t idx = -1;
        for (uint8_t i = MAX_OBJECTS; i > 0; i--) {
            if (!s_objects[i - 1].deleted) {
                idx = (int8_t)(i - 1);
                break;
            }
        }
        if (idx >= 0 && app_select_object((uint8_t)idx)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_NO_OBJECT, NULL, 0);
        }
        break;
    }
    case BLE_OTS_OLCP_NEXT: {
        int8_t next = app_next_non_deleted(s_current_object);
        if (next >= 0) {
            app_select_object((uint8_t)next);
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_OUT_OF_BOUNDS, NULL, 0);
        }
        break;
    }
    case BLE_OTS_OLCP_PREVIOUS: {
        int8_t prev = app_prev_non_deleted(s_current_object);
        if (prev >= 0) {
            app_select_object((uint8_t)prev);
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_SUCCESS, NULL, 0);
        } else {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_OUT_OF_BOUNDS, NULL, 0);
        }
        break;
    }
    case BLE_OTS_OLCP_GO_TO: {
        if (param_len < sizeof(esp_ble_ots_id_t)) {
            esp_ble_otp_server_send_olcp_response(op_code, BLE_OTS_OLCP_RSP_INVALID_PARAMETER, NULL, 0);
            break;
        }
        esp_ble_ots_id_t obj_id = {0};
        memcpy(obj_id.id, parameter, sizeof(obj_id.id));
        int index = app_find_object_by_id(&obj_id);
        if (index >= 0 && !s_objects[index].deleted) {
            app_select_object((uint8_t)index);
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

static void app_ble_otp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_OTP_EVENTS || !event_data) {
        return;
    }

    esp_ble_otp_event_data_t *evt = (esp_ble_otp_event_data_t *)event_data;

    switch (id) {
    case BLE_OTP_EVENT_TRANSFER_CHANNEL_CONNECTED:
        ESP_LOGI(TAG, "Transfer channel connected");
        s_transfer.transfer_info = evt->transfer_channel_connected.transfer_info;
        if (s_transfer.active && s_transfer.op_code == BLE_OTS_OACP_READ) {
            app_send_next_read_chunk();
        }
        break;
    case BLE_OTP_EVENT_TRANSFER_DATA_RECEIVED:
        ESP_LOGI(TAG, "Received %u bytes from client",
                 evt->transfer_data_received.data_len);
        if (evt->transfer_data_received.data_len > 0) {
            ESP_LOG_BUFFER_HEXDUMP(TAG, evt->transfer_data_received.data,
                                   evt->transfer_data_received.data_len, ESP_LOG_INFO);
        }
        if (s_transfer.active && s_transfer.op_code == BLE_OTS_OACP_WRITE) {
            otp_object_t *obj = &s_objects[s_current_object];
            uint32_t write_pos = s_transfer.offset + s_transfer.progress;
            uint32_t copy_len = evt->transfer_data_received.data_len;
            if (copy_len <= obj->alloc_size - write_pos) {
                memcpy(&obj->buffer[write_pos], evt->transfer_data_received.data, copy_len);
                s_transfer.progress += copy_len;
            } else {
                ESP_LOGW(TAG, "Write overflow ignored");
            }
        }
        break;
    case BLE_OTP_EVENT_TRANSFER_DATA_SENT:
        if (s_transfer.active && s_transfer.op_code == BLE_OTS_OACP_READ) {
            app_send_next_read_chunk();
        }
        break;
    case BLE_OTP_EVENT_TRANSFER_EOF:
        ESP_LOGI(TAG, "Transfer EOF");
        break;
    case BLE_OTP_EVENT_TRANSFER_COMPLETE:
        ESP_LOGI(TAG, "Transfer complete");
        if (evt && !evt->transfer_complete.transfer_info.is_read) {
            otp_object_t *obj = &s_objects[s_current_object];
            obj->final_size = s_transfer.offset + s_transfer.progress;
            esp_ble_ots_size_t size = {
                .allocated_size = obj->alloc_size,
                .current_size = obj->final_size,
            };
            esp_ble_ots_set_size(&size);
        }
        s_transfer.expected_size = 0;
        s_transfer.active = false;
        break;
    case BLE_OTP_EVENT_TRANSFER_CHANNEL_DISCONNECTED:
        ESP_LOGI(TAG, "Transfer channel disconnected");
        s_transfer.expected_size = 0;
        s_transfer.active = false;
        break;
    case BLE_OTP_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "Transfer error");
        s_transfer.expected_size = 0;
        s_transfer.active = false;
        break;
    default:
        break;
    }
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)event_data;
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "BLE connected");
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "BLE disconnected");
        break;
    default:
        break;
    }
}

static void app_ots_init(void)
{
    esp_ble_ots_feature_t feature = {0};
    feature.oacp.read_op = 1;
    feature.oacp.write_op = 1;
    feature.oacp.appending_op = 1;
    feature.oacp.execute_op = 1;
    feature.oacp.abort_op = 1;
    feature.oacp.delete_op = 1;
    feature.olcp.goto_op = 1;
    feature.olcp.order_op = 1;
    feature.olcp.req_num_op = 1;
    feature.olcp.clear_mark_op = 1;
    ESP_ERROR_CHECK(esp_ble_otp_server_set_feature(&feature));

    app_objects_init();
    app_select_object(0);
}

void app_main(void)
{
    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_DEVICE_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV,
        .include_service_uuid = 1,
        .adv_uuid_type = BLE_CONN_UUID_TYPE_16,
        .adv_uuid16 = BLE_OTS_UUID16,
    };

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_otp_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_l2cap_coc_mem_init());

    esp_ble_otp_config_t otp_cfg = {
        .role = BLE_OTP_ROLE_SERVER,
        .psm = CONFIG_EXAMPLE_OTP_PSM,
        .l2cap_coc_mtu = CONFIG_BLE_CONN_MGR_L2CAP_COC_MTU,
        .auto_discover_ots = false,
    };
    ESP_ERROR_CHECK(esp_ble_otp_init(&otp_cfg));

    ESP_ERROR_CHECK(esp_ble_otp_server_register_oacp_callback(app_oacp_cb, NULL));
    ESP_ERROR_CHECK(esp_ble_otp_server_register_olcp_callback(app_olcp_cb, NULL));
    app_ots_init();

    ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager: %s", esp_err_to_name(ret));
        esp_ble_otp_deinit();
        esp_ble_conn_l2cap_coc_mem_release();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        esp_event_handler_unregister(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, app_ble_otp_event_handler);
        esp_event_loop_delete_default();
    }
}
