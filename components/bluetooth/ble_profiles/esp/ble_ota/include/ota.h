/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OTA_H_
#define _OTA_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ota_ctrl_handlers {
    /**
     * Handler function called when ota sending file is requested. Status
     * is given the parameters :
     *
     * file (input) - file received through OTA
     *
     * length (input) - length of file received
     */
    esp_err_t (*ota_send_file)(uint8_t *file, size_t length);

    /**
     * Handler function called when ota start is requested. Status
     * is given the parameters :
     *
     * file_size (input) - total size of the file
     *
     * block_size (input) - size of one block
     *
     * partition_name (input) - custom partition where data is to be written
     */
    esp_err_t (*ota_start_cmd)(size_t file_size, size_t block_size, char *partition_name);

    /**
     * Handler function called when ota is completed.
     */
    esp_err_t (*ota_finish_cmd)();
} ota_handlers_t;

/**
 * @brief   Handler for ota requests
 *
 * This is to be registered as the `recv-fw`, 'ota-bar', 'ota-command',
 * 'ota-customer' endpoint handler (protocomm `protocomm_req_handler_t`)
 * using `protocomm_add_endpoint()`
*/
esp_err_t ota_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                      uint8_t **outbuf, ssize_t *outlen, void *priv_data);

#ifdef __cplusplus
}
#endif

#endif
