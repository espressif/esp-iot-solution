/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_xmodem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CAN                     (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */
#define XMODEM_DATA_LEN         128     /* Normal Xmodem data length */
#define XMODEM_1K_DATA_LEN      1024    /* 1k Xmodem data length */
#define XMODEM_HEAD_LEN         3       /* SOH + BLK + 255 - BLK */

typedef enum {
    XMODEM_STATE_UNINIT = 0,
    XMODEM_STATE_INIT,
    XMODEM_STATE_CONNECTING,
    XMODEM_STATE_CONNECTED,
    XMODEM_STATE_SENDER_SEND_DATA,
    XMODEM_STATE_SENDER_SEND_EOT,
    XMODEM_STATE_RECEIVER_RECEIVE_DATA,
    XMODEM_STATE_RECEIVER_RECEIVE_EOT,
    XMODEM_STATE_FINISH,
} esp_xmodem_state_t;

struct esp_xmodem_transport {
    int uart_num;
    int recv_timeout;
    QueueHandle_t  uart_queue;
    TaskHandle_t   uart_task_handle;
};

typedef struct esp_xmodem_transport esp_xmodem_transport_t;

typedef struct esp_xmodem_file_data {
    char filename[64];
    uint32_t file_length;
} esp_xmodem_file_data_t;

/* Xmodem class*/
struct esp_xmodem {
    esp_xmodem_transport_t *transport;
    void *config;
    esp_xmodem_role_t role;
    xmodem_event_handle_cb event_handler;
    esp_xmodem_event_t     event;
    TaskHandle_t process_handle;
    esp_xmodem_crc_type_t crc_type;
    esp_xmodem_file_data_t file_data;
    uint32_t pack_num;
    int error_code;
    esp_xmodem_state_t state;
    bool is_file_data;
    uint32_t write_len;
    uint32_t data_len;
    void *data;
};

typedef struct esp_xmodem esp_xmodem_t;

/**
 * @brief      Start a Xmodem sender session
 *             and it returns the result of connect.
 *
 * @param[in]  sender   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 */

esp_err_t esp_xmodem_sender_start(esp_xmodem_handle_t sender);

/**
 * @brief      Start the Xmodem receiver and wait sender to connect
 *             and it returns the result of receiver start.
 *
 * @param[in]  receiver   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_receiver_start(esp_xmodem_handle_t receiver);

/**
  * @brief  Calculate crc16 value.
  *
  * @param  uint8_t *buffer : buffer to start calculate crc.
  *
  * @param  uint32_t len : buffer length in byte.
  *
  * @return crc16 value
  */
uint16_t esp_xmodem_crc16(uint8_t *buffer, uint32_t len);

/**
  * @brief  Calculate checksum value.
  *
  * @param  uint8_t *buffer : buffer to start calculate checksum.
  *
  * @param  uint32_t len : buffer length in byte.
  *
  * @return checksum value
  */
uint8_t esp_xmodem_checksum(uint8_t *buffer, uint32_t len);

/**
  * @brief  Send data to UART.
  *
  * @param  esp_xmodem_t *handle : The handle of xmodem.
  *
  * @param  uint8_t *data : The data to send.
  * 
  * @param  uint32_t *len  : The data length to send.
  *
  * @return
  *     - (-1) Parameter error
  *     - OTHERS (>=0) The number of bytes pushed to the TX FIFO
  */
int esp_xmodem_send_data(esp_xmodem_t *handle, uint8_t *data, uint32_t *len);

/**
  * @brief  Read a byte data from UART.
  *
  * @param  esp_xmodem_t *handle : The handle of xmodem.
  *
  * @param  uint8_t *ch : The byte data to read.
  * 
  * @param  int timeout_ms : The timeout of byte data to read.
  *
  * @return
  *     - ESP_OK
  *     - ESP_FAIL
  */
esp_err_t esp_xmodem_read_byte(esp_xmodem_t *handle, uint8_t *ch, int timeout_ms);

/**
  * @brief  Read data from UART.
  *
  * @param  esp_xmodem_t *handle : The handle of xmodem.
  * 
  * @param  int timeout_ms : The timeout of data to read.
  *
  * @return
  *     - (-1) Parameter error
  *     - OTHERS (>=0) The number of bytes pushed to the TX FIFO
  */
uint32_t esp_xmodem_read_data(esp_xmodem_t *handle, int timeout_ms);

/**
  * @brief  Wait time and read a byte data from UART.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @param  uint8_t *ch : The byte data to read.
  *
  * @return
  *     - ESP_OK
  *     - ESP_FAIL
  */
esp_err_t esp_xmodem_wait_response(esp_xmodem_t *handle, uint8_t *ch);

/**
  * @brief  Flush the UART RX buffer.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @return
  *     - None
  */
void esp_xmodem_transport_flush(esp_xmodem_t *handle);

/**
  * @brief  Send ch data to UART and sync the err_code and event.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @param  int err_code : The error code when Xmodem send or receive.
  *
  * @param  int event : The event that report to userspace.
  *
  * @param  uint8_t ch : The Xmodem handle flag, such as CRC16, NAK, ACK, EOT and CAN.
  *
  * @return 
  *     - None
  */
void esp_xmodem_send_char_code_event(esp_xmodem_t *handle, int err_code, int event, uint8_t ch);

/**
  * @brief  Set Xmodem state.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @param  esp_xmodem_state_t state : The state of Xmodem.
  *
  * @return
  *     - None
  */
void esp_xmodem_set_state(esp_xmodem_t *handle, esp_xmodem_state_t state);

/**
  * @brief  Get Xmodem state.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @return
  *     - The state of Xmodem
  */
esp_xmodem_state_t esp_xmodem_get_state(esp_xmodem_t *handle);

/**
  * @brief  Set Xmodem error code.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @param  int code : The code of Xmodem.
  *
  * @return
  *     - None
  */
void esp_xmodem_set_error_code(esp_xmodem_t *handle, int code);

/**
  * @brief  Send event to userspace.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @param  esp_xmodem_event_id_t event_id : The event id.
  * 
  * @param  void *data : The data to send to userspace.
  * 
  * @param  uint32_t len : The data length to send to userspace. 
  *
  * @return
  *     - ESP_OK
  *     - ESP_FAIL
  */
esp_err_t esp_xmodem_dispatch_event(esp_xmodem_t *handle, esp_xmodem_event_id_t event_id, void *data, uint32_t len);

/**
  * @brief  Get Xmodem crc or checksum length.
  *
  * @param  esp_xmodem_t *handle : The Xmodem handle.
  *
  * @return
  *     - len
  */
int esp_xmodem_get_crc_len(esp_xmodem_t *handle);

/**
  * @brief  Print packet data for debug
  *
  * @param  uint8_t *packet : The packet data.
  *
  * @param  uint32_t len : The packet length.
  *
  * @return
  *     - None
  */
void esp_xmodem_print_packet(uint8_t *packet, uint32_t len);

#ifdef __cplusplus
}
#endif
