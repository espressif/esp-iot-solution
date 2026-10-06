/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_esl.h"
#include "esp_eslp_ap.h"
#include "esp_eslp_state.h"
#include "esp_otp.h"

static const char *TAG = "ble_esl_ap";

#define EXAMPLE_DEMO_IMAGE_INDEX    0
#define EXAMPLE_DEMO_IMAGE_LEN      64

typedef enum {
    DEMO_CMD_IDLE = 0,
    DEMO_CMD_DISPLAY,
    DEMO_CMD_LED,
    DEMO_CMD_SENSOR,
    DEMO_CMD_DONE,
} demo_cmd_step_t;

static uint16_t s_tag_conn = BLE_CONN_HANDLE_INVALID;
static bool s_disc_done;
static bool s_encrypted;
static bool s_configured;
static bool s_pawr_started;
static bool s_image_started;
static bool s_ots_ready;
static bool s_want_image;
static bool s_sync_started;
static demo_cmd_step_t s_demo_step;
static esp_ble_esl_address_t s_tag_addr;

/* Must remain valid until IMAGE_TRANSFERRED. */
static uint8_t s_demo_image[EXAMPLE_DEMO_IMAGE_LEN];

static void try_associate_and_sync(void);
static void try_start_image_transfer(void);
static void start_pawr_and_past(uint16_t conn_handle);
static void demo_cmd_advance(void);

static void fill_demo_keys(esp_ble_eslp_ap_tag_config_t *cfg)
{
    for (int i = 0; i < BLE_ESL_SESSION_KEY_SIZE; i++) {
        cfg->ap_sync_key.session_key[i] = (uint8_t)(0xA0 + i);
        cfg->resp_key.session_key[i] = (uint8_t)(0xB0 + i);
    }
    for (int i = 0; i < BLE_ESL_IV_SIZE; i++) {
        cfg->ap_sync_key.iv[i] = (uint8_t)(0x10 + i);
        cfg->resp_key.iv[i] = (uint8_t)(0x20 + i);
    }
}

static void prepare_demo_image(void)
{
    for (int i = 0; i < EXAMPLE_DEMO_IMAGE_LEN; i++) {
        s_demo_image[i] = (uint8_t)(0xE0 + (i & 0x0F));
    }
}

static const char *eslp_state_str(esp_ble_eslp_state_t state)
{
    switch (state) {
    case BLE_ESLP_STATE_UNASSOCIATED: return "UNASSOCIATED";
    case BLE_ESLP_STATE_CONFIGURING:  return "CONFIGURING";
    case BLE_ESLP_STATE_UPDATING:     return "UPDATING";
    case BLE_ESLP_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
    case BLE_ESLP_STATE_UNSYNCHRONIZED: return "UNSYNCHRONIZED";
    default:                          return "UNKNOWN";
    }
}

static void try_start_image_transfer(void)
{
    if (s_image_started || !s_want_image || !s_ots_ready) {
        return;
    }
    if (s_tag_conn == BLE_CONN_HANDLE_INVALID) {
        return;
    }

    prepare_demo_image();

    esp_ble_eslp_ap_image_transfer_t xfer = {
        .conn_handle = s_tag_conn,
        .image_index = EXAMPLE_DEMO_IMAGE_INDEX,
        .data = s_demo_image,
        .data_len = sizeof(s_demo_image),
    };
    esp_err_t ret = esp_ble_eslp_ap_transfer_image(&xfer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "transfer_image failed: %s - continue without image", esp_err_to_name(ret));
        s_want_image = false;
        s_demo_step = DEMO_CMD_IDLE;
        demo_cmd_advance();
        return;
    }
    s_image_started = true;
    ESP_LOGI(TAG, "Image transfer started: index=%u len=%u",
             EXAMPLE_DEMO_IMAGE_INDEX, (unsigned)sizeof(s_demo_image));
}

static void start_image_transfer(uint16_t conn_handle)
{
    (void)conn_handle;
    s_want_image = true;
    try_start_image_transfer();
}

static void demo_cmd_advance(void)
{
    uint8_t esl_id = s_tag_addr.esl_id;
    uint8_t group_id = BLE_ESL_ADDR_GROUP_ID(s_tag_addr);
    esp_err_t ret = ESP_OK;

    switch (s_demo_step) {
    case DEMO_CMD_IDLE:
        s_demo_step = DEMO_CMD_DISPLAY;
        ret = esp_ble_eslp_ap_display_image(esl_id, group_id, 0, EXAMPLE_DEMO_IMAGE_INDEX);
        ESP_LOGI(TAG, "display_image: %s", esp_err_to_name(ret));
        break;
    case DEMO_CMD_DISPLAY:
        s_demo_step = DEMO_CMD_LED;
        {
            esp_ble_eslp_ap_led_settings_t led = {
                .color_red = 3,
                .color_green = 0,
                .color_blue = 0,
                .brightness = 2,
                .flashing_pattern = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0x05, 0x05 },
                .repeat_type = 0,
                .repeats_duration = 3,
            };
            ret = esp_ble_eslp_ap_led_control(esl_id, group_id, 0, &led);
            ESP_LOGI(TAG, "led_control: %s", esp_err_to_name(ret));
        }
        break;
    case DEMO_CMD_LED:
        s_demo_step = DEMO_CMD_SENSOR;
        ret = esp_ble_eslp_ap_read_sensor(esl_id, group_id, 0);
        ESP_LOGI(TAG, "read_sensor: %s", esp_err_to_name(ret));
        break;
    case DEMO_CMD_SENSOR:
        s_demo_step = DEMO_CMD_DONE;
        /* Update Complete, then PAST. */
        ret = esp_ble_eslp_ap_update_complete(esl_id, group_id);
        ESP_LOGI(TAG, "update_complete: %s", esp_err_to_name(ret));
        if (!s_sync_started && s_tag_conn != BLE_CONN_HANDLE_INVALID) {
            start_pawr_and_past(s_tag_conn);
        }
        break;
    default:
        break;
    }

    if (ret != ESP_OK && s_demo_step != DEMO_CMD_DONE && s_demo_step != DEMO_CMD_IDLE) {
        /* Skip failed step and continue the chain. */
        demo_cmd_advance();
    }
}

static void start_pawr_and_past(uint16_t conn_handle)
{
    if (s_sync_started) {
        return;
    }
    s_sync_started = true;

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV)
    if (!s_pawr_started) {
        esp_err_t ret = esp_ble_eslp_ap_pawr_start(NULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "pawr_start: %s", esp_err_to_name(ret));
        }
    }
#endif
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC_TRANSFER)
    esp_err_t ret = esp_ble_eslp_ap_sync_transfer(conn_handle, 0x0001);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "sync_transfer: %s", esp_err_to_name(ret));
    }
#else
    (void)conn_handle;
#endif
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
    (void)esp_ble_eslp_ap_queue_ping(BLE_ESL_ADDR_GROUP_ID(s_tag_addr), s_tag_addr.esl_id);
#endif
}

static void app_ap_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_ESLP_AP_EVENTS) {
        return;
    }

    switch (id) {
    case BLE_ESLP_AP_EVENT_TAG_CONFIGURED: {
        const esp_ble_eslp_ap_tag_configured_t *ev = event_data;
        ESP_LOGI(TAG, "Tag provisioned (GATT writes): conn=%u esl_id=0x%02x group=%u",
                 (unsigned)ev->conn_handle, ev->address.esl_id,
                 (unsigned)BLE_ESL_ADDR_GROUP_ID(ev->address));
        s_configured = true;
        s_tag_addr = ev->address;

        /* Read Info / PnP before Update Complete when possible. */
        esp_ble_eslp_ap_tag_info_t info;
        esp_err_t iret = esp_ble_eslp_ap_read_info(ev->conn_handle, &info);
        if (iret != ESP_OK) {
            ESP_LOGW(TAG, "read_info: %s (no optional info chars)", esp_err_to_name(iret));
        }

        /* Transfer image while still connected. */
        start_image_transfer(ev->conn_handle);
        break;
    }
    case BLE_ESLP_AP_EVENT_IMAGE_TRANSFERRED: {
        const esp_ble_eslp_ap_image_transferred_t *ev = event_data;
        if (ev->status == ESP_OK) {
            ESP_LOGI(TAG, "Image transferred: index=%u status=%s",
                     ev->image_index, esp_err_to_name(ev->status));
        } else {
            ESP_LOGW(TAG, "Image transfer failed: index=%u status=%s - continue ECP",
                     ev->image_index, esp_err_to_name(ev->status));
        }
        s_want_image = false;
        s_demo_step = DEMO_CMD_IDLE;
        demo_cmd_advance();
        break;
    }
    case BLE_ESLP_AP_EVENT_ECP_RESPONSE: {
        const esp_ble_eslp_ap_ecp_response_t *ev = event_data;
        ESP_LOGI(TAG, "ECP response conn=%u len=%u", (unsigned)ev->conn_handle, ev->data_len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, ev->data, ev->data_len, ESP_LOG_INFO);
        if (s_demo_step != DEMO_CMD_IDLE && s_demo_step != DEMO_CMD_DONE) {
            demo_cmd_advance();
        }
        break;
    }
    case BLE_ESLP_AP_EVENT_ECP_TIMEOUT: {
        const esp_ble_eslp_ap_ecp_timeout_t *ev = event_data;
        ESP_LOGW(TAG, "ECP timeout conn=%u esl=0x%02x group=%u",
                 (unsigned)ev->conn_handle, ev->esl_id, ev->group_id);
        if (s_demo_step != DEMO_CMD_IDLE && s_demo_step != DEMO_CMD_DONE) {
            demo_cmd_advance();
        }
        break;
    }
    case BLE_ESLP_AP_EVENT_TAG_INFO: {
        const esp_ble_eslp_ap_tag_info_t *ev = event_data;
        ESP_LOGI(TAG, "Tag info: displays=%u image=%s sensors=%u leds=%u pnp=%s",
                 ev->num_displays,
                 ev->has_image_info ? "yes" : "no",
                 (unsigned)ev->sensor_info_len,
                 ev->num_leds,
                 ev->has_pnp_id ? "yes" : "no");
        break;
    }
    case BLE_ESLP_AP_EVENT_TAG_STATE_CHANGED: {
        const esp_ble_eslp_ap_tag_state_changed_t *ev = event_data;
        ESP_LOGI(TAG, "Tag state: esl=0x%02x %s -> %s",
                 ev->address.esl_id,
                 eslp_state_str(ev->prev_state), eslp_state_str(ev->new_state));
        break;
    }
    case BLE_ESLP_AP_EVENT_TAG_TIMEOUT: {
        const esp_ble_eslp_ap_tag_timeout_t *ev = event_data;
        ESP_LOGW(TAG, "Tag timeout: esl=0x%02x kind=%d", ev->address.esl_id, (int)ev->kind);
        break;
    }
    case BLE_ESLP_AP_EVENT_PAWR_STARTED:
        ESP_LOGI(TAG, "PAwR started");
        s_pawr_started = true;
        break;
    case BLE_ESLP_AP_EVENT_SYNC_TRANSFERRED:
        ESP_LOGI(TAG, "PAST set_info done");
        break;
    case BLE_ESLP_AP_EVENT_SUBEV_RESP: {
        const esp_ble_eslp_ap_subev_resp_t *ev = event_data;
        if (ev->address_valid) {
            ESP_LOGI(TAG, "PAwR resp subev=%u slot=%u esl_id=0x%02x len=%u",
                     ev->subevent, ev->response_slot, ev->address.esl_id, ev->data_len);
        } else {
            ESP_LOGW(TAG, "PAwR resp subev=%u slot=%u len=%u (EAD unresolved)",
                     ev->subevent, ev->response_slot, ev->data_len);
        }
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, ev->data, ev->data_len, ESP_LOG_INFO);
        break;
    }
    default:
        break;
    }
}

static void app_otp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)event_data;
    if (base != BLE_OTP_EVENTS) {
        return;
    }

    switch (id) {
    case BLE_OTP_EVENT_OTS_DISCOVERED:
        ESP_LOGI(TAG, "OTS discovered on Tag");
        s_ots_ready = true;
        try_start_image_transfer();
        break;
    case BLE_OTP_EVENT_OTS_DISCOVERY_FAILED:
        ESP_LOGE(TAG, "OTS discovery failed; skip image transfer");
        s_ots_ready = false;
        if (s_want_image && !s_image_started) {
            s_want_image = false;
            s_demo_step = DEMO_CMD_IDLE;
            demo_cmd_advance();
        }
        break;
    default:
        break;
    }
}

static void try_associate_and_sync(void)
{
    if (s_tag_conn == BLE_CONN_HANDLE_INVALID || !s_disc_done || !s_encrypted || s_configured) {
        return;
    }

    esp_ble_eslp_ap_tag_config_t cfg;
    ESP_ERROR_CHECK(esp_ble_eslp_ap_tag_config_default(&cfg));
    cfg.address.esl_id = CONFIG_EXAMPLE_ESL_ID;
    BLE_ESL_ADDR_SET_GROUP_ID(cfg.address, CONFIG_EXAMPLE_ESL_GROUP_ID);
    fill_demo_keys(&cfg);

    esp_err_t ret = esp_ble_eslp_ap_configure_tag(s_tag_conn, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "configure_tag failed: %s", esp_err_to_name(ret));
    }
}

static void app_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        if (event_data) {
            s_tag_conn = ((const uint16_t *)event_data)[0];
        }
        s_disc_done = false;
        s_encrypted = false;
        s_configured = false;
        s_image_started = false;
        s_ots_ready = false;
        s_want_image = false;
        s_sync_started = false;
        s_demo_step = DEMO_CMD_IDLE;
        ESP_LOGI(TAG, "Connected conn=%u", (unsigned)s_tag_conn);
        break;
    case ESP_BLE_CONN_EVENT_ENC_CHANGE: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev || ev->enc_change.status != 0 || !ev->enc_change.encrypted) {
            ESP_LOGW(TAG, "Encryption not ready");
            break;
        }
        ESP_LOGI(TAG, "Link encrypted");
        s_encrypted = true;
        try_associate_and_sync();
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        s_tag_conn = BLE_CONN_HANDLE_INVALID;
        s_disc_done = false;
        s_encrypted = false;
        s_ots_ready = false;
        s_want_image = false;
        break;
    case ESP_BLE_CONN_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "Discovery complete");
        s_disc_done = true;
        try_associate_and_sync();
        break;
    case ESP_BLE_CONN_EVENT_SCAN_RESULT: {
        const esp_ble_conn_scan_result_t *res = event_data;
        if (!res || s_tag_conn != BLE_CONN_HANDLE_INVALID) {
            break;
        }
        ESP_LOGD(TAG, "Scan result RSSI=%d", res->rssi);
        break;
    }
    default:
        break;
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_ESLP_AP_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID,
                                               app_otp_event_handler, NULL));

    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .remote_name = CONFIG_EXAMPLE_TAG_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV,
        .include_service_uuid = 1,
        .adv_uuid_type = BLE_CONN_UUID_TYPE_16,
        .adv_uuid16 = BLE_ESL_UUID16,
    };

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_eslp_ap_init());

    ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_conn_start failed: %s", esp_err_to_name(ret));
        esp_ble_conn_stop();
        esp_ble_eslp_ap_deinit();
        esp_ble_conn_deinit();
        return;
    }

    ESP_LOGI(TAG, "ESL AP started. Scanning for Tag name '%s'", CONFIG_EXAMPLE_TAG_ADV_NAME);
}
