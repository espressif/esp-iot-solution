/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/param.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cJSON.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>

#include <protocomm.h>
#include <protocomm_security0.h>
#include <protocomm_security1.h>
#include <protocomm_security2.h>

#include "esp_ble_ota_priv.h"

#include "ble_ota.h"

#define ESP_BLE_OTA_MGR_VERSION      "v1.1"
#define ESP_BLE_OTA_STORAGE_BIT       BIT0
#define ESP_BLE_OTA_SETTING_BIT       BIT1
#define MAX_SCAN_RESULTS           CONFIG_ESP_BLE_OTA_SCAN_MAX_ENTRIES

#define ACQUIRE_LOCK(mux)     assert(xSemaphoreTake(mux, portMAX_DELAY) == pdTRUE)
#define RELEASE_LOCK(mux)     assert(xSemaphoreGive(mux) == pdTRUE)

static const char *TAG = "esp_ble_ota";
static uint8_t *fw_buf = NULL;
static uint32_t fw_buf_offset = 0;

ESP_EVENT_DEFINE_BASE(ESP_BLE_OTA_EVENT);

typedef enum {
    ESP_BLE_OTA_STATE_IDLE,
    ESP_BLE_OTA_STATE_STARTING,
    ESP_BLE_OTA_STATE_STARTED,
    ESP_BLE_OTA_STATE_FAIL,
    ESP_BLE_OTA_STATE_SUCCESS,
    ESP_BLE_OTA_STATE_STOPPING
} esp_ble_ota_state_t;

/**
 * @brief  Structure for storing capabilities supported by
 *         the ota service
 */
struct esp_ble_ota_capabilities {
    /* Security 0 is used */
    bool no_sec;

    /* Proof of Possession is not required for establishing session */
    bool no_pop;
};

/**
 * @brief  Structure for storing miscellaneous information about
 *         ota service that will be conveyed to clients
 */
struct esp_ble_ota_info {
    const char *version;
    struct esp_ble_ota_capabilities capabilities;
};

/**
 * @brief  Context data for ota manager
 */
struct esp_ble_ota_ctx {
    /* manager configuration */
    esp_ble_ota_config_t mgr_config;

    /* State of the ota service */
    esp_ble_ota_state_t prov_state;

    /* scheme configuration */
    void *prov_scheme_config;

    /* Protocomm handle */
    protocomm_t *pc;

    /* Type of security to use with protocomm */
    int security;

    /* Pointer to security params */
    const void *protocomm_sec_params;

    /* Handle for Auto Stop timer */
    esp_timer_handle_t autostop_timer;

    ota_handlers_t *ota_handlers;

    /* Count of used endpoint UUIDs */
    unsigned int endpoint_uuid_used;

    /* service information */
    struct esp_ble_ota_info mgr_info;

    /* Application related information in JSON format */
    cJSON *app_info_json;

    /* Delay after which resources will be cleaned up asynchronously
     * upon execution of esp_ble_ota_mgr_stop() */
    uint32_t cleanup_delay;
};

static SemaphoreHandle_t prov_ctx_lock = NULL;

/* Pointer to ota context data */
static struct esp_ble_ota_ctx *prov_ctx;

static cJSON *
esp_ble_ota_get_info_json(void)
{
    cJSON *full_info_json = prov_ctx->app_info_json ?
                            cJSON_Duplicate(prov_ctx->app_info_json, 1) : cJSON_CreateObject();
    cJSON *prov_info_json = cJSON_CreateObject();
    cJSON *prov_capabilities = cJSON_CreateArray();

    /* Use label "prov" to indicate ota related information */
    cJSON_AddItemToObject(full_info_json, "prov", prov_info_json);

    /* Version field */
    cJSON_AddStringToObject(prov_info_json, "ver", prov_ctx->mgr_info.version);

    cJSON_AddNumberToObject(prov_info_json, "sec_ver", prov_ctx->security);
    /* Capabilities field */
    cJSON_AddItemToObject(prov_info_json, "cap", prov_capabilities);

    /* If Security / Proof of Possession is not used, indicate in capabilities */
    if (prov_ctx->mgr_info.capabilities.no_sec) {
        cJSON_AddItemToArray(prov_capabilities, cJSON_CreateString("no_sec"));
    } else if (prov_ctx->mgr_info.capabilities.no_pop) {
        cJSON_AddItemToArray(prov_capabilities, cJSON_CreateString("no_pop"));
    }

    return full_info_json;
}

/* Declare the internal event handler */
static void
esp_ble_ota_event_handler_internal(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "%s invoked", __func__);
}

static esp_err_t
esp_ble_ota_start_service(const char *service_name, const char *service_key)
{
    const esp_ble_ota_scheme_t *scheme = &prov_ctx->mgr_config.scheme;
    esp_err_t ret;

    /* Create new protocomm instance */
    prov_ctx->pc = protocomm_new();
    if (prov_ctx->pc == NULL) {
        ESP_LOGE(TAG, "Failed to create new protocomm instance");
        return ESP_FAIL;
    }

    ret = scheme->set_config_service(prov_ctx->prov_scheme_config, service_name, service_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure service");
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    /* Start ota */
    ret = scheme->ota_start(prov_ctx->pc, prov_ctx->prov_scheme_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start service");
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    /* Set version information / capabilities of ota service and application */
    cJSON *version_json = esp_ble_ota_get_info_json();
    char *version_str = cJSON_Print(version_json);
    ret = protocomm_set_version(prov_ctx->pc, "proto-ver", version_str);
    free(version_str);
    cJSON_Delete(version_json);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set version endpoint");
        scheme->ota_stop(prov_ctx->pc);
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    /* Set protocomm security type for endpoint */
    if (prov_ctx->security == 0) {
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_0
        ret = protocomm_set_security(prov_ctx->pc, "prov-session",
                                     &protocomm_security0, NULL);
#else
        // Enable SECURITY_VERSION_0 in Protocomm configuration menu
        return ESP_ERR_NOT_SUPPORTED;
#endif
    } else if (prov_ctx->security == 1) {
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1
        ret = protocomm_set_security(prov_ctx->pc, "prov-session",
                                     &protocomm_security1, prov_ctx->protocomm_sec_params);
#else
        // Enable SECURITY_VERSION_1 in Protocomm configuration menu
        return ESP_ERR_NOT_SUPPORTED;
#endif
    } else if (prov_ctx->security == 2) {
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
        ret = protocomm_set_security(prov_ctx->pc, "prov-session",
                                     &protocomm_security2, prov_ctx->protocomm_sec_params);
#else
        // Enable SECURITY_VERSION_2 in Protocomm configuration menu
        return ESP_ERR_NOT_SUPPORTED;
#endif
    } else {
        ESP_LOGE(TAG, "Unsupported protocomm security version %d", prov_ctx->security);
        ret = ESP_ERR_INVALID_ARG;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set security endpoint");
        scheme->ota_stop(prov_ctx->pc);
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    prov_ctx->ota_handlers = malloc(sizeof(ota_handlers_t));
    ret = get_ota_handlers(prov_ctx->ota_handlers);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate memory for OTA handlers");
        scheme->ota_stop(prov_ctx->pc);
        protocomm_delete(prov_ctx->pc);
        return ESP_ERR_NO_MEM;
    }

    // Add endpoint for OTA
    ret = protocomm_add_endpoint(prov_ctx->pc, "recv-fw",
                                 ota_handler,
                                 prov_ctx->ota_handlers);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set OTA endpoint");
        free(prov_ctx->ota_handlers);
        scheme->ota_stop(prov_ctx->pc);
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    ret = protocomm_add_endpoint(prov_ctx->pc, "ota-bar",
                                 ota_handler,
                                 prov_ctx->ota_handlers);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set OTA endpoint");
        free(prov_ctx->ota_handlers);
        scheme->ota_stop(prov_ctx->pc);
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    ret = protocomm_add_endpoint(prov_ctx->pc, "ota-command",
                                 ota_handler,
                                 prov_ctx->ota_handlers);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set OTA endpoint");
        free(prov_ctx->ota_handlers);
        scheme->ota_stop(prov_ctx->pc);
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    ret = protocomm_add_endpoint(prov_ctx->pc, "ota-customer",
                                 ota_handler,
                                 prov_ctx->ota_handlers);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set OTA endpoint");
        free(prov_ctx->ota_handlers);
        scheme->ota_stop(prov_ctx->pc);
        protocomm_delete(prov_ctx->pc);
        return ret;
    }

    ESP_LOGI(TAG, "OTA started with service name : %s ",
             service_name ? service_name : "<NULL>");
    return ESP_OK;
}

esp_err_t
esp_ble_ota_endpoint_create(const char *ep_name)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_FAIL;

    ACQUIRE_LOCK(prov_ctx_lock);
    if (prov_ctx &&
            prov_ctx->prov_state == ESP_BLE_OTA_STATE_IDLE) {
        err = prov_ctx->mgr_config.scheme.set_config_endpoint(
                  prov_ctx->prov_scheme_config, ep_name,
                  prov_ctx->endpoint_uuid_used + 1);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create additional endpoint");
    } else {
        prov_ctx->endpoint_uuid_used++;
    }
    RELEASE_LOCK(prov_ctx_lock);
    return err;
}

esp_err_t
esp_ble_ota_endpoint_register(const char *ep_name, protocomm_req_handler_t handler, void *user_ctx)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_FAIL;

    ACQUIRE_LOCK(prov_ctx_lock);
    if (prov_ctx &&
            prov_ctx->prov_state > ESP_BLE_OTA_STATE_STARTING &&
            prov_ctx->prov_state < ESP_BLE_OTA_STATE_STOPPING) {
        err = protocomm_add_endpoint(prov_ctx->pc, ep_name, handler, user_ctx);
    }
    RELEASE_LOCK(prov_ctx_lock);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register handler for endpoint err = %d", err);
    }
    return err;
}

void
esp_ble_ota_endpoint_unregister(const char *ep_name)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return;
    }

    ACQUIRE_LOCK(prov_ctx_lock);
    if (prov_ctx &&
            prov_ctx->prov_state > ESP_BLE_OTA_STATE_STARTING &&
            prov_ctx->prov_state < ESP_BLE_OTA_STATE_STOPPING) {
        protocomm_remove_endpoint(prov_ctx->pc, ep_name);
    }
    RELEASE_LOCK(prov_ctx_lock);
}

static void
ota_stop_task(void *arg)
{
    bool is_this_a_task = (bool) arg;

    esp_ble_ota_cb_func_t app_cb = prov_ctx->mgr_config.app_event_handler.event_cb;
    void *app_data = prov_ctx->mgr_config.app_event_handler.user_data;

    esp_ble_ota_cb_func_t scheme_cb = prov_ctx->mgr_config.scheme_event_handler.event_cb;
    void *scheme_data = prov_ctx->mgr_config.scheme_event_handler.user_data;

    /* This delay is so that the client side app is notified first
     * and then the ota is stopped. Generally 1000ms is enough. */
    uint32_t cleanup_delay = prov_ctx->cleanup_delay > 100 ? prov_ctx->cleanup_delay : 100;
    vTaskDelay(cleanup_delay / portTICK_PERIOD_MS);

    /* All the extra application added endpoints are also
     * removed automatically when ota_stop is called */
    prov_ctx->mgr_config.scheme.ota_stop(prov_ctx->pc);

    /* Delete protocomm instance */
    protocomm_delete(prov_ctx->pc);
    prov_ctx->pc = NULL;

    free(prov_ctx->ota_handlers);
    prov_ctx->ota_handlers = NULL;

    /* Switch device to Wi-Fi STA mode irrespective of
     * whether ota was completed or not */
    ESP_LOGI(TAG, "stopped");

    if (is_this_a_task) {
        ACQUIRE_LOCK(prov_ctx_lock);
        prov_ctx->prov_state = ESP_BLE_OTA_STATE_IDLE;
        RELEASE_LOCK(prov_ctx_lock);

        ESP_LOGD(TAG, "execute_event_cb : %d", OTA_END);
        if (scheme_cb) {
            scheme_cb(scheme_data, OTA_END, NULL);
        }
        if (app_cb) {
            app_cb(app_data, OTA_END, NULL);
        }
        if (esp_event_post(ESP_BLE_OTA_EVENT, OTA_END, NULL, 0, portMAX_DELAY) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to post event ESP_BLE_OTA_END");
        }

        vTaskDelete(NULL);
    }
}

static bool
esp_ble_ota_stop_service(bool blocking)
{
    if (blocking) {
        /* Wait for any ongoing calls to esp_ble_ota_start_service() or
         * esp_ble_ota_stop_service() from another thread to finish */
        while (prov_ctx && (
                    prov_ctx->prov_state == ESP_BLE_OTA_STATE_STARTING ||
                    prov_ctx->prov_state == ESP_BLE_OTA_STATE_STOPPING)) {
            RELEASE_LOCK(prov_ctx_lock);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ACQUIRE_LOCK(prov_ctx_lock);
        }
    } else {
        /* Wait for any ongoing call to esp_ble_ota_start_service()
         * from another thread to finish */
        while (prov_ctx &&
                prov_ctx->prov_state == ESP_BLE_OTA_STATE_STARTING) {
            RELEASE_LOCK(prov_ctx_lock);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ACQUIRE_LOCK(prov_ctx_lock);
        }

        if (prov_ctx && prov_ctx->prov_state == ESP_BLE_OTA_STATE_STOPPING) {
            ESP_LOGD(TAG, "OTA is already stopping");
            return false;
        }
    }

    if (!prov_ctx || prov_ctx->prov_state == ESP_BLE_OTA_STATE_IDLE) {
        ESP_LOGD(TAG, "not running");
        return false;
    }

    /* Timers not needed anymore */
    if (prov_ctx->autostop_timer) {
        esp_timer_stop(prov_ctx->autostop_timer);
        esp_timer_delete(prov_ctx->autostop_timer);
        prov_ctx->autostop_timer = NULL;
    }

    ESP_LOGD(TAG, "Stopping ota");
    prov_ctx->prov_state = ESP_BLE_OTA_STATE_STOPPING;

    /* Free proof of possession */
    if (prov_ctx->protocomm_sec_params) {
        if (prov_ctx->security == 1) {
            // In case of security 1 we keep an internal copy of "pop".
            // Hence free it at this point
            uint8_t *pop = (uint8_t *)((protocomm_security1_params_t *) prov_ctx->protocomm_sec_params)->data;
            free(pop);
        }
        prov_ctx->protocomm_sec_params = NULL;
    }

    /* Remove event handler */
    esp_event_handler_unregister(ESP_BLE_OTA_EVENT, ESP_EVENT_ANY_ID,
                                 esp_ble_ota_event_handler_internal);

    if (blocking) {
        RELEASE_LOCK(prov_ctx_lock);
        ota_stop_task((void *)0);
        ACQUIRE_LOCK(prov_ctx_lock);
        prov_ctx->prov_state = ESP_BLE_OTA_STATE_IDLE;
    } else {
        if (xTaskCreate(ota_stop_task, "ota_stop_task", 4000, (void *)1, tskIDLE_PRIORITY, NULL) != pdPASS) {
            ESP_LOGE(TAG, "Failed to create ota_stop_task!");
            abort();
        }
        ESP_LOGD(TAG, "scheduled for stopping");
    }
    return true;
}

/* Call this if ota is completed before the timeout occurs */
esp_err_t
esp_ble_ota_done(void)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_ble_ota_stop();
    return ESP_OK;
}

esp_err_t
esp_ble_ota_init(esp_ble_ota_config_t config)
{
    if (!prov_ctx_lock) {
        prov_ctx_lock = xSemaphoreCreateMutex();
        if (!prov_ctx_lock) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    void *fn_ptrs[] = {
        config.scheme.ota_stop,
        config.scheme.ota_start,
        config.scheme.new_config,
        config.scheme.delete_config,
        config.scheme.set_config_service,
        config.scheme.set_config_endpoint
    };

    /* All function pointers in the scheme structure must be non-null */
    for (size_t i = 0; i < sizeof(fn_ptrs) / sizeof(fn_ptrs[0]); i++) {
        if (!fn_ptrs[i]) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    ACQUIRE_LOCK(prov_ctx_lock);
    if (prov_ctx) {
        ESP_LOGE(TAG, "manager already initialized");
        RELEASE_LOCK(prov_ctx_lock);
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for ota app data */
    prov_ctx = (struct esp_ble_ota_ctx *) calloc(1, sizeof(struct esp_ble_ota_ctx));
    if (!prov_ctx) {
        ESP_LOGE(TAG, "Error allocating memory for singleton instance");
        RELEASE_LOCK(prov_ctx_lock);
        return ESP_ERR_NO_MEM;
    }

    prov_ctx->mgr_config = config;
    prov_ctx->prov_state = ESP_BLE_OTA_STATE_IDLE;
    prov_ctx->mgr_info.version = ESP_BLE_OTA_MGR_VERSION;

    /* Allocate memory for ota scheme configuration */
    const esp_ble_ota_scheme_t *scheme = &prov_ctx->mgr_config.scheme;
    esp_err_t ret = ESP_OK;
    prov_ctx->prov_scheme_config = scheme->new_config();
    if (!prov_ctx->prov_scheme_config) {
        ESP_LOGE(TAG, "failed to allocate ota scheme configuration");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }

    ret = scheme->set_config_endpoint(prov_ctx->prov_scheme_config, "prov-session", 0xFF51);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure security endpoint");
        goto exit;
    }

    ret = scheme->set_config_endpoint(prov_ctx->prov_scheme_config, "prov-config", 0xFF52);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure Wi-Fi configuration endpoint");
        goto exit;
    }

    ret = scheme->set_config_endpoint(prov_ctx->prov_scheme_config, "proto-ver", 0xFF53);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure version endpoint");
        goto exit;
    }

    ret = scheme->set_config_endpoint(prov_ctx->prov_scheme_config, "recv-fw", 0x8020);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure version endpoint");
        goto exit;
    }

    ret = scheme->set_config_endpoint(prov_ctx->prov_scheme_config, "ota-bar", 0x8021);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure version endpoint");
        goto exit;
    }

    ret = scheme->set_config_endpoint(prov_ctx->prov_scheme_config, "ota-command", 0x8022);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure version endpoint");
        goto exit;
    }

    ret = scheme->set_config_endpoint(prov_ctx->prov_scheme_config, "ota-customer", 0x8023);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure version endpoint");
        goto exit;
    }
    prov_ctx->endpoint_uuid_used = 0xFF54;

    prov_ctx->cleanup_delay = 1000;

exit:
    if (ret != ESP_OK) {
        if (prov_ctx->prov_scheme_config) {
            config.scheme.delete_config(prov_ctx->prov_scheme_config);
        }
        free(prov_ctx);
    }
    RELEASE_LOCK(prov_ctx_lock);
    return ret;
}

void
esp_ble_ota_wait(void)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return;
    }

    while (1) {
        ACQUIRE_LOCK(prov_ctx_lock);
        if (prov_ctx &&
                prov_ctx->prov_state != ESP_BLE_OTA_STATE_IDLE) {
            RELEASE_LOCK(prov_ctx_lock);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        break;
    }
    RELEASE_LOCK(prov_ctx_lock);
}

void
esp_ble_ota_deinit(void)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return;
    }

    ACQUIRE_LOCK(prov_ctx_lock);

    bool service_was_running = esp_ble_ota_stop_service(1);

    if (!service_was_running && !prov_ctx) {
        ESP_LOGD(TAG, "Manager already de-initialized");
        RELEASE_LOCK(prov_ctx_lock);
        vSemaphoreDelete(prov_ctx_lock);
        prov_ctx_lock = NULL;
        return;
    }

    if (prov_ctx->app_info_json) {
        cJSON_Delete(prov_ctx->app_info_json);
    }

    if (prov_ctx->prov_scheme_config) {
        prov_ctx->mgr_config.scheme.delete_config(prov_ctx->prov_scheme_config);
    }

    esp_ble_ota_cb_func_t app_cb = prov_ctx->mgr_config.app_event_handler.event_cb;
    void *app_data = prov_ctx->mgr_config.app_event_handler.user_data;

    esp_ble_ota_cb_func_t scheme_cb = prov_ctx->mgr_config.scheme_event_handler.event_cb;
    void *scheme_data = prov_ctx->mgr_config.scheme_event_handler.user_data;

    /* Free manager context */
    free(prov_ctx);
    prov_ctx = NULL;
    RELEASE_LOCK(prov_ctx_lock);

    /* If a running service was also stopped during de-initialization
     * then ESP_BLE_OTA_END event also needs to be emitted before deinit */
    if (service_was_running) {
        ESP_LOGD(TAG, "execute_event_cb : %d", OTA_END);
        if (scheme_cb) {
            scheme_cb(scheme_data, OTA_END, NULL);
        }
        if (app_cb) {
            app_cb(app_data, OTA_END, NULL);
        }
        if (esp_event_post(ESP_BLE_OTA_EVENT, OTA_END, NULL, 0, portMAX_DELAY) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to post event ESP_BLE_OTA_END");
        }
    }

    ESP_LOGD(TAG, "execute_event_cb : %d", OTA_DEINIT);

    /* Execute deinit event callbacks */
    if (scheme_cb) {
        scheme_cb(scheme_data, OTA_DEINIT, NULL);
    }
    if (app_cb) {
        app_cb(app_data, OTA_DEINIT, NULL);
    }
    if (esp_event_post(ESP_BLE_OTA_EVENT, OTA_DEINIT, NULL, 0, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post event ESP_BLE_OTA_DEINIT");
    }

    vSemaphoreDelete(prov_ctx_lock);
    prov_ctx_lock = NULL;
}

esp_err_t
esp_ble_ota_start(esp_ble_ota_security_t security, const void *esp_ble_ota_sec_params,
                  const char *service_name, const char *service_key)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ACQUIRE_LOCK(prov_ctx_lock);
    if (!prov_ctx) {
        ESP_LOGE(TAG, "manager not initialized");
        RELEASE_LOCK(prov_ctx_lock);
        return ESP_ERR_INVALID_STATE;
    }

    if (prov_ctx->prov_state != ESP_BLE_OTA_STATE_IDLE) {
        ESP_LOGE(TAG, "service already started");
        RELEASE_LOCK(prov_ctx_lock);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    prov_ctx->prov_state = ESP_BLE_OTA_STATE_STARTING;

    fw_buf = (uint8_t *)malloc(4096 * sizeof(uint8_t));
    if (fw_buf == NULL) {
        ESP_LOGE(TAG, "%s -  malloc fail", __func__);
    } else {
        memset(fw_buf, 0x0, 4096);
    }
    fw_buf_offset = 0;

#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_0
    /* Initialize app data */
    if (security == ESP_BLE_OTA_SECURITY_0) {
        prov_ctx->mgr_info.capabilities.no_sec = true;
    }
#endif
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1
    if (security == ESP_BLE_OTA_SECURITY_1) {
        if (esp_ble_ota_sec_params) {
            static protocomm_security1_params_t sec1_params;
            // Generate internal copy of "pop", that shall be freed at the end
            char *pop = strdup(esp_ble_ota_sec_params);
            if (pop == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for pop");
                ret = ESP_ERR_NO_MEM;
                return ret;
            }
            sec1_params.data = (const uint8_t *)pop;
            sec1_params.len = strlen(pop);
            prov_ctx->protocomm_sec_params = (const void *) &sec1_params;
        } else {
            prov_ctx->mgr_info.capabilities.no_pop = true;
        }
    }
#endif
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
    if (security == ESP_BLE_OTA_SECURITY_2) {
        if (esp_ble_ota_sec_params) {
            prov_ctx->protocomm_sec_params = esp_ble_ota_sec_params;
        }
    }
#endif
    prov_ctx->security = security;

    RELEASE_LOCK(prov_ctx_lock);

    /* Start service */
    ret = esp_ble_ota_start_service(service_name, service_key);
    if (ret != ESP_OK) {
        esp_timer_delete(prov_ctx->autostop_timer);
    }
    return ret;
}

void
esp_ble_ota_stop(void)
{
    if (!prov_ctx_lock) {
        ESP_LOGE(TAG, "manager not initialized");
        return;
    }

    ACQUIRE_LOCK(prov_ctx_lock);

    esp_ble_ota_stop_service(0);

    RELEASE_LOCK(prov_ctx_lock);
}
