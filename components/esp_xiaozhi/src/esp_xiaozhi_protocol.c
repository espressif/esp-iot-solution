/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <math.h>
#include "esp_xiaozhi_protocol_priv.h"

static esp_xiaozhi_protocol_capabilities_t s_capabilities = {
    .mcp = true, .mcp_20260728 = true, .udp_probe = true,
};
static char s_bundle[128];
static char s_charset[64];

esp_err_t esp_xiaozhi_protocol_set_capabilities(const esp_xiaozhi_protocol_capabilities_t *capabilities)
{
    if (!capabilities) {
        s_capabilities = (esp_xiaozhi_protocol_capabilities_t) {
            .mcp = true, .mcp_20260728 = true, .udp_probe = true,
        };
        return ESP_OK;
    }
    if (capabilities->mcp_20260728 && !capabilities->mcp) {
        return ESP_ERR_INVALID_ARG;
    }
    if (capabilities->glyph_push && (!capabilities->font_bundle || !capabilities->font_charset ||
                                     !capabilities->font_size || !capabilities->font_bpp ||
                                     strlen(capabilities->font_bundle) >= sizeof(s_bundle) ||
                                     strlen(capabilities->font_charset) >= sizeof(s_charset))) {
        return ESP_ERR_INVALID_ARG;
    }
    s_capabilities = *capabilities;
    if (capabilities->glyph_push) {
        strcpy(s_bundle, capabilities->font_bundle);
        strcpy(s_charset, capabilities->font_charset);
        s_capabilities.font_bundle = s_bundle;
        s_capabilities.font_charset = s_charset;
    } else {
        s_capabilities.font_bundle = NULL;
        s_capabilities.font_charset = NULL;
    }
    return ESP_OK;
}

bool esp_xiaozhi_protocol_probe_enabled(void)
{
    return s_capabilities.udp_probe;
}

esp_err_t esp_xiaozhi_protocol_read_session_id(const cJSON *root, char *session, size_t capacity)
{
    if (!cJSON_IsObject(root) || !session || !capacity) {
        return ESP_ERR_INVALID_ARG;
    }
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    if (id && (!cJSON_IsString(id) || strlen(id->valuestring) >= capacity)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    strcpy(session, id ? id->valuestring : "");
    return ESP_OK;
}

bool esp_xiaozhi_protocol_is_legacy_initialize(const cJSON *request)
{
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(request, "jsonrpc");
    const cJSON *method = cJSON_GetObjectItemCaseSensitive(request, "method");
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(request, "id");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(request, "params");
    const cJSON *meta = cJSON_GetObjectItemCaseSensitive(params, "_meta");
    bool valid_id = cJSON_IsString(id) || (cJSON_IsNumber(id) && isfinite(id->valuedouble) &&
                                           trunc(id->valuedouble) == id->valuedouble && fabs(id->valuedouble) <= 9007199254740991.0);
    return cJSON_IsObject(request) && cJSON_IsString(version) && !strcmp(version->valuestring, "2.0") &&
           cJSON_IsString(method) && !strcmp(method->valuestring, "initialize") && valid_id &&
           (!params || cJSON_IsObject(params)) && (!meta || cJSON_IsObject(meta)) &&
           !cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/protocolVersion") &&
           !cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/clientCapabilities") &&
           !cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/clientInfo") &&
           !cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/logLevel");
}

esp_err_t esp_xiaozhi_protocol_add_capabilities(cJSON *root, bool include_font)
{
    cJSON *features = cJSON_AddObjectToObject(root, "features");
    if (!features || !cJSON_AddBoolToObject(features, "mcp", s_capabilities.mcp) ||
            !cJSON_AddBoolToObject(features, "mcp_20260728", s_capabilities.mcp_20260728) ||
            !cJSON_AddBoolToObject(features, "udp_probe", s_capabilities.udp_probe) ||
            !cJSON_AddBoolToObject(features, "aec", s_capabilities.aec) ||
            !cJSON_AddBoolToObject(features, "glyph_push", s_capabilities.glyph_push)) {
        return ESP_ERR_NO_MEM;
    }
    if (include_font && s_capabilities.glyph_push) {
        cJSON *font = cJSON_AddObjectToObject(root, "text_font");
        if (!font || !cJSON_AddStringToObject(font, "bundle", s_capabilities.font_bundle) ||
                !cJSON_AddStringToObject(font, "charset", s_capabilities.font_charset) ||
                !cJSON_AddNumberToObject(font, "size", s_capabilities.font_size) ||
                !cJSON_AddNumberToObject(font, "bpp", s_capabilities.font_bpp)) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

static uint32_t frame_read_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
}

static void frame_write_u32(uint8_t *data, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) {
        data[i] = value >> (24 - 8 * i);
    }
}

esp_err_t esp_xiaozhi_protocol_encode_audio(int version, const uint8_t *data, size_t length,
                                            uint32_t timestamp, uint8_t *output, size_t capacity, size_t *written)
{
    if (version < 1 || version > 3 || !data || !output || !written || !length) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t header = version == 2 ? 16 : version == 3 ? 4 : 0;
    if (length > UINT32_MAX || (version == 3 && length > UINT16_MAX) ||
            capacity < header || length > capacity - header) {
        return ESP_ERR_INVALID_SIZE;
    }
    memset(output, 0, header);
    if (version == 2) {
        output[1] = 2;
        frame_write_u32(output + 8, timestamp);
        frame_write_u32(output + 12, length);
    } else if (version == 3) {
        output[2] = length >> 8;
        output[3] = length;
    }
    memcpy(output + header, data, length);
    *written = header + length;
    return ESP_OK;
}

esp_err_t esp_xiaozhi_protocol_decode_frame(int version, const uint8_t *data, size_t length,
                                            const uint8_t **payload, size_t *payload_length,
                                            uint32_t *timestamp, unsigned *type)
{
    if (version < 1 || version > 3 || !data || !payload || !payload_length || !timestamp || !type) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t header = version == 2 ? 16 : version == 3 ? 4 : 0;
    if (length < header) {
        return ESP_ERR_INVALID_SIZE;
    }
    *timestamp = 0;
    *type = 0;
    size_t declared = length;
    if (version == 2) {
        if (data[0] || data[1] != 2 || data[2]) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        *type = data[3];
        *timestamp = frame_read_u32(data + 8);
        declared = frame_read_u32(data + 12);
    } else if (version == 3) {
        *type = data[0];
        declared = ((size_t)data[2] << 8) | data[3];
    }
    if (*type > 1 || declared != length - header) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *payload = data + header;
    *payload_length = length - header;
    return ESP_OK;
}
