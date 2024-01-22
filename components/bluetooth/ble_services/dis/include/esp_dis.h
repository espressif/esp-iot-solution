/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* 16 Bit Device Information Service UUID */
#define BLE_DIS_UUID16                          0x180A

/* 16 Bit Device Information Service Characteristic UUIDs */
#define BLE_DIS_CHR_UUID16_SYSTEM_ID            0x2A23
#define BLE_DIS_CHR_UUID16_MODEL_NUMBER         0x2A24
#define BLE_DIS_CHR_UUID16_SERIAL_NUMBER        0x2A25
#define BLE_DIS_CHR_UUID16_FIRMWARE_REVISION    0x2A26
#define BLE_DIS_CHR_UUID16_HARDWARE_REVISION    0x2A27
#define BLE_DIS_CHR_UUID16_SOFTWARE_REVISION    0x2A28
#define BLE_DIS_CHR_UUID16_MANUFACTURER_NAME    0x2A29
#define BLE_DIS_CHR_UUID16_REG_CERT             0x2A2A
#define BLE_DIS_CHR_UUID16_PNP_ID               0x2A50

/**
 * The structure represent a set of values that are used to create a device ID value.
 * These values are used to identify all devices of a given type/model/version using numbers.
 */
typedef struct esp_ble_dis_pnp {
    uint8_t  src;                                   /*!< The vendor ID source */
    uint16_t vid;                                   /*!< The product vendor from the namespace in the vendor ID source*/
    uint16_t pid;                                   /*!< Manufacturer managed identifier for this product*/
    uint16_t ver;                                   /*!< Manufacturer managed version for this product*/
} __attribute__((packed)) esp_ble_dis_pnp_t;

/**
 * Structure holding data for the main characteristics
 */
typedef struct esp_ble_dis_data {
    /**
     * Model number.
     * Represent the model number that is assigned by the device vendor.
     */
    char model_number[CONFIG_BLE_DIS_STR_MAX];
    /**
     * Serial number.
     * Represent the serial number for a particular instance of the device.
     */
    char serial_number[CONFIG_BLE_DIS_STR_MAX];
    /**
     * Firmware revision.
     * Represent the firmware revision for the firmware within the device.
     */
    char firmware_revision[CONFIG_BLE_DIS_STR_MAX];
    /**
     * Hardware revision.
     * Represent the hardware revision for the hardware within the device.
     */
    char hardware_revision[CONFIG_BLE_DIS_STR_MAX];
    /**
     * Software revision.
     * Represent the software revision for the software within the device.
     */
    char software_revision[CONFIG_BLE_DIS_STR_MAX];
    /**
     * Manufacturer name.
     * Represent the name of the manufacturer of the device.
     */
    char manufacturer_name[CONFIG_BLE_DIS_STR_MAX];
    /**
     * System ID.
     * Represent the System Id of the device.
     */
    char system_id[CONFIG_BLE_DIS_STR_MAX];
    /**
     * PnP ID.
     * Represent the PnP Id of the device.
     */
    esp_ble_dis_pnp_t pnp_id;
} esp_ble_dis_data_t;

/**
 * @brief Get the model number that is assigned by the device vendor.
 *
 * @param[out]  value The pointer to store the model number.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong model number
 */
esp_err_t esp_ble_dis_get_model_number(const char **value);

/**
 * @brief Set the model number of the device.
 *
 * @param[in]  value The pointer to store the model number.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong model number
 */
esp_err_t esp_ble_dis_set_model_number(const char *value);

/**
 * @brief Get the serial number for a particular instance of the device.
 *
 * @param[out]  value The pointer to store the serial number.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong serial number
 */
esp_err_t esp_ble_dis_get_serial_number(const char **value);

/**
 * @brief Set the serial number of the device.
 *
 * @param[in]  value The pointer to store the serial number.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong serial number
 */
esp_err_t esp_ble_dis_set_serial_number(const char *value);

/**
 * @brief Get the firmware revision for the firmware within the device.
 *
 * @param[out]  value The pointer to store the firmware revision.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong firmware revision
 */
esp_err_t esp_ble_dis_get_firmware_revision(const char **value);

/**
 * @brief Set the firmware revision of the device.
 *
 * @param[in]  value The pointer to store the firmware revision.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong firmware revision
 */
esp_err_t esp_ble_dis_set_firmware_revision(const char *value);

/**
 * @brief Get the hardware revision for the hardware within the device.
 *
 * @param[out]  value The pointer to store the hardware revision.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong hardware revision
 */
esp_err_t esp_ble_dis_get_hardware_revision(const char **value);

/**
 * @brief Set the hardware revision of the device.
 *
 * @param[in]  value The pointer to store the hardware revision.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong hardware revision
 */
esp_err_t esp_ble_dis_set_hardware_revision(const char *value);

/**
 * @brief Get the software revision for the software within the device.
 *
 * @param[out]  value The pointer to store the software revision.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong software revision
 */
esp_err_t esp_ble_dis_get_software_revision(const char **value);

/**
 * @brief Set the software revision of the device.
 *
 * @param[in]  value The pointer to store the software revision.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong software revision
 */
esp_err_t esp_ble_dis_set_software_revision(const char *value);

/**
 * @brief Get the name of the manufacturer of the device.
 *
 * @param[out]  value The pointer to store the name of the manufacturer.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong name of the manufacturer
 */
esp_err_t esp_ble_dis_get_manufacturer_name(const char **value);

/**
 * @brief Set the name of the manufacturer of the device.
 *
 * @param[in]  value The pointer to store the name of the manufacturer.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong name of the manufacturer
 */
esp_err_t esp_ble_dis_set_manufacturer_name(const char *value);

/**
 * @brief Get the System Id of the device.
 *
 * @param[out]  value The pointer to store the System Id.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong System Id
 */
esp_err_t esp_ble_dis_get_system_id(const char **value);

/**
 * @brief Set the System Id of the device.
 *
 * @param[in]  value The pointer to store the System Id.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong System Id
 */
esp_err_t esp_ble_dis_set_system_id(const char *value);

/**
 * @brief Get the PnP Id of the device.
 *
 * @param[in]  pnp_id The pointer to store the PnP Id.
 *
 * @return:
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong PnP Id
 */
esp_err_t esp_ble_dis_get_pnp_id(esp_ble_dis_pnp_t *pnp_id);

/**
 * @brief Set the PnP Id of the device.
 *
 * @param[in]  pnp_id The pointer to store the PnP Id.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong PnP Id
 */
esp_err_t esp_ble_dis_set_pnp_id(esp_ble_dis_pnp_t *pnp_id);

/**
 * @brief Initialization GATT Device Information Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_dis_init(void);

#ifdef __cplusplus
}
#endif
