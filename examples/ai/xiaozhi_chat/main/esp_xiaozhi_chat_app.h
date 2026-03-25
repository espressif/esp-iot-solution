/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include "esp_xiaozhi_chat.h"
#include "esp_gmf_oal_thread.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_XIAOZHI_CHAT_REC_READ_SIZE 512 * 3
#define ESP_XIAOZHI_CHAT_APP_ONLINE (1 << 1)
#define ESP_XIAOZHI_CHAT_APP_OFFLINE (1 << 2)

/**
 * @brief  ESP IoT Chat App structure
 *
 * @note   This structure is used to store the Xiaozhi chat app data
 */
typedef struct esp_xiaozhi_chat_app_s {
    bool                       wakeuped;       /*!< Wake-up state flag */
    EventGroupHandle_t         data_evt_group;  /*!< Data event group */
    esp_xiaozhi_chat_handle_t  chat;           /*!< Xiaozhi chat handle */
    esp_xiaozhi_chat_audio_t   audio;          /*!< Audio configuration */
    esp_gmf_oal_thread_t       read_thread;    /*!< Audio data read thread */
    esp_gmf_oal_thread_t       audio_channel;  /*!< Audio channel thread */
} esp_xiaozhi_chat_app_t;

/**
 * @brief  Run the Xiaozhi chat application
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_NO_MEM   Failed to allocate app context
 */
esp_err_t esp_xiaozhi_chat_app(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
