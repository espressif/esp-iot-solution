/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
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
#include "esp_msc_host.h"

/**
 * @brief Declare Event Base for ESP MSC OTA
 *
 */
/** @cond **/
ESP_EVENT_DECLARE_BASE(ESP_MSC_OTA_EVENT);
/** @endcond **/

/**
 * @brief MSC OTA events posted on ESP_MSC_OTA_EVENT event base.
 */
typedef enum {
    ESP_MSC_OTA_START,                 /*!< Start update, event data: NULL */
    ESP_MSC_OTA_READY_UPDATE,          /*!< Ready to update, event data: NULL */
    ESP_MSC_OTA_WRITE_FLASH,           /*!< Flash write operation, event data: float *progress */
    ESP_MSC_OTA_FAILED,                /*!< Update failed, event data: esp_err_t *err */
    ESP_MSC_OTA_GET_IMG_DESC,          /*!< Get image description, event data: NULL */
    ESP_MSC_OTA_VERIFY_CHIP_ID,        /*!< Verify chip id, event data: esp_chip_id_t *chip_id */
    ESP_MSC_OTA_UPDATE_BOOT_PARTITION, /*!< Boot partition update after successful ota update, event data: esp_partition_subtype_t *subtype */
    ESP_MSC_OTA_FINISH,                /*!< OTA finished, event data: NULL */
    ESP_MSC_OTA_ABORT,                 /*!< OTA aborted, event data: NULL */
} esp_msc_ota_event_t;

/**
 * @brief Internal state of an esp_msc_ota handle.
 */
typedef enum {
    ESP_MSC_OTA_INIT,        /*!< Handle allocated but esp_msc_ota_begin() not called yet */
    ESP_MSC_OTA_BEGIN,       /*!< esp_msc_ota_begin() succeeded, ready for esp_msc_ota_perform() */
    ESP_MSC_OTA_IN_PROGRESS, /*!< Firmware image is being written to flash */
    ESP_MSC_OTA_SUCCESS,     /*!< Full firmware image has been written */
} esp_msc_ota_status_t;

/**
 * @brief esp msc ota config
 *
 */
typedef struct {
    esp_msc_host_handle_t host_handle; /*!< MSC host handle. OTA waits for this host's VFS mounted state and locks file access through the host API */
    const char *ota_bin_path;          /*!< OTA binary name, must be an exact match. Note: By default file names cannot exceed 11 bytes e.g. "/usb/ota.bin" */
    TickType_t wait_msc_connect;       /*!< Wait time for MSC VFS mount in FreeRTOS ticks */
    size_t buffer_size;                /*!< Buffer size for OTA write operation, must larger than 1024 */
    bool bulk_flash_erase;             /*!< Erase entire flash partition during initialization. By default flash partition is erased during write operation and in chunk of 4K sector size */
} esp_msc_ota_config_t;

/**
 * @brief Opaque MSC OTA handle.
 *
 * The concrete type is private to the component; users must only pass the
 * value returned by esp_msc_ota_begin() to the OTA APIs. Note that this is a
 * different type from esp_msc_host_handle_t and the two handles must not be
 * used interchangeably.
 */
typedef struct esp_msc_ota_ctx *esp_msc_ota_handle_t;

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
esp_err_t esp_msc_ota_begin(const esp_msc_ota_config_t *config, esp_msc_ota_handle_t *handle);

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
esp_err_t esp_msc_ota(const esp_msc_ota_config_t *config);

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
