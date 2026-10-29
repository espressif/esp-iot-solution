/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal SRP-6a client: NG_3072, SHA512 (matches IDF esp_srp / Python srp6a.py)
 */

#include <stdlib.h>
#include <string.h>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_random.h>

#include "protocomm_ext_crypto.h"
#include <mbedtls/bignum.h>

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
#include "psa/crypto.h"
#else
#include <mbedtls/sha512.h>
#endif

#include "protocomm_ext_srp6a_client.h"

static const char *TAG = "srp6a_client";

#define N_LEN           PROTOCOMM_EXT_SRP6A_PUBKEY_LEN
#define HASH_LEN        PROTOCOMM_EXT_SRP6A_PROOF_LEN
#define SESSION_KEY_LEN PROTOCOMM_EXT_SRP6A_SESSION_KEY_LEN

/* RFC 5054 3072-bit group */
static const uint8_t N_3072[N_LEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
    0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, 0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
    0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
    0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
    0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, 0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
    0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
    0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
    0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D, 0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
    0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
    0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
    0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D, 0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
    0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
    0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
    0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9, 0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
    0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
    0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D, 0xAD, 0x33, 0x17, 0x0D, 0x04, 0x50, 0x7A, 0x33,
    0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64, 0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A,
    0x8A, 0xEA, 0x71, 0x57, 0x5D, 0x06, 0x0C, 0x7D, 0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7,
    0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7, 0x1E, 0x8C, 0x94, 0xE0, 0x4A, 0x25, 0x61, 0x9D,
    0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B, 0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64,
    0xD8, 0x76, 0x02, 0x73, 0x3E, 0xC8, 0x6A, 0x64, 0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C,
    0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C, 0x77, 0x09, 0x88, 0xC0, 0xBA, 0xD9, 0x46, 0xE2,
    0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31, 0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E,
    0x4B, 0x82, 0xD1, 0x20, 0xA9, 0x3A, 0xD2, 0xCA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const uint8_t G_3072[] = { 5 };

struct protocomm_ext_srp6a_client {
    char *username;
    uint16_t username_len;
    char *password;
    uint16_t password_len;

    mbedtls_mpi N;
    mbedtls_mpi g;
    mbedtls_mpi k;
    mbedtls_mpi a;
    mbedtls_mpi A;
    mbedtls_mpi RR; /* reuse for exp_mod */

    uint8_t bytes_A[N_LEN];
    size_t len_A;

    uint8_t *bytes_B;
    size_t len_B;

    uint8_t *bytes_s;
    size_t len_s;

    uint8_t session_key[SESSION_KEY_LEN];
    uint8_t M[HASH_LEN];
    uint8_t H_AMK[HASH_LEN];
    bool authenticated;
    bool challenge_done;
};

static int srp_rng(void *ctx, unsigned char *out, size_t len)
{
    (void) ctx;
    esp_fill_random(out, len);
    return 0;
}

static esp_err_t sha512_digest(const uint8_t *data, size_t len, uint8_t out[HASH_LEN])
{
#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    size_t olen = 0;
    if (psa_hash_compute(PSA_ALG_SHA_512, data, len, out, HASH_LEN, &olen) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    return ESP_OK;
#else
    int ret = mbedtls_sha512(data, len, out, 0);
    return (ret == 0) ? ESP_OK : ESP_FAIL;
#endif
}

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
static esp_err_t sha512_update_finish(psa_hash_operation_t *op, uint8_t out[HASH_LEN])
{
    size_t olen = 0;
    psa_status_t status = psa_hash_finish(op, out, HASH_LEN, &olen);
    if (status != PSA_SUCCESS) {
        psa_hash_abort(op);
        return ESP_FAIL;
    }
    return ESP_OK;
}
#endif

/* H(PAD(a) | PAD(b)) with each operand left-padded to N_LEN (RFC 5054 / Python width=) */
static esp_err_t padded_hash(const uint8_t *a, size_t len_a, const uint8_t *b, size_t len_b,
                             uint8_t out[HASH_LEN])
{
    if (len_a > N_LEN || len_b > N_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    psa_hash_operation_t ctx = PSA_HASH_OPERATION_INIT;
    if (psa_hash_setup(&ctx, PSA_ALG_SHA_512) != PSA_SUCCESS) {
        return ESP_FAIL;
    }

    uint8_t zeros[N_LEN] = {0};
    if (len_a < N_LEN && psa_hash_update(&ctx, zeros, N_LEN - len_a) != PSA_SUCCESS) {
        psa_hash_abort(&ctx);
        return ESP_FAIL;
    }
    if (psa_hash_update(&ctx, a, len_a) != PSA_SUCCESS) {
        psa_hash_abort(&ctx);
        return ESP_FAIL;
    }
    if (len_b < N_LEN && psa_hash_update(&ctx, zeros, N_LEN - len_b) != PSA_SUCCESS) {
        psa_hash_abort(&ctx);
        return ESP_FAIL;
    }
    if (psa_hash_update(&ctx, b, len_b) != PSA_SUCCESS) {
        psa_hash_abort(&ctx);
        return ESP_FAIL;
    }
    return sha512_update_finish(&ctx, out);
#else
    mbedtls_sha512_context ctx;
    mbedtls_sha512_init(&ctx);
    if (mbedtls_sha512_starts(&ctx, 0) != 0) {
        mbedtls_sha512_free(&ctx);
        return ESP_FAIL;
    }

    uint8_t zeros[N_LEN] = {0};
    if (len_a < N_LEN) {
        mbedtls_sha512_update(&ctx, zeros, N_LEN - len_a);
    }
    mbedtls_sha512_update(&ctx, a, len_a);
    if (len_b < N_LEN) {
        mbedtls_sha512_update(&ctx, zeros, N_LEN - len_b);
    }
    mbedtls_sha512_update(&ctx, b, len_b);

    int ret = mbedtls_sha512_finish(&ctx, out);
    mbedtls_sha512_free(&ctx);
    return (ret == 0) ? ESP_OK : ESP_FAIL;
#endif
}

static esp_err_t calculate_k(protocomm_ext_srp6a_client_t *ctx)
{
    uint8_t digest[HASH_LEN];
    esp_err_t err = padded_hash(N_3072, N_LEN, G_3072, sizeof(G_3072), digest);
    if (err != ESP_OK) {
        return err;
    }
    if (mbedtls_mpi_read_binary(&ctx->k, digest, HASH_LEN) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t calculate_x(const uint8_t *salt, size_t salt_len,
                             const char *username, uint16_t username_len,
                             const char *password, uint16_t password_len,
                             mbedtls_mpi *x)
{
    uint8_t inner[HASH_LEN];

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    psa_hash_operation_t ctx = PSA_HASH_OPERATION_INIT;
    if (psa_hash_setup(&ctx, PSA_ALG_SHA_512) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    if (psa_hash_update(&ctx, (const unsigned char *) username, username_len) != PSA_SUCCESS ||
            psa_hash_update(&ctx, (const unsigned char *) ":", 1) != PSA_SUCCESS ||
            psa_hash_update(&ctx, (const unsigned char *) password, password_len) != PSA_SUCCESS) {
        psa_hash_abort(&ctx);
        return ESP_FAIL;
    }
    if (sha512_update_finish(&ctx, inner) != ESP_OK) {
        return ESP_FAIL;
    }

    ctx = psa_hash_operation_init();
    if (psa_hash_setup(&ctx, PSA_ALG_SHA_512) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    if (psa_hash_update(&ctx, salt, salt_len) != PSA_SUCCESS ||
            psa_hash_update(&ctx, inner, HASH_LEN) != PSA_SUCCESS) {
        psa_hash_abort(&ctx);
        return ESP_FAIL;
    }
    if (sha512_update_finish(&ctx, inner) != ESP_OK) {
        return ESP_FAIL;
    }
#else
    mbedtls_sha512_context ctx;

    mbedtls_sha512_init(&ctx);
    if (mbedtls_sha512_starts(&ctx, 0) != 0) {
        mbedtls_sha512_free(&ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_update(&ctx, (const unsigned char *) username, username_len);
    mbedtls_sha512_update(&ctx, (const unsigned char *) ":", 1);
    mbedtls_sha512_update(&ctx, (const unsigned char *) password, password_len);
    if (mbedtls_sha512_finish(&ctx, inner) != 0) {
        mbedtls_sha512_free(&ctx);
        return ESP_FAIL;
    }

    mbedtls_sha512_init(&ctx);
    if (mbedtls_sha512_starts(&ctx, 0) != 0) {
        mbedtls_sha512_free(&ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_update(&ctx, salt, salt_len);
    mbedtls_sha512_update(&ctx, inner, HASH_LEN);
    if (mbedtls_sha512_finish(&ctx, inner) != 0) {
        mbedtls_sha512_free(&ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_free(&ctx);
#endif

    if (mbedtls_mpi_read_binary(x, inner, HASH_LEN) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t mpi_mod_normalize(mbedtls_mpi *val, const mbedtls_mpi *N)
{
    while (mbedtls_mpi_cmp_int(val, 0) < 0) {
        if (mbedtls_mpi_add_mpi(val, val, N) != 0) {
            return ESP_FAIL;
        }
    }
    if (mbedtls_mpi_cmp_mpi(val, N) >= 0) {
        if (mbedtls_mpi_mod_mpi(val, val, N) != 0) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t calculate_M(protocomm_ext_srp6a_client_t *ctx)
{
    uint8_t hash_n[HASH_LEN];
    uint8_t hash_g[HASH_LEN];
    uint8_t hash_I[HASH_LEN];
    uint8_t hash_n_xor_g[HASH_LEN];
    uint8_t zeros[N_LEN] = {0};

    if (sha512_digest(N_3072, N_LEN, hash_n) != ESP_OK) {
        return ESP_FAIL;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    {
        psa_hash_operation_t sha_ctx = PSA_HASH_OPERATION_INIT;
        if (psa_hash_setup(&sha_ctx, PSA_ALG_SHA_512) != PSA_SUCCESS) {
            return ESP_FAIL;
        }
        if (psa_hash_update(&sha_ctx, zeros, N_LEN - sizeof(G_3072)) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, G_3072, sizeof(G_3072)) != PSA_SUCCESS) {
            psa_hash_abort(&sha_ctx);
            return ESP_FAIL;
        }
        if (sha512_update_finish(&sha_ctx, hash_g) != ESP_OK) {
            return ESP_FAIL;
        }
    }
#else
    mbedtls_sha512_context sha_ctx;
    mbedtls_sha512_init(&sha_ctx);
    if (mbedtls_sha512_starts(&sha_ctx, 0) != 0) {
        mbedtls_sha512_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_update(&sha_ctx, zeros, N_LEN - sizeof(G_3072));
    mbedtls_sha512_update(&sha_ctx, G_3072, sizeof(G_3072));
    if (mbedtls_sha512_finish(&sha_ctx, hash_g) != 0) {
        mbedtls_sha512_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_free(&sha_ctx);
#endif

    for (int i = 0; i < HASH_LEN; i++) {
        hash_n_xor_g[i] = hash_n[i] ^ hash_g[i];
    }

    if (sha512_digest((const uint8_t *) ctx->username, ctx->username_len, hash_I) != ESP_OK) {
        return ESP_FAIL;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    {
        psa_hash_operation_t sha_ctx = PSA_HASH_OPERATION_INIT;
        if (psa_hash_setup(&sha_ctx, PSA_ALG_SHA_512) != PSA_SUCCESS) {
            return ESP_FAIL;
        }
        if (psa_hash_update(&sha_ctx, hash_n_xor_g, HASH_LEN) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, hash_I, HASH_LEN) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, ctx->bytes_s, ctx->len_s) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, ctx->bytes_A, ctx->len_A) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, ctx->bytes_B, ctx->len_B) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, ctx->session_key, SESSION_KEY_LEN) != PSA_SUCCESS) {
            psa_hash_abort(&sha_ctx);
            return ESP_FAIL;
        }
        if (sha512_update_finish(&sha_ctx, ctx->M) != ESP_OK) {
            return ESP_FAIL;
        }

        sha_ctx = psa_hash_operation_init();
        if (psa_hash_setup(&sha_ctx, PSA_ALG_SHA_512) != PSA_SUCCESS) {
            return ESP_FAIL;
        }
        if (psa_hash_update(&sha_ctx, ctx->bytes_A, ctx->len_A) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, ctx->M, HASH_LEN) != PSA_SUCCESS ||
                psa_hash_update(&sha_ctx, ctx->session_key, SESSION_KEY_LEN) != PSA_SUCCESS) {
            psa_hash_abort(&sha_ctx);
            return ESP_FAIL;
        }
        if (sha512_update_finish(&sha_ctx, ctx->H_AMK) != ESP_OK) {
            return ESP_FAIL;
        }
    }
#else
    mbedtls_sha512_init(&sha_ctx);
    if (mbedtls_sha512_starts(&sha_ctx, 0) != 0) {
        mbedtls_sha512_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_update(&sha_ctx, hash_n_xor_g, HASH_LEN);
    mbedtls_sha512_update(&sha_ctx, hash_I, HASH_LEN);
    mbedtls_sha512_update(&sha_ctx, ctx->bytes_s, ctx->len_s);
    mbedtls_sha512_update(&sha_ctx, ctx->bytes_A, ctx->len_A);
    mbedtls_sha512_update(&sha_ctx, ctx->bytes_B, ctx->len_B);
    mbedtls_sha512_update(&sha_ctx, ctx->session_key, SESSION_KEY_LEN);
    if (mbedtls_sha512_finish(&sha_ctx, ctx->M) != 0) {
        mbedtls_sha512_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_free(&sha_ctx);

    mbedtls_sha512_init(&sha_ctx);
    if (mbedtls_sha512_starts(&sha_ctx, 0) != 0) {
        mbedtls_sha512_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_update(&sha_ctx, ctx->bytes_A, ctx->len_A);
    mbedtls_sha512_update(&sha_ctx, ctx->M, HASH_LEN);
    mbedtls_sha512_update(&sha_ctx, ctx->session_key, SESSION_KEY_LEN);
    if (mbedtls_sha512_finish(&sha_ctx, ctx->H_AMK) != 0) {
        mbedtls_sha512_free(&sha_ctx);
        return ESP_FAIL;
    }
    mbedtls_sha512_free(&sha_ctx);
#endif

    return ESP_OK;
}

protocomm_ext_srp6a_client_t *protocomm_ext_srp6a_client_new(const char *username, uint16_t username_len,
                                                             const char *password, uint16_t password_len)
{
    if (!username || username_len == 0 || !password || password_len == 0) {
        return NULL;
    }

    protocomm_ext_srp6a_client_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

#ifdef PROTOCOMM_EXT_USE_PSA_CRYPTO
    if (psa_crypto_init() != PSA_SUCCESS) {
        free(ctx);
        return NULL;
    }
#endif

    mbedtls_mpi_init(&ctx->N);
    mbedtls_mpi_init(&ctx->g);
    mbedtls_mpi_init(&ctx->k);
    mbedtls_mpi_init(&ctx->a);
    mbedtls_mpi_init(&ctx->A);
    mbedtls_mpi_init(&ctx->RR);

    ctx->username = malloc(username_len);
    ctx->password = malloc(password_len);
    if (!ctx->username || !ctx->password) {
        goto fail;
    }
    memcpy(ctx->username, username, username_len);
    memcpy(ctx->password, password, password_len);
    ctx->username_len = username_len;
    ctx->password_len = password_len;

    if (mbedtls_mpi_read_binary(&ctx->N, N_3072, N_LEN) != 0 ||
            mbedtls_mpi_read_binary(&ctx->g, G_3072, sizeof(G_3072)) != 0) {
        goto fail;
    }

    if (calculate_k(ctx) != ESP_OK) {
        goto fail;
    }

    /* a = 256-bit random */
    if (mbedtls_mpi_fill_random(&ctx->a, 32, srp_rng, NULL) != 0) {
        goto fail;
    }

    /* A = g^a mod N */
    if (mbedtls_mpi_exp_mod(&ctx->A, &ctx->g, &ctx->a, &ctx->N, &ctx->RR) != 0) {
        goto fail;
    }

    ctx->len_A = N_LEN;
    if (mbedtls_mpi_write_binary(&ctx->A, ctx->bytes_A, N_LEN) != 0) {
        goto fail;
    }

    return ctx;

fail:
    protocomm_ext_srp6a_client_free(ctx);
    return NULL;
}

void protocomm_ext_srp6a_client_free(protocomm_ext_srp6a_client_t *ctx)
{
    if (!ctx) {
        return;
    }
    free(ctx->username);
    free(ctx->password);
    free(ctx->bytes_B);
    free(ctx->bytes_s);
    mbedtls_mpi_free(&ctx->N);
    mbedtls_mpi_free(&ctx->g);
    mbedtls_mpi_free(&ctx->k);
    mbedtls_mpi_free(&ctx->a);
    mbedtls_mpi_free(&ctx->A);
    mbedtls_mpi_free(&ctx->RR);
    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
}

esp_err_t protocomm_ext_srp6a_client_get_pubkey(protocomm_ext_srp6a_client_t *ctx,
                                                uint8_t *bytes_A, size_t *len_A)
{
    if (!ctx || !bytes_A || !len_A || *len_A < N_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(bytes_A, ctx->bytes_A, N_LEN);
    *len_A = N_LEN;
    return ESP_OK;
}

esp_err_t protocomm_ext_srp6a_client_process_challenge(protocomm_ext_srp6a_client_t *ctx,
                                                       const uint8_t *salt, size_t salt_len,
                                                       const uint8_t *bytes_B, size_t len_B,
                                                       uint8_t *client_proof, size_t proof_len)
{
    if (!ctx || !salt || salt_len == 0 || !bytes_B || len_B == 0 || len_B > N_LEN ||
            !client_proof || proof_len < HASH_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_mpi B, B_mod, u, x, v, kv, base, ux, exp, S;
    uint8_t u_digest[HASH_LEN];
    uint8_t *bytes_S = NULL;
    size_t len_S = 0;
    esp_err_t err = ESP_FAIL;

    mbedtls_mpi_init(&B);
    mbedtls_mpi_init(&B_mod);
    mbedtls_mpi_init(&u);
    mbedtls_mpi_init(&x);
    mbedtls_mpi_init(&v);
    mbedtls_mpi_init(&kv);
    mbedtls_mpi_init(&base);
    mbedtls_mpi_init(&ux);
    mbedtls_mpi_init(&exp);
    mbedtls_mpi_init(&S);

    free(ctx->bytes_B);
    free(ctx->bytes_s);
    ctx->bytes_B = malloc(len_B);
    ctx->bytes_s = malloc(salt_len);
    if (!ctx->bytes_B || !ctx->bytes_s) {
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memcpy(ctx->bytes_B, bytes_B, len_B);
    memcpy(ctx->bytes_s, salt, salt_len);
    ctx->len_B = len_B;
    ctx->len_s = salt_len;

    if (mbedtls_mpi_read_binary(&B, bytes_B, len_B) != 0) {
        goto cleanup;
    }

    /* SRP-6a safety: B % N != 0 */
    if (mbedtls_mpi_mod_mpi(&B_mod, &B, &ctx->N) != 0 || mbedtls_mpi_cmp_int(&B_mod, 0) == 0) {
        ESP_LOGE(TAG, "Invalid server public key B");
        goto cleanup;
    }

    if (padded_hash(ctx->bytes_A, ctx->len_A, bytes_B, len_B, u_digest) != ESP_OK) {
        goto cleanup;
    }
    if (mbedtls_mpi_read_binary(&u, u_digest, HASH_LEN) != 0) {
        goto cleanup;
    }
    if (mbedtls_mpi_cmp_int(&u, 0) == 0) {
        ESP_LOGE(TAG, "Invalid scrambling parameter u");
        goto cleanup;
    }

    if (calculate_x(salt, salt_len, ctx->username, ctx->username_len,
                    ctx->password, ctx->password_len, &x) != ESP_OK) {
        goto cleanup;
    }

    /* v = g^x mod N */
    if (mbedtls_mpi_exp_mod(&v, &ctx->g, &x, &ctx->N, &ctx->RR) != 0) {
        goto cleanup;
    }

    /* base = (B - k*v) mod N */
    if (mbedtls_mpi_mul_mpi(&kv, &ctx->k, &v) != 0 ||
            mbedtls_mpi_mod_mpi(&kv, &kv, &ctx->N) != 0 ||
            mbedtls_mpi_sub_mpi(&base, &B, &kv) != 0) {
        goto cleanup;
    }
    if (mpi_mod_normalize(&base, &ctx->N) != ESP_OK) {
        goto cleanup;
    }

    /* exp = a + u*x */
    if (mbedtls_mpi_mul_mpi(&ux, &u, &x) != 0 ||
            mbedtls_mpi_add_mpi(&exp, &ctx->a, &ux) != 0) {
        goto cleanup;
    }

    /* S = base^exp mod N */
    if (mbedtls_mpi_exp_mod(&S, &base, &exp, &ctx->N, &ctx->RR) != 0) {
        goto cleanup;
    }

    len_S = mbedtls_mpi_size(&S);
    if (len_S == 0) {
        goto cleanup;
    }
    bytes_S = malloc(len_S);
    if (!bytes_S) {
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    if (mbedtls_mpi_write_binary(&S, bytes_S, len_S) != 0) {
        goto cleanup;
    }

    if (sha512_digest(bytes_S, len_S, ctx->session_key) != ESP_OK) {
        goto cleanup;
    }

    if (calculate_M(ctx) != ESP_OK) {
        goto cleanup;
    }

    memcpy(client_proof, ctx->M, HASH_LEN);
    ctx->challenge_done = true;
    err = ESP_OK;

cleanup:
    free(bytes_S);
    mbedtls_mpi_free(&B);
    mbedtls_mpi_free(&B_mod);
    mbedtls_mpi_free(&u);
    mbedtls_mpi_free(&x);
    mbedtls_mpi_free(&v);
    mbedtls_mpi_free(&kv);
    mbedtls_mpi_free(&base);
    mbedtls_mpi_free(&ux);
    mbedtls_mpi_free(&exp);
    mbedtls_mpi_free(&S);
    return err;
}

esp_err_t protocomm_ext_srp6a_client_verify_session(protocomm_ext_srp6a_client_t *ctx,
                                                    const uint8_t *host_proof, size_t proof_len)
{
    if (!ctx || !host_proof || proof_len < HASH_LEN || !ctx->challenge_done) {
        return ESP_ERR_INVALID_ARG;
    }
    if (memcmp(ctx->H_AMK, host_proof, HASH_LEN) != 0) {
        ESP_LOGE(TAG, "Device proof mismatch");
        ctx->authenticated = false;
        return ESP_FAIL;
    }
    ctx->authenticated = true;
    return ESP_OK;
}

const uint8_t *protocomm_ext_srp6a_client_get_session_key(protocomm_ext_srp6a_client_t *ctx, size_t *len)
{
    if (!ctx || !ctx->challenge_done) {
        if (len) {
            *len = 0;
        }
        return NULL;
    }
    if (len) {
        *len = SESSION_KEY_LEN;
    }
    return ctx->session_key;
}

bool protocomm_ext_srp6a_client_authenticated(const protocomm_ext_srp6a_client_t *ctx)
{
    return ctx && ctx->authenticated;
}
