/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "iot_eth.h"
#include "iot_eth_types.h"
#include "iot_eth_interface.h"
#include "iot_eth_netif_glue.h"
#include "esp_modem_dte.h"
#include "dte_helper.h"
#include "iot_usbh_modem.h"
#include "at_3gpp_ts_27_007.h"

static const char *TAG = "modem_board";

static const int MODEM_DESTROY_BIT       = BIT0;
static const int MODEM_DESTROY_DONE_BIT  = BIT1;
static const int MODEM_NEW_STAGE_BIT     = BIT2;
static const int MODEM_IDLE_BIT          = BIT4;

typedef enum {
    STAGE_DTE_LOSS,    /* dte loss, restoring to command state */
    STAGE_IDLE,      /* in command state, waiting ppp on event */
    STAGE_SYNC,         /* trying sync using AT modem */
    STAGE_STOP_PPP,     /* restoring to command state */
    STAGE_START_PPP,    /* trying dial-up */
    STAGE_RUNNING,      /* perfect, enjoy network */
    STAGE_ERROR,      /* error stage */
} modem_stage_t;

typedef struct {
    modem_stage_t code;
    const char *msg;
} _modem_stage_msg_t;

static const _modem_stage_msg_t modem_stage_msg_table[] = {
#define MODEM_STAGE_CODE2STR(code) {code, #code}
    MODEM_STAGE_CODE2STR(STAGE_DTE_LOSS),
    MODEM_STAGE_CODE2STR(STAGE_IDLE),
    MODEM_STAGE_CODE2STR(STAGE_SYNC),
    MODEM_STAGE_CODE2STR(STAGE_STOP_PPP),
    MODEM_STAGE_CODE2STR(STAGE_START_PPP),
    MODEM_STAGE_CODE2STR(STAGE_RUNNING),
    MODEM_STAGE_CODE2STR(STAGE_ERROR),
#undef MODEM_STAGE_CODE2STR
};

typedef struct {
    iot_eth_driver_t *dte_drv;
    esp_netif_t *ppp_netif;
    iot_eth_handle_t eth_handle;

    modem_stage_t modem_stage;  /*!< Modem stage */
    bool auto_connect;
    EventGroupHandle_t evt_hdl;
    iot_eth_netif_glue_handle_t glue;
} modem_board_t;

static modem_board_t *g_modem_board;

static const char *MODEM_STAGE_STR(int code)
{
    size_t i;
    for (i = 0; i < sizeof(modem_stage_msg_table) / sizeof(modem_stage_msg_table[0]); ++i) {
        if (modem_stage_msg_table[i].code == code) {
            return modem_stage_msg_table[i].msg;
        }
    }
    return "unknown";
}

static esp_err_t modem_sync_state(iot_eth_driver_t *dte_drv)
{
    at_handle_t atparser = esp_modem_dte_get_atparser(dte_drv);

    // Try to set the modem to command mode first
    esp_err_t ret = esp_modem_dte_sync(dte_drv);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to change to command mode");

    ret = at_cmd_set_echo(atparser, true);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to enable echo");

    // Print modem information
    char str[64] = {0};
    at_cmd_get_manufacturer_id(atparser, str, sizeof(str));
    ESP_LOGI(TAG, "Modem manufacturer ID: %s", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_module_id(atparser, str, sizeof(str));
    ESP_LOGI(TAG, "Modem module ID: %s", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_revision_id(atparser, str, sizeof(str));
    ESP_LOGI(TAG, "Modem revision ID: %s", str);
    return ESP_OK;
}

static void _usb_dte_conn_callback(iot_eth_driver_t *dte_drv, void *arg)
{
    ESP_LOGI(TAG, "DTE connected");
    g_modem_board->modem_stage = STAGE_SYNC;
    xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_NEW_STAGE_BIT);
    modem_at_start(esp_modem_dte_get_atparser(dte_drv));
}

static void _usb_dte_disconn_callback(iot_eth_driver_t *dte_drv, void *arg)
{
    ESP_LOGI(TAG, "DTE disconnected");
    if (g_modem_board->modem_stage == STAGE_RUNNING) {
        g_modem_board->modem_stage = STAGE_STOP_PPP;
    } else {
        g_modem_board->modem_stage = STAGE_DTE_LOSS;
    }
    xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_NEW_STAGE_BIT);
    modem_at_stop(esp_modem_dte_get_atparser(dte_drv));
}

static void _modem_daemon_task(void *param)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    esp_err_t ret = ESP_OK;

    g_modem_board->modem_stage = STAGE_DTE_LOSS;
    while (true) {
        /********************************** handle external event *********************************************************/
        EventBits_t bits = xEventGroupWaitBits(g_modem_board->evt_hdl, (MODEM_NEW_STAGE_BIT | MODEM_DESTROY_BIT), pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & MODEM_DESTROY_BIT) {
            break; // destroy task
        }

        /************************************ Processing stage **********************************/
        ESP_LOGI(TAG, "Handling stage = %s", MODEM_STAGE_STR(g_modem_board->modem_stage));
        iot_eth_driver_t *dte_drv = g_modem_board->dte_drv;

        if (g_modem_board->modem_stage == STAGE_IDLE) {
            xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_IDLE_BIT); // set idle bit
        } else {
            xEventGroupClearBits(g_modem_board->evt_hdl, MODEM_IDLE_BIT); // clear idle bit
        }

        switch (g_modem_board->modem_stage) {
        case STAGE_DTE_LOSS:
            ESP_LOGI(TAG, "Modem DTE loss, ...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        case STAGE_IDLE:
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case STAGE_SYNC:
            ret = modem_sync_state(dte_drv);
            ESP_GOTO_ON_ERROR(ret, _ppp_abort, TAG, "Modem sync failed!");
            if (g_modem_board->auto_connect) {
                ESP_LOGI(TAG, "Modem auto connect enabled, starting PPP...");
                g_modem_board->modem_stage = STAGE_START_PPP;
            } else {
                g_modem_board->modem_stage = STAGE_IDLE;
            }
            goto _stage_succeed;
            break;
        case STAGE_STOP_PPP: {
            iot_eth_stop(g_modem_board->eth_handle); // stop ppp
            vTaskDelay(pdMS_TO_TICKS(300));
            /**
             * When the ppp netif is stopped, the 4G module may take some time to process the termination of the PPP connection.
             * Some modules may return to command mode automatically, while others may require an explicit AT command to exit PPP mode.
             */
            bool exit_ppp_with_at_cmd = false;
#ifdef CONFIG_MOEDM_EXIT_PPP_WITH_AT_CMD
            exit_ppp_with_at_cmd = true;
#endif
            // if the dte is still connected, change to command mode
            if (esp_modem_dte_is_connected(dte_drv)) {
                esp_modem_dte_change_port_mode(dte_drv, ESP_MODEM_COMMAND_MODE, exit_ppp_with_at_cmd); // change to command mode first
                ret = esp_modem_dte_hang_up(dte_drv); // hang up
                ESP_GOTO_ON_ERROR(ret, _ppp_abort, TAG, "Failed to stop PPP");
                g_modem_board->modem_stage = STAGE_IDLE;
            } else {
                esp_modem_dte_on_stage_changed(dte_drv, IOT_ETH_LINK_DOWN); // notify link down
                vTaskDelay(pdMS_TO_TICKS(100));
                g_modem_board->modem_stage = STAGE_DTE_LOSS;
            }
            goto _stage_succeed;
            break;
        }
        case STAGE_START_PPP: {
            at_handle_t at_parser = esp_modem_dte_get_atparser(g_modem_board->dte_drv);
            ESP_LOGI(TAG, "Check SIM card state...");
            esp_modem_pin_state_t pin_state;
            DTE_RETRY_OPERATION(at_cmd_read_pin(at_parser, &pin_state) == ESP_OK && pin_state == PIN_READY, !esp_modem_dte_is_connected(dte_drv));
            ESP_GOTO_ON_FALSE(pin_state == PIN_READY, 0, _ppp_abort, TAG, "SIM card not ready!");

            ESP_LOGI(TAG, "Check signal quality...");
            esp_modem_at_csq_t result;
            DTE_RETRY_OPERATION(at_cmd_get_signal_quality(at_parser, &result) == ESP_OK && result.rssi > CONFIG_MODEM_RSSI_THRESHOLD && result.rssi < 99 && result.ber <= 99,
                                !esp_modem_dte_is_connected(dte_drv));
            ESP_GOTO_ON_FALSE(result.rssi > CONFIG_MODEM_RSSI_THRESHOLD && result.rssi < 99 && result.ber <= 99,
                              0, _ppp_abort, TAG, "Modem signal quality not ready! rssi=%d, ber=%d", result.rssi, result.ber);

            char str[128] = {0};
            at_cmd_get_pdp_context(at_parser, str, sizeof(str));
            ESP_LOGI(TAG, "PDP context: \"%s\"", str);

            ESP_LOGI(TAG, "Check network registration...");
            esp_modem_at_cereg_t _cereg = {0};
            DTE_RETRY_OPERATION(at_cmd_get_network_reg_status(at_parser, &_cereg) == ESP_OK && _cereg.stat == 1, !esp_modem_dte_is_connected(dte_drv));
            ESP_GOTO_ON_FALSE(_cereg.stat == 1, 0, _ppp_abort, TAG, "Modem network registration not ready!");

            ESP_LOGI(TAG, "PPP Dial up...");
            ret = esp_modem_dte_dial_up(dte_drv);
            if (ret == ESP_OK) {
                ESP_GOTO_ON_ERROR(iot_eth_start(g_modem_board->eth_handle), _ppp_abort, TAG, "Failed to start PPP driver");
                esp_modem_dte_on_stage_changed(dte_drv, IOT_ETH_LINK_UP);
                g_modem_board->modem_stage = STAGE_RUNNING;
                goto _stage_succeed;
            }
_ppp_abort:
            g_modem_board->modem_stage = STAGE_ERROR;
            xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_NEW_STAGE_BIT);
            break;
        }
        case STAGE_RUNNING:
            ESP_LOGI(TAG, "Modem is running");
            break;

        case STAGE_ERROR:
            ESP_LOGE(TAG, "Modem in error state");
            break;
        default:
            assert(0); //no stage get in here
_stage_succeed:
            //add delay between each stage
            vTaskDelay(pdMS_TO_TICKS(100));
            xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_NEW_STAGE_BIT);
            break;
        }
    }

    ESP_LOGI(TAG, "Modem Daemon Task Deleted!");
    xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_DESTROY_DONE_BIT);
    vTaskDelete(NULL);
}

esp_err_t usbh_modem_install(const usbh_modem_config_t *config)
{
    ESP_RETURN_ON_FALSE(g_modem_board == NULL, ESP_ERR_INVALID_STATE, TAG, "Modem has been installed");
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Modem config can not be NULL");
    ESP_RETURN_ON_FALSE(config->modem_id_list != NULL, ESP_ERR_INVALID_ARG, TAG, "Modem ID list can not be NULL");
    ESP_RETURN_ON_FALSE(config->at_tx_buffer_size, ESP_ERR_INVALID_ARG, TAG, "AT TX buffer size can not be 0");
    ESP_RETURN_ON_FALSE(config->at_rx_buffer_size, ESP_ERR_INVALID_ARG, TAG, "AT RX buffer size can not be 0");

    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "iot_usbh_modem, version: %d.%d.%d", IOT_USBH_MODEM_VER_MAJOR, IOT_USBH_MODEM_VER_MINOR, IOT_USBH_MODEM_VER_PATCH);
    g_modem_board = calloc(1, sizeof(modem_board_t));
    ESP_RETURN_ON_FALSE(g_modem_board != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate modem board");

    g_modem_board->evt_hdl = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(g_modem_board->evt_hdl != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to create event group");

    // init the USB DTE
    esp_modem_dte_config_t dte_config = {
        .modem_id_list = config->modem_id_list,
        .cbs = {
            .connect = _usb_dte_conn_callback,
            .disconnect = _usb_dte_disconn_callback,
            .user_data = NULL,
        },
        .at_tx_buffer_size = config->at_tx_buffer_size,
        .at_rx_buffer_size = config->at_rx_buffer_size,
    };
    iot_eth_driver_t *ppp_eth_driver = NULL;
    ESP_GOTO_ON_ERROR(esp_modem_dte_new(&dte_config, &ppp_eth_driver), err, TAG, "Failed to create modem DTE");
    g_modem_board->dte_drv = ppp_eth_driver;

    iot_eth_config_t eth_cfg = {
        .driver = ppp_eth_driver,
        .stack_input = NULL,
    };
    ESP_GOTO_ON_ERROR(iot_eth_install(&eth_cfg, &g_modem_board->eth_handle), err, TAG, "Failed to install USB PPP driver");

    g_modem_board->glue = iot_eth_new_netif_glue(g_modem_board->eth_handle);
    ESP_GOTO_ON_FALSE(g_modem_board->glue != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to create netif glue");
    esp_netif_inherent_config_t _inherent_eth_config = ESP_NETIF_INHERENT_DEFAULT_PPP();
    _inherent_eth_config.if_key = "USB PPP";
    _inherent_eth_config.if_desc = "USB PPP";
    esp_netif_config_t netif_cfg = {
        .base = &_inherent_eth_config,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP,
    };
    g_modem_board->ppp_netif = esp_netif_new(&netif_cfg);
    ESP_GOTO_ON_FALSE(g_modem_board->ppp_netif != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to create netif");
    esp_netif_attach(g_modem_board->ppp_netif, g_modem_board->glue);

    // check if PPP error events are enabled, if not, do enable the error occurred/state changed
    // to notify the modem layer when switching modes
    esp_netif_ppp_config_t ppp_config = {
        .ppp_error_event_enabled = true,
        .ppp_phase_event_enabled = true,
    };
    esp_netif_ppp_set_params(g_modem_board->ppp_netif, &ppp_config);

    usbh_modem_ppp_auto_connect(true); // enable auto connect by default

    /* Create Modem Daemon task */
    TaskHandle_t daemon_task_handle = NULL;
    xTaskCreate(_modem_daemon_task, "modem_daemon", 1024 * 3, NULL, 5, &daemon_task_handle);
    ESP_GOTO_ON_FALSE(daemon_task_handle != NULL, ESP_ERR_NO_MEM, err, TAG, "Failed to create Modem Daemon Task");
    xTaskNotifyGive(daemon_task_handle);
    return ESP_OK;

err:
    if (g_modem_board->glue) {
        iot_eth_del_netif_glue(g_modem_board->glue);
    }
    if (g_modem_board->eth_handle) {
        iot_eth_uninstall(g_modem_board->eth_handle);
    }
    if (g_modem_board->ppp_netif) {
        esp_netif_destroy(g_modem_board->ppp_netif);
    }
    if (g_modem_board->evt_hdl != NULL) {
        vEventGroupDelete(g_modem_board->evt_hdl);
    }
    free(g_modem_board);
    g_modem_board = NULL;
    return ret;
}

esp_err_t usbh_modem_uninstall(void)
{
    ESP_RETURN_ON_FALSE(g_modem_board != NULL, ESP_ERR_INVALID_STATE, TAG, "Modem board not initialized");
    ESP_RETURN_ON_FALSE(g_modem_board->evt_hdl != NULL, ESP_ERR_INVALID_STATE, TAG, "Modem not init");

    if (g_modem_board->modem_stage == STAGE_RUNNING) {
        usbh_modem_ppp_stop();
    }
    iot_eth_uninstall(g_modem_board->eth_handle);
    iot_eth_del_netif_glue(g_modem_board->glue);
    esp_netif_destroy(g_modem_board->ppp_netif);

    xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_DESTROY_BIT);
    xEventGroupWaitBits(g_modem_board->evt_hdl, MODEM_DESTROY_DONE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    vEventGroupDelete(g_modem_board->evt_hdl);
    free(g_modem_board);
    g_modem_board = NULL;
    return ESP_OK;
}

esp_err_t usbh_modem_ppp_start(TickType_t timeout)
{
    ESP_RETURN_ON_FALSE(g_modem_board != NULL, ESP_ERR_INVALID_STATE, TAG, "Modem board not initialized");
    ESP_RETURN_ON_FALSE(g_modem_board->modem_stage != STAGE_RUNNING, ESP_ERR_INVALID_STATE, TAG, "PPP is already running, cannot start PPP again!");
    ESP_RETURN_ON_FALSE(g_modem_board->auto_connect == false, ESP_ERR_INVALID_STATE, TAG, "Modem auto connect is enabled, this function should not be called!");

    //wait for modem idle
    EventBits_t bits = xEventGroupWaitBits(g_modem_board->evt_hdl, MODEM_IDLE_BIT, pdFALSE, pdFALSE, timeout);
    ESP_RETURN_ON_FALSE(bits & MODEM_IDLE_BIT, ESP_ERR_TIMEOUT, TAG, "Modem not idle after timeout %"PRIu32" ms", pdMS_TO_TICKS(timeout));

    g_modem_board->modem_stage = STAGE_START_PPP;
    xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_NEW_STAGE_BIT);
    return ESP_OK;
}

esp_err_t usbh_modem_ppp_stop()
{
    ESP_RETURN_ON_FALSE(g_modem_board != NULL, ESP_ERR_INVALID_STATE, TAG, "Modem board not initialized");
    ESP_RETURN_ON_FALSE(g_modem_board->modem_stage == STAGE_RUNNING, ESP_ERR_INVALID_STATE, TAG, "Modem stage is not PPP, cannot stop PPP now!");

    g_modem_board->modem_stage = STAGE_STOP_PPP;
    xEventGroupSetBits(g_modem_board->evt_hdl, MODEM_NEW_STAGE_BIT);
    xEventGroupWaitBits(g_modem_board->evt_hdl, MODEM_IDLE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t usbh_modem_ppp_auto_connect(bool enable)
{
    ESP_RETURN_ON_FALSE(g_modem_board != NULL, ESP_ERR_INVALID_STATE, TAG, "Modem board not initialized");
    g_modem_board->auto_connect = enable;
    return ESP_OK;
}

esp_netif_t *usbh_modem_get_netif()
{
    ESP_RETURN_ON_FALSE(g_modem_board != NULL, NULL, TAG, "Modem board not initialized");
    return g_modem_board->ppp_netif;
}

at_handle_t usbh_modem_get_atparser()
{
    ESP_RETURN_ON_FALSE(g_modem_board != NULL, NULL, TAG, "Modem board not initialized");
    ESP_RETURN_ON_FALSE(g_modem_board->dte_drv != NULL, NULL, TAG, "Modem DTE driver not initialized");
    return esp_modem_dte_get_atparser(g_modem_board->dte_drv);
}
