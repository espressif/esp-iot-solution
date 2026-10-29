/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <esp_err.h>
#include <esp_log.h>
#include "sdkconfig.h"

#include <protocomm_ext.h>
#include <protocomm_ext_security.h>
#include <protocomm_ext_security0.h>
#include <protocomm_ext_security1.h>
#include <protocomm_ext_security2.h>
#include <protocomm_ext_transports.h>
#include <protocomm_ext_http.h>
#include <protocomm_ext_console.h>
#if CONFIG_BT_NIMBLE_ENABLED
#include <protocomm_ext_nimble.h>
#endif
#include <protocomm_ext_priv.h>

static const char *TAG = "protocomm_ext";

#define PROTOCOMM_EXT_RETURN_IF_FALSE(cond, err, msg) do { \
        if (!(cond)) {                                     \
            ESP_LOGE(TAG, "%s", (msg));                    \
            return (err);                                  \
        }                                                   \
    } while (0)

#define PROTOCOMM_EXT_RETURN_NULL_IF_FALSE(cond, msg) do { \
        if (!(cond)) {                                     \
            ESP_LOGE(TAG, "%s", (msg));                    \
            return NULL;                                   \
        }                                                   \
    } while (0)

#define PROTOCOMM_EXT_RETURN_VOID_IF_FALSE(cond, msg) do { \
        if (!(cond)) {                                     \
            ESP_LOGE(TAG, "%s", (msg));                    \
            return;                                        \
        }                                                   \
    } while (0)

#define PROTOCOMM_EXT_CHECK_ARG(cond, msg) \
    PROTOCOMM_EXT_RETURN_IF_FALSE((cond), ESP_ERR_INVALID_ARG, (msg))

#define PROTOCOMM_EXT_CHECK_STATE(cond, msg) \
    PROTOCOMM_EXT_RETURN_IF_FALSE((cond), ESP_ERR_INVALID_STATE, (msg))

static void free_security_data(protocomm_ext_t *pc)
{
    if (!pc || !pc->security_data) {
        return;
    }

    if (pc->security_method == PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1) {
        protocomm_ext_security1_params_t *security_data =
            (protocomm_ext_security1_params_t *)pc->security_data;
        free((void *)security_data->data);
        free(security_data);
    } else if (pc->security_method == PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2) {
        protocomm_ext_security2_params_t *security_data =
            (protocomm_ext_security2_params_t *)pc->security_data;
        free((void *)security_data->username);
        free((void *)security_data->password);
        free(security_data);
    }

    pc->security_data = NULL;
}

static esp_err_t copy_security1_params(protocomm_ext_t *pc, const void *security_data)
{
    if (!security_data) {
        return ESP_OK;
    }

    const protocomm_ext_security1_params_t *src =
        (const protocomm_ext_security1_params_t *)security_data;
    if (!src->data || src->len == 0) {
        ESP_LOGE(TAG, "Security1 PoP data is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_ext_security1_params_t *dst = calloc(1, sizeof(*dst));
    if (!dst) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t *pop_copy = calloc(1, src->len);
    if (!pop_copy) {
        free(dst);
        return ESP_ERR_NO_MEM;
    }

    memcpy(pop_copy, src->data, src->len);
    dst->data = pop_copy;
    dst->len = src->len;
    pc->security_data = dst;
    return ESP_OK;
}

static esp_err_t copy_security2_params(protocomm_ext_t *pc, const void *security_data)
{
    if (!security_data) {
        ESP_LOGE(TAG, "Security2 requires username/password params");
        return ESP_ERR_INVALID_ARG;
    }

    const protocomm_ext_security2_params_t *src =
        (const protocomm_ext_security2_params_t *)security_data;
    if (!src->username || src->username_len == 0 ||
            !src->password || src->password_len == 0) {
        ESP_LOGE(TAG, "Security2 username/password is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_ext_security2_params_t *dst = calloc(1, sizeof(*dst));
    if (!dst) {
        return ESP_ERR_NO_MEM;
    }

    char *user_copy = calloc(1, src->username_len + 1);
    char *pass_copy = calloc(1, src->password_len + 1);
    if (!user_copy || !pass_copy) {
        free(user_copy);
        free(pass_copy);
        free(dst);
        return ESP_ERR_NO_MEM;
    }

    memcpy(user_copy, src->username, src->username_len);
    memcpy(pass_copy, src->password, src->password_len);
    dst->username = user_copy;
    dst->username_len = src->username_len;
    dst->password = pass_copy;
    dst->password_len = src->password_len;
    pc->security_data = dst;
    return ESP_OK;
}

protocomm_ext_t *protocomm_ext_new(protocomm_ext_config_data_t *config)
{
    PROTOCOMM_EXT_RETURN_NULL_IF_FALSE(config, "Invalid config");

    protocomm_ext_t *pc = (protocomm_ext_t *)calloc(1, sizeof(protocomm_ext_t));
    PROTOCOMM_EXT_RETURN_NULL_IF_FALSE(pc, "Error allocating protocomm_ext");

    switch (config->transport_method) {
    case PROTOCOMM_EXT_TRANSPORT_METHOD_HTTP:
        pc->transport = &protocomm_ext_transport_http;
        break;
    case PROTOCOMM_EXT_TRANSPORT_METHOD_CONSOLE:
        pc->transport = &protocomm_ext_transport_console;
        break;
    case PROTOCOMM_EXT_TRANSPORT_METHOD_BLE:
#if CONFIG_BT_NIMBLE_ENABLED
        pc->transport = &protocomm_ext_transport_nimble;
        break;
#else
        ESP_LOGE(TAG, "NimBLE transport requires CONFIG_BT_NIMBLE_ENABLED");
        goto error;
#endif
    default:
        ESP_LOGE(TAG, "Unsupported transport method %d", config->transport_method);
        goto error;
    }

    pc->transport_method = config->transport_method;

    esp_err_t ret = pc->transport->init(&pc->transport_inst, config->transport_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize transport");
        goto error;
    }

    switch (config->security_method) {
    case PROTOCOMM_EXT_SECURITY_METHOD_NONE:
#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_0
        pc->sec = &protocomm_ext_security0;
        break;
#else
        ESP_LOGE(TAG, "Security 0 disabled in Kconfig");
        goto error;
#endif
    case PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1:
#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_1
        pc->sec = &protocomm_ext_security1;
        if (copy_security1_params(pc, config->security_data) != ESP_OK) {
            goto error;
        }
        break;
#else
        ESP_LOGE(TAG, "Security 1 disabled in Kconfig");
        goto error;
#endif
    case PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2:
#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_2
        pc->sec = &protocomm_ext_security2;
        if (copy_security2_params(pc, config->security_data) != ESP_OK) {
            goto error;
        }
        break;
#else
        ESP_LOGE(TAG, "Security 2 disabled in Kconfig");
        goto error;
#endif
    default:
        ESP_LOGE(TAG, "Unsupported security method %d", config->security_method);
        goto error;
    }

    pc->security_method = config->security_method;
    ESP_LOGI(TAG, "protocomm_ext instance created");
    return pc;

error:
    protocomm_ext_delete(pc);
    return NULL;
}

void protocomm_ext_delete(protocomm_ext_t *pc)
{
    PROTOCOMM_EXT_RETURN_VOID_IF_FALSE(pc, "Invalid instance to delete");

    if (pc->transport && pc->transport_inst) {
        if (pc->transport->disconnect) {
            pc->transport->disconnect(pc->transport_inst);
        }
        if (pc->transport->deinit) {
            pc->transport->deinit(pc->transport_inst);
        }
        pc->transport_inst = NULL;
    }

    if (pc->sec && pc->sec_inst && pc->sec->cleanup) {
        pc->sec->cleanup(pc->sec_inst);
        pc->sec_inst = NULL;
    }

    free_security_data(pc);

    free(pc->version_str);
    pc->version_str = NULL;

    free(pc);
    ESP_LOGI(TAG, "protocomm_ext instance deleted");
}

esp_err_t protocomm_ext_open_session(protocomm_ext_t *pc, const void *config)
{
    PROTOCOMM_EXT_CHECK_ARG(pc, "Invalid instance to open session");
    PROTOCOMM_EXT_CHECK_STATE(pc->transport && pc->transport_inst,
                              "Transport not initialized");

    esp_err_t ret = pc->transport->connect(pc->transport_inst, config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open session");
        return ret;
    }

    ESP_LOGI(TAG, "Session opened");
    return ESP_OK;
}

esp_err_t protocomm_ext_close_session(protocomm_ext_t *pc)
{
    PROTOCOMM_EXT_CHECK_ARG(pc, "Invalid instance to close session");
    PROTOCOMM_EXT_CHECK_STATE(pc->transport && pc->transport_inst,
                              "Transport not initialized");

    esp_err_t ret = pc->transport->disconnect(pc->transport_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to close session");
        return ret;
    }

    ESP_LOGI(TAG, "Session closed");
    return ESP_OK;
}

esp_err_t protocomm_ext_get_version_capabilities(protocomm_ext_t *pc, const char *ep_name,
                                                 uint8_t **out_data, size_t *out_data_len)
{
    PROTOCOMM_EXT_CHECK_ARG(pc && ep_name, "Invalid instance or endpoint name");
    PROTOCOMM_EXT_CHECK_ARG(out_data && out_data_len, "Invalid output pointer");
    PROTOCOMM_EXT_CHECK_STATE(pc->transport && pc->transport_inst,
                              "Transport not initialized");

    *out_data = NULL;
    *out_data_len = 0;

    /*
     * Device NimBLE skips zero-length writes. esp_prov sends "---" for proto-ver;
     * the peer ignores the body and returns the version JSON on the subsequent read.
     */
    static const char ver_probe[] = "---";
    uint8_t *resp_buf = NULL;
    ssize_t resp_len = 0;

    esp_err_t ret = pc->transport->send_data(pc->transport_inst, ep_name,
                                             (const uint8_t *)ver_probe,
                                             (ssize_t)(sizeof(ver_probe) - 1),
                                             &resp_buf, &resp_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to query version endpoint");
        return ret;
    }

    if (pc->version_str) {
        free(pc->version_str);
        pc->version_str = NULL;
    }
    if (resp_buf && resp_len > 0) {
        pc->version_str = calloc(1, (size_t)resp_len + 1);
        if (pc->version_str) {
            memcpy(pc->version_str, resp_buf, (size_t)resp_len);
        }
    }

    *out_data = resp_buf;
    *out_data_len = (size_t)resp_len;
    return ESP_OK;
}

esp_err_t protocomm_ext_set_security(protocomm_ext_t *pc,
                                     protocomm_ext_security_method_t security_method,
                                     const void *security_data)
{
    PROTOCOMM_EXT_CHECK_ARG(pc, "Invalid instance");

    if (pc->sec && pc->sec_inst && pc->sec->cleanup) {
        pc->sec->cleanup(pc->sec_inst);
        pc->sec_inst = NULL;
    }
    free_security_data(pc);
    pc->sec = NULL;

    switch (security_method) {
    case PROTOCOMM_EXT_SECURITY_METHOD_NONE:
#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_0
        pc->sec = &protocomm_ext_security0;
        break;
#else
        ESP_LOGE(TAG, "Security 0 disabled in Kconfig");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    case PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_1:
#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_1
        pc->sec = &protocomm_ext_security1;
        if (copy_security1_params(pc, security_data) != ESP_OK) {
            pc->sec = NULL;
            return ESP_ERR_INVALID_ARG;
        }
        break;
#else
        ESP_LOGE(TAG, "Security 1 disabled in Kconfig");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    case PROTOCOMM_EXT_SECURITY_METHOD_SECURITY_2:
#if CONFIG_ESP_PROTOCOMM_EXT_SUPPORT_SECURITY_VERSION_2
        pc->sec = &protocomm_ext_security2;
        if (copy_security2_params(pc, security_data) != ESP_OK) {
            pc->sec = NULL;
            return ESP_ERR_INVALID_ARG;
        }
        break;
#else
        ESP_LOGE(TAG, "Security 2 disabled in Kconfig");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    default:
        ESP_LOGE(TAG, "Unsupported security method %d", security_method);
        return ESP_ERR_INVALID_ARG;
    }

    pc->security_method = security_method;
    ESP_LOGI(TAG, "Security method set to %d", (int)security_method);
    return ESP_OK;
}

esp_err_t protocomm_ext_security_init(protocomm_ext_t *pc)
{
    PROTOCOMM_EXT_CHECK_ARG(pc, "Invalid instance");
    PROTOCOMM_EXT_CHECK_STATE(pc->sec, "Security not configured");

    if (pc->sec_inst) {
        return ESP_OK;
    }
    if (!pc->sec->init) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing security (Sec2 SRP may take several seconds)...");
    esp_err_t ret = pc->sec->init(&pc->sec_inst, pc->security_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize security");
        return ret;
    }
    ESP_LOGI(TAG, "Security local init done");
    return ESP_OK;
}

esp_err_t protocomm_ext_establish_security(protocomm_ext_t *pc, const char *ep_name)
{
    PROTOCOMM_EXT_CHECK_ARG(pc && ep_name, "Invalid instance or endpoint name");
    PROTOCOMM_EXT_CHECK_STATE(pc->sec, "Security not configured");
    PROTOCOMM_EXT_CHECK_STATE(pc->transport && pc->transport_inst,
                              "Transport not initialized");

    if (!pc->sec_inst) {
        esp_err_t ret = protocomm_ext_security_init(pc);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    uint8_t *req_buf = NULL;
    uint8_t *resp_buf = NULL;
    ssize_t req_len = 0;
    ssize_t resp_len = 0;

    esp_err_t ret = pc->sec->security_send_command0(pc->sec_inst, &req_buf, &req_len, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build security command0");
        return ret;
    }

    ret = pc->transport->send_data(pc->transport_inst, ep_name, req_buf, req_len,
                                   &resp_buf, &resp_len);
    free(req_buf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send security command0");
        return ret;
    }

    ret = pc->sec->security_parse_command0(pc->sec_inst, resp_buf, resp_len, NULL);
    free(resp_buf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse security command0 response");
        return ret;
    }

    if (pc->sec->security_send_command1) {
        req_buf = NULL;
        resp_buf = NULL;
        req_len = 0;
        resp_len = 0;

        ret = pc->sec->security_send_command1(pc->sec_inst, &req_buf, &req_len, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to build security command1");
            return ret;
        }

        ret = pc->transport->send_data(pc->transport_inst, ep_name, req_buf, req_len,
                                       &resp_buf, &resp_len);
        free(req_buf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send security command1");
            return ret;
        }

        if (pc->sec->security_parse_command1) {
            ret = pc->sec->security_parse_command1(pc->sec_inst, resp_buf, resp_len, NULL);
        }
        free(resp_buf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to parse security command1 response");
            return ret;
        }
    }

    ESP_LOGI(TAG, "Security established");
    return ESP_OK;
}

esp_err_t protocomm_ext_send_data(protocomm_ext_t *pc, const char *ep_name,
                                  const uint8_t *data, size_t data_len,
                                  uint8_t **out_data, size_t *out_data_len)
{
    PROTOCOMM_EXT_CHECK_ARG(pc && ep_name, "Invalid instance or endpoint name");
    PROTOCOMM_EXT_CHECK_ARG(data && data_len, "Invalid data");
    PROTOCOMM_EXT_CHECK_ARG(out_data && out_data_len, "Invalid output pointer");
    PROTOCOMM_EXT_CHECK_STATE(pc->sec, "Security not configured");
    PROTOCOMM_EXT_CHECK_STATE(pc->transport && pc->transport_inst,
                              "Transport not initialized");

    /* Sec0 may leave sec_inst unset if init is a no-op that still sets a sentinel;
     * require either a handle or that encrypt/decrypt are unused (Sec0). */
    if (pc->sec->encrypt || pc->sec->decrypt) {
        PROTOCOMM_EXT_CHECK_STATE(pc->sec_inst, "Security session not established");
    }

    *out_data = NULL;
    *out_data_len = 0;

    uint8_t *encrypted_data = NULL;
    uint8_t *resp_buf = NULL;
    ssize_t encrypted_len = 0;
    ssize_t resp_len = 0;
    bool free_encrypted = false;

    if (pc->sec->encrypt) {
        esp_err_t ret = pc->sec->encrypt(pc->sec_inst, data, (ssize_t)data_len,
                                         &encrypted_data, &encrypted_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to encrypt request");
            return ret;
        }
        free_encrypted = true;
    } else {
        encrypted_data = (uint8_t *)data;
        encrypted_len = (ssize_t)data_len;
    }

    esp_err_t ret = pc->transport->send_data(pc->transport_inst, ep_name,
                                             encrypted_data, encrypted_len,
                                             &resp_buf, &resp_len);
    if (free_encrypted) {
        free(encrypted_data);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send data");
        return ret;
    }

    if (pc->sec->decrypt) {
        ssize_t dec_len = 0;
        ret = pc->sec->decrypt(pc->sec_inst, resp_buf, resp_len, out_data, &dec_len);
        free(resp_buf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to decrypt response");
            return ret;
        }
        if (dec_len < 0) {
            ESP_LOGE(TAG, "Decrypt returned negative length");
            free(*out_data);
            *out_data = NULL;
            *out_data_len = 0;
            return ESP_FAIL;
        }
        *out_data_len = (size_t)dec_len;
    } else {
        *out_data = resp_buf;
        *out_data_len = (size_t)resp_len;
    }

    return ESP_OK;
}

void *protocomm_ext_get_transport_handle(protocomm_ext_t *pc)
{
    if (!pc) {
        return NULL;
    }
    return pc->transport_inst;
}

protocomm_ext_transport_method_t protocomm_ext_get_transport_method(const protocomm_ext_t *pc)
{
    if (!pc) {
        return PROTOCOMM_EXT_TRANSPORT_METHOD_MAX;
    }
    return pc->transport_method;
}

esp_err_t protocomm_ext_set_config_endpoint(protocomm_ext_t *pc,
                                            const char *endpoint_name,
                                            uint16_t uuid)
{
    PROTOCOMM_EXT_CHECK_ARG(pc && endpoint_name, "Invalid instance or endpoint name");
    PROTOCOMM_EXT_CHECK_STATE(pc->transport && pc->transport_inst,
                              "Transport not initialized");

    /* HTTP / Console: characteristic UUID is unused. */
    if (!pc->transport->set_config_endpoint) {
        return ESP_OK;
    }

    return pc->transport->set_config_endpoint(pc->transport_inst, endpoint_name, uuid);
}
