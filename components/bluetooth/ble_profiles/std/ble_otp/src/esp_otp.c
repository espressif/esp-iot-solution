/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_event.h"
#include "esp_log.h"

#include "esp_otp.h"

ESP_EVENT_DEFINE_BASE(BLE_OTP_EVENTS);

static const char *TAG = "esp_otp";

typedef enum {
    OTP_DIR_NONE = 0,
    OTP_DIR_READ,
    OTP_DIR_WRITE,
} otp_transfer_dir_t;

typedef enum {
    OTP_OTS_STATE_IDLE = 0,
    OTP_OTS_STATE_DISCOVERING,
    OTP_OTS_STATE_DISCOVERED,
    OTP_OTS_STATE_FAILED,
} otp_ots_state_t;

typedef enum {
    OTP_OBJSEL_NONE = 0,
    OTP_OBJSEL_SELECTING,
    OTP_OBJSEL_SELECTED,
    OTP_OBJSEL_ERROR,
} otp_object_select_state_t;

typedef enum {
    OTP_META_UNKNOWN = 0,
    OTP_META_VALID,
    OTP_META_STALE,
} otp_metadata_state_t;

typedef enum {
    OTP_STATE_IDLE = 0,
    OTP_STATE_OACP_PENDING,
    OTP_STATE_OACP_ACCEPTED,
    OTP_STATE_OACP_REJECTED,
    OTP_STATE_COC_CONNECTING,
    OTP_STATE_TRANSFERRING,
    OTP_STATE_EOF,
    OTP_STATE_ABORTING,
    OTP_STATE_EXECUTING,
    OTP_STATE_COMPLETED,
    OTP_STATE_ERROR,
} otp_transfer_state_t;

typedef enum {
    OTP_EOF_NONE = 0,
    OTP_EOF_BY_SIZE,
    OTP_EOF_BY_APP,
} otp_eof_reason_t;

typedef enum {
    OTP_COC_IDLE = 0,
    OTP_COC_CONNECTING,
    OTP_COC_CONNECTED,
    OTP_COC_DISCONNECTING,
    OTP_COC_ERROR,
} otp_coc_state_t;

typedef enum {
    OTP_SESSION_IDLE = 0,
    OTP_SESSION_BUSY,
    OTP_SESSION_ERROR,
} otp_session_state_t;

typedef struct {
    uint16_t conn_id;
    uint16_t coc_cid;
    esp_ble_conn_l2cap_coc_chan_t coc_chan;

    uint64_t object_id;
    uint32_t object_size;
    uint32_t object_offset;

    otp_transfer_dir_t direction;
    uint32_t tx_len;
    uint32_t rx_len;

    otp_transfer_state_t state;
    otp_eof_reason_t eof_reason;

    uint8_t coc_connected      : 1;
    uint8_t eof_reported       : 1;
    uint8_t abort_requested    : 1;
    uint8_t execute_requested  : 1;

    uint8_t oacp_opcode;
    uint8_t oacp_result;

    esp_err_t last_error;
    void *user_ctx;

    otp_ots_state_t ots_state;
    otp_object_select_state_t objsel_state;
    otp_metadata_state_t meta_state;
    otp_coc_state_t coc_state;
    otp_session_state_t session_state;

    bool feature_valid;
    esp_ble_ots_feature_t ots_feature;

    bool write_long_not_supported;
    bool dir_listing_pending;
} otp_transfer_ctx_t;

#if defined(CONFIG_BT_NIMBLE_MAX_CONNECTIONS)
#define OTP_MAX_CONNECTIONS CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#elif defined(CONFIG_BT_ACL_CONNECTIONS)
#define OTP_MAX_CONNECTIONS CONFIG_BT_ACL_CONNECTIONS
#else
#define OTP_MAX_CONNECTIONS 1
#endif

typedef struct {
    bool inited;
    esp_ble_otp_config_t config;
    bool l2cap_server_created;

    esp_err_t (*oacp_cb)(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx);
    void *oacp_ctx;
    esp_err_t (*olcp_cb)(uint8_t op_code, const uint8_t *parameter, uint16_t param_len, void *ctx);
    void *olcp_ctx;

    otp_transfer_ctx_t transfers[OTP_MAX_CONNECTIONS];
    SemaphoreHandle_t lock;
} otp_ctx_t;

static otp_ctx_t s_otp = {0};

#define OTP_LOCK_TIMEOUT_MS 5000

static bool otp_lock(void)
{
    if (!s_otp.lock) {
        ESP_LOGE(TAG, "otp_lock: lock not initialized");
        return false;
    }
    if (xSemaphoreTake(s_otp.lock, pdMS_TO_TICKS(OTP_LOCK_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "otp_lock timeout (%u ms) - possible deadlock", OTP_LOCK_TIMEOUT_MS);
        return false;
    }
    return true;
}

static void otp_unlock(void)
{
    if (s_otp.lock) {
        xSemaphoreGive(s_otp.lock);
    }
}

static bool otp_client_ready(void)
{
    return s_otp.inited && s_otp.config.role == BLE_OTP_ROLE_CLIENT;
}

static bool otp_server_ready(void)
{
    return s_otp.inited && s_otp.config.role == BLE_OTP_ROLE_SERVER;
}

static esp_err_t otp_write_chr16(uint16_t uuid16, const uint8_t *data, uint16_t data_len)
{
    if (!data || data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t inbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = uuid16,
        },
        .data = (uint8_t *)data,
        .data_len = data_len,
    };

    return esp_ble_conn_write(&inbuff);
}

static esp_err_t otp_read_chr16(uint16_t uuid16, uint8_t *out_buf, uint16_t buf_len, uint16_t *out_len)
{
    if (!out_buf || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_data_t outbuff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = uuid16,
        },
        .data = out_buf,
        .data_len = buf_len,
    };

    esp_err_t rc = esp_ble_conn_read(&outbuff);
    if (rc == ESP_OK && out_len) {
        *out_len = outbuff.data_len;
    }
    return rc;
}

static esp_err_t otp_start_transfer(otp_transfer_ctx_t *ctx);

static void otp_post_event(int32_t id, const esp_ble_otp_event_data_t *event_data, size_t data_len)
{
    if (!event_data || data_len == 0) {
        return;
    }
    esp_event_post(BLE_OTP_EVENTS, id, event_data, data_len, portMAX_DELAY);
}

static void otp_post_oacp_started(uint16_t conn_handle, uint8_t op_code)
{
    esp_ble_otp_event_data_t evt = {0};
    evt.oacp_started.conn_handle = conn_handle;
    evt.oacp_started.op_code = op_code;
    otp_post_event(BLE_OTP_EVENT_OACP_STARTED, &evt, sizeof(evt));
}

static void otp_post_oacp_aborted(uint16_t conn_handle, uint8_t op_code)
{
    esp_ble_otp_event_data_t evt = {0};
    evt.oacp_aborted.conn_handle = conn_handle;
    evt.oacp_aborted.op_code = op_code;
    otp_post_event(BLE_OTP_EVENT_OACP_ABORTED, &evt, sizeof(evt));
}

static bool otp_name_is_valid(const uint8_t *name, size_t name_len)
{
    if (!name || name_len == 0) {
        return false;
    }
    for (size_t i = 0; i < name_len; i++) {
        uint8_t ch = name[i];
        if (ch < 0x20 || ch == 0x7F) {
            return false;
        }
    }
    return true;
}

static void otp_u64_to_id(uint64_t value, esp_ble_ots_id_t *out_id)
{
    if (!out_id) {
        return;
    }
    memset(out_id, 0, sizeof(*out_id));
    memcpy(out_id->id, &value, sizeof(out_id->id));
}

static uint64_t otp_id_to_u64(const esp_ble_ots_id_t *id)
{
    uint64_t value = 0;
    if (!id) {
        return value;
    }
    memcpy(&value, id->id, sizeof(id->id));
    return value;
}

static void otp_ctx_init(otp_transfer_ctx_t *ctx, uint16_t conn_id)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->conn_id = conn_id;
    ctx->object_size = BLE_OTP_OBJECT_SIZE_UNKNOWN;
}

static otp_transfer_ctx_t *otp_ctx_get(uint16_t conn_id, bool create)
{
    if (conn_id == 0) {
        return NULL;
    }

    for (size_t i = 0; i < OTP_MAX_CONNECTIONS; i++) {
        if (s_otp.transfers[i].conn_id == conn_id) {
            return &s_otp.transfers[i];
        }
    }

    if (!create) {
        return NULL;
    }

    for (size_t i = 0; i < OTP_MAX_CONNECTIONS; i++) {
        if (s_otp.transfers[i].conn_id == 0) {
            otp_ctx_init(&s_otp.transfers[i], conn_id);
            return &s_otp.transfers[i];
        }
    }

    return NULL;
}

static void otp_fill_transfer_info(const otp_transfer_ctx_t *ctx, esp_ble_otp_transfer_info_t *info)
{
    if (!ctx || !info) {
        return;
    }
    memset(info, 0, sizeof(*info));
    info->channel = ctx->coc_chan;
    info->conn_handle = ctx->conn_id;
    info->psm = s_otp.config.psm;
    info->mtu = s_otp.config.l2cap_coc_mtu;
    info->is_read = (ctx->direction == OTP_DIR_READ);
    otp_u64_to_id(ctx->object_id, &info->object_id);
}

static void otp_post_transfer_error(otp_transfer_ctx_t *ctx, esp_err_t error)
{
    if (!ctx) {
        return;
    }
    ctx->last_error = error;
    ctx->state = OTP_STATE_ERROR;
    ctx->session_state = OTP_SESSION_ERROR;
    esp_ble_otp_event_data_t evt = {0};
    otp_fill_transfer_info(ctx, &evt.transfer_error.transfer_info);
    evt.transfer_error.error = error;
    otp_post_event(BLE_OTP_EVENT_TRANSFER_ERROR, &evt, sizeof(evt));
}

static void otp_try_complete(otp_transfer_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->state == OTP_STATE_COMPLETED ||
            ctx->eof_reason == OTP_EOF_NONE ||
            ctx->oacp_result != BLE_OTS_OACP_RSP_SUCCESS) {
        return;
    }
    if (ctx->direction != OTP_DIR_READ && ctx->direction != OTP_DIR_WRITE) {
        return;
    }
    ctx->state = OTP_STATE_COMPLETED;
    ctx->session_state = OTP_SESSION_IDLE;
    esp_ble_otp_event_data_t evt = {0};
    otp_fill_transfer_info(ctx, &evt.transfer_complete.transfer_info);
    evt.transfer_complete.success = true;
    otp_post_event(BLE_OTP_EVENT_TRANSFER_COMPLETE, &evt, sizeof(evt));
}

static int otp_l2cap_event_handler(esp_ble_conn_l2cap_coc_event_t *event, void *arg)
{
    (void)arg;
    if (!event) {
        return 0;
    }

    otp_transfer_ctx_t *ctx = NULL;
    esp_ble_otp_event_data_t otp_event = {0};

    switch (event->type) {
    case ESP_BLE_CONN_L2CAP_COC_EVENT_CONNECTED:
        ESP_LOGI(TAG, "L2CAP CoC connected, handle=%u, status=%d",
                 event->connect.conn_handle, event->connect.status);
        if (event->connect.status != 0) {
            otp_event.transfer_error.error = ESP_FAIL;
            otp_post_event(BLE_OTP_EVENT_TRANSFER_ERROR, &otp_event, sizeof(otp_event));
            break;
        }

        if (!otp_lock()) {
            ESP_LOGW(TAG, "L2CAP connected: lock timeout for handle=%u", event->connect.conn_handle);
            break;
        }
        ctx = otp_ctx_get(event->connect.conn_handle, true);
        if (ctx) {
            esp_ble_conn_l2cap_coc_chan_info_t chan_info = {0};
            if (esp_ble_conn_l2cap_coc_get_chan_info(event->connect.chan, &chan_info) == ESP_OK) {
                ctx->coc_cid = chan_info.dcid;
            }
            ctx->coc_chan = event->connect.chan;
            ctx->coc_connected = 1;
            ctx->state = OTP_STATE_TRANSFERRING;
            ctx->coc_state = OTP_COC_CONNECTED;
            otp_fill_transfer_info(ctx, &otp_event.transfer_channel_connected.transfer_info);
        }
        otp_unlock();

        if (ctx) {
            otp_post_event(BLE_OTP_EVENT_TRANSFER_CHANNEL_CONNECTED, &otp_event, sizeof(otp_event));
        }
        break;
    case ESP_BLE_CONN_L2CAP_COC_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "L2CAP CoC disconnected, handle=%u", event->disconnect.conn_handle);
        if (!otp_lock()) {
            ESP_LOGW(TAG, "L2CAP disconnected: lock timeout for handle=%u", event->disconnect.conn_handle);
            break;
        }
        ctx = otp_ctx_get(event->disconnect.conn_handle, false);
        if (ctx) {
            ctx->coc_connected = 0;
            ctx->coc_chan = NULL;
            ctx->coc_state = OTP_COC_IDLE;
            if (ctx->direction == OTP_DIR_READ) {
                if (ctx->eof_reason != OTP_EOF_BY_SIZE &&
                        ctx->object_size != BLE_OTP_OBJECT_SIZE_UNKNOWN &&
                        ctx->tx_len >= ctx->object_size) {
                    ctx->eof_reason = OTP_EOF_BY_SIZE;
                    ctx->eof_reported = 1;
                    ctx->state = OTP_STATE_EOF;
                    ctx->session_state = OTP_SESSION_BUSY;
                }
                if (ctx->eof_reason == OTP_EOF_BY_SIZE) {
                    otp_try_complete(ctx);
                } else {
                    otp_post_transfer_error(ctx, ESP_FAIL);
                }
            } else if (ctx->direction == OTP_DIR_WRITE) {
                if (ctx->eof_reason == OTP_EOF_BY_APP) {
                    otp_try_complete(ctx);
                } else if (!ctx->eof_reported && !ctx->abort_requested) {
                    otp_post_transfer_error(ctx, ESP_FAIL);
                }
            }
            otp_fill_transfer_info(ctx, &otp_event.transfer_channel_disconnected.transfer_info);
        }
        otp_unlock();

        if (ctx) {
            otp_post_event(BLE_OTP_EVENT_TRANSFER_CHANNEL_DISCONNECTED, &otp_event, sizeof(otp_event));
        }
        break;
    case ESP_BLE_CONN_L2CAP_COC_EVENT_ACCEPT:
        ESP_LOGI(TAG, "L2CAP CoC accept, handle=%u, peer_sdu_size=%u",
                 event->accept.conn_handle, event->accept.peer_sdu_size);
        if (!otp_lock()) {
            ESP_LOGW(TAG, "L2CAP CoC accept: lock timeout for handle=%u", event->accept.conn_handle);
            break;
        }
        ctx = otp_ctx_get(event->accept.conn_handle, true);
        if (ctx) {
            ctx->coc_chan = event->accept.chan;
            ctx->state = OTP_STATE_COC_CONNECTING;
            ctx->coc_state = OTP_COC_CONNECTING;
        }
        otp_unlock();
        if (esp_ble_conn_l2cap_coc_accept(event->accept.conn_handle, event->accept.peer_sdu_size,
                                          event->accept.chan) != ESP_OK) {
            ESP_LOGW(TAG, "L2CAP CoC accept failed");
        }
        break;
    case ESP_BLE_CONN_L2CAP_COC_EVENT_DATA_RECEIVED:
        if (!otp_lock()) {
            ESP_LOGW(TAG, "L2CAP CoC data received: lock timeout for handle=%u", event->receive.conn_handle);
            break;
        }
        ctx = otp_ctx_get(event->receive.conn_handle, false);
        if (ctx) {
            if (ctx->direction == OTP_DIR_READ || ctx->direction == OTP_DIR_WRITE) {
                uint32_t sdu_len = event->receive.sdu.len;
                uint32_t new_rx_len = ctx->rx_len + sdu_len;
                /* Check overflow before addition - reject if would wrap */
                if (new_rx_len < ctx->rx_len) {
                    otp_post_transfer_error(ctx, ESP_ERR_INVALID_SIZE);
                    otp_unlock();
                    esp_ble_conn_l2cap_coc_recv_ready(event->receive.chan, s_otp.config.l2cap_coc_mtu);
                    break;
                }
                /* Check size limit before addition when object size is known */
                if (ctx->object_size != BLE_OTP_OBJECT_SIZE_UNKNOWN && new_rx_len > ctx->object_size) {
                    otp_post_transfer_error(ctx, ESP_ERR_INVALID_SIZE);
                    otp_unlock();
                    esp_ble_conn_l2cap_coc_recv_ready(event->receive.chan, s_otp.config.l2cap_coc_mtu);
                    break;
                }
                ctx->rx_len = new_rx_len;
            }
            otp_fill_transfer_info(ctx, &otp_event.transfer_data_received.transfer_info);
            otp_event.transfer_data_received.total_len = event->receive.sdu.len;

            if (event->receive.sdu.len > 0 && event->receive.sdu.data) {
                uint16_t offset = 0;
                uint16_t remaining = event->receive.sdu.len;
                while (remaining > 0) {
                    uint16_t copy_len = (remaining > BLE_OTP_EVENT_DATA_MAX_LEN) ?
                                        BLE_OTP_EVENT_DATA_MAX_LEN : remaining;
                    memcpy(otp_event.transfer_data_received.data,
                           event->receive.sdu.data + offset, copy_len);
                    otp_event.transfer_data_received.data_len = copy_len;
                    otp_event.transfer_data_received.chunk_offset = offset;
                    otp_post_event(BLE_OTP_EVENT_TRANSFER_DATA_RECEIVED, &otp_event, sizeof(otp_event));
                    offset += copy_len;
                    remaining -= copy_len;
                }
            } else {
                otp_event.transfer_data_received.data_len = 0;
                otp_event.transfer_data_received.chunk_offset = 0;
                otp_post_event(BLE_OTP_EVENT_TRANSFER_DATA_RECEIVED, &otp_event, sizeof(otp_event));
            }

            if (ctx->direction == OTP_DIR_READ && ctx->object_size != BLE_OTP_OBJECT_SIZE_UNKNOWN) {
                if (ctx->rx_len > ctx->object_size) {
                    otp_post_transfer_error(ctx, ESP_ERR_INVALID_SIZE);
                } else if (ctx->rx_len == ctx->object_size && !ctx->eof_reported) {
                    ctx->eof_reason = OTP_EOF_BY_SIZE;
                    ctx->eof_reported = 1;
                    ctx->state = OTP_STATE_EOF;
                    ctx->session_state = OTP_SESSION_BUSY;
                    otp_fill_transfer_info(ctx, &otp_event.transfer_eof.transfer_info);
                    otp_post_event(BLE_OTP_EVENT_TRANSFER_EOF, &otp_event, sizeof(otp_event));
                    otp_try_complete(ctx);
                }
            } else if (ctx->direction == OTP_DIR_WRITE && ctx->object_size != BLE_OTP_OBJECT_SIZE_UNKNOWN) {
                if (ctx->rx_len > ctx->object_size) {
                    otp_post_transfer_error(ctx, ESP_ERR_INVALID_SIZE);
                } else if (ctx->rx_len == ctx->object_size && !ctx->eof_reported) {
                    ctx->eof_reason = OTP_EOF_BY_SIZE;
                    ctx->eof_reported = 1;
                    ctx->state = OTP_STATE_EOF;
                    ctx->session_state = OTP_SESSION_BUSY;
                    otp_fill_transfer_info(ctx, &otp_event.transfer_eof.transfer_info);
                    otp_post_event(BLE_OTP_EVENT_TRANSFER_EOF, &otp_event, sizeof(otp_event));
                    otp_try_complete(ctx);
                }
            }
        }
        otp_unlock();
        esp_ble_conn_l2cap_coc_recv_ready(event->receive.chan, s_otp.config.l2cap_coc_mtu);
        break;
    case ESP_BLE_CONN_L2CAP_COC_EVENT_TX_UNSTALLED:
    default:
        break;
    }

    return 0;
}

static void otp_handle_oacp_response(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    if (!data || len < 3) {
        return;
    }
    if (data[0] != BLE_OTS_OACP_RESPONSE) {
        return;
    }

    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_id, true);
    esp_ble_otp_event_data_t otp_event = {0};
    uint8_t req_op = data[1];
    uint8_t rsp_code = data[2];
    otp_event.oacp_response.response.req_op_code = req_op;
    otp_event.oacp_response.response.rsp_code = rsp_code;
    if (len > 3) {
        size_t copy_len = MIN(len - 3, sizeof(otp_event.oacp_response.response.rsp_parameter));
        memcpy(otp_event.oacp_response.response.rsp_parameter, &data[3], copy_len);
    }
    otp_post_event(BLE_OTP_EVENT_OACP_RESPONSE, &otp_event, sizeof(otp_event));

    if (!ctx) {
        return;
    }

    ctx->oacp_opcode = req_op;
    ctx->oacp_result = rsp_code;

    if (req_op == BLE_OTS_OACP_ABORT && rsp_code == BLE_OTS_OACP_RSP_SUCCESS) {
        ctx->abort_requested = 0;
        ctx->state = OTP_STATE_ERROR;
        ctx->session_state = OTP_SESSION_ERROR;
        otp_post_oacp_aborted(conn_id, req_op);
        return;
    }

    if ((req_op == BLE_OTS_OACP_READ || req_op == BLE_OTS_OACP_WRITE) &&
            rsp_code == BLE_OTS_OACP_RSP_SUCCESS) {
        ctx->state = OTP_STATE_OACP_ACCEPTED;
        ctx->last_error = ESP_OK;
        if (otp_start_transfer(ctx) != ESP_OK) {
            otp_post_transfer_error(ctx, ESP_FAIL);
        } else {
            otp_try_complete(ctx);
        }
        return;
    }

    if (req_op == BLE_OTS_OACP_CREATE && rsp_code == BLE_OTS_OACP_RSP_SUCCESS) {
        ctx->objsel_state = OTP_OBJSEL_SELECTED;
        ctx->meta_state = OTP_META_UNKNOWN;
        ctx->object_id = 0;
        ctx->object_size = BLE_OTP_OBJECT_SIZE_UNKNOWN;
        ctx->session_state = OTP_SESSION_IDLE;
        return;
    }

    if (req_op == BLE_OTS_OACP_DELETE && rsp_code == BLE_OTS_OACP_RSP_SUCCESS) {
        ctx->objsel_state = OTP_OBJSEL_NONE;
        ctx->meta_state = OTP_META_UNKNOWN;
        ctx->object_id = 0;
        ctx->object_size = BLE_OTP_OBJECT_SIZE_UNKNOWN;
        ctx->session_state = OTP_SESSION_IDLE;
        return;
    }

    if (req_op == BLE_OTS_OACP_EXECUTE) {
        ctx->state = (rsp_code == BLE_OTS_OACP_RSP_SUCCESS) ? OTP_STATE_COMPLETED : OTP_STATE_ERROR;
        ctx->session_state = (rsp_code == BLE_OTS_OACP_RSP_SUCCESS) ? OTP_SESSION_IDLE : OTP_SESSION_ERROR;
        return;
    }

    if (rsp_code != BLE_OTS_OACP_RSP_SUCCESS) {
        ctx->state = OTP_STATE_OACP_REJECTED;
        ctx->last_error = ESP_FAIL;
        otp_post_transfer_error(ctx, ESP_FAIL);
    }
}

static void otp_handle_olcp_response(uint16_t conn_id, const uint8_t *data, uint16_t len)
{
    if (!data || len < 3) {
        return;
    }
    if (data[0] != BLE_OTS_OLCP_RESPONSE) {
        return;
    }

    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_id, true);
    esp_ble_otp_event_data_t otp_event = {0};
    otp_event.olcp_response.response.req_op_code = data[1];
    otp_event.olcp_response.response.rsp_code = data[2];
    if (len > 3) {
        size_t copy_len = MIN(len - 3, sizeof(otp_event.olcp_response.response.rsp_parameter));
        memcpy(otp_event.olcp_response.response.rsp_parameter, &data[3], copy_len);
    }
    if (otp_event.olcp_response.response.req_op_code == BLE_OTS_OLCP_REQ_NUM_OF_OBJ && len >= 7) {
        memcpy(&otp_event.olcp_response.response.num_objects, &data[3], sizeof(uint32_t));
    }
    otp_post_event(BLE_OTP_EVENT_OLCP_RESPONSE, &otp_event, sizeof(otp_event));

    if (!ctx) {
        return;
    }

    switch (otp_event.olcp_response.response.req_op_code) {
    case BLE_OTS_OLCP_FIRST:
    case BLE_OTS_OLCP_LAST:
    case BLE_OTS_OLCP_PREVIOUS:
    case BLE_OTS_OLCP_NEXT:
    case BLE_OTS_OLCP_GO_TO:
        if (otp_event.olcp_response.response.rsp_code == BLE_OTS_OLCP_RSP_SUCCESS) {
            ctx->objsel_state = OTP_OBJSEL_SELECTED;
            ctx->meta_state = OTP_META_UNKNOWN;
            if (ctx->dir_listing_pending) {
                esp_ble_ots_size_t size = {0};
                uint16_t out_len = 0;
                if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_SIZE, (uint8_t *)&size,
                                   sizeof(size), &out_len) == ESP_OK) {
                    ctx->meta_state = OTP_META_VALID;
                    ctx->dir_listing_pending = false;
                    esp_ble_otp_client_read_object(conn_id, 0, size.current_size);
                } else {
                    ctx->dir_listing_pending = false;
                }
            }
        } else {
            ctx->objsel_state = OTP_OBJSEL_ERROR;
        }
        break;
    default:
        break;
    }
}

static void otp_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        if (s_otp.config.role == BLE_OTP_ROLE_SERVER && !s_otp.l2cap_server_created) {
            esp_ble_conn_l2cap_coc_create_server(s_otp.config.psm, s_otp.config.l2cap_coc_mtu,
                                                 otp_l2cap_event_handler, NULL);
            s_otp.l2cap_server_created = true;
        }
        if (s_otp.config.role == BLE_OTP_ROLE_CLIENT) {
            uint16_t conn_handle = 0;
            if (esp_ble_conn_get_conn_handle(&conn_handle) == ESP_OK) {
                otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
                if (ctx) {
                    ctx->objsel_state = OTP_OBJSEL_NONE;
                    ctx->meta_state = OTP_META_UNKNOWN;
                }
            }
        }
        break;
    case ESP_BLE_CONN_EVENT_DISC_COMPLETE:
        if (s_otp.config.role == BLE_OTP_ROLE_CLIENT && s_otp.config.auto_discover_ots) {
            uint16_t conn_handle = 0;
            if (esp_ble_conn_get_conn_handle(&conn_handle) == ESP_OK) {
                esp_ble_otp_client_discover_ots(conn_handle);
            }
        }
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED: {
        if (otp_lock()) {
            for (size_t i = 0; i < OTP_MAX_CONNECTIONS; i++) {
                if (s_otp.transfers[i].conn_id != 0) {
                    otp_ctx_init(&s_otp.transfers[i], 0);
                }
            }
            otp_unlock();
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE: {
        if (s_otp.config.role != BLE_OTP_ROLE_CLIENT) {
            break;
        }
        esp_ble_conn_data_t *conn_data = (esp_ble_conn_data_t *)event_data;
        if (!conn_data || conn_data->type != BLE_CONN_UUID_TYPE_16) {
            break;
        }
        switch (conn_data->uuid.uuid16) {
        case BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT:
            otp_handle_oacp_response(conn_data->write_conn_id, conn_data->data, conn_data->data_len);
            break;
        case BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT:
            otp_handle_olcp_response(conn_data->write_conn_id, conn_data->data, conn_data->data_len);
            break;
        case BLE_OTS_CHR_UUID16_OBJECT_CHANGED: {
            if (conn_data->data_len >= sizeof(esp_ble_ots_change_t)) {
                esp_ble_otp_event_data_t otp_event = {0};
                memcpy(&otp_event.object_changed.change, conn_data->data, sizeof(esp_ble_ots_change_t));
                otp_post_event(BLE_OTP_EVENT_OBJECT_CHANGED, &otp_event, sizeof(otp_event));
                otp_transfer_ctx_t *ctx = otp_ctx_get(conn_data->write_conn_id, false);
                if (ctx) {
                    ctx->meta_state = OTP_META_STALE;
                }
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void otp_ots_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_OTS_EVENTS) {
        return;
    }

    if (s_otp.config.role != BLE_OTP_ROLE_SERVER) {
        return;
    }

    switch (id) {
    case BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT: {
        const esp_ble_ots_oacp_t *oacp = (const esp_ble_ots_oacp_t *)event_data;
        if (oacp) {
            uint16_t conn_id = 0;
            otp_transfer_ctx_t *ctx = NULL;
            if (esp_ble_conn_get_conn_handle(&conn_id) == ESP_OK) {
                ctx = otp_ctx_get(conn_id, true);
            }
            if (ctx) {
                ctx->oacp_opcode = oacp->op_code;
                ctx->state = OTP_STATE_OACP_PENDING;
                if (oacp->op_code == BLE_OTS_OACP_READ) {
                    uint32_t offset = 0;
                    uint32_t length = 0;
                    memcpy(&offset, oacp->parameter, sizeof(offset));
                    memcpy(&length, &oacp->parameter[sizeof(offset)], sizeof(length));
                    ctx->direction = OTP_DIR_READ;
                    ctx->rx_len = 0;
                    ctx->tx_len = 0;
                    ctx->object_offset = offset;
                    if (length != 0) {
                        ctx->object_size = length;
                    } else {
                        esp_ble_ots_size_t size = {0};
                        if (esp_ble_ots_get_size(&size) == ESP_OK) {
                            ctx->object_size = size.current_size;
                        } else {
                            ctx->object_size = BLE_OTP_OBJECT_SIZE_UNKNOWN;
                        }
                    }
                    ctx->eof_reason = OTP_EOF_NONE;
                    ctx->eof_reported = 0;
                } else if (oacp->op_code == BLE_OTS_OACP_WRITE) {
                    uint32_t offset = 0;
                    uint32_t length = 0;
                    memcpy(&offset, oacp->parameter, sizeof(offset));
                    memcpy(&length, &oacp->parameter[sizeof(offset)], sizeof(length));
                    ctx->direction = OTP_DIR_WRITE;
                    ctx->rx_len = 0;
                    ctx->tx_len = 0;
                    ctx->object_offset = offset;
                    ctx->object_size = length;
                    ctx->eof_reason = OTP_EOF_NONE;
                    ctx->eof_reported = 0;
                }
            }
        }
        if (oacp && s_otp.oacp_cb) {
            s_otp.oacp_cb(oacp->op_code, oacp->parameter, sizeof(oacp->parameter), s_otp.oacp_ctx);
        }
        break;
    }
    case BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT: {
        const esp_ble_ots_olcp_t *olcp = (const esp_ble_ots_olcp_t *)event_data;
        if (olcp && s_otp.olcp_cb) {
            s_otp.olcp_cb(olcp->op_code, olcp->parameter, sizeof(olcp->parameter), s_otp.olcp_ctx);
        }
        break;
    }
    default:
        break;
    }
}

esp_err_t esp_ble_otp_client_discover_ots(uint16_t conn_handle)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (conn_handle == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    otp_ctx_get(conn_handle, true);

    uint8_t cccd_indicate[2] = {0x02, 0x00};
    esp_ble_conn_data_t sub = {
        .type = BLE_CONN_UUID_TYPE_16,
        .data = cccd_indicate,
        .data_len = sizeof(cccd_indicate),
    };

    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (ctx) {
        ctx->ots_state = OTP_OTS_STATE_DISCOVERING;
    }

    sub.uuid.uuid16 = BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT;
    esp_ble_conn_subscribe(ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);
    sub.uuid.uuid16 = BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT;
    esp_ble_conn_subscribe(ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);
    sub.uuid.uuid16 = BLE_OTS_CHR_UUID16_OBJECT_CHANGED;
    esp_ble_conn_subscribe(ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);

    esp_ble_ots_feature_t feature = {0};
    if (esp_ble_otp_client_read_feature(conn_handle, &feature) == ESP_OK) {
        if (ctx) {
            ctx->ots_feature = feature;
            ctx->feature_valid = true;
            ctx->ots_state = OTP_OTS_STATE_DISCOVERED;
        }
        esp_ble_otp_event_data_t otp_event = {0};
        memcpy(&otp_event.ots_discovered.feature, &feature, sizeof(feature));
        otp_post_event(BLE_OTP_EVENT_OTS_DISCOVERED, &otp_event, sizeof(otp_event));
    } else {
        if (ctx) {
            ctx->ots_state = OTP_OTS_STATE_FAILED;
        }
        esp_ble_otp_event_data_t otp_event = {0};
        otp_post_event(BLE_OTP_EVENT_OTS_DISCOVERY_FAILED, &otp_event, sizeof(otp_event));
    }

    return ESP_OK;
}

esp_err_t esp_ble_otp_client_read_feature(uint16_t conn_handle, esp_ble_ots_feature_t *feature)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!feature) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t out_len = 0;
    esp_err_t rc = otp_read_chr16(BLE_OTS_CHR_UUID16_OTS_FEATURE, (uint8_t *)feature,
                                  sizeof(esp_ble_ots_feature_t), &out_len);
    if (rc == ESP_OK && out_len < sizeof(esp_ble_ots_feature_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return rc;
}

esp_err_t esp_ble_otp_client_select_first(uint16_t conn_handle)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    uint8_t buf[1] = { BLE_OTS_OLCP_FIRST };
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK && ctx) {
        ctx->objsel_state = OTP_OBJSEL_SELECTING;
    }
    return rc;
}

esp_err_t esp_ble_otp_client_select_last(uint16_t conn_handle)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    uint8_t buf[1] = { BLE_OTS_OLCP_LAST };
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK && ctx) {
        ctx->objsel_state = OTP_OBJSEL_SELECTING;
    }
    return rc;
}

esp_err_t esp_ble_otp_client_select_previous(uint16_t conn_handle)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    uint8_t buf[1] = { BLE_OTS_OLCP_PREVIOUS };
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK && ctx) {
        ctx->objsel_state = OTP_OBJSEL_SELECTING;
    }
    return rc;
}

esp_err_t esp_ble_otp_client_select_next(uint16_t conn_handle)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    uint8_t buf[1] = { BLE_OTS_OLCP_NEXT };
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK && ctx) {
        ctx->objsel_state = OTP_OBJSEL_SELECTING;
    }
    return rc;
}

esp_err_t esp_ble_otp_client_select_by_id(uint16_t conn_handle, const esp_ble_ots_id_t *object_id)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!object_id) {
        return ESP_ERR_INVALID_ARG;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED || !ctx->feature_valid ||
            !ctx->ots_feature.olcp.goto_op) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint8_t buf[1 + sizeof(object_id->id)] = { BLE_OTS_OLCP_GO_TO };
    memcpy(&buf[1], object_id->id, sizeof(object_id->id));
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK && ctx) {
        ctx->objsel_state = OTP_OBJSEL_SELECTING;
        ctx->meta_state = OTP_META_UNKNOWN;
    }
    return rc;
}

esp_err_t esp_ble_otp_client_select_by_index(uint16_t conn_handle, uint32_t index)
{
    (void)conn_handle;
    (void)index;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_ble_otp_client_set_sort_order(uint16_t conn_handle, uint8_t sort_order)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t buf[2] = { BLE_OTS_OLCP_ORDER, sort_order };
    return otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
}

esp_err_t esp_ble_otp_client_request_num_objects(uint16_t conn_handle)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t buf[1] = { BLE_OTS_OLCP_REQ_NUM_OF_OBJ };
    return otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
}

esp_err_t esp_ble_otp_client_clear_mark(uint16_t conn_handle)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t buf[1] = { BLE_OTS_OLCP_CLEAR_MARK };
    return otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT, buf, sizeof(buf));
}

esp_err_t esp_ble_otp_client_set_filter(uint16_t conn_handle, const esp_ble_ots_filter_t *filter)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!filter) {
        return ESP_ERR_INVALID_ARG;
    }
    return otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LIST_FILTER, (const uint8_t *)filter,
                           sizeof(esp_ble_ots_filter_t));
}

esp_err_t esp_ble_otp_client_read_object_info(uint16_t conn_handle, esp_ble_otp_object_info_t *object_info)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!object_info) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(object_info, 0, sizeof(*object_info));

    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    uint16_t out_len = 0;
    esp_err_t rc;

    /* Mandatory object metadata characteristics - fail if any read fails */
    rc = otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_NAME, object_info->object_name,
                        sizeof(object_info->object_name), &out_len);
    if (rc != ESP_OK) {
        return rc;
    }
    rc = otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_TYPE, (uint8_t *)&object_info->object_type,
                        sizeof(object_info->object_type), &out_len);
    if (rc != ESP_OK) {
        return rc;
    }
    rc = otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_SIZE, (uint8_t *)&object_info->object_size,
                        sizeof(object_info->object_size), &out_len);
    if (rc != ESP_OK) {
        return rc;
    }
    rc = otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_ID, (uint8_t *)&object_info->object_id,
                        sizeof(object_info->object_id), &out_len);
    if (rc != ESP_OK) {
        return rc;
    }
    rc = otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_PROP, (uint8_t *)&object_info->object_prop,
                        sizeof(object_info->object_prop), &out_len);
    if (rc != ESP_OK) {
        return rc;
    }
    /* Optional characteristics - ignore failures */
#ifdef CONFIG_BLE_OTS_FIRST_CREATED_CHARACTERISTIC_ENABLE
    otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_FIRST_CREATED, (uint8_t *)&object_info->first_created,
                   sizeof(object_info->first_created), &out_len);
#endif
#ifdef CONFIG_BLE_OTS_LAST_MODIFIED_CHARACTERISTIC_ENABLE
    otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_LAST_MODIFIED, (uint8_t *)&object_info->last_modified,
                   sizeof(object_info->last_modified), &out_len);
#endif

    esp_ble_otp_event_data_t otp_event = {0};
    memcpy(&otp_event.object_selected.object_info, object_info, sizeof(*object_info));
    otp_post_event(BLE_OTP_EVENT_OBJECT_SELECTED, &otp_event, sizeof(otp_event));

    if (ctx) {
        ctx->object_id = otp_id_to_u64(&object_info->object_id);
        ctx->object_size = object_info->object_size.current_size;
        ctx->meta_state = OTP_META_VALID;
    }

    return ESP_OK;
}

esp_err_t esp_ble_otp_client_write_name(uint16_t conn_handle, const uint8_t *name, size_t name_len)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!otp_name_is_valid(name, name_len)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (name_len > BLE_OTS_ATT_VAL_LEN) {
        if (ctx) {
            ctx->write_long_not_supported = true;
        }
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_NAME, name, (uint16_t)name_len);
    if (rc == ESP_OK) {
        ctx->meta_state = OTP_META_VALID;
    }
    return rc;
}

esp_err_t esp_ble_otp_client_write_prop(uint16_t conn_handle, const esp_ble_ots_prop_t *prop)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!prop) {
        return ESP_ERR_INVALID_ARG;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_PROP, (const uint8_t *)prop,
                                   sizeof(esp_ble_ots_prop_t));
    if (rc == ESP_OK) {
        ctx->meta_state = OTP_META_VALID;
    }
    return rc;
}

#ifdef CONFIG_BLE_OTS_FIRST_CREATED_CHARACTERISTIC_ENABLE
esp_err_t esp_ble_otp_client_write_first_created(uint16_t conn_handle, const esp_ble_ots_utc_t *utc)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!utc) {
        return ESP_ERR_INVALID_ARG;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_FIRST_CREATED, (const uint8_t *)utc,
                                   sizeof(esp_ble_ots_utc_t));
    if (rc == ESP_OK) {
        ctx->meta_state = OTP_META_VALID;
    }
    return rc;
}
#endif

#ifdef CONFIG_BLE_OTS_LAST_MODIFIED_CHARACTERISTIC_ENABLE
esp_err_t esp_ble_otp_client_write_last_modified(uint16_t conn_handle, const esp_ble_ots_utc_t *utc)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!utc) {
        return ESP_ERR_INVALID_ARG;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_LAST_MODIFIED, (const uint8_t *)utc,
                                   sizeof(esp_ble_ots_utc_t));
    if (rc == ESP_OK) {
        ctx->meta_state = OTP_META_VALID;
    }
    return rc;
}
#endif

static bool otp_olcp_supported(const otp_transfer_ctx_t *ctx)
{
    if (!ctx || !ctx->feature_valid) {
        return false;
    }
    return (ctx->ots_feature.olcp.goto_op ||
            ctx->ots_feature.olcp.order_op ||
            ctx->ots_feature.olcp.req_num_op ||
            ctx->ots_feature.olcp.clear_mark_op);
}

esp_err_t esp_ble_otp_client_discover_all_start(uint16_t conn_handle)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!otp_olcp_supported(ctx)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_ble_ots_filter_t filter = {0};
    filter.filter = 0x00;
    esp_ble_otp_client_set_filter(conn_handle, &filter);
    return esp_ble_otp_client_select_first(conn_handle);
}

esp_err_t esp_ble_otp_client_discover_next(uint16_t conn_handle)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!otp_olcp_supported(ctx)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return esp_ble_otp_client_select_next(conn_handle);
}

esp_err_t esp_ble_otp_client_discover_by_filter(uint16_t conn_handle, const esp_ble_ots_filter_t *filter)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!filter) {
        return ESP_ERR_INVALID_ARG;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!otp_olcp_supported(ctx)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t rc = esp_ble_otp_client_set_filter(conn_handle, filter);
    if (rc != ESP_OK) {
        return rc;
    }
    return esp_ble_otp_client_select_first(conn_handle);
}

esp_err_t esp_ble_otp_client_select_directory_listing(uint16_t conn_handle)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!otp_olcp_supported(ctx)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_ble_ots_id_t id = { .id = {0} };
    return esp_ble_otp_client_select_by_id(conn_handle, &id);
}

esp_err_t esp_ble_otp_client_read_directory_listing(uint16_t conn_handle)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!otp_olcp_supported(ctx)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    ctx->dir_listing_pending = true;
    return esp_ble_otp_client_select_directory_listing(conn_handle);
}

esp_err_t esp_ble_otp_client_resume_read_checksum(uint16_t conn_handle, uint32_t offset, uint32_t length)
{
    return esp_ble_otp_client_calculate_checksum(conn_handle, offset, length);
}

esp_err_t esp_ble_otp_client_resume_write_checksum(uint16_t conn_handle, uint32_t offset, uint32_t length)
{
    return esp_ble_otp_client_calculate_checksum(conn_handle, offset, length);
}

esp_err_t esp_ble_otp_client_resume_write_current_size(uint16_t conn_handle, uint32_t total_size,
                                                       esp_ble_otp_write_mode_t mode)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (total_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ble_ots_size_t size = {0};
    uint16_t out_len = 0;
    if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_SIZE, (uint8_t *)&size,
                       sizeof(size), &out_len) != ESP_OK) {
        return ESP_FAIL;
    }
    if (size.current_size >= total_size) {
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t offset = size.current_size;
    uint32_t remain = total_size - size.current_size;
    return esp_ble_otp_client_write_object(conn_handle, offset, remain, mode);
}
esp_err_t esp_ble_otp_client_create_object(uint16_t conn_handle, uint16_t object_type, uint32_t object_size)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->session_state == OTP_SESSION_BUSY ||
            ctx->ots_state != OTP_OTS_STATE_DISCOVERED ||
            !ctx->feature_valid || !ctx->ots_feature.oacp.create_op) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (object_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buf[1 + 6] = {0};
    buf[0] = BLE_OTS_OACP_CREATE;
    memcpy(&buf[1], &object_size, sizeof(object_size));
    memcpy(&buf[1 + sizeof(object_size)], &object_type, sizeof(object_type));
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK) {
        if (ctx) {
            ctx->state = OTP_STATE_OACP_PENDING;
            ctx->oacp_opcode = BLE_OTS_OACP_CREATE;
        }
        otp_post_oacp_started(conn_handle, BLE_OTS_OACP_CREATE);
    }
    return rc;
}

esp_err_t esp_ble_otp_client_delete_object(uint16_t conn_handle)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->session_state == OTP_SESSION_BUSY ||
            ctx->ots_state != OTP_OTS_STATE_DISCOVERED ||
            !ctx->feature_valid || !ctx->ots_feature.oacp.delete_op) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->meta_state != OTP_META_VALID) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_ble_ots_prop_t prop = {0};
    uint16_t out_len = 0;
    if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_PROP, (uint8_t *)&prop,
                       sizeof(prop), &out_len) != ESP_OK) {
        return ESP_FAIL;
    }
    if (!prop.delete_prop) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint8_t buf[1] = { BLE_OTS_OACP_DELETE };
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK) {
        if (ctx) {
            ctx->state = OTP_STATE_OACP_PENDING;
            ctx->oacp_opcode = BLE_OTS_OACP_DELETE;
        }
        otp_post_oacp_started(conn_handle, BLE_OTS_OACP_DELETE);
    }
    return rc;
}

static esp_err_t otp_start_transfer(otp_transfer_ctx_t *ctx)
{
    if (!otp_client_ready() || !ctx) {
        return ESP_ERR_INVALID_STATE;
    }
    ctx->state = OTP_STATE_COC_CONNECTING;
    ctx->coc_connected = 0;
    ctx->eof_reason = OTP_EOF_NONE;
    ctx->eof_reported = 0;

    return esp_ble_conn_l2cap_coc_connect(ctx->conn_id, s_otp.config.psm,
                                          s_otp.config.l2cap_coc_mtu,
                                          s_otp.config.l2cap_coc_mtu,
                                          otp_l2cap_event_handler, NULL);
}

esp_err_t esp_ble_otp_client_read_object(uint16_t conn_handle, uint32_t offset, uint32_t length)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->session_state == OTP_SESSION_BUSY ||
            ctx->ots_state != OTP_OTS_STATE_DISCOVERED ||
            !ctx->feature_valid || !ctx->ots_feature.oacp.read_op) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->meta_state != OTP_META_VALID) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t buf[1 + 8] = {0};
    buf[0] = BLE_OTS_OACP_READ;
    memcpy(&buf[1], &offset, sizeof(offset));
    memcpy(&buf[1 + sizeof(offset)], &length, sizeof(length));
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, buf, sizeof(buf));
    if (rc != ESP_OK) {
        return rc;
    }
    if (ctx) {
        ctx->state = OTP_STATE_OACP_PENDING;
        ctx->direction = OTP_DIR_READ;
        ctx->object_offset = offset;
        ctx->tx_len = 0;
        ctx->rx_len = 0;
        ctx->eof_reason = OTP_EOF_NONE;
        ctx->eof_reported = 0;
        ctx->abort_requested = 0;
        ctx->execute_requested = 0;
        ctx->oacp_opcode = BLE_OTS_OACP_READ;
        ctx->oacp_result = 0;
        ctx->session_state = OTP_SESSION_BUSY;
        if (length != 0) {
            ctx->object_size = length;
        } else {
            esp_ble_ots_size_t size = {0};
            uint16_t out_len = 0;
            if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_SIZE, (uint8_t *)&size,
                               sizeof(size), &out_len) == ESP_OK) {
                ctx->object_size = size.current_size;
            } else {
                ctx->object_size = BLE_OTP_OBJECT_SIZE_UNKNOWN;
            }
        }
    }
    otp_post_oacp_started(conn_handle, BLE_OTS_OACP_READ);
    return ESP_OK;
}

esp_err_t esp_ble_otp_client_write_object(uint16_t conn_handle, uint32_t offset, uint32_t length,
                                          esp_ble_otp_write_mode_t mode)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->session_state == OTP_SESSION_BUSY ||
            ctx->ots_state != OTP_OTS_STATE_DISCOVERED ||
            !ctx->feature_valid || !ctx->ots_feature.oacp.write_op) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->meta_state != OTP_META_VALID) {
        return ESP_ERR_INVALID_STATE;
    }
    if (length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ble_ots_prop_t prop = {0};
    esp_ble_ots_size_t size = {0};
    uint16_t out_len = 0;
    if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_PROP, (uint8_t *)&prop,
                       sizeof(prop), &out_len) != ESP_OK) {
        return ESP_FAIL;
    }
    if (!prop.write_prop) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_SIZE, (uint8_t *)&size,
                       sizeof(size), &out_len) != ESP_OK) {
        return ESP_FAIL;
    }
    if (offset > size.current_size) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (mode) {
    case BLE_OTP_WRITE_MODE_APPEND:
        if (!ctx->ots_feature.oacp.appending_op) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        break;
    case BLE_OTP_WRITE_MODE_TRUNCATE:
        if (!ctx->ots_feature.oacp.truncation_op) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        if ((offset + length) >= size.current_size) {
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case BLE_OTP_WRITE_MODE_PATCH:
        if (!ctx->ots_feature.oacp.patching_op) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        if ((offset + length) >= size.current_size) {
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case BLE_OTP_WRITE_MODE_OVERWRITE:
    default:
        if ((offset + length) > size.allocated_size) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        break;
    }
    uint8_t buf[1 + 9] = {0};
    buf[0] = BLE_OTS_OACP_WRITE;
    memcpy(&buf[1], &offset, sizeof(offset));
    memcpy(&buf[1 + sizeof(offset)], &length, sizeof(length));
    buf[1 + sizeof(offset) + sizeof(length)] = (uint8_t)mode;
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, buf, sizeof(buf));
    if (rc != ESP_OK) {
        return rc;
    }
    if (ctx) {
        ctx->state = OTP_STATE_OACP_PENDING;
        ctx->direction = OTP_DIR_WRITE;
        ctx->object_offset = offset;
        ctx->tx_len = 0;
        ctx->rx_len = 0;
        ctx->eof_reason = OTP_EOF_NONE;
        ctx->eof_reported = 0;
        ctx->abort_requested = 0;
        ctx->execute_requested = 0;
        ctx->oacp_opcode = BLE_OTS_OACP_WRITE;
        ctx->oacp_result = 0;
        ctx->object_size = size.current_size;
        ctx->session_state = OTP_SESSION_BUSY;
        if (length != 0) {
            ctx->object_size = length;
        }
    }
    otp_post_oacp_started(conn_handle, BLE_OTS_OACP_WRITE);
    return ESP_OK;
}

esp_err_t esp_ble_otp_client_calculate_checksum(uint16_t conn_handle, uint32_t offset, uint32_t length)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED ||
            !ctx->feature_valid || !ctx->ots_feature.oacp.calculate_op) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint8_t buf[1 + 8] = {0};
    buf[0] = BLE_OTS_OACP_CALCULATE_CHECKSUM;
    memcpy(&buf[1], &offset, sizeof(offset));
    memcpy(&buf[1 + sizeof(offset)], &length, sizeof(length));
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK) {
        if (ctx) {
            ctx->state = OTP_STATE_OACP_PENDING;
            ctx->oacp_opcode = BLE_OTS_OACP_CALCULATE_CHECKSUM;
        }
        otp_post_oacp_started(conn_handle, BLE_OTS_OACP_CALCULATE_CHECKSUM);
    }
    return rc;
}

esp_err_t esp_ble_otp_client_execute_object(uint16_t conn_handle, const uint8_t *parameters, uint8_t param_len)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    if (!ctx || ctx->ots_state != OTP_OTS_STATE_DISCOVERED || !ctx->feature_valid ||
            !ctx->ots_feature.oacp.execute_op) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (ctx->objsel_state != OTP_OBJSEL_SELECTED || ctx->meta_state != OTP_META_VALID) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->session_state == OTP_SESSION_BUSY &&
            !(ctx->direction == OTP_DIR_WRITE && ctx->state == OTP_STATE_EOF)) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_ble_ots_prop_t prop = {0};
    uint16_t obj_type = 0;
    uint16_t out_len = 0;
    if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_TYPE, (uint8_t *)&obj_type,
                       sizeof(obj_type), &out_len) != ESP_OK) {
        return ESP_FAIL;
    }
    (void)obj_type;
    if (otp_read_chr16(BLE_OTS_CHR_UUID16_OBJECT_PROP, (uint8_t *)&prop,
                       sizeof(prop), &out_len) != ESP_OK) {
        return ESP_FAIL;
    }
    if (!prop.execute_prop) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint8_t buf[1 + 20] = {0};
    if (param_len > 20) {
        return ESP_ERR_INVALID_ARG;
    }
    buf[0] = BLE_OTS_OACP_EXECUTE;
    if (parameters && param_len) {
        memcpy(&buf[1], parameters, param_len);
    }
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, buf, 1 + param_len);
    if (rc == ESP_OK) {
        ctx->state = OTP_STATE_EXECUTING;
        ctx->execute_requested = 1;
        ctx->oacp_opcode = BLE_OTS_OACP_EXECUTE;
        otp_post_oacp_started(conn_handle, BLE_OTS_OACP_EXECUTE);
    }
    return rc;
}

esp_err_t esp_ble_otp_client_abort(uint16_t conn_handle)
{
    (void)conn_handle;
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(conn_handle, true);
    uint8_t buf[1] = { BLE_OTS_OACP_ABORT };
    esp_err_t rc = otp_write_chr16(BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT, buf, sizeof(buf));
    if (rc == ESP_OK) {
        if (ctx) {
            ctx->state = OTP_STATE_ABORTING;
            ctx->abort_requested = 1;
            ctx->oacp_opcode = BLE_OTS_OACP_ABORT;
        }
    }
    return rc;
}

esp_err_t esp_ble_otp_client_send_data(const esp_ble_otp_transfer_info_t *transfer_info,
                                       const uint8_t *data, uint16_t data_len)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!transfer_info || !data || data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ble_conn_l2cap_coc_sdu_t sdu = {
        .data = (uint8_t *)data,
        .len = data_len,
    };
    esp_err_t rc = esp_ble_conn_l2cap_coc_send(transfer_info->channel, &sdu);
    if (rc == ESP_OK) {
        otp_transfer_ctx_t *ctx = otp_ctx_get(transfer_info->conn_handle, false);
        if (ctx) {
            ctx->tx_len += data_len;
            esp_ble_otp_event_data_t evt = {0};
            otp_fill_transfer_info(ctx, &evt.transfer_data_sent.transfer_info);
            evt.transfer_data_sent.data_len = data_len;
            otp_post_event(BLE_OTP_EVENT_TRANSFER_DATA_SENT, &evt, sizeof(evt));
            if (ctx->direction == OTP_DIR_READ && ctx->object_size != BLE_OTP_OBJECT_SIZE_UNKNOWN &&
                    ctx->tx_len >= ctx->object_size && !ctx->eof_reported) {
                ctx->eof_reason = OTP_EOF_BY_SIZE;
                ctx->eof_reported = 1;
                ctx->state = OTP_STATE_EOF;
                ctx->session_state = OTP_SESSION_BUSY;
                otp_fill_transfer_info(ctx, &evt.transfer_eof.transfer_info);
                otp_post_event(BLE_OTP_EVENT_TRANSFER_EOF, &evt, sizeof(evt));
                otp_try_complete(ctx);
            }
        }
    }
    return rc;
}

esp_err_t esp_ble_otp_client_disconnect_transfer_channel(const esp_ble_otp_transfer_info_t *transfer_info)
{
    if (!otp_client_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!transfer_info) {
        return ESP_ERR_INVALID_ARG;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(transfer_info->conn_handle, false);
    if (ctx && ctx->direction == OTP_DIR_WRITE && !ctx->eof_reported) {
        ctx->eof_reason = OTP_EOF_BY_APP;
        ctx->eof_reported = 1;
        ctx->state = OTP_STATE_EOF;
        esp_ble_otp_event_data_t evt = {0};
        otp_fill_transfer_info(ctx, &evt.transfer_eof.transfer_info);
        otp_post_event(BLE_OTP_EVENT_TRANSFER_EOF, &evt, sizeof(evt));
        otp_try_complete(ctx);
    }
    return esp_ble_conn_l2cap_coc_disconnect(transfer_info->channel);
}

esp_err_t esp_ble_otp_server_set_feature(const esp_ble_ots_feature_t *feature)
{
    if (!otp_server_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!feature) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_ble_ots_set_feature((esp_ble_ots_feature_t *)feature);
}

esp_err_t esp_ble_otp_server_register_oacp_callback(esp_err_t (*callback)(uint8_t op_code, const uint8_t *parameter,
                                                                          uint16_t param_len, void *ctx), void *ctx)
{
    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }
    s_otp.oacp_cb = callback;
    s_otp.oacp_ctx = ctx;
    return ESP_OK;
}

esp_err_t esp_ble_otp_server_register_olcp_callback(esp_err_t (*callback)(uint8_t op_code, const uint8_t *parameter,
                                                                          uint16_t param_len, void *ctx), void *ctx)
{
    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }
    s_otp.olcp_cb = callback;
    s_otp.olcp_ctx = ctx;
    return ESP_OK;
}

esp_err_t esp_ble_otp_server_send_oacp_response(uint8_t req_op_code, uint8_t rsp_code,
                                                const uint8_t *rsp_parameter, uint8_t param_len)
{
    if (!otp_server_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (param_len > 18) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t conn_id = 0;
    if (esp_ble_conn_get_conn_handle(&conn_id) == ESP_OK) {
        otp_transfer_ctx_t *ctx = otp_ctx_get(conn_id, true);
        if (ctx) {
            ctx->oacp_opcode = req_op_code;
            ctx->oacp_result = rsp_code;
        }
    }

    esp_ble_ots_oacp_t oacp = {0};
    oacp.op_code = BLE_OTS_OACP_RESPONSE;
    oacp.parameter[0] = req_op_code;
    oacp.parameter[1] = rsp_code;
    if (rsp_parameter && param_len) {
        memcpy(&oacp.parameter[2], rsp_parameter, param_len);
    }

    return esp_ble_ots_set_oacp(&oacp, true);
}

esp_err_t esp_ble_otp_server_send_olcp_response(uint8_t req_op_code, uint8_t rsp_code,
                                                const uint8_t *rsp_parameter, uint8_t param_len)
{
    if (!otp_server_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (param_len > 4) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_ots_olcp_t olcp = {0};
    olcp.op_code = BLE_OTS_OLCP_RESPONSE;
    olcp.parameter[0] = req_op_code;
    olcp.parameter[1] = rsp_code;
    if (rsp_parameter && param_len) {
        memcpy(&olcp.parameter[2], rsp_parameter, param_len);
    }
    return esp_ble_ots_set_olcp(&olcp, true);
}

esp_err_t esp_ble_otp_server_send_data(const esp_ble_otp_transfer_info_t *transfer_info,
                                       const uint8_t *data, uint16_t data_len)
{
    if (!otp_server_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!transfer_info || !data || data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_ble_conn_l2cap_coc_sdu_t sdu = {
        .data = (uint8_t *)data,
        .len = data_len,
    };
    esp_err_t rc = esp_ble_conn_l2cap_coc_send(transfer_info->channel, &sdu);
    if (rc == ESP_OK) {
        otp_transfer_ctx_t *ctx = otp_ctx_get(transfer_info->conn_handle, false);
        if (ctx) {
            ctx->tx_len += data_len;
            esp_ble_otp_event_data_t evt = {0};
            otp_fill_transfer_info(ctx, &evt.transfer_data_sent.transfer_info);
            evt.transfer_data_sent.data_len = data_len;
            otp_post_event(BLE_OTP_EVENT_TRANSFER_DATA_SENT, &evt, sizeof(evt));
        }
    }
    return rc;
}

esp_err_t esp_ble_otp_server_disconnect_transfer_channel(const esp_ble_otp_transfer_info_t *transfer_info)
{
    if (!otp_server_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!transfer_info) {
        return ESP_ERR_INVALID_ARG;
    }
    otp_transfer_ctx_t *ctx = otp_ctx_get(transfer_info->conn_handle, false);
    if (ctx && ctx->direction == OTP_DIR_WRITE && !ctx->eof_reported) {
        ctx->eof_reason = OTP_EOF_BY_APP;
        ctx->eof_reported = 1;
        ctx->state = OTP_STATE_EOF;
        esp_ble_otp_event_data_t evt = {0};
        otp_fill_transfer_info(ctx, &evt.transfer_eof.transfer_info);
        otp_post_event(BLE_OTP_EVENT_TRANSFER_EOF, &evt, sizeof(evt));
        otp_try_complete(ctx);
    }
    return esp_ble_conn_l2cap_coc_disconnect(transfer_info->channel);
}

esp_err_t esp_ble_otp_init(const esp_ble_otp_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_otp.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_otp.config = *config;
    if (s_otp.config.psm == 0) {
        s_otp.config.psm = BLE_OTP_PSM_DEFAULT;
    }
    if (s_otp.config.l2cap_coc_mtu == 0) {
        s_otp.config.l2cap_coc_mtu = BLE_OTP_L2CAP_COC_MTU_DEFAULT;
    }

    s_otp.lock = xSemaphoreCreateMutex();
    if (!s_otp.lock) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t rc = esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                              otp_conn_event_handler, NULL);
    if (rc != ESP_OK) {
        vSemaphoreDelete(s_otp.lock);
        s_otp.lock = NULL;
        ESP_LOGE(TAG, "otp_init: failed to register connection manager event handler: %s", esp_err_to_name(rc));
        return rc;
    }

    if (s_otp.config.role == BLE_OTP_ROLE_SERVER) {
        rc = esp_ble_ots_init();
        if (rc != ESP_OK) {
            esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, otp_conn_event_handler);
            vSemaphoreDelete(s_otp.lock);
            s_otp.lock = NULL;
            return rc;
        }
        rc = esp_event_handler_register(BLE_OTS_EVENTS, ESP_EVENT_ANY_ID, otp_ots_event_handler, NULL);
        if (rc != ESP_OK) {
            esp_ble_ots_deinit();
            esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, otp_conn_event_handler);
            vSemaphoreDelete(s_otp.lock);
            s_otp.lock = NULL;
            ESP_LOGE(TAG, "otp_init: failed to register OTS event handler: %s", esp_err_to_name(rc));
            return rc;
        }
    }

    for (size_t i = 0; i < OTP_MAX_CONNECTIONS; i++) {
        otp_ctx_init(&s_otp.transfers[i], 0);
    }

    s_otp.inited = true;
    return ESP_OK;
}

esp_err_t esp_ble_otp_deinit(void)
{
    if (!s_otp.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, otp_conn_event_handler);

    if (s_otp.config.role == BLE_OTP_ROLE_SERVER) {
        esp_event_handler_unregister(BLE_OTS_EVENTS, ESP_EVENT_ANY_ID, otp_ots_event_handler);
        esp_ble_ots_deinit();
    }

    if (s_otp.lock) {
        vSemaphoreDelete(s_otp.lock);
        s_otp.lock = NULL;
    }

    memset(&s_otp, 0, sizeof(s_otp));
    return ESP_OK;
}
