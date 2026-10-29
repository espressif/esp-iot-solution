/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <protocomm_ext.h>
#include <protocomm_ext_security.h>
#include <protocomm_ext_transports.h>

/**
 * @brief   Prototype structure of a Protocomm ext instance
 *
 * This structure corresponds to a unique instance of protocomm ext returned,
 * when the API protocomm_ext_new() is called. The remaining Protocomm ext
 * APIs require this object as the first parameter.
 */
struct protocomm_ext {

    /* Transport method */
    protocomm_ext_transport_method_t transport_method;

    /* Security method */
    protocomm_ext_security_method_t security_method;

    /* Pointer to security layer to be used internally for
     * establishing secure sessions */
    const protocomm_ext_security_t *sec;

    /* Handle to the security layer instance */
    protocomm_ext_security_handle_t sec_inst;

    /* Pointer to transport layer to be used internally for
     * establishing secure sessions */
    const protocomm_ext_transport_t *transport;

    /* Handle to the transport layer instance */
    protocomm_ext_transport_handle_t transport_inst;

    /* Pointer to transport data */
    void *transport_data;

    /* Pointer to security data */
    void *security_data;

    /* Private data to be used internally by the protocomm ext instance */
    void *priv;

    /* Version string to store */
    char *version_str;
};

#ifdef __cplusplus
}
#endif
