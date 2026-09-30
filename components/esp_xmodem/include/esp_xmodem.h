/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_xmodem *esp_xmodem_handle_t;

typedef enum {
    ESP_XMODEM_CHECKSUM,
    ESP_XMODEM_CRC16,
} esp_xmodem_crc_type_t;

typedef enum {
    ESP_XMODEM_SENDER,
    ESP_XMODEM_RECEIVER,
} esp_xmodem_role_t;

typedef enum {
    ESP_XMODEM_EVENT_INIT,
    ESP_XMODEM_EVENT_ERROR,
    ESP_XMODEM_EVENT_CONNECTED,
    ESP_XMODEM_EVENT_FINISHED,
    ESP_XMODEM_EVENT_ON_SEND_DATA,
    ESP_XMODEM_EVENT_ON_RECEIVE_DATA,
    ESP_XMODEM_EVENT_ON_FILE,
} esp_xmodem_event_id_t;

typedef struct esp_xmodem_event {
    esp_xmodem_handle_t handle;             /*!< The Xmodem handle */
    esp_xmodem_event_id_t event_id;         /*!< event_id for esp_xmodem_event_id_t */
    void *data;                             /*!< data of the event */
    uint32_t data_len;                      /*!< data length of data */
} esp_xmodem_event_t;

typedef esp_err_t (*xmodem_event_handle_cb)(esp_xmodem_event_t *evt);
typedef esp_err_t (*xmodem_data_handle_cb)(uint8_t *data, uint32_t len);

typedef struct {
    esp_xmodem_role_t     role;                     /*!< The xmodem role.(default ESP_XMODEM_SENDER) */
    esp_xmodem_crc_type_t crc_type;                 /*!< The crc that Xmodem use. This crc_type is not useful for sender mode(default checksum). This config is only for ESP_XMODEM_RECEIVER */
    int                   timeout_ms;               /*!< For receiver, The timeout(ms) that wait sender response data(default 1s) */
                                                    /*!< For sender, The timeout(ms) that wait receiver response data(default 1s) */
    int                   max_retry;                /*!< For receiver, The max retry count to get sender response data(default 10 counts) */
                                                    /*!< For sender, The max retry count to get receiver response data(default 10 counts) */
    int                   cycle_timeout_ms;         /*!< For receiver, The timeout(ms) that send 'C' or NAK for sender response data(default 3s) */
                                                    /*!< For sender, The timeout(ms) that wait 'C' or NAK for receiver send(default 10s) */
    int                   cycle_max_retry;          /*!< For receiver, The max retry count that send 'C' or NAK for sender response data(default 25 counts) */
                                                    /*!< For sender, The max retry count that wait 'C' or NAK(default 25 counts) */
    bool                  support_xmodem_1k;        /*!< Support 1024 byte transport(default false). This config is only for ESP_XMODEM_SENDER */
    int                   user_data_size;           /*!< Configure for extra user data size */
    xmodem_event_handle_cb event_handler;           /*!< Event handler callback */
    xmodem_data_handle_cb recv_cb;                 /*!< Xmodem data receive callback handle. This config is only for ESP_XMODEM_RECEIVER */
} esp_xmodem_config_t;

#define ESP_ERR_XMODEM_BASE             (0x9000)                    /*!< Starting number of Xmodem error codes */
#define ESP_ERR_NO_XMODEM_RECEIVER      (ESP_ERR_XMODEM_BASE + 1)   /*!< No Xmodem receiver found */
#define ESP_ERR_NO_XMODEM_SENDER        (ESP_ERR_XMODEM_BASE + 2)   /*!< No Xmodem sender found */
#define ESP_ERR_XMODEM_TIMEOUT          (ESP_ERR_XMODEM_BASE + 3)   /*!< Xmodem data receive timeout */
#define ESP_ERR_XMODEM_MAX_RETRY        (ESP_ERR_XMODEM_BASE + 4)   /*!< Xmodem retry reach the max fail count */
#define ESP_ERR_XMODEM_RECEIVE_CAN      (ESP_ERR_XMODEM_BASE + 5)   /*!< Xmodem sender/receiver receive peer CAN data */
#define ESP_ERR_XMODEM_DATA_CRC_ERROR   (ESP_ERR_XMODEM_BASE + 6)   /*!< Xmodem data crc16 or checksum error */
#define ESP_ERR_XMODEM_STATE            (ESP_ERR_XMODEM_BASE + 7)   /*!< Xmodem module state is not right */
#define ESP_ERR_XMODEM_CRC_NOT_SUPPORT  (ESP_ERR_XMODEM_BASE + 8)   /*!< Xmodem crc not support */
#define ESP_ERR_XMODEM_DATA_SEND_ERROR  (ESP_ERR_XMODEM_BASE + 9)   /*!< Xmodem data send error */
#define ESP_ERR_XMODEM_DATA_NUM_ERROR   (ESP_ERR_XMODEM_BASE + 10)  /*!< Xmodem data number error */
#define ESP_ERR_XMODEM_DATA_RECV_ERROR  (ESP_ERR_XMODEM_BASE + 11)  /*!< Xmodem data recvive error */

/**
 * @brief      Start a Xmodem sender/receiver session. Depend on role value.
 *             Before this function, please first call esp_xmodem_transport_init,
 *             and it returns a esp_xmodem_handle_t that you must use as input to other functions in the interface.
 *             This call MUST have a corresponding call to esp_xmodem_cleanup when the operation is complete.
 *
 * @param[in]  config             The configurations, see `esp_xmodem_config_t`
 * @param[in]  transport_config   The transport configurations, return by esp_xmodem_transport_init
 *
 * @return
 *     - `esp_xmodem_handle_t`
 *     - NULL if any errors
 */
esp_xmodem_handle_t esp_xmodem_init(const esp_xmodem_config_t *config, void *transport_config);

/**
 * @brief      Start a Xmodem sender/receiver session.
 *             This function must be called after esp_xmodem_init,
 *             and it returns the result of connect.
 *
 * @param[in]  handle   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 */

esp_err_t esp_xmodem_start(esp_xmodem_handle_t handle);

/**
 * @brief      Send the data to Xmodem receiver
 *
 * @param[in]  sender   The esp_xmodem_handle_t handle
 * @param[in]  data     The data to send
 * @param[in]  len      The data len
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_sender_send(esp_xmodem_handle_t sender, uint8_t *data, uint32_t len);

/**
 * @brief      Support send file to receiver. Must call it first when the sender is connected to receiver
 *
 * @param[in]  sender   The esp_xmodem_handle_t handle
 * @param[in]  sender   The file name
 * @param[in]  sender   The length of file name
 * @param[in]  sender   The total length of file
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_sender_send_file_packet(esp_xmodem_handle_t sender, char *filename, int filename_length, uint32_t total_length);

/**
 * @brief      Send cancel(CAN) data to receiver
 *
 * @param[in]  sender   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_sender_send_cancel(esp_xmodem_handle_t sender);

/**
 * @brief      Send finish(EOT) data to receiver
 *
 * @param[in]  sender   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_sender_send_eot(esp_xmodem_handle_t sender);

/**
 * @brief      Stop and free the resource for Xmodem sender or receiver
 *
 * @param[in]  handle   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_clean(esp_xmodem_handle_t handle);

/**
 * @brief      Stop Xmodem sender or receiver. If want to start xmodem again, only need call `esp_xmodem_start`
 *
 * @param[in]  handle   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_stop(esp_xmodem_handle_t handle);

/**
 * @brief      Get the Xmodem handle error code
 *
 * @param[in]  handle   The esp_xmodem_handle_t handle
 *
 * @return
 *     - error code
 */
int esp_xmodem_get_error_code(esp_xmodem_handle_t handle);

#ifdef __cplusplus
}
#endif
