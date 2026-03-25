/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Board information structure
 */
typedef struct esp_xiaozhi_chat_board_info_s {
    char uuid[37];         /*!< Unique identifier (UUID) for the board */
    char mac_address[18]; /*!< MAC address of the board */
    bool initialized;     /*!< Initialization status flag */
} esp_xiaozhi_chat_board_info_t;

/**
 * @brief  Get board information
 *
 * @param[out] board_info  Pointer to board information
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid board_info pointer
 *       - Other                 Error from keystore init
 */
esp_err_t esp_xiaozhi_chat_get_board_info(esp_xiaozhi_chat_board_info_t *board_info);

/**
 * @brief  Get system information as JSON string
 *
 * @param[in]  board_info   Board information
 * @param[out] json_buffer  Buffer to store JSON string
 * @param[in]  buffer_size  Size of the JSON buffer
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid board_info, json_buffer, or buffer_size
 *       - ESP_ERR_INVALID_STATE   Board not initialized
 *       - ESP_ERR_NO_MEM        Failed to create JSON object
 *       - Other                 Error from cJSON_Print
 */
esp_err_t esp_xiaozhi_chat_get_board_json(esp_xiaozhi_chat_board_info_t *board_info, char *json_buffer, uint16_t buffer_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
