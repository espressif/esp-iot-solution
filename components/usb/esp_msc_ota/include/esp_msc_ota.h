/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"

/**
 * @brief Declare Event Base for ESP MSC OTA
 *
 */
/** @cond **/
ESP_EVENT_DECLARE_BASE(ESP_MSC_OTA_EVENT);
/** @endcond **/

typedef enum {
    ESP_MSC_OTA_START,                 /*!< Start update */
    ESP_MSC_OTA_READY_UPDATE,          /*!< Ready to update */
    ESP_MSC_OTA_WRITE_FLASH,           /*!< Flash write operation  */
    ESP_MSC_OTA_FAILED,                /*!< Update failed */
    ESP_MSC_OTA_GET_IMG_DESC,          /*!< Get image description */
    ESP_MSC_OTA_VERIFY_CHIP_ID,        /*!< Verify chip id */
    ESP_MSC_OTA_UPDATE_BOOT_PARTITION, /*!< Boot partition update after successful ota update */
    ESP_MSC_OTA_FINISH,                /*!< OTA finished */
    ESP_MSC_OTA_ABORT,                 /*!< OTA aborted */
} esp_msc_ota_event_t;

typedef enum {
    ESP_MSC_OTA_INIT,
    ESP_MSC_OTA_BEGIN,
    ESP_MSC_OTA_IN_PROGRESS,
    ESP_MSC_OTA_SUCCESS,
} esp_msc_ota_status_t;

/**
 * @brief esp msc ota config
 *
 */
typedef struct {
    const char *ota_bin_path;    /*!< OTA binary name, must be an exact match. Note: By default file names cannot exceed 11 bytes e.g. "/usb/ota.bin" */
    bool bulk_flash_erase;       /*!< Erase entire flash partition during initialization. By default flash partition is erased during write operation and in chunk of 4K sector size */
    TickType_t wait_msc_connect; /*!< Wait time for MSC device to connect */
    size_t buffer_size;          /*!< Buffer size for OTA write operation, must larger than 1024 */
} esp_msc_ota_config_t;

/**
 * @brief Handle for the MSC ota
 *
 */
typedef void *esp_msc_ota_handle_t;

/**
 * @brief Start MSC OTA Firmware upgrade
 *
 * If this function succeeds, then call `esp_msc_ota_perform`to continue with the OTA process otherwise call `esp_msc_ota_end`.
 *
 * @param[in] config pointer to esp_msc_ota_config_t structure
 * @param[out] handle pointer to an allocated data of type `esp_msc_ota_handle_t` which will be initialised in this function
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG: Invalid argument (missing/incorrect config, handle, etc.)
 *     - ESP_ERR_NO_MEM: Failed to allocate memory for msc_ota handle
 *     - ESP_FAIL: For generic failure.
 */
esp_err_t esp_msc_ota_begin(esp_msc_ota_config_t *config, esp_msc_ota_handle_t *handle);

/**
 * @brief Read data from the firmware on the USB flash drive and start the upgrade,
 *
 * It is necessary to call this function several times and ensure that the value returned each time is ESP_OK.
 * and call `esp_msc_ota_is_complete_data_received` to monitor whether the firmware upgrade is complete or not.
 * Make sure that the VFS file system is not unmounted during the `fread` process. If you manually unplug the USB
 * flash drive or log out of the USB HOST, stop calling `esp_msc_ota_perform` before and call `esp_msc_ota_abort` afterwards.
 *
 * @param[in] handle Handle for the MSC ota
 * @return
 *    - ESP_OK on success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_ERR_INVALID_STATE: Invalid state (handle not initialized, etc.)
 *    - ESP_ERR_INVALID_SIZE: Fread failed
 *    - ESP_FAIL: For generic failure.
 *    - For other errors, please check the API for the specific error.
 */
esp_err_t esp_msc_ota_perform(esp_msc_ota_handle_t handle);

/**
 * @brief Clean-up MSC OTA Firmware upgrade
 *
 * @note  If this API returns successfully, esp_restart() must be called to
 *        boot from the new firmware image
 *        esp_https_ota_finish should not be called after calling esp_msc_ota_abort
 *
 * @param[in] handle Handle for the MSC ota
 * @return
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - ESP_ERR_INVALID_STATE: Incorrect status
 *      - ESP_OK: Success
 *      - For other errors, please check the API for the specific error.
 */
esp_err_t esp_msc_ota_end(esp_msc_ota_handle_t handle);

/**
 * @brief Clean-up MSC OTA Firmware upgrade and call `esp_ota_abort`
 *
 * @note esp_msc_ota_abort should not be called after calling esp_msc_ota_finish
 *
 * @param[in] handle Handle for the MSC ota
 * @return
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - ESP_ERR_INVALID_STATE: Incorrect status
 *      - ESP_OK: Success
 *      - For other errors, please check the API for the specific error.
 */
esp_err_t esp_msc_ota_abort(esp_msc_ota_handle_t handle);

/**
 * @brief MSC OTA Firmware upgrade
 *
 * This function provides a complete set of MSC_OTA upgrade procedures.
 * When the USB flash disk is inserted, it will be upgraded automatically.
 * After the upgrade is completed, please call `esp_restart()`
 *
 * @param[in] config pointer to esp_msc_ota_config_t structure
 * @return
 *    - ESP_OK on success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_OK: Success
 *    - For other errors, please check the API for the specific error.
 */
esp_err_t esp_msc_ota(esp_msc_ota_config_t *config);

/**
 * @brief Reads app description from image header. The app description provides information
 *        like the "Firmware version" of the image.
 *
 * @param[in] handle pointer to esp_msc_ota_config_t structure
 * @param[out] new_app_info pointer to an allocated esp_app_desc_t structure
 * @return
 *    - ESP_OK on success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_ERR_INVALID_STATE: Incorrect status
 *    - ESP_FAIL: Fail to read image header
 */
esp_err_t esp_msc_ota_get_img_desc(esp_msc_ota_handle_t handle, esp_app_desc_t *new_app_info);

/**
 * @brief When you manage the MSC HOST function, call this api to notify msc_ota that the u-disk
 *        has been inserted and the VFS filesystem has been mounted.
 *
 * @param[in] handle Handle for the MSC ota
 * @param[in] if_connect
 * @return alaways return ESP_OK
 */
esp_err_t esp_msc_ota_set_msc_connect_state(esp_msc_ota_handle_t handle, bool if_connect);

/**
 * @brief Get the status of the MSC ota
 *
 * @param[in] handle Handle for the MSC ota
 * @return esp_msc_ota_status_t
 */
esp_msc_ota_status_t esp_msc_ota_get_status(esp_msc_ota_handle_t handle);

/**
 * @brief Checks if complete data was received or not
 *
 * This API can be called just before esp_msc_ota_end() to validate if the complete image was indeed received.
 *
 * @param[in] handle Handle for the MSC ota
 * @return true
 * @return false
 */
bool esp_msc_ota_is_complete_data_received(esp_msc_ota_handle_t handle);

#ifdef __cplusplus
}
#endif
