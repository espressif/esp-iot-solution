/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_local_controller.h
 * @brief Controller-side client for IDF esp_local_ctrl property get/set
 *
 * Builds on protocomm_ext (transport + Sec0/1/2). This component only
 * encodes/decodes the esp_local_ctrl/control protobuf and manages version
 * discovery + session helpers.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>
#include <protocomm_ext.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LOCAL_CONTROLLER_EP_VERSION  "esp_local_ctrl/version" /**< Version endpoint name (IDF `esp_local_ctrl`) */
#define ESP_LOCAL_CONTROLLER_EP_SESSION  "esp_local_ctrl/session" /**< Session endpoint name (IDF `esp_local_ctrl`) */
#define ESP_LOCAL_CONTROLLER_EP_CONTROL  "esp_local_ctrl/control" /**< Control endpoint name (IDF `esp_local_ctrl`) */

/** BLE UUID16 for version, matching IDF `declare_ep` starting at 0xFF50. */
#define ESP_LOCAL_CONTROLLER_EP_VERSION_UUID  0xFF50
/** BLE UUID16 for session (`version + 1`). */
#define ESP_LOCAL_CONTROLLER_EP_SESSION_UUID  0xFF51
/** BLE UUID16 for control (`version + 2`). */
#define ESP_LOCAL_CONTROLLER_EP_CONTROL_UUID  0xFF52

/**
 * Upper bound on property indices / returned props accepted from a peer.
 * Rejects oversized count / n_props before allocating (DoS / OOM guard).
 * IDF device examples typically use a much smaller max_properties.
 */
#define ESP_LOCAL_CONTROLLER_MAX_PROPERTIES  64

/** Opaque local-controller instance. */
typedef struct esp_local_controller esp_local_controller_t;

/**
 * @brief Parsed fields from the plaintext version endpoint JSON
 *
 * Device format:
 * `{"local_ctrl":{"ver":"...","sec_ver":N,"sec_patch_ver":M}}`
 */
typedef struct {
    char *ver;           /**< Heap copy of `local_ctrl.ver`; free with version free helper */
    int sec_ver;         /**< 0 / 1 / 2; -1 if missing */
    int sec_patch_ver;   /**< Patch; -1 if missing */
} esp_local_controller_version_t;

/**
 * @brief One property returned by esp_local_controller_get_property_values()
 *
 * Caller owns `name` and `value`; free with esp_local_controller_props_free().
 */
typedef struct {
    char *name;          /**< Property name (may be NULL) */
    uint32_t type;       /**< Application-defined type */
    uint32_t flags;      /**< Application-defined flags (e.g. read-only) */
    uint8_t *value;      /**< Raw value bytes (may be NULL) */
    size_t value_len;    /**< Length of value */
} esp_local_controller_prop_t;

/**
 * @brief One property value for esp_local_controller_set_property_values()
 */
typedef struct {
    uint32_t index;      /**< Zero-based property index on the device */
    const uint8_t *value;/**< Value bytes (may be NULL if value_len == 0) */
    size_t value_len;    /**< Length of value */
} esp_local_controller_prop_set_t;

/**
 * @brief Wrap an existing protocomm_ext instance
 *
 * Ownership of @p pc remains with the caller; call esp_local_controller_delete()
 * before protocomm_ext_delete().
 *
 * Registers BLE endpoint UUID mappings (`ESP_LOCAL_CONTROLLER_EP_*_UUID`).
 *
 * @param[in] pc Existing protocomm_ext instance (must not be NULL)
 *
 * @return
 *      - non-NULL: new controller instance
 *      - NULL: invalid argument, endpoint registration failure, or out of memory
 *
 * @note APIs on a given instance are not thread-safe. Serialize from one task.
 */
esp_local_controller_t *esp_local_controller_create(protocomm_ext_t *pc);

/**
 * @brief Delete a controller created by esp_local_controller_create()
 *
 * Does not delete the underlying protocomm_ext instance.
 *
 * @param[in] ctrl Instance to delete (NULL-safe)
 */
void esp_local_controller_delete(esp_local_controller_t *ctrl);

/**
 * @brief Return the wrapped protocomm_ext instance
 *
 * @param[in] ctrl Controller instance
 *
 * @return Underlying protocomm_ext pointer, or NULL if @p ctrl is NULL
 */
protocomm_ext_t *esp_local_controller_get_protocomm(esp_local_controller_t *ctrl);

/**
 * @brief Open transport session and read version JSON (plaintext)
 *
 * Does not run the security handshake. Stores parsed version for
 * esp_local_controller_get_version(). On failure, any previously cached
 * version is invalidated and the transport session is closed.
 *
 * @param[in] ctrl                   Controller instance
 * @param[in] transport_connect_cfg  HTTP/Console: NULL; BLE: `ble_addr_t *`
 *
 * @return
 *      - ESP_OK: version fetched and parsed
 *      - ESP_ERR_INVALID_ARG: bad arguments
 *      - ESP_ERR_INVALID_RESPONSE: JSON missing / invalid sec_ver
 *      - Other: transport or allocation errors from protocomm_ext
 */
esp_err_t esp_local_controller_fetch_version(esp_local_controller_t *ctrl,
                                             const void *transport_connect_cfg);

/**
 * @brief Run security_init + establish_security on esp_local_ctrl/session
 *
 * Call after protocomm_ext_set_security() with peer sec_ver / credentials
 * (typically after esp_local_controller_fetch_version() + get_version()).
 *
 * @param[in] ctrl Controller instance (session must already be open)
 *
 * @return
 *      - ESP_OK: security established; property APIs may be used
 *      - ESP_ERR_INVALID_ARG: bad arguments
 *      - Other: errors from protocomm_ext security APIs
 */
esp_err_t esp_local_controller_establish_security(esp_local_controller_t *ctrl);

/**
 * @brief Convenience: fetch_version + establish_security
 *
 * Prerequisite: call protocomm_ext_set_security() first when the peer scheme
 * is already known (Sec0/1/2 credentials). Equivalent to the recommended
 * split flow when discovery is not needed at runtime.
 *
 * On establish failure, closes the session and invalidates cached version.
 *
 * @param[in] ctrl                   Controller instance
 * @param[in] transport_connect_cfg  Same as esp_local_controller_fetch_version()
 *
 * @return
 *      - ESP_OK: session ready for property get/set
 *      - Other: see fetch_version / establish_security
 */
esp_err_t esp_local_controller_start_session(esp_local_controller_t *ctrl,
                                             const void *transport_connect_cfg);

/**
 * @brief Close the transport session
 *
 * @param[in] ctrl Controller instance
 *
 * @return Result of protocomm_ext_close_session(), or ESP_ERR_INVALID_ARG
 */
esp_err_t esp_local_controller_stop_session(esp_local_controller_t *ctrl);

/**
 * @brief Deep-copy the last successfully fetched version into @p out
 *
 * @param[in]  ctrl Controller instance
 * @param[out] out  Output version (must not be NULL)
 *
 * @return
 *      - ESP_OK: @p out filled; caller must call esp_local_controller_version_free()
 *      - ESP_ERR_INVALID_ARG: bad arguments
 *      - ESP_ERR_INVALID_STATE: no successful fetch yet
 *      - ESP_ERR_NO_MEM: strdup failed
 */
esp_err_t esp_local_controller_get_version(esp_local_controller_t *ctrl,
                                           esp_local_controller_version_t *out);

/**
 * @brief Free heap fields inside a version struct from get_version / parse_version
 *
 * @param[in,out] ver Version to clear (NULL-safe for pointer; no-op if NULL)
 */
void esp_local_controller_version_free(esp_local_controller_version_t *ver);

/**
 * @brief Query how many properties the peer exposes
 *
 * Requires an established secure session (or Sec0 after establish_security).
 *
 * @param[in]  ctrl  Controller instance
 * @param[out] count Property count
 *
 * @return ESP_OK on success, or a transport / protocol error
 */
esp_err_t esp_local_controller_get_property_count(esp_local_controller_t *ctrl,
                                                  uint32_t *count);

/**
 * @brief Fetch property metadata + values for the given indices
 *
 * On success, allocates @p *out_props of length @p *out_count. If the peer
 * returns zero properties, @p *out_props is NULL and @p *out_count is 0.
 * Caller frees with esp_local_controller_props_free() (NULL-safe).
 *
 * @param[in]  ctrl         Controller instance
 * @param[in]  indices      Array of zero-based property indices
 * @param[in]  index_count  Number of indices (must be > 0)
 * @param[out] out_props    Allocated property array
 * @param[out] out_count    Number of entries in @p *out_props
 *
 * @return ESP_OK on success, or a transport / protocol / OOM error
 */
esp_err_t esp_local_controller_get_property_values(esp_local_controller_t *ctrl,
                                                   const uint32_t *indices,
                                                   size_t index_count,
                                                   esp_local_controller_prop_t **out_props,
                                                   size_t *out_count);

/**
 * @brief Free an array from esp_local_controller_get_property_values()
 *
 * @param[in] props Property array (NULL-safe)
 * @param[in] count Number of elements
 */
void esp_local_controller_props_free(esp_local_controller_prop_t *props, size_t count);

/**
 * @brief Set one or more property values by index
 *
 * @param[in] ctrl       Controller instance
 * @param[in] props      Array of set descriptors
 * @param[in] prop_count Number of entries (must be > 0)
 *
 * @return ESP_OK on success, or a transport / protocol / OOM error
 */
esp_err_t esp_local_controller_set_property_values(esp_local_controller_t *ctrl,
                                                   const esp_local_controller_prop_set_t *props,
                                                   size_t prop_count);

/**
 * @brief Parse version JSON into @p out (exposed for unit tests / advanced use)
 *
 * @param[in]  json Version payload string
 * @param[out] out  Parsed version; caller frees with esp_local_controller_version_free()
 *
 * @return
 *      - ESP_OK: parsed
 *      - ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_RESPONSE / ESP_ERR_NO_MEM
 *
 * @note Prefer esp_local_controller_fetch_version() in application code.
 */
esp_err_t esp_local_controller_parse_version(const char *json,
                                             esp_local_controller_version_t *out);

#ifdef __cplusplus
}
#endif
