/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOMM_EXT_SRP6A_PUBKEY_LEN   384
#define PROTOCOMM_EXT_SRP6A_PROOF_LEN    64
#define PROTOCOMM_EXT_SRP6A_SESSION_KEY_LEN 64

typedef struct protocomm_ext_srp6a_client protocomm_ext_srp6a_client_t;

/**
 * @brief Create SRP-6a client (NG_3072, SHA512)
 *
 * @param username     Username (need not be null-terminated)
 * @param username_len Username length
 * @param password     Password (need not be null-terminated)
 * @param password_len Password length
 * @return Client context, or NULL on failure
 */
protocomm_ext_srp6a_client_t *protocomm_ext_srp6a_client_new(const char *username, uint16_t username_len,
                                                             const char *password, uint16_t password_len);

/**
 * @brief Free SRP-6a client context
 */
void protocomm_ext_srp6a_client_free(protocomm_ext_srp6a_client_t *ctx);

/**
 * @brief Get client public key A (always PROTOCOMM_EXT_SRP6A_PUBKEY_LEN bytes, big-endian)
 */
esp_err_t protocomm_ext_srp6a_client_get_pubkey(protocomm_ext_srp6a_client_t *ctx,
                                                uint8_t *bytes_A, size_t *len_A);

/**
 * @brief Process server challenge (salt, B); produce client proof M1 and session key K
 *
 * @param[out] client_proof Buffer of at least PROTOCOMM_EXT_SRP6A_PROOF_LEN bytes
 */
esp_err_t protocomm_ext_srp6a_client_process_challenge(protocomm_ext_srp6a_client_t *ctx,
                                                       const uint8_t *salt, size_t salt_len,
                                                       const uint8_t *bytes_B, size_t len_B,
                                                       uint8_t *client_proof, size_t proof_len);

/**
 * @brief Verify device proof M2
 */
esp_err_t protocomm_ext_srp6a_client_verify_session(protocomm_ext_srp6a_client_t *ctx,
                                                    const uint8_t *host_proof, size_t proof_len);

/**
 * @brief Get session key K (SHA512 digest, 64 bytes). Valid after process_challenge.
 */
const uint8_t *protocomm_ext_srp6a_client_get_session_key(protocomm_ext_srp6a_client_t *ctx, size_t *len);

/**
 * @brief Whether verify_session succeeded
 */
bool protocomm_ext_srp6a_client_authenticated(const protocomm_ext_srp6a_client_t *ctx);

#ifdef __cplusplus
}
#endif
