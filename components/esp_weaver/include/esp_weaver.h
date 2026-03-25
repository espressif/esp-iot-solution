/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP Weaver Value type
 *
 * Supported data types for Weaver parameter values.
 */
typedef enum {
    WEAVER_VAL_TYPE_INVALID = 0, /*!< Invalid */
    WEAVER_VAL_TYPE_BOOLEAN,     /*!< Boolean */
    WEAVER_VAL_TYPE_INTEGER,     /*!< Integer. Mapped to a 32 bit signed integer */
    WEAVER_VAL_TYPE_FLOAT,       /*!< Floating point number */
    WEAVER_VAL_TYPE_STRING,      /*!< NULL terminated string */
    WEAVER_VAL_TYPE_OBJECT,      /*!< NULL terminated JSON Object string Eg. {"name":"value"} */
    WEAVER_VAL_TYPE_ARRAY,       /*!< NULL terminated JSON Array string Eg. [1,2,3] */
} esp_weaver_val_type_t;

/**
 * @brief ESP Weaver Value data union
 *
 * Union containing the actual data for Weaver values. The active member is
 * determined by the type field in the parent esp_weaver_param_val_t structure.
 *
 * @note Only access the member corresponding to the value type.
 */
typedef union {
    bool b;    /*!< Boolean data value */
    int i;     /*!< Integer data value (signed 32-bit) */
    float f;   /*!< Floating-point data value (32-bit) */
    char *s;   /*!< String data value (pointer to null-terminated string) */
} esp_weaver_val_t;

/**
 * @brief ESP Weaver Parameter Value
 *
 * Complete Weaver value with its type and associated data. Provides a unified
 * way to handle different data types in the Weaver protocol.
 *
 * @note The type field determines which member of the val union is valid.
 * @note For STRING type, val.s points to dynamically allocated memory.
 */
typedef struct {
    esp_weaver_val_type_t type; /*!< Type of the value (boolean, integer, float, or string) */
    esp_weaver_val_t val;       /*!< Union containing the actual value data */
} esp_weaver_param_val_t;

/**
 * @brief Param property flags
 */
typedef enum {
    PROP_FLAG_WRITE = (1 << 0),   /*!< Parameter is writable */
    PROP_FLAG_READ = (1 << 1),    /*!< Parameter is readable */
    PROP_FLAG_PERSIST = (1 << 3), /*!< Parameter value is persisted in NVS */
} esp_weaver_param_property_flags_t;

/**
 * @brief ESP Weaver Node handle
 *
 * Opaque structure representing a Weaver node instance.
 */
typedef struct esp_weaver_node esp_weaver_node_t;

/**
 * @brief ESP Weaver Device handle
 *
 * Opaque structure representing a device attached to a Weaver node.
 */
typedef struct esp_weaver_device esp_weaver_device_t;

/**
 * @brief ESP Weaver Parameter handle
 *
 * Opaque structure representing a single parameter within a device.
 */
typedef struct esp_weaver_param esp_weaver_param_t;

/**
 * @brief Parameter read/write request source
 */
typedef enum {
    ESP_WEAVER_REQ_SRC_INIT,  /*!< Request triggered in the init sequence i.e. when a value is found
                                    in persistent memory for parameters with PROP_FLAG_PERSIST */
    ESP_WEAVER_REQ_SRC_LOCAL, /*!< Request from local control (HTTP) */
    ESP_WEAVER_REQ_SRC_MAX,   /*!< This will always be the last value. Any value equal to
                                    or greater than this should be considered invalid */
} esp_weaver_req_src_t;

/**
 * @brief Write request Context
 */
typedef struct {
    esp_weaver_req_src_t src; /*!< Source of the write request */
} esp_weaver_write_ctx_t;

/**
 * @brief Parameter write request payload
 */
typedef struct {
    esp_weaver_param_t *param;  /*!< Parameter handle */
    esp_weaver_param_val_t val; /*!< Value to write */
} esp_weaver_param_write_req_t;

/**
 * @brief Callback for bulk parameter value write requests
 *
 * This callback is invoked when multiple parameters are received in a single request.
 *
 * @note This callback may execute in a transport-specific task context (e.g., HTTPD task),
 *       not the main task. Avoid blocking operations and ensure any shared state access
 *       is properly synchronized.
 *
 * @param[in] device Device handle (must not be NULL)
 * @param[in] write_req Array of write requests (must not be NULL)
 * @param[in] count Number of write requests in the array
 * @param[in] priv_data Pointer to the private data passed while creating the device
 * @param[in] ctx Context associated with the request
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_FAIL: Error
 */
typedef esp_err_t (*esp_weaver_device_bulk_write_cb_t)(const esp_weaver_device_t *device, const esp_weaver_param_write_req_t write_req[],
                                                       uint8_t count, void *priv_data, esp_weaver_write_ctx_t *ctx);

/**
 * @brief ESP Weaver Configuration
 */
typedef struct {
    const char *node_id; /*!< Custom node ID (max 32 chars). NULL to auto-generate from MAC address */
} esp_weaver_config_t;

/** Default Weaver configuration (auto-generated node ID) */
#define ESP_WEAVER_CONFIG_DEFAULT() { \
    .node_id = NULL, \
}

/**
 * @brief Create a Weaver value with a boolean
 *
 * @param[in] bval Boolean value to store
 * @return Weaver param value structure with type WEAVER_VAL_TYPE_BOOLEAN
 */
esp_weaver_param_val_t esp_weaver_bool(bool bval);

/**
 * @brief Create a Weaver value with an integer
 *
 * @param[in] ival Integer value to store (signed 32-bit)
 * @return Weaver param value structure with type WEAVER_VAL_TYPE_INTEGER
 */
esp_weaver_param_val_t esp_weaver_int(int ival);

/**
 * @brief Create a Weaver value with a float
 *
 * @param[in] fval Floating-point value to store (32-bit)
 * @return Weaver param value structure with type WEAVER_VAL_TYPE_FLOAT
 */
esp_weaver_param_val_t esp_weaver_float(float fval);

/**
 * @brief Create a Weaver value with a string
 *
 * @param[in] sval Pointer to a null-terminated string
 * @return Weaver param value structure with type WEAVER_VAL_TYPE_STRING
 */
esp_weaver_param_val_t esp_weaver_str(const char *sval);

/**
 * @brief Create a Weaver value with a JSON object string
 *
 * @param[in] val Pointer to a null-terminated JSON object string (e.g. "{\"name\":\"value\"}")
 * @return Weaver param value structure with type WEAVER_VAL_TYPE_OBJECT
 */
esp_weaver_param_val_t esp_weaver_obj(const char *val);

/**
 * @brief Create a Weaver value with a JSON array string
 *
 * @param[in] val Pointer to a null-terminated JSON array string (e.g. "[1,2,3]")
 * @return Weaver param value structure with type WEAVER_VAL_TYPE_ARRAY
 */
esp_weaver_param_val_t esp_weaver_array(const char *val);

/**
 * @brief Initialize the Weaver node (singleton)
 *
 * Only one node may exist at a time. Calling this again without
 * esp_weaver_node_deinit() will return NULL.
 *
 * @param[in] config Pointer to configuration structure (must not be NULL, use ESP_WEAVER_CONFIG_DEFAULT())
 * @param[in] name Node name (must not be NULL)
 * @param[in] type Node type (must not be NULL)
 * @return Node handle on success, or NULL on failure
 */
esp_weaver_node_t *esp_weaver_node_init(const esp_weaver_config_t *config, const char *name, const char *type);

/**
 * @brief Deinitialize the Weaver node and free all resources
 *
 * This function frees all devices, parameters, and node resources.
 * After calling this, esp_weaver_node_init() can be called again.
 *
 * @param[in] node Node handle returned by esp_weaver_node_init() (must not be NULL)
 * @return
 *      - ESP_OK: Node deinitialized successfully
 *      - ESP_ERR_INVALID_STATE: Node not initialized
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_weaver_node_deinit(const esp_weaver_node_t *node);

/**
 * @brief Get Node ID
 *
 * Returns pointer to the NULL terminated Node ID string.
 *
 * @return Pointer to a NULL terminated Node ID string, or NULL if not initialized
 */
const char *esp_weaver_get_node_id(void);

/**
 * @brief Add a device to a node
 *
 * @param[in] node Node handle (must not be NULL)
 * @param[in] device Device handle (must not be NULL)
 * @return
 *      - ESP_OK: Device added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter or duplicate device name
 */
esp_err_t esp_weaver_node_add_device(const esp_weaver_node_t *node, esp_weaver_device_t *device);

/**
 * @brief Remove a device from a node
 *
 * @note The device should be removed before deleting it with esp_weaver_device_delete().
 *
 * @param[in] node Node handle (must not be NULL)
 * @param[in] device Device handle (must not be NULL)
 * @return
 *      - ESP_OK: Device removed successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NOT_FOUND: Device not found in node
 */
esp_err_t esp_weaver_node_remove_device(const esp_weaver_node_t *node, esp_weaver_device_t *device);

/**
 * @brief Create a Device
 *
 * @note The device created needs to be added to a node using esp_weaver_node_add_device().
 *
 * @param[in] dev_name The unique device name (must not be NULL)
 * @param[in] type Device type (must not be NULL, e.g., "esp.device.lightbulb")
 * @param[in] priv_data Private data associated with the device (can be NULL). This will be passed to callbacks
 * @return Pointer to the created device, or NULL on failure
 */
esp_weaver_device_t *esp_weaver_device_create(const char *dev_name, const char *type, void *priv_data);

/**
 * @brief Delete a Device
 *
 * This API will delete a device created using esp_weaver_device_create().
 * All parameters associated with the device are also freed.
 *
 * @note The device should first be removed from the node using esp_weaver_node_remove_device() before deleting.
 *
 * @param[in] device Device handle (must not be NULL)
 * @return
 *      - ESP_OK: Device deleted successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_weaver_device_delete(esp_weaver_device_t *device);

/**
 * @brief Add a parameter to a device
 *
 * @note A device supports up to 255 parameters.
 *
 * @param[in] device Device handle (must not be NULL)
 * @param[in] param Parameter handle (must not be NULL)
 * @return
 *      - ESP_OK: Parameter added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter or duplicate parameter name
 *      - ESP_ERR_NO_MEM: Maximum number of parameters reached
 */
esp_err_t esp_weaver_device_add_param(esp_weaver_device_t *device, esp_weaver_param_t *param);

/**
 * @brief Add bulk write callback for a device
 *
 * @param[in] device Device handle (must not be NULL)
 * @param[in] write_cb Bulk write callback function (must not be NULL)
 * @return
 *      - ESP_OK: Callback set successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_weaver_device_add_bulk_cb(esp_weaver_device_t *device, esp_weaver_device_bulk_write_cb_t write_cb);

/**
 * @brief Assign a primary parameter
 *
 * Assign a parameter (already added using esp_weaver_device_add_param()) as a primary parameter,
 * which can be used by clients to give prominence to it.
 *
 * @param[in] device Device handle (must not be NULL)
 * @param[in] param Parameter handle (must not be NULL)
 * @return
 *      - ESP_OK: Primary parameter assigned successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NOT_FOUND: Parameter not found in device
 */
esp_err_t esp_weaver_device_assign_primary_param(esp_weaver_device_t *device, esp_weaver_param_t *param);

/**
 * @brief Get device name from handle
 *
 * @param[in] device Device handle (must not be NULL)
 * @return NULL terminated device name string, or NULL on failure
 */
const char *esp_weaver_device_get_name(const esp_weaver_device_t *device);

/**
 * @brief Get device private data from handle
 *
 * @param[in] device Device handle (must not be NULL)
 * @return Pointer to the private data, or NULL if no private data found
 */
void *esp_weaver_device_get_priv_data(const esp_weaver_device_t *device);

/**
 * @brief Get parameter by name
 *
 * Get handle for a parameter based on the name.
 *
 * @param[in] device Device handle (must not be NULL)
 * @param[in] param_name Parameter name to search (must not be NULL)
 * @return Parameter handle on success, or NULL on failure
 */
esp_weaver_param_t *esp_weaver_device_get_param_by_name(const esp_weaver_device_t *device, const char *param_name);

/**
 * @brief Get parameter by type
 *
 * Get handle for a parameter based on the type.
 *
 * @param[in] device Device handle (must not be NULL)
 * @param[in] param_type Parameter type to search (must not be NULL, e.g., "esp.param.power")
 * @return Parameter handle on success, or NULL on failure
 */
esp_weaver_param_t *esp_weaver_device_get_param_by_type(const esp_weaver_device_t *device, const char *param_type);

/**
 * @brief Create a Parameter
 *
 * @note The parameter created needs to be added to a device using esp_weaver_device_add_param().
 *       Parameter name should be unique in a given device.
 *
 * @param[in] param_name Name of the parameter (must not be NULL)
 * @param[in] type Parameter type (must not be NULL, e.g., "esp.param.power")
 * @param[in] val Value of the parameter. This also specifies the type that will be assigned
 *                to this parameter. You can use esp_weaver_bool(), esp_weaver_int(), esp_weaver_float()
 *                or esp_weaver_str() functions as the argument here. Eg, esp_weaver_bool(true)
 * @param[in] properties Properties of the parameter, which will be a logical OR of flags in
 *                        esp_weaver_param_property_flags_t
 * @return Pointer to the created parameter, or NULL on failure
 */
esp_weaver_param_t *esp_weaver_param_create(const char *param_name, const char *type, esp_weaver_param_val_t val, uint8_t properties);

/**
 * @brief Delete a Parameter
 *
 * Use this to clean up a parameter that was created with esp_weaver_param_create()
 * but not added to a device (e.g., if esp_weaver_device_add_param() failed).
 *
 * @note Parameters that have been added to a device are automatically freed when
 *       the device is deleted with esp_weaver_device_delete().
 *
 * @param[in] param Parameter handle (must not be NULL)
 * @return
 *      - ESP_OK: Parameter deleted successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_weaver_param_delete(esp_weaver_param_t *param);

/**
 * @brief Add a UI Type to a parameter
 *
 * @param[in] param Parameter handle (must not be NULL)
 * @param[in] ui_type String describing the UI Type (must not be NULL, e.g., "esp.ui.toggle")
 * @return
 *      - ESP_OK: UI type added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_weaver_param_add_ui_type(esp_weaver_param_t *param, const char *ui_type);

/**
 * @brief Add bounds for an integer/float parameter
 *
 * Eg. esp_weaver_param_add_bounds(brightness_param, esp_weaver_int(0), esp_weaver_int(100), esp_weaver_int(5));
 *
 * @param[in] param Parameter handle (must not be NULL)
 * @param[in] min Minimum allowed value
 * @param[in] max Maximum allowed value (must be >= min)
 * @param[in] step Minimum stepping (set to 0 if no specific value is desired)
 * @return
 *      - ESP_OK: Bounds added successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_weaver_param_add_bounds(esp_weaver_param_t *param, esp_weaver_param_val_t min, esp_weaver_param_val_t max, esp_weaver_param_val_t step);

/**
 * @brief Get parameter name from handle
 *
 * @param[in] param Parameter handle (must not be NULL)
 * @return NULL terminated parameter name string, or NULL on failure
 */
const char *esp_weaver_param_get_name(const esp_weaver_param_t *param);

/**
 * @brief Get parameter value
 *
 * @note This does not call any explicit functions to read value from hardware/driver.
 *
 * @param[in] param Parameter handle (must not be NULL)
 * @return Pointer to parameter value, or NULL on failure
 */
const esp_weaver_param_val_t *esp_weaver_param_get_val(const esp_weaver_param_t *param);

/**
 * @brief Update a parameter
 *
 * This will just update the value of a parameter without actually reporting
 * it. This can be used when multiple parameters need to be reported together.
 * Eg. If x parameters are to be reported, this API can be used for the first x - 1 parameters
 * and the last one can be updated using esp_weaver_param_update_and_report().
 * This will report all parameters which were updated prior to this call.
 *
 * Sample:
 *
 * esp_weaver_param_update(param1, esp_weaver_int(180));
 * esp_weaver_param_update(param2, esp_weaver_int(80));
 * esp_weaver_param_update_and_report(param3, esp_weaver_int(50));
 *
 * @param[in] param Parameter handle (must not be NULL)
 * @param[in] val New value of the parameter
 * @return
 *      - ESP_OK: Parameter updated successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter or type mismatch
 *      - ESP_ERR_NO_MEM: Memory allocation failed (for string/object/array types)
 */
esp_err_t esp_weaver_param_update(esp_weaver_param_t *param, esp_weaver_param_val_t val);

/**
 * @brief Update and report a parameter
 *
 * Calling this API will update the parameter and report it to connected clients.
 * This should be used whenever there is any local change.
 *
 * @param[in] param Parameter handle (must not be NULL)
 * @param[in] val New value of the parameter
 * @return
 *      - ESP_OK: Parameter updated and reported successfully
 *      - ESP_ERR_INVALID_ARG: Invalid parameter or type mismatch
 *      - ESP_ERR_NO_MEM: Memory allocation failed (for string/object/array types)
 */
esp_err_t esp_weaver_param_update_and_report(esp_weaver_param_t *param, esp_weaver_param_val_t val);

/**
 * @brief Set Proof of Possession (PoP) for local control security
 *
 * @note This must be called before esp_weaver_local_ctrl_start() for it to take effect.
 *       If not called, a random PoP will be generated and stored in NVS.
 *
 * @param[in] pop NULL terminated PoP string.
 *                Pass NULL to clear any previously set custom PoP.
 * @return
 *      - ESP_OK: PoP set successfully
 *      - ESP_ERR_INVALID_ARG: Empty PoP string
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_weaver_local_ctrl_set_pop(const char *pop);

/**
 * @brief Get the Proof of Possession (PoP) for local control
 *
 * Returns the current PoP string. The PoP is resolved in this order:
 *   1. User-set PoP (via esp_weaver_local_ctrl_set_pop())
 *   2. PoP stored in NVS (from a previous boot)
 *   3. Auto-generated random PoP (saved to NVS for persistence)
 *
 * @note This should be called after esp_weaver_local_ctrl_start() to ensure
 *       the PoP has been resolved.
 *
 * @return PoP string on success, or NULL on failure
 */
const char *esp_weaver_local_ctrl_get_pop(void);

/**
 * @brief Start local control service (HTTP + mDNS)
 *
 * All configuration (HTTP port, security version, etc.) is read from Kconfig.
 *
 * @return
 *      - ESP_OK: Local control started successfully
 *      - ESP_ERR_INVALID_STATE: Weaver not initialized or already started
 *      - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_weaver_local_ctrl_start(void);

/**
 * @brief Set a custom mDNS TXT record for the local control service
 *
 * Must be called after esp_weaver_local_ctrl_start().
 *
 * @param[in] key TXT record key (must not be NULL)
 * @param[in] value TXT record value (must not be NULL)
 * @return
 *      - ESP_OK: TXT record set successfully
 *      - ESP_ERR_INVALID_STATE: Local control not started
 *      - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t esp_weaver_local_ctrl_set_txt(const char *key, const char *value);

/**
 * @brief Stop local control service
 *
 * @return
 *      - ESP_OK: Local control stopped successfully
 *      - ESP_ERR_INVALID_STATE: Local control not started
 */
esp_err_t esp_weaver_local_ctrl_stop(void);

/**
 * @brief Push current parameter values to local control clients
 *
 * Call this after updating parameters with esp_weaver_param_update()
 * to push the changes to connected clients. Not needed after
 * esp_weaver_param_update_and_report() which sends automatically.
 *
 * @return
 *      - ESP_OK: Parameters sent successfully
 *      - ESP_ERR_INVALID_STATE: Local control not started
 *      - ESP_FAIL: Failed to generate or send parameters
 */
esp_err_t esp_weaver_local_ctrl_send_params(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
