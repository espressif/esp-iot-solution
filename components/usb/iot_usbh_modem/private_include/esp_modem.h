/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_event.h"
#include "esp_netif.h"
#include "driver/uart.h"
#include "iot_usbh_cdc.h"

/**
 * @brief Forward declare DTE and DCE objects
 *
 */
typedef struct esp_modem_dte esp_modem_dte_t;
typedef struct esp_modem_dce esp_modem_dce_t;

/**
 * @brief Declare Event Base for ESP Modem
 *
 */
ESP_EVENT_DECLARE_BASE(ESP_MODEM_EVENT);

/**
 * @brief ESP Modem Event
 *
 */
typedef enum {
    ESP_MODEM_EVENT_PPP_START = 0,       /*!< ESP Modem Start PPP Session */
    ESP_MODEM_EVENT_PPP_RESTART = 1,     /*!< ESP Modem Restart PPP Session */
    ESP_MODEM_EVENT_PPP_STOP  = 3,       /*!< ESP Modem Stop PPP Session*/
    ESP_MODEM_EVENT_UNKNOWN   = 4        /*!< ESP Modem Unknown Response */
} esp_modem_event_t;

/**
 * @defgroup ESP_MODEM_DTE_TYPES DTE Types
 * @brief  Configuration and related types used to init and setup a new DTE object
 */

/** @addtogroup ESP_MODEM_DTE_TYPES
 * @{
 */

/**
 * @brief Modem flow control type
 *
 */
typedef enum {
    ESP_MODEM_FLOW_CONTROL_NONE = 0,
    ESP_MODEM_FLOW_CONTROL_SW,
    ESP_MODEM_FLOW_CONTROL_HW
} esp_modem_flow_ctrl_t;

/**
 * @brief ESP Modem DTE Configuration
 *
 */
typedef struct {
    uart_port_t port_num;           /*!< UART port number */
    uart_word_length_t data_bits;   /*!< Data bits of UART */
    uart_stop_bits_t stop_bits;     /*!< Stop bits of UART */
    uart_parity_t parity;           /*!< Parity type */
    esp_modem_flow_ctrl_t flow_control; /*!< Flow control type */
    uint32_t baud_rate;             /*!< Communication baud rate */
    int tx_io_num;                  /*!< TXD Pin Number */
    int rx_io_num;                  /*!< RXD Pin Number */
    int rts_io_num;                 /*!< RTS Pin Number */
    int cts_io_num;                 /*!< CTS Pin Number */
    int rx_buffer_size;             /*!< UART RX Buffer Size */
    int tx_buffer_size;             /*!< UART TX Buffer Size */
    int pattern_queue_size;         /*!< UART Pattern Queue Size */
    int event_queue_size;           /*!< UART Event Queue Size */
    uint32_t event_task_stack_size; /*!< UART Event Task Stack size */
    int event_task_priority;        /*!< UART Event Task Priority */
    int line_buffer_size;           /*!< Line buffer size for command mode */
    usbh_cdc_cb_t conn_callback;
    usbh_cdc_cb_t disconn_callback;
} esp_modem_dte_config_t;

/**
 * @brief ESP Modem DTE Default Configuration
 *
 */
#define ESP_MODEM_DTE_DEFAULT_CONFIG()          \
    {                                           \
        .port_num = UART_NUM_1,                 \
        .data_bits = UART_DATA_8_BITS,          \
        .stop_bits = UART_STOP_BITS_1,          \
        .parity = UART_PARITY_DISABLE,          \
        .baud_rate = 115200,                    \
        .flow_control = ESP_MODEM_FLOW_CONTROL_NONE,\
        .tx_io_num = 25,                        \
        .rx_io_num = 26,                        \
        .rts_io_num = 27,                       \
        .cts_io_num = 23,                       \
        .rx_buffer_size = 1024,                 \
        .tx_buffer_size = 512,                  \
        .pattern_queue_size = 20,               \
        .event_queue_size = 30,                 \
        .event_task_stack_size = 2048,          \
        .event_task_priority = 5,               \
        .line_buffer_size = 512,                \
        .conn_callback = NULL,                  \
        .disconn_callback = NULL,               \
    }

/**
 * @defgroup ESP_MODEM_DCE_TYPES DCE Types
 * @brief  Configuration and related types used to init and setup a new DCE object
 */

/** @addtogroup ESP_MODEM_DCE_TYPES
 * @{
 */

/**
 * @brief PDP context type used as an input parameter to esp_modem_dce_set_pdp_context
 * also used as a part of configuration structure
 */
typedef struct esp_modem_dce_pdp_ctx_s {
    size_t cid;             /*!< PDP context identifier */
    const char *type;       /*!< Protocol type */
    const char *apn;        /*!< Modem APN (Access Point Name, a logical name to choose data network) */
} esp_modem_dce_pdp_ctx_t;

/**
 * @brief Devices that the DCE will act as
 *
 */
typedef enum esp_modem_dce_device_e {
    ESP_MODEM_DEVICE_UNSPECIFIED,
} esp_modem_dce_device_t;

/**
 * @brief DCE's configuration structure
 */
typedef struct esp_modem_dce_config_s {
    esp_modem_dce_pdp_ctx_t pdp_context;    /*!<  modem PDP context including APN */
    bool populate_command_list;             /*!<  use command list interface: Setting this to true creates
                                                  a list of supported AT commands enabling sending
                                                  these commands, but will occupy data memory */
    esp_modem_dce_device_t device;          /*!<  predefined device enum that the DCE will initialise as */
} esp_modem_dce_config_t;

/**
* @brief Default configuration of DCE unit of ESP-MODEM
*
*/
#define ESP_MODEM_DCE_DEFAULT_CONFIG(APN) \
    {                                  \
        .pdp_context = {               \
            .cid = 1,                  \
            .type = "IP",              \
            .apn = APN },              \
        .populate_command_list = true,\
        .device = ESP_MODEM_DEVICE_UNSPECIFIED   \
    }

/**
 * @defgroup ESP_MODEM_DTE_DCE DCE and DCE object init and setup API
 * @brief  Creating and init objects of DTE and DCE
 */

/** @addtogroup ESP_MODEM_DTE_DCE
 * @{
 */

/**
 * @brief Create and initialize Modem DTE object
 *
 * @param config configuration of ESP Modem DTE object
 * @return modem_dte_t*
 *      - Modem DTE object
 */
esp_modem_dte_t *esp_modem_dte_new(const esp_modem_dte_config_t *config);

/**
 * @brief Initialize the DCE object that has already been created
 *
 * This API is typically used to initialize extended DCE object,
 * "sub-class" of esp_modem_dce_t
 *
 * @param config Configuration for DCE object
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on error (init issue, set specific command issue)
 *      - ESP_ERR_INVALID_ARG on invalid parameters
 */
esp_err_t esp_modem_dce_init(esp_modem_dce_t *dce, esp_modem_dce_config_t *config);

/**
 * @}
 */

/**
 * @defgroup ESP_MODEM_EVENTS Event handling API
 */

/** @addtogroup ESP_MODEM_EVENTS
 * @{
 */

/**
 * @brief Register event handler for ESP Modem event loop
 *
 * @param dte modem_dte_t type object
 * @param handler event handler to register
 * @param handler_args arguments for registered handler
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_NO_MEM on allocating memory for the handler failed
 *      - ESP_ERR_INVALID_ARG on invalid combination of event base and event id
 */
esp_err_t esp_modem_set_event_handler(esp_modem_dte_t *dte, esp_event_handler_t handler, int32_t event_id, void *handler_args);

/**
 * @brief Unregister event handler for ESP Modem event loop
 *
 * @param dte modem_dte_t type object
 * @param handler event handler to unregister
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG on invalid combination of event base and event id
 */
esp_err_t esp_modem_remove_event_handler(esp_modem_dte_t *dte, esp_event_handler_t handler);

esp_err_t esp_modem_post_event(esp_modem_dte_t *dte, int32_t event_id, void* event_data, size_t event_data_size, TickType_t ticks_to_wait);

/**
 * @defgroup ESP_MODEM_LIFECYCLE Modem lifecycle API
 * @brief  Basic modem API to start/stop the PPP mode
 */

/** @addtogroup ESP_MODEM_LIFECYCLE
 * @{
 */

/**
 * @brief Setup PPP Session
 *
 * @param dte Modem DTE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_start_ppp(esp_modem_dte_t *dte);

/**
 * @brief Exit PPP Session
 *
 * @param dte Modem DTE Object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_stop_ppp(esp_modem_dte_t *dte);

/**
 * @brief Basic start of the modem. This API performs default dce's start_up() function
 *
 * @param dte Modem DTE Object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_INVALID_ARG on invalid arguments
 */
esp_err_t esp_modem_default_start(esp_modem_dte_t *dte);

/**
 * @brief Basic attach operation of modem sub-elements
 *
 * This API binds the supplied DCE and netif to the modem's DTE and initializes the modem
 *
 * @param dte Modem DTE Object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t esp_modem_default_attach(esp_modem_dte_t *dte, esp_modem_dce_t *dce, esp_netif_t* ppp_netif);

/**
 * @brief Basic destroy operation of the modem DTE and all the sub-elements bound to it
 *
 * This API deletes the DCE, modem netif adapter as well as the esp_netif supplied to
 * esp_modem_default_attach(). Then it deletes the DTE itself.
 *
 * @param dte Modem DTE Object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 *      - ESP_ERR_INVALID_ARG on invalid arguments
 */
esp_err_t esp_modem_default_destroy(esp_modem_dte_t *dte);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
