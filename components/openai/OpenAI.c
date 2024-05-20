/* SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/cdefs.h>
#include <inttypes.h>
#include "cJSON.h"
#include "esp_log.h"
#include "OpenAI.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG = "OpenAI";

#define OPENAI_DEFAULT_BASE_URL CONFIG_DEFAULT_OPENAI_BASE_URL

#define OPENAI_ERROR_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#define OPENAI_ERROR_CHECK_ABORT(a, str) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        abort(); \
    }

#define OPENAI_ERROR_CHECK_RETURN_VOID(a, str) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return; \
    }

#define OPENAI_ERROR_CHECK_CONTINUE(a, str) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    }

#define OPENAI_ERROR_CHECK_GOTO(a, str, label) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        goto label; \
    }

#define str_extend(wt, ...) {                              \
    char *tmp = (wt);                                     \
    if (asprintf(&(wt), __VA_ARGS__) < 0) abort();         \
    if (tmp) { free(tmp); tmp = NULL; }                    \
}

// Macros for building the request
#define reqAddString(var, val)                            \
    if (cJSON_AddStringToObject(req, var, val) == NULL) { \
        cJSON_Delete(req);                                \
        ESP_LOGE(TAG, "cJSON_AddStringToObject failed!");      \
        return result;                                    \
    }

#define reqAddNumber(var, val)                            \
    if (cJSON_AddNumberToObject(req, var, val) == NULL) { \
        cJSON_Delete(req);                                \
        ESP_LOGE(TAG, "cJSON_AddNumberToObject failed!");      \
        return result;                                    \
    }

#define reqAddBool(var, val)                            \
    if (cJSON_AddBoolToObject(req, var, val) == NULL) { \
        cJSON_Delete(req);                              \
        ESP_LOGE(TAG, "cJSON_AddBoolToObject failed!");      \
        return result;                                  \
    }

#define reqAddItem(var, val)                       \
    if (!cJSON_AddItemToObject(req, var, val)) {   \
        cJSON_Delete(req);                         \
        cJSON_Delete(val);                         \
        ESP_LOGE(TAG, "cJSON_AddItemToObject failed!"); \
        return result;                             \
    }

char *getJsonError(cJSON *json)
{
    if (json == NULL) {
        return strdup("cJSON_Parse failed!");
    }
    if (!cJSON_IsObject(json)) {
        char *jsonStr = cJSON_Print(json);
        char *errorMsg = NULL;
        asprintf(&errorMsg, "\"code\": Response is not an object! %s", jsonStr);
        OPENAI_ERROR_CHECK(errorMsg != NULL, "asprintf failed!", NULL);
        free(jsonStr);
        return errorMsg;
    }
    if (cJSON_HasObjectItem(json, "error")) {
        cJSON *error = cJSON_GetObjectItem(json, "error");
        if (!cJSON_IsObject(error)) {
            char *jsonStr = cJSON_Print(error);
            char *errorMsg = NULL;
            asprintf(&errorMsg, "\"code\": Error is not an object! %s", jsonStr);
            OPENAI_ERROR_CHECK(errorMsg != NULL, "asprintf failed!", NULL);
            free(jsonStr);
            return errorMsg;
        }
        if (!cJSON_HasObjectItem(error, "code")) {
            char *jsonStr = cJSON_Print(error);
            char *errorMsg = NULL;
            asprintf(&errorMsg, "\"code\": Error does not contain code! %s", jsonStr);
            OPENAI_ERROR_CHECK(errorMsg != NULL, "asprintf failed!", NULL);
            free(jsonStr);
            return errorMsg;
        }
        cJSON *error_code = cJSON_GetObjectItem(error, "code");
        char *errorMsg = NULL;
        asprintf(&errorMsg, "\"code\": %s!", cJSON_GetStringValue(error_code));
        return errorMsg;
    }
    return NULL;
}

//
// OpenAI
//

typedef struct {
    OpenAI_t parent;                                                                                             /*!<  Parent object */
    char *api_key;                                                                                               /*!<  API key for OpenAI */
    char *base_url;                                                                                              /*!<  Base URL for OpenAI or Other compatible API */

    char *(*get)(const char *base_url, const char *api_key, const char *endpoint);                                                     /*!<  Perform an HTTP GET request. */
    char *(*del)(const char *base_url, const char *api_key, const char *endpoint);                                                     /*!<  Perform an HTTP DELETE request. */
    char *(*post)(const char *base_url, const char *api_key, const char *endpoint, char *jsonBody);                                    /*!<  Perform an HTTP POST request. */
    char *(*speechpost)(const char *base_url, const char *api_key, const char *endpoint, char *jsonBody, size_t *output_len);          /*!<  Perform an HTTP POST request for speech. */
    char *(*upload)(const char *base_url, const char *api_key, const char *endpoint, const char *boundary, uint8_t *data, size_t len); /*!<  Upload data using an HTTP request. */
} _OpenAI_t;

//
// OpenAI_EmbeddingResponse
//

typedef struct {
    OpenAI_EmbeddingResponse_t parent;
    uint32_t usage;
    uint32_t len;
    OpenAI_EmbeddingData_t *data;
    char *error_str;
} _OpenAI_EmbeddingResponse_t;

static void OpenAI_EmbeddingResponseDelete(OpenAI_EmbeddingResponse_t *embeddingResponse)
{
    _OpenAI_EmbeddingResponse_t *_embeddingResponse = __containerof(embeddingResponse, _OpenAI_EmbeddingResponse_t, parent);
    if (_embeddingResponse != NULL) {
        if (_embeddingResponse->data) {
            for (int i = 0; i < _embeddingResponse->len; i++) {
                free(_embeddingResponse->data[i].data);
                _embeddingResponse->data[i].data = NULL;
            }
            free(_embeddingResponse->data);
            _embeddingResponse->data = NULL;
        }

        if (_embeddingResponse->error_str != NULL) {
            free(_embeddingResponse->error_str);
            _embeddingResponse->error_str = NULL;
        }

        free(_embeddingResponse);
        _embeddingResponse = NULL;
    }
}

static uint32_t OpenAI_EmbeddingResponseGetUsage(OpenAI_EmbeddingResponse_t *embeddingResponse)
{
    _OpenAI_EmbeddingResponse_t *_embeddingResponse = __containerof(embeddingResponse, _OpenAI_EmbeddingResponse_t, parent);
    return _embeddingResponse->usage;
}

static uint32_t OpenAI_EmbeddingResponseGetLen(OpenAI_EmbeddingResponse_t *embeddingResponse)
{
    _OpenAI_EmbeddingResponse_t *_embeddingResponse = __containerof(embeddingResponse, _OpenAI_EmbeddingResponse_t, parent);
    return _embeddingResponse->len;
}

static OpenAI_EmbeddingData_t *OpenAI_EmbeddingResponseGetDate(OpenAI_EmbeddingResponse_t *embeddingResponse, uint32_t index)
{
    _OpenAI_EmbeddingResponse_t *_embeddingResponse = __containerof(embeddingResponse, _OpenAI_EmbeddingResponse_t, parent);
    if (index < _embeddingResponse->len) {
        return &_embeddingResponse->data[index];
    }
    return NULL;
}

static char *OpenAI_EmbeddingResponseGetError(OpenAI_EmbeddingResponse_t *embeddingResponse)
{
    _OpenAI_EmbeddingResponse_t *_embeddingResponse = __containerof(embeddingResponse, _OpenAI_EmbeddingResponse_t, parent);
    return _embeddingResponse->error_str;
}

static OpenAI_EmbeddingResponse_t *OpenAI_EmbeddingResponseCreate(const char *payload)
{
    cJSON *u, *tokens, *d, *json;
    int dl = 0;

    OPENAI_ERROR_CHECK(payload != NULL, "payload is NULL", NULL);
    json = cJSON_Parse(payload);
    _OpenAI_EmbeddingResponse_t *_embeddingResponse = (_OpenAI_EmbeddingResponse_t *)calloc(1, sizeof(_OpenAI_EmbeddingResponse_t));
    OPENAI_ERROR_CHECK(NULL != _embeddingResponse, "calloc failed!", NULL);
    char *error =  getJsonError(json);
    if (error != NULL) {
        _embeddingResponse->error_str = error;
        ESP_LOGE(TAG, "Error: %s", error);
        goto end;
    }

    // Get total_tokens
    OPENAI_ERROR_CHECK_GOTO(cJSON_HasObjectItem(json, "usage"), "Usage was not found", end);
    u = cJSON_GetObjectItem(json, "u");
    if (u == NULL || !cJSON_IsObject(u) || !cJSON_HasObjectItem(u, "total_tokens")) {
        ESP_LOGE(TAG, "Total tokens were not found");
        goto end;
    }
    tokens = cJSON_GetObjectItem(u, "total_tokens");
    OPENAI_ERROR_CHECK_GOTO(NULL != tokens, "Total tokens were not found", end);
    _embeddingResponse->usage = cJSON_GetNumberValue(tokens);

    // Parse data
    OPENAI_ERROR_CHECK_GOTO(cJSON_HasObjectItem(json, "data"), "Data was not found", end);
    d = cJSON_GetObjectItem(json, "data");
    if (d == NULL || !cJSON_IsArray(d)) {
        ESP_LOGE(TAG, "Data is not array");
        goto end;
    }

    dl = cJSON_GetArraySize(d);
    OPENAI_ERROR_CHECK_GOTO(dl > 0, "Data is empty", end);
    _embeddingResponse->data = (OpenAI_EmbeddingData_t *)malloc(dl * sizeof(OpenAI_EmbeddingData_t));
    OPENAI_ERROR_CHECK_GOTO(_embeddingResponse->data != NULL, "Data could not be allocated", end);
    for (int di = 0; di < dl; di++) {
        cJSON *ditem = cJSON_GetArrayItem(d, di);
        if (ditem == NULL || !cJSON_IsObject(ditem) || !cJSON_HasObjectItem(ditem, "embedding")) {
            ESP_LOGE(TAG, "Embedding was not found");
            goto end;
        }
        cJSON *numberArray = cJSON_GetObjectItem(ditem, "embedding");
        if (numberArray == NULL || !cJSON_IsArray(numberArray)) {
            ESP_LOGE(TAG, "Embedding is not array");
            goto end;
        }
        int l = cJSON_GetArraySize(numberArray);
        OPENAI_ERROR_CHECK_GOTO(l > 0, "Embedding is empty", end);
        _embeddingResponse->data[di].data = (double *)malloc(l * sizeof(double));
        OPENAI_ERROR_CHECK_GOTO(_embeddingResponse->data[di].data != NULL, "Embedding could not be allocated", end);
        _embeddingResponse->len++;
        _embeddingResponse->data[di].len = l;
        for (int i = 0; i < l; i++) {
            cJSON *item = cJSON_GetArrayItem(numberArray, i);
            OPENAI_ERROR_CHECK_GOTO(item != NULL, "Embedding item could not be read", end)
            _embeddingResponse->data[di].data[i] = cJSON_GetNumberValue(item);
        }
    }

    _embeddingResponse->parent.getUsage = &OpenAI_EmbeddingResponseGetUsage;
    _embeddingResponse->parent.getLen = &OpenAI_EmbeddingResponseGetLen;
    _embeddingResponse->parent.getData = &OpenAI_EmbeddingResponseGetDate;
    _embeddingResponse->parent.getError = &OpenAI_EmbeddingResponseGetError;
    _embeddingResponse->parent.deleteResponse = &OpenAI_EmbeddingResponseDelete;
    cJSON_Delete(json);
    return &_embeddingResponse->parent;
end:
    if (json != NULL) {
        cJSON_Delete(json);
    }
    OpenAI_EmbeddingResponseDelete(&_embeddingResponse->parent);
    return NULL;
}

//
// OpenAI_ModerationResponse
//

typedef struct {
    OpenAI_ModerationResponse_t parent;
    uint32_t len;
    bool *data;
    char *error_str;
} _OpenAI_ModerationResponse_t;

static void OpenAI_ModerationResponseDelete(OpenAI_ModerationResponse_t *moderationResponse)
{
    _OpenAI_ModerationResponse_t *_moderationResponse = __containerof(moderationResponse, _OpenAI_ModerationResponse_t, parent);
    if (_moderationResponse != NULL) {
        if (_moderationResponse->data) {
            free(_moderationResponse->data);
            _moderationResponse->data = NULL;
        }

        if (_moderationResponse->error_str != NULL) {
            free(_moderationResponse->error_str);
            _moderationResponse->error_str = NULL;
        }

        free(_moderationResponse);
        _moderationResponse = NULL;
    }
}

static uint32_t OpenAI_ModerationResponseGetLen(struct OpenAI_ModerationResponse *moderationResponse)
{
    _OpenAI_ModerationResponse_t *_moderationResponse = __containerof(moderationResponse, _OpenAI_ModerationResponse_t, parent);
    return _moderationResponse->len;
}

static bool OpenAI_ModerationResponseGetDate(struct OpenAI_ModerationResponse *moderationResponse, uint32_t index)
{
    _OpenAI_ModerationResponse_t *_moderationResponse = __containerof(moderationResponse, _OpenAI_ModerationResponse_t, parent);
    if (index < _moderationResponse->len) {
        return _moderationResponse->data[index];
    }
    return false;
}

static char *OpenAI_ModerationResponseGetError(struct OpenAI_ModerationResponse *moderationResponse)
{
    _OpenAI_ModerationResponse_t *_moderationResponse = __containerof(moderationResponse, _OpenAI_ModerationResponse_t, parent);
    return _moderationResponse->error_str;
}

static OpenAI_ModerationResponse_t *OpenAI_ModerationResponseCreate(const char *payload)
{
    cJSON *d;
    int dl;
    _OpenAI_ModerationResponse_t *_moderationResponse = (_OpenAI_ModerationResponse_t *)calloc(1, sizeof(_OpenAI_ModerationResponse_t));
    OPENAI_ERROR_CHECK(NULL != _moderationResponse, "calloc failed!", NULL);
    if (payload == NULL) {
        return &_moderationResponse->parent;
    }

    // Parse payload
    cJSON *json = cJSON_Parse(payload);
    char *error = getJsonError(json);
    if (error != NULL) {
        _moderationResponse->error_str = error;
        ESP_LOGE(TAG, "Error: %s", error);
        goto end;
    }

    // Parse data
    OPENAI_ERROR_CHECK_GOTO(cJSON_HasObjectItem(json, "results"), "Data was not found", end);
    d = cJSON_GetObjectItem(json, "results");
    if (d == NULL || !cJSON_IsArray(d)) {
        ESP_LOGE(TAG, "Results is not array");
        goto end;
    }
    dl = cJSON_GetArraySize(d);
    OPENAI_ERROR_CHECK_GOTO(dl > 0, "Results is empty", end);
    _moderationResponse->data = (bool *)malloc(dl * sizeof(bool));
    OPENAI_ERROR_CHECK_GOTO(_moderationResponse->data != NULL, "Data could not be allocated", end);
    _moderationResponse->len = dl;
    for (int di = 0; di < dl; di++) {
        cJSON *ditem = cJSON_GetArrayItem(d, di);
        if (ditem == NULL || !cJSON_IsObject(ditem) || !cJSON_HasObjectItem(ditem, "flagged")) {
            ESP_LOGE(TAG, "Flagged was not found");
            goto end;
        }
        cJSON *flagged = cJSON_GetObjectItem(ditem, "flagged");
        if (flagged == NULL || !cJSON_IsBool(flagged)) {
            ESP_LOGE(TAG, "Flagged is not bool");
            goto end;
        }
    }
    cJSON_Delete(json);
    _moderationResponse->parent.getLen = &OpenAI_ModerationResponseGetLen;
    _moderationResponse->parent.getData = &OpenAI_ModerationResponseGetDate;
    _moderationResponse->parent.getError = &OpenAI_ModerationResponseGetError;
    _moderationResponse->parent.deleteResponse = &OpenAI_ModerationResponseDelete;
    return &_moderationResponse->parent;
end:
    cJSON_Delete(json);
    OpenAI_ModerationResponseDelete(&_moderationResponse->parent);
    return NULL;
}

//
// OpenAI_ImageResponse
//

typedef struct  {
    OpenAI_ImageResponse_t parent;
    uint32_t len;
    char **data;
    char *error_str;
} _OpenAI_ImageResponse_t;

static void OpenAI_ImageResponseDelete(OpenAI_ImageResponse_t *imageResponse)
{
    _OpenAI_ImageResponse_t *_imageResponse = __containerof(imageResponse, _OpenAI_ImageResponse_t, parent);
    if (_imageResponse != NULL) {
        if (_imageResponse->data) {
            for (int di = 0; di < _imageResponse->len; di++) {
                if (_imageResponse->data[di] != NULL) {
                    free(_imageResponse->data[di]);
                    _imageResponse->data[di] = NULL;
                }
            }
            free(_imageResponse->data);
            _imageResponse->data = NULL;
        }

        if (_imageResponse->error_str != NULL) {
            free(_imageResponse->error_str);
            _imageResponse->error_str = NULL;
        }

        free(_imageResponse);
        _imageResponse = NULL;
    }
}

static uint32_t OpenAI_ImageResponseGetLen(OpenAI_ImageResponse_t *imageResponse)
{
    _OpenAI_ImageResponse_t *_imageResponse = __containerof(imageResponse, _OpenAI_ImageResponse_t, parent);
    return _imageResponse->len;
}

static char *OpenAI_ImageResponseGetDate(OpenAI_ImageResponse_t *imageResponse, uint32_t index)
{
    _OpenAI_ImageResponse_t *_imageResponse = __containerof(imageResponse, _OpenAI_ImageResponse_t, parent);
    if (index < _imageResponse->len) {
        return _imageResponse->data[index];
    }
    return NULL;
}

static char *OpenAI_ImageResponseGetError(OpenAI_ImageResponse_t *imageResponse)
{
    _OpenAI_ImageResponse_t *_imageResponse = __containerof(imageResponse, _OpenAI_ImageResponse_t, parent);
    return _imageResponse->error_str;
}

OpenAI_ImageResponse_t *OpenAI_ImageResponseCreate(const char *payload)
{
    cJSON *d;
    int dl;
    _OpenAI_ImageResponse_t *_imageResponse = (_OpenAI_ImageResponse_t *)calloc(1, sizeof(_OpenAI_ImageResponse_t));
    OPENAI_ERROR_CHECK(NULL != _imageResponse, "calloc failed!", NULL);
    if (payload == NULL) {
        return &_imageResponse->parent;
    }
    // Parse payload
    cJSON *json = cJSON_Parse(payload);

    // Check for error
    char *error = getJsonError(json);
    if (error != NULL) {
        _imageResponse->error_str = error;
        ESP_LOGE(TAG, "Error: %s", error);
        goto end;
    }

    // Parse data
    OPENAI_ERROR_CHECK_GOTO(cJSON_HasObjectItem(json, "data"), "Data was not found", end);
    d = cJSON_GetObjectItem(json, "data");
    if (d == NULL || !cJSON_IsArray(d)) {
        ESP_LOGE(TAG, "Data is not array");
        goto end;
    }
    dl = cJSON_GetArraySize(d);
    OPENAI_ERROR_CHECK_GOTO(dl > 0, "Data is empty", end);
    _imageResponse->data = (char **)malloc(dl * sizeof(char *));
    OPENAI_ERROR_CHECK_GOTO(_imageResponse->data != NULL, "Data could not be allocated", end);
    for (int di = 0; di < dl; di++) {
        cJSON *item = cJSON_GetArrayItem(d, di);
        if (item == NULL || !cJSON_IsObject(item) || (!cJSON_HasObjectItem(item, "url") && !cJSON_HasObjectItem(item, "b64_json"))) {
            ESP_LOGE(TAG, "Image was not found");
            goto end;
        }
        if (cJSON_HasObjectItem(item, "url")) {
            cJSON *url = cJSON_GetObjectItem(item, "url");
            if (url == NULL || !cJSON_IsString(url)) {
                ESP_LOGE(TAG, "Image url could not be read");
                goto end;
            }
            _imageResponse->data[di] = strdup(cJSON_GetStringValue(url));
            if (_imageResponse->data[di] == NULL) {
                ESP_LOGE(TAG, "Image url could not be copied");
                goto end;
            }
            _imageResponse->len++;
        } else if (cJSON_HasObjectItem(item, "b64_json")) {
            cJSON *b64_json = cJSON_GetObjectItem(item, "b64_json");
            if (b64_json == NULL || !cJSON_IsString(b64_json)) {
                ESP_LOGE(TAG, "Image b64_json could not be read");
                goto end;
            }
            _imageResponse->data[di] = strdup(cJSON_GetStringValue(b64_json));
            if (_imageResponse->data[di] == NULL) {
                ESP_LOGE(TAG, "Image b64_json could not be copied");
                goto end;
            }
            _imageResponse->len++;
        }
    }
    cJSON_Delete(json);
    _imageResponse->parent.getLen = &OpenAI_ImageResponseGetLen;
    _imageResponse->parent.getData = &OpenAI_ImageResponseGetDate;
    _imageResponse->parent.getError = &OpenAI_ImageResponseGetError;
    _imageResponse->parent.deleteResponse = &OpenAI_ImageResponseDelete;
    return &_imageResponse->parent;
end:
    cJSON_Delete(json);
    OpenAI_ImageResponseDelete(&_imageResponse->parent);
    return NULL;
}

//
// OpenAI_StringResponse
//

typedef struct {
    OpenAI_StringResponse_t parent;
    uint32_t usage;
    uint32_t len;
    char **data;
    char *error_str;
} _OpenAI_StringResponse_t;

static void OpenAI_StringResponseDelete(OpenAI_StringResponse_t *stringResponse)
{
    _OpenAI_StringResponse_t *_stringResponse = __containerof(stringResponse, _OpenAI_StringResponse_t, parent);
    if (_stringResponse != NULL) {
        if (_stringResponse->data) {
            for (int di = 0; di < _stringResponse->len; di++) {
                if (_stringResponse->data[di] != NULL) {
                    free(_stringResponse->data[di]);
                    _stringResponse->data[di] = NULL;
                }
            }
            free(_stringResponse->data);
            _stringResponse->data = NULL;
        }

        if (_stringResponse->error_str != NULL) {
            free(_stringResponse->error_str);
            _stringResponse->error_str = NULL;
        }

        free(_stringResponse);
        _stringResponse = NULL;
    }
}

static uint32_t OpenAI_StringResponseGetUsage(OpenAI_StringResponse_t *stringResponse)
{
    _OpenAI_StringResponse_t *_stringResponse = __containerof(stringResponse, _OpenAI_StringResponse_t, parent);
    return _stringResponse->usage;
}

static uint32_t OpenAI_StringResponseGetLen(OpenAI_StringResponse_t *stringResponse)
{
    _OpenAI_StringResponse_t *_stringResponse = __containerof(stringResponse, _OpenAI_StringResponse_t, parent);
    return _stringResponse->len;
}

static char *OpenAI_StringResponseGetDate(OpenAI_StringResponse_t *stringResponse, uint32_t index)
{
    _OpenAI_StringResponse_t *_stringResponse = __containerof(stringResponse, _OpenAI_StringResponse_t, parent);
    if (index < _stringResponse->len) {
        return _stringResponse->data[index];
    }
    return NULL;
}

static char *OpenAI_StringResponseGetError(OpenAI_StringResponse_t *stringResponse)
{
    _OpenAI_StringResponse_t *_stringResponse = __containerof(stringResponse, _OpenAI_StringResponse_t, parent);
    return _stringResponse->error_str;
}

static OpenAI_StringResponse_t *OpenAI_StringResponseCreate(char *payload)
{
    cJSON *u, *tokens, *d;
    int dl;
    _OpenAI_StringResponse_t *_stringResponse = (_OpenAI_StringResponse_t *)calloc(1, sizeof(_OpenAI_StringResponse_t));
    OPENAI_ERROR_CHECK(NULL != _stringResponse, "calloc failed!", NULL);
    if (payload == NULL) {
        return &_stringResponse->parent;
    }

    // Parse payload
    cJSON *json = cJSON_Parse(payload);
    free(payload);

    // Check for error
    char *error = getJsonError(json);
    if (error != NULL) {
        _stringResponse->error_str = error;
        ESP_LOGE(TAG, "Error: %s", error);
        goto end;
    }

    // Get total_tokens
    OPENAI_ERROR_CHECK_GOTO(cJSON_HasObjectItem(json, "usage"), "Usage was not found", end);
    u = cJSON_GetObjectItem(json, "usage");
    if (u == NULL || !cJSON_IsObject(u) || !cJSON_HasObjectItem(u, "total_tokens")) {
        ESP_LOGE(TAG, "Total tokens were not found");
        goto end;
    }
    tokens = cJSON_GetObjectItem(u, "total_tokens");
    OPENAI_ERROR_CHECK_GOTO(tokens != NULL, "Total tokens could not be read", end);
    _stringResponse->usage = cJSON_GetNumberValue(tokens);

    // Parse data
    OPENAI_ERROR_CHECK_GOTO(cJSON_HasObjectItem(json, "choices"), "Choices was not found", end);
    d = cJSON_GetObjectItem(json, "choices");
    if (d == NULL || !cJSON_IsArray(d)) {
        ESP_LOGE(TAG, "Choices is not array");
        goto end;
    }
    dl = cJSON_GetArraySize(d);
    OPENAI_ERROR_CHECK_GOTO(dl > 0, "Choices is empty", end);
    _stringResponse->data = (char **)malloc(dl * sizeof(char *));
    OPENAI_ERROR_CHECK_GOTO(_stringResponse->data != NULL, "Data could not be allocated", end);

    for (int di = 0; di < dl; di++) {
        cJSON *item = cJSON_GetArrayItem(d, di);
        if (item == NULL || !cJSON_IsObject(item) || (!cJSON_HasObjectItem(item, "text") && !cJSON_HasObjectItem(item, "message"))) {
            ESP_LOGE(TAG, "Message was not found");
            goto end;
        }
        if (cJSON_HasObjectItem(item, "text")) {
            cJSON *text = cJSON_GetObjectItem(item, "text");
            if (text == NULL || !cJSON_IsString(text)) {
                ESP_LOGE(TAG, "Text could not be read");
                goto end;
            }
            _stringResponse->data[di] = strdup(cJSON_GetStringValue(text));
            OPENAI_ERROR_CHECK_GOTO(_stringResponse->data[di] != NULL, "Text could not be copied", end);
            _stringResponse->len++;
        } else if (cJSON_HasObjectItem(item, "message")) {
            cJSON *message = cJSON_GetObjectItem(item, "message");
            if (message == NULL || !cJSON_IsObject(message) || !cJSON_HasObjectItem(message, "content")) {
                ESP_LOGE(TAG, "Message is not object");
                goto end;
            }
            cJSON *mesg = cJSON_GetObjectItem(message, "content");
            if (mesg == NULL || !cJSON_IsString(mesg)) {
                ESP_LOGE(TAG, "Message could not be read");
                goto end;
            }
            _stringResponse->data[di] = strdup(cJSON_GetStringValue(mesg));
            OPENAI_ERROR_CHECK_GOTO(_stringResponse->data[di] != NULL, "Message could not be copied", end);
            _stringResponse->len = di + 1;
        }
    }

    cJSON_Delete(json);
    _stringResponse->parent.getUsage = &OpenAI_StringResponseGetUsage;
    _stringResponse->parent.getLen = &OpenAI_StringResponseGetLen;
    _stringResponse->parent.getData = &OpenAI_StringResponseGetDate;
    _stringResponse->parent.getError = &OpenAI_StringResponseGetError;
    _stringResponse->parent.deleteResponse = &OpenAI_StringResponseDelete;
    return &_stringResponse->parent;
end:
    cJSON_Delete(json);
    OpenAI_StringResponseDelete(&_stringResponse->parent);
    return NULL;
}

// completions { //Creates a completion for the provided prompt and parameters
//   "model": "text-davinci-003",//required
//   "prompt": "<|endoftext|>",//string, array of strings, array of tokens, or array of token arrays.
//   "max_tokens": 16,//integer. The maximum number of tokens to generate in the completion.
//   "temperature": 1,//float between 0 and 2
//   "top_p": 1,//float between 0 and 1. recommended to alter this or temperature but not both.
//   "n": 1,//integer. How many completions to generate for each prompt.
//   "stream": false,//boolean. Whether to stream back partial progress. keep false
//   "logprobs": null,//integer. Include the log probabilities on the logprobs most likely tokens, as well the chosen tokens.
//   "echo": false,//boolean. Echo back the prompt in addition to the completion
//   "stop": null,//string or array. Up to 4 sequences where the API will stop generating further tokens.
//   "presence_penalty": 0,//float between -2.0 and 2.0. Positive values penalize new tokens based on whether they appear in the text so far, increasing the model's likelihood to talk about new topics.
//   "frequency_penalty": 0,//float between -2.0 and 2.0. Positive values penalize new tokens based on their existing frequency in the text so far, decreasing the model's likelihood to repeat the same line verbatim.
//   "best_of": 1,//integer. Generates best_of completions server-side and returns the "best". best_of must be greater than n
//   "logit_bias": null,//map. Modify the likelihood of specified tokens appearing in the completion.
//   "user": null//string. A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
// }

/**
 * @brief Given a prompt, the model will return one or more predicted completions,
 * and can also return the probabilities of alternative tokens at each position.
 *
 */
typedef struct {
    OpenAI_Completion_t parent; /*!< Parent object */
    _OpenAI_t *oai;             /*!< Pointer to the OpenAI object */
    char *model;                /*!< ID of the model to use. */
    uint32_t max_tokens;        /*!< The maximum number of tokens to generate in the completion. */
    float temperature;          /*!< float between 0 and 2. Higher value gives more random results. */
    float top_p;                /*!< An alternative to sampling with temperature, called nucleus sampling, where the model considers the results of the tokens with top_p probability mass. So 0.1 means only the tokens comprising the top 10% probability mass are considered. */
    uint32_t n;                 /*!< How many completions to generate for each prompt. */
    bool echo;                  /*!< Echo back the prompt in addition to the completion */
    char *stop;                 /*!< Up to 4 sequences where the API will stop generating further tokens. The returned text will not contain the stop sequence. */
    float presence_penalty;     /*!< Number between -2.0 and 2.0. Positive values penalize new tokens based on whether they appear in the text so far, increasing the model's likelihood to talk about new topics. */
    float frequency_penalty;    /*!< Number between -2.0 and 2.0. Positive values penalize new tokens based on their existing frequency in the text so far, decreasing the model's likelihood to repeat the same line verbatim. */
    uint32_t best_of;           /*!< Generates best_of completions server-side and returns the "best" (the one with the highest log probability per token). Results cannot be streamed.
                                     When used with n, best_of controls the number of candidate completions and n specifies how many to return – best_of must be greater than n.*/
    char *user;                 /*!< A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse. */
} _OpenAI_Completion_t;

static void OpenAI_CompletionDelete(OpenAI_Completion_t *completion)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (_completion != NULL) {
        if (_completion->model != NULL) {
            free(_completion->model);
            _completion->model = NULL;
        }

        if (_completion->stop != NULL) {
            free(_completion->stop);
            _completion->stop = NULL;
        }

        if (_completion->user != NULL) {
            free(_completion->user);
            _completion->user = NULL;
        }

        free(_completion);
        _completion = NULL;
    }
}

static void OpenAI_CompletionSetModel(OpenAI_Completion_t *completion, const char *m)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (_completion->model != NULL) {
        free(_completion->model);
        _completion->model = NULL;
    }
    _completion->model = strdup(m);
}

static void OpenAI_CompletionSetMaxTokens(OpenAI_Completion_t *completion, uint32_t mt)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (mt > 0) {
        _completion->max_tokens = mt;
    }
}

static void OpenAI_CompletionSetTemperature(OpenAI_Completion_t *completion, float t)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (t >= 0 && t <= 2.0) {
        _completion->temperature = t;
    }
}

static void OpenAI_CompletionSetTopP(OpenAI_Completion_t *completion, float tp)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (tp >= 0 && tp <= 1.0) {
        _completion->top_p = tp;
    }
}

static void OpenAI_CompletionSetN(OpenAI_Completion_t *completion, uint32_t n)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (n > 0) {
        _completion->n = n;
    }
}

static void OpenAI_CompletionSetEcho(OpenAI_Completion_t *completion, bool e)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    _completion->echo = e;
}

static void OpenAI_CompletionSetStop(OpenAI_Completion_t *completion, const char *s)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (_completion->stop != NULL) {
        free(_completion->stop);
        _completion->stop = NULL;
    }
    _completion->stop = strdup(s);
}

static void OpenAI_CompletionSetPresencePenalty(OpenAI_Completion_t *completion, float pp)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (pp >= -2.0 && pp <= 2.0) {
        _completion->presence_penalty = pp;
    }
}

static void OpenAI_CompletionSetFrequencyPenalty(OpenAI_Completion_t *completion, float fp)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (fp >= -2.0 && fp <= 2.0) {
        _completion->frequency_penalty = fp;
    }
}

static void OpenAI_CompletionSetBestOf(OpenAI_Completion_t *completion, uint32_t bo)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (bo > 0) {
        _completion->best_of = bo;
    }
}

static void OpenAI_CompletionSetUser(OpenAI_Completion_t *completion, const char *u)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    if (_completion->user != NULL) {
        free(_completion->user);
        _completion->user = NULL;
    }
    _completion->user = strdup(u);
}

static OpenAI_StringResponse_t *OpenAI_CompletionPrompt(OpenAI_Completion_t *completion, char *p)
{
    _OpenAI_Completion_t *_completion = __containerof(completion, _OpenAI_Completion_t, parent);
    const char *endpoint = "completions";
    OpenAI_StringResponse_t *result = NULL;
    cJSON *req = cJSON_CreateObject();
    OPENAI_ERROR_CHECK(req != NULL, "cJSON_CreateObject failed!", NULL);
    reqAddString("model", (_completion->model == NULL) ? "text-davinci-003" : _completion->model);
    if (strncmp(p, "[", 1) == 0) {
        cJSON *in = cJSON_Parse(p);
        if (in == NULL || !cJSON_IsArray(in)) {
            ESP_LOGE(TAG, "Input not JSON Array!");
            cJSON_Delete(req);
            return NULL;
        }
        reqAddItem("prompt", in);
    } else {
        reqAddString("prompt", p);
    }
    if (_completion->max_tokens) {
        reqAddNumber("max_tokens", _completion->max_tokens);
    }
    if (_completion->temperature != 1) {
        reqAddNumber("temperature", _completion->temperature);
    }
    if (_completion->top_p != 1) {
        reqAddNumber("top_p", _completion->top_p);
    }
    if (_completion->n != 1) {
        reqAddNumber("n", _completion->n);
    }
    if (_completion->echo) {
        reqAddBool("echo", true);
    }
    if (_completion->stop != NULL) {
        reqAddString("stop", _completion->stop);
    }
    if (_completion->presence_penalty != 0) {
        reqAddNumber("presence_penalty", _completion->presence_penalty);
    }
    if (_completion->frequency_penalty != 0) {
        reqAddNumber("frequency_penalty", _completion->frequency_penalty);
    }
    if (_completion->best_of != 1) {
        reqAddNumber("best_of", _completion->best_of);
    }
    if (_completion->user != NULL) {
        reqAddString("user", _completion->user);
    }
    char *jsonBody = cJSON_Print(req);
    cJSON_Delete(req);
    char *res = _completion->oai->post(_completion->oai->base_url, _completion->oai->api_key, endpoint, jsonBody);
    free(jsonBody);
    OPENAI_ERROR_CHECK(res != NULL, "OpenAI API call failed", NULL);

    return OpenAI_StringResponseCreate(res);
}

static OpenAI_Completion_t *OpenAI_CompletionCreate(OpenAI_t *openai)
{
    _OpenAI_Completion_t *_completion = (_OpenAI_Completion_t *)calloc(1, sizeof(_OpenAI_Completion_t));
    OPENAI_ERROR_CHECK(_completion != NULL, "Completion could not be allocated", NULL);
    _completion->oai = __containerof(openai, _OpenAI_t, parent);
    _completion->max_tokens = 1;
    _completion->top_p = 1;
    _completion->n = 1;
    _completion->best_of = 1;

    _completion->parent.setModel = &OpenAI_CompletionSetModel;
    _completion->parent.setMaxTokens = &OpenAI_CompletionSetMaxTokens;
    _completion->parent.setTemperature = &OpenAI_CompletionSetTemperature;
    _completion->parent.setTopP = &OpenAI_CompletionSetTopP;
    _completion->parent.setN = &OpenAI_CompletionSetN;
    _completion->parent.setEcho = &OpenAI_CompletionSetEcho;
    _completion->parent.setStop = &OpenAI_CompletionSetStop;
    _completion->parent.setPresencePenalty = &OpenAI_CompletionSetPresencePenalty;
    _completion->parent.setFrequencyPenalty = &OpenAI_CompletionSetFrequencyPenalty;
    _completion->parent.setBestOf = &OpenAI_CompletionSetBestOf;
    _completion->parent.setUser = &OpenAI_CompletionSetUser;
    _completion->parent.prompt = &OpenAI_CompletionPrompt;
    return &_completion->parent;
}

// chat/completions { //Given a chat conversation, the model will return a chat completion response.
//   "model": "gpt-3.5-turbo",//required
//   "messages": [//required array
//     {"role": "system", "content": "Description of the required assistant"},
//     {"role": "user", "content": "First question from the user"},
//     {"role": "assistant", "content": "Response from the assistant"},
//     {"role": "user", "content": "Next question from the user to be answered"}
//   ],
//   "temperature": 1,//float between 0 and 2
//   "top_p": 1,//float between 0 and 1. recommended to alter this or temperature but not both.
//   "stream": false,//boolean. Whether to stream back partial progress. keep false
//   "stop": null,//string or array. Up to 4 sequences where the API will stop generating further tokens.
//   "max_tokens": 16,//integer. The maximum number of tokens to generate in the completion.
//   "presence_penalty": 0,//float between -2.0 and 2.0. Positive values penalize new tokens based on whether they appear in the text so far, increasing the model's likelihood to talk about new topics.
//   "frequency_penalty": 0,//float between -2.0 and 2.0. Positive values penalize new tokens based on their existing frequency in the text so far, decreasing the model's likelihood to repeat the same line verbatim.
//   "logit_bias": null,//map. Modify the likelihood of specified tokens appearing in the completion.
//   "user": null//string. A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
// }

typedef struct {
    OpenAI_ChatCompletion_t parent; /*!< Parent object */
    _OpenAI_t *oai;                 /*!< Pointer to the OpenAI object */
    cJSON *messages;                /*!< Array of messages */
    char *model;                    /*!< ID of the model to use. */
    char *description;              /*!< The description of what the function does.. */
    uint32_t max_tokens;            /*!< The maximum number of tokens to generate in the chat completion. */
    float temperature;              /*!< float between 0 and 2. Higher value gives more random results. */
    float top_p;                    /*!< An alternative to sampling with temperature, called nucleus sampling,
                                         where the model considers the results of the tokens with top_p probability mass.
                                         So 0.1 means only the tokens comprising the top 10% probability mass are considered. */
    char *stop;                     /*!< Up to 4 sequences where the API will stop generating further tokens. */
    float presence_penalty;         /*!< Number between -2.0 and 2.0. Positive values penalize new tokens based on whether they
                                         appear in the text so far, increasing the model's likelihood to talk about new topics. */
    float frequency_penalty;        /*!< Number between -2.0 and 2.0. Positive values penalize new tokens based on their existing
                                         frequency in the text so far, decreasing the model's likelihood to repeat the same line verbatim. */
    char *user;                     /*!< A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse. */
} _OpenAI_ChatCompletion_t;

static void OpenAI_ChatCompletionDelete(OpenAI_ChatCompletion_t *chatCompletion)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (_chatCompletion != NULL) {
        if (_chatCompletion->model != NULL) {
            free(_chatCompletion->model);
            _chatCompletion->model = NULL;
        }
        if (_chatCompletion->description != NULL) {
            free(_chatCompletion->description);
            _chatCompletion->description = NULL;
        }
        if (_chatCompletion->stop != NULL) {
            free(_chatCompletion->stop);
            _chatCompletion->stop = NULL;
        }
        if (_chatCompletion->user != NULL) {
            free(_chatCompletion->user);
            _chatCompletion->user = NULL;
        }
        if (_chatCompletion->messages != NULL) {
            cJSON_Delete(_chatCompletion->messages);
            _chatCompletion->messages = NULL;
        }
        free(_chatCompletion);
        _chatCompletion = NULL;
    }
}

static void OpenAI_ChatCompletionSetModel(OpenAI_ChatCompletion_t *chatCompletion, const char *m)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (_chatCompletion->model != NULL) {
        free(_chatCompletion->model);
    }
    _chatCompletion->model = strdup(m);
}

static void OpenAI_ChatCompletionSetSystem(OpenAI_ChatCompletion_t *chatCompletion, const char *s)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (_chatCompletion->description != NULL) {
        free(_chatCompletion->description);
    }
    _chatCompletion->description = strdup(s);
}

static void OpenAI_ChatCompletionSetMaxTokens(OpenAI_ChatCompletion_t *chatCompletion, uint32_t mt)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (mt > 0) {
        _chatCompletion->max_tokens = mt;
    }
}

static void OpenAI_ChatCompletionSetTemperature(OpenAI_ChatCompletion_t *chatCompletion, float t)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (t >= 0 && t <= 2.0) {
        _chatCompletion->temperature = t;
    }
}

static void OpenAI_ChatCompletionSetTopP(OpenAI_ChatCompletion_t *chatCompletion, float tp)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (tp >= 0 && tp <= 1.0) {
        _chatCompletion->top_p = tp;
    }
}

static void OpenAI_ChatCompletionSetStop(OpenAI_ChatCompletion_t *chatCompletion, const char *s)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (_chatCompletion->stop != NULL) {
        free(_chatCompletion->stop);
    }
    _chatCompletion->stop = strdup(s);
}

static void OpenAI_ChatCompletionSetPresencePenalty(OpenAI_ChatCompletion_t *chatCompletion, float pp)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (pp >= -2.0 && pp <= 2.0) {
        _chatCompletion->presence_penalty = pp;
    }
}

static void OpenAI_ChatCompletionSetFrequencyPenalty(OpenAI_ChatCompletion_t *chatCompletion, float fp)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (fp >= -2.0 && fp <= 2.0) {
        _chatCompletion->frequency_penalty = fp;
    }
}

static void OpenAI_ChatCompletionSetUser(OpenAI_ChatCompletion_t *chatCompletion, const char *u)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (_chatCompletion->user != NULL) {
        free(_chatCompletion->user);
    }
    _chatCompletion->user = strdup(u);
}

static void OpenAI_ChatCompletionClearConversation(OpenAI_ChatCompletion_t *chatCompletion)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    if (_chatCompletion->messages != NULL) {
        cJSON_Delete(_chatCompletion->messages);
        _chatCompletion->messages = cJSON_CreateArray();
    }
}

static cJSON *createChatMessage(cJSON *messages, const char *role, const char *content)
{
    cJSON *message = cJSON_CreateObject();
    OPENAI_ERROR_CHECK(message != NULL, "cJSON_CreateObject failed!", NULL);
    if (cJSON_AddStringToObject(message, "role", role) == NULL) {
        cJSON_Delete(message);
        ESP_LOGE(TAG, "cJSON_AddStringToObject failed!");
        return NULL;
    }
    if (cJSON_AddStringToObject(message, "content", content) == NULL) {
        cJSON_Delete(message);
        ESP_LOGE(TAG, "cJSON_AddStringToObject failed!");
        return NULL;
    }
    if (!cJSON_AddItemToArray(messages, message)) {
        cJSON_Delete(message);
        ESP_LOGE(TAG, "cJSON_AddItemToArray failed!");
        return NULL;
    }
    return message;
}

OpenAI_StringResponse_t *OpenAI_ChatCompletionMessage(OpenAI_ChatCompletion_t *chatCompletion, const char *p, bool save)
{
    const char *endpoint = "chat/completions";
    OpenAI_StringResponse_t *result = NULL;

    cJSON *req = cJSON_CreateObject();
    OPENAI_ERROR_CHECK(req != NULL, "cJSON_CreateObject failed!", result);
    _OpenAI_ChatCompletion_t *_chatCompletion = __containerof(chatCompletion, _OpenAI_ChatCompletion_t, parent);
    reqAddString("model", (_chatCompletion->model == NULL) ? "gpt-3.5-turbo" : _chatCompletion->model);

    cJSON *_messages = cJSON_CreateArray();

    if (_messages == NULL) {
        cJSON_Delete(req);
        ESP_LOGE(TAG, "cJSON_CreateArray failed!");
        return result;
    }
    if (_chatCompletion->description != NULL) {
        if (createChatMessage(_messages, "system", _chatCompletion->description) == NULL) {
            cJSON_Delete(req);
            cJSON_Delete(_messages);
            ESP_LOGE(TAG, "createChatMessage failed!");
            return result;
        }
    }
    if (_chatCompletion->messages != NULL && cJSON_IsArray(_chatCompletion->messages)) {
        int mlen = cJSON_GetArraySize(_chatCompletion->messages);
        for (int i = 0; i < mlen; ++i) {
            cJSON *item = cJSON_GetArrayItem(_chatCompletion->messages, i);
            if (item != NULL && cJSON_IsObject(item)) {
                if (!cJSON_AddItemReferenceToArray(_messages, item)) {
                    cJSON_Delete(req);
                    cJSON_Delete(_messages);
                    ESP_LOGE(TAG, "cJSON_AddItemReferenceToArray failed!");
                    return result;
                }
            }
        }
    }
    if (createChatMessage(_messages, "user", p) == NULL) {
        cJSON_Delete(req);
        cJSON_Delete(_messages);
        ESP_LOGE(TAG, "createChatMessage failed!");
        return result;
    }

    reqAddItem("messages", _messages);
    if (_chatCompletion->max_tokens) {
        reqAddNumber("max_tokens", _chatCompletion->max_tokens);
    }
    if (_chatCompletion->temperature != 1) {
        reqAddNumber("temperature", _chatCompletion->temperature);
    }
    if (_chatCompletion->top_p != 1) {
        reqAddNumber("top_p", _chatCompletion->top_p);
    }
    if (_chatCompletion->stop != NULL) {
        reqAddString("stop", _chatCompletion->stop);
    }
    if (_chatCompletion->presence_penalty != 0) {
        reqAddNumber("presence_penalty", _chatCompletion->presence_penalty);
    }
    if (_chatCompletion->frequency_penalty != 0) {
        reqAddNumber("frequency_penalty", _chatCompletion->frequency_penalty);
    }
    if (_chatCompletion->user != NULL) {
        reqAddString("user", _chatCompletion->user);
    }
    char *jsonBody = cJSON_Print(req);
    cJSON_Delete(req);
    char *res = _chatCompletion->oai->post(_chatCompletion->oai->base_url, _chatCompletion->oai->api_key, endpoint, jsonBody);
    free(jsonBody);
    OPENAI_ERROR_CHECK(res != NULL, "Empty result!", result);
    if (save) {
        //add the responses to the messages here
        //double parsing is here as workaround
        OpenAI_StringResponse_t *r = OpenAI_StringResponseCreate(res);
        if (r->getLen(r)) {
            if (createChatMessage(_chatCompletion->messages, "user", p) == NULL) {
                ESP_LOGE(TAG, "createChatMessage failed!");
            }
            if (createChatMessage(_chatCompletion->messages, "assistant", r->getData(r, 0)) == NULL) {
                ESP_LOGE(TAG, "createChatMessage failed!");
            }
        }
        return r;
    }
    return OpenAI_StringResponseCreate(res);
}

static OpenAI_ChatCompletion_t *OpenAI_ChatCompletionCreate(OpenAI_t *openai)
{
    _OpenAI_ChatCompletion_t *_chatCompletion = (_OpenAI_ChatCompletion_t *)calloc(1, sizeof(_OpenAI_ChatCompletion_t));
    OPENAI_ERROR_CHECK(_chatCompletion != NULL, "chatCompletion could not be allocated", NULL);
    _chatCompletion->oai = __containerof(openai, _OpenAI_t, parent);
    _chatCompletion->temperature = 1;
    _chatCompletion->top_p = 1;
    _chatCompletion->messages = cJSON_CreateArray();

    _chatCompletion->parent.setModel = &OpenAI_ChatCompletionSetModel;
    _chatCompletion->parent.setSystem = &OpenAI_ChatCompletionSetSystem;
    _chatCompletion->parent.setMaxTokens = &OpenAI_ChatCompletionSetMaxTokens;
    _chatCompletion->parent.setTemperature = &OpenAI_ChatCompletionSetTemperature;
    _chatCompletion->parent.setTopP = &OpenAI_ChatCompletionSetTopP;
    _chatCompletion->parent.setStop = &OpenAI_ChatCompletionSetStop;
    _chatCompletion->parent.setPresencePenalty = &OpenAI_ChatCompletionSetPresencePenalty;
    _chatCompletion->parent.setFrequencyPenalty = &OpenAI_ChatCompletionSetFrequencyPenalty;
    _chatCompletion->parent.setUser = &OpenAI_ChatCompletionSetUser;
    _chatCompletion->parent.clearConversation = &OpenAI_ChatCompletionClearConversation;
    _chatCompletion->parent.message = &OpenAI_ChatCompletionMessage;

    return &_chatCompletion->parent;
}

// edits { //Creates a new edit for the provided input, instruction, and parameters.
//   "model": "text-davinci-edit-001",//required
//   "input": "",//string. The input text to use as a starting point for the edit.
//   "instruction": "Fix the spelling mistakes",//required string. The instruction that tells the model how to edit the prompt.
//   "n": 1,//integer. How many edits to generate for the input and instruction.
//   "temperature": 1,//float between 0 and 2
//   "top_p": 1//float between 0 and 1. recommended to alter this or temperature but not both.
// }

/**
 * @brief Given a prompt and an instruction, the model will return an edited version of the prompt.
 *
 */
typedef struct {
    OpenAI_Edit_t parent;    /*!< Pointer to the parent object */
    _OpenAI_t *oai;          /*!< Pointer to the OpenAI object */
    char *model;             /*!< default "text-davinci-edit-001". ID of the model to use. */
    float temperature;       /*!< float between 0 and 2. Higher value gives more random results. */
    float top_p;             /*!< float between 0 and 1. recommended to alter this or temperature but not both.*/
    uint32_t n;              /*!< How many edits to generate for the input and instruction. */
} _OpenAI_Edit_t;

static void OpenAI_EditDelete(OpenAI_Edit_t *edit)
{
    _OpenAI_Edit_t *_edit = __containerof(edit, _OpenAI_Edit_t, parent);
    if (_edit != NULL) {
        if (_edit->model != NULL) {
            free(_edit->model);
            _edit->model = NULL;
        }

        free(_edit);
        _edit = NULL;
    }
}

static void OpenAI_EditSetModel(OpenAI_Edit_t *edit, const char *m)
{
    _OpenAI_Edit_t *_edit = __containerof(edit, _OpenAI_Edit_t, parent);
    if (_edit->model != NULL) {
        free(_edit->model);
        _edit->model = NULL;
    }

    _edit->model = strdup(m);
}

static void OpenAI_EditSetTemperature(OpenAI_Edit_t *edit, float t)
{
    _OpenAI_Edit_t *_edit = __containerof(edit, _OpenAI_Edit_t, parent);
    if (t >= 0 && t <= 2.0) {
        _edit->temperature = t;
    }
}

static void OpenAI_EditSetTopP(OpenAI_Edit_t *edit, float t)
{
    _OpenAI_Edit_t *_edit = __containerof(edit, _OpenAI_Edit_t, parent);
    if (t >= 0 && t <= 1.0) {
        _edit->top_p = t;
    }
}

static void OpenAI_EditSetN(OpenAI_Edit_t *edit, uint32_t n)
{
    _OpenAI_Edit_t *_edit = __containerof(edit, _OpenAI_Edit_t, parent);
    if (n > 0) {
        _edit->n = n;
    }
}

OpenAI_StringResponse_t *OpenAI_EditProcess(OpenAI_Edit_t *edit, char *instruction, char *input)
{
    const char *endpoint = "edits";
    OpenAI_StringResponse_t *result = NULL;
    cJSON *req = cJSON_CreateObject();
    OPENAI_ERROR_CHECK(req != NULL, "cJSON_CreateObject failed!", NULL);
    _OpenAI_Edit_t *_edit = __containerof(edit, _OpenAI_Edit_t, parent);
    reqAddString("model", (_edit->model == NULL) ? "text-davinci-edit-001" : _edit->model);
    reqAddString("instruction", instruction);
    if (input != NULL) {
        reqAddString("input", input);
    }
    if (_edit->temperature != 1) {
        reqAddNumber("temperature", _edit->temperature);
    }
    if (_edit->top_p != 1) {
        reqAddNumber("top_p", _edit->top_p);
    }
    if (_edit->n != 1) {
        reqAddNumber("n", _edit->n);
    }
    char *jsonBody = cJSON_Print(req);
    cJSON_Delete(req);
    char *res = _edit->oai->post(_edit->oai->base_url, _edit->oai->api_key, endpoint, jsonBody);
    free(jsonBody);
    OPENAI_ERROR_CHECK(res != NULL, "Empty result!", result);
    return OpenAI_StringResponseCreate(res);
}

static OpenAI_Edit_t *OpenAI_EditCreate(OpenAI_t *openai)
{
    _OpenAI_Edit_t *_edit = (_OpenAI_Edit_t *)calloc(1, sizeof(_OpenAI_Edit_t));
    OPENAI_ERROR_CHECK(_edit != NULL, "edit calloc failed!", NULL);
    _edit->oai = __containerof(openai, _OpenAI_t, parent);;
    _edit->temperature = 1;
    _edit->top_p = 1;
    _edit->n = 1;

    _edit->parent.setModel = &OpenAI_EditSetModel;
    _edit->parent.setTemperature = &OpenAI_EditSetTemperature;
    _edit->parent.setTopP = &OpenAI_EditSetTopP;
    _edit->parent.setN = &OpenAI_EditSetN;
    _edit->parent.process = &OpenAI_EditProcess;
    return &_edit->parent;
}

//
// Images
//

static const char *image_sizes[] = {"1024x1024", "512x512", "256x256"};
static const char *image_response_formats[] = {"url", "b64_json"};

// images/generations { //Creates an image given a prompt.
//   "prompt": "A cute baby sea otter",//required
//   "n": 1,//integer. The number of images to generate. Must be between 1 and 10.
//   "size": "1024x1024",//string. The size of the generated images. Must be one of "256x256", "512x512", or "1024x1024"
//   "response_format": "url",//string. The format in which the generated images are returned. Must be one of "url" or "b64_json".
//   "user": null//string. A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
// }

/**
 * @brief Creates an image given a prompt.
 *
 */
typedef struct {
    OpenAI_ImageGeneration_t parent;               /*!< The parent object */
    _OpenAI_t *oai;                                /*!< Pointer to the OpenAI object */
    OpenAI_Image_Size size;                        /*!< The size of the generated images. Must be one of "256x256", "512x512", or "1024x1024" */
    OpenAI_Image_Response_Format response_format;  /*!< The format in which the generated images are returned. Must be one of url or b64_json */
    uint32_t n;                                    /*!< The number of images to generate. Must be between 1 and 10. */
    char *user;                                    /*!< A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse. */
} _OpenAI_ImageGeneration_t;

static void OpenAI_ImageGenerationDelete(OpenAI_ImageGeneration_t *imageGeneration)
{
    _OpenAI_ImageGeneration_t *_imageGeneration = __containerof(imageGeneration, _OpenAI_ImageGeneration_t, parent);
    if (_imageGeneration != NULL) {
        if (_imageGeneration->user != NULL) {
            free(_imageGeneration->user);
            _imageGeneration->user = NULL;
        }

        free(_imageGeneration);
        _imageGeneration = NULL;
    }
}

static void OpenAI_ImageGenerationSetSize(OpenAI_ImageGeneration_t *imageGeneration, OpenAI_Image_Size s)
{
    _OpenAI_ImageGeneration_t *_imageGeneration = __containerof(imageGeneration, _OpenAI_ImageGeneration_t, parent);
    if (s >= OPENAI_IMAGE_SIZE_1024x1024 && s <= OPENAI_IMAGE_SIZE_256x256) {
        _imageGeneration->size = s;
    }
}

static void OpenAI_ImageGenerationSetResponseFormat(OpenAI_ImageGeneration_t *imageGeneration, OpenAI_Image_Response_Format rf)
{
    _OpenAI_ImageGeneration_t *_imageGeneration = __containerof(imageGeneration, _OpenAI_ImageGeneration_t, parent);
    if (rf >= OPENAI_IMAGE_RESPONSE_FORMAT_URL && rf <= OPENAI_IMAGE_RESPONSE_FORMAT_B64_JSON) {
        _imageGeneration->response_format = rf;
    }
}

static void OpenAI_ImageGenerationSetN(OpenAI_ImageGeneration_t *imageGeneration, uint32_t n)
{
    _OpenAI_ImageGeneration_t *_imageGeneration = __containerof(imageGeneration, _OpenAI_ImageGeneration_t, parent);
    if (n > 0 && n <= 10) {
        _imageGeneration->n = n;
    }
}

static void OpenAI_ImageGenerationSetUser(OpenAI_ImageGeneration_t *imageGeneration, const char *u)
{
    _OpenAI_ImageGeneration_t *_imageGeneration = __containerof(imageGeneration, _OpenAI_ImageGeneration_t, parent);
    if (_imageGeneration->user != NULL) {
        free(_imageGeneration->user);
        _imageGeneration->user = NULL;
    }

    _imageGeneration->user = strdup(u);
}

OpenAI_ImageResponse_t *OpenAI_ImageGenerationPrompt(OpenAI_ImageGeneration_t *imageGeneration, char *p)
{
    const char *endpoint = "images/generations";
    OpenAI_ImageResponse_t *result = NULL;
    cJSON *req = cJSON_CreateObject();
    OPENAI_ERROR_CHECK(req != NULL, "cJSON_CreateObject failed!", NULL);
    reqAddString("prompt", p);
    _OpenAI_ImageGeneration_t *_imageGeneration = __containerof(imageGeneration, _OpenAI_ImageGeneration_t, parent);
    if (_imageGeneration->size != OPENAI_IMAGE_SIZE_1024x1024) {
        reqAddString("size", image_sizes[_imageGeneration->size]);
    }
    if (_imageGeneration->response_format != OPENAI_IMAGE_RESPONSE_FORMAT_URL) {
        reqAddString("response_format", image_response_formats[_imageGeneration->response_format]);
    }
    if (_imageGeneration->n != 1) {
        reqAddNumber("n", _imageGeneration->n);
    }
    if (_imageGeneration->user != NULL) {
        reqAddString("user", _imageGeneration->user);
    }
    char *jsonBody = cJSON_Print(req);
    cJSON_Delete(req);
    char *res = _imageGeneration->oai->post(_imageGeneration->oai->base_url, _imageGeneration->oai->api_key, endpoint, jsonBody);
    free(jsonBody);
    OPENAI_ERROR_CHECK(res != NULL, "Empty result!", result);
    return OpenAI_ImageResponseCreate(res);
}

static OpenAI_ImageGeneration_t *OpenAI_ImageGenerationCreate(OpenAI_t *openai)
{
    _OpenAI_ImageGeneration_t *_imageGeneration = (_OpenAI_ImageGeneration_t *)calloc(1, sizeof(_OpenAI_ImageGeneration_t));
    OPENAI_ERROR_CHECK(_imageGeneration != NULL, "_imageGeneration calloc failed!", NULL);
    _imageGeneration->oai = __containerof(openai, _OpenAI_t, parent);;
    _imageGeneration->size = OPENAI_IMAGE_SIZE_1024x1024;
    _imageGeneration->response_format = OPENAI_IMAGE_RESPONSE_FORMAT_URL;
    _imageGeneration->n = 1;

    _imageGeneration->parent.setSize = &OpenAI_ImageGenerationSetSize;
    _imageGeneration->parent.setResponseFormat = &OpenAI_ImageGenerationSetResponseFormat;
    _imageGeneration->parent.setN = &OpenAI_ImageGenerationSetN;
    _imageGeneration->parent.setUser = &OpenAI_ImageGenerationSetUser;
    _imageGeneration->parent.prompt = &OpenAI_ImageGenerationPrompt;

    return &_imageGeneration->parent;
}

// images/variations { //Creates a variation of a given image.
//   "image": "",//required string. The image to edit. Must be a valid PNG file, less than 4MB, and square.
//   "n": 1,//integer. The number of images to generate. Must be between 1 and 10.
//   "size": "1024x1024",//string. The size of the generated images. Must be one of "256x256", "512x512", or "1024x1024"
//   "response_format": "url",//string. The format in which the generated images are returned. Must be one of "url" or "b64_json".
//   "user": null//string. A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
// }

/**
 * @brief Creates a variation of a given image.
 *
 */
typedef struct {
    OpenAI_ImageVariation_t parent;               /*!< Parent object */
    _OpenAI_t *oai;                               /*!< Pointer to the OpenAI object */
    OpenAI_Image_Size size;                       /*!< The size of the generated images. Must be one of 256x256, 512x512, or 1024x1024. */
    OpenAI_Image_Response_Format response_format; /*!< The format in which the generated images are returned. Must be one of url or b64_json. */
    uint32_t n;                                   /*!< The number of images to generate. Must be between 1 and 10. */
    char *user;                                   /*!< A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse. */
} _OpenAI_ImageVariation_t;

static void OpenAI_ImageVariationDelete(OpenAI_ImageVariation_t *imageVariation)
{
    _OpenAI_ImageVariation_t *_imageVariation = __containerof(imageVariation, _OpenAI_ImageVariation_t, parent);
    if (_imageVariation != NULL) {
        if (_imageVariation->user != NULL) {
            free(_imageVariation->user);
            _imageVariation->user = NULL;
        }

        free(_imageVariation);
        _imageVariation = NULL;
    }
}

static void OpenAI_ImageVariationSetSize(OpenAI_ImageVariation_t *imageVariation, OpenAI_Image_Size s)
{
    _OpenAI_ImageVariation_t *_imageVariation = __containerof(imageVariation, _OpenAI_ImageVariation_t, parent);
    if (s >= OPENAI_IMAGE_SIZE_1024x1024 && s <= OPENAI_IMAGE_SIZE_256x256) {
        _imageVariation->size = s;
    }
}

static void OpenAI_ImageVariationSetResponseFormat(OpenAI_ImageVariation_t *imageVariation, OpenAI_Image_Response_Format rf)
{
    _OpenAI_ImageVariation_t *_imageVariation = __containerof(imageVariation, _OpenAI_ImageVariation_t, parent);
    if (rf >= OPENAI_IMAGE_RESPONSE_FORMAT_URL && rf <= OPENAI_IMAGE_RESPONSE_FORMAT_B64_JSON) {
        _imageVariation->response_format = rf;
    }
}

static void OpenAI_ImageVariationSetN(OpenAI_ImageVariation_t *imageVariation, uint32_t n)
{
    _OpenAI_ImageVariation_t *_imageVariation = __containerof(imageVariation, _OpenAI_ImageVariation_t, parent);
    if (n > 0 && n <= 10) {
        _imageVariation->n = n;
    }
}

static void OpenAI_ImageVariationSetUser(OpenAI_ImageVariation_t *imageVariation, const char *u)
{
    _OpenAI_ImageVariation_t *_imageVariation = __containerof(imageVariation, _OpenAI_ImageVariation_t, parent);
    if (_imageVariation->user != NULL) {
        free(_imageVariation->user);
        _imageVariation->user = NULL;
    }

    _imageVariation->user = strdup(u);
}

static OpenAI_ImageResponse_t *OpenAI_ImageVariationImage(OpenAI_ImageVariation_t *imageVariation, uint8_t *img_data, size_t img_len)
{
    const char *endpoint = "images/variations";
    const char *boundary = "----WebKitFormBoundaryb9v538xFWfzLzRO3";
    char *itemPrefix = NULL;
    asprintf(&itemPrefix, "--%s\r\nContent-Disposition: form-data; name=", boundary);
    OPENAI_ERROR_CHECK(itemPrefix != NULL, "asprintf failed!", NULL);
    uint8_t *data = NULL;
    size_t len = 0;
    char *reqBody = NULL;
    _OpenAI_ImageVariation_t *_imageVariation = __containerof(imageVariation, _OpenAI_ImageVariation_t, parent);
    if (_imageVariation->size != OPENAI_IMAGE_SIZE_1024x1024) {
        asprintf(&reqBody, "%s\"size\"\r\n\r\n%s\r\n", itemPrefix, image_sizes[_imageVariation->size]);
        OPENAI_ERROR_CHECK(reqBody != NULL, "asprintf failed!", NULL);
    }
    if (_imageVariation->response_format != OPENAI_IMAGE_RESPONSE_FORMAT_URL) {
        str_extend(reqBody, "%s%s\"response_format\"\r\n\r\n%s\r\n", reqBody, itemPrefix, image_response_formats[_imageVariation->response_format]);
        OPENAI_ERROR_CHECK(reqBody != NULL, "asprintf failed!", NULL);
    }
    if (_imageVariation->n != 1) {
        str_extend(reqBody, "%s%s\"n\"\r\n\r\n%"PRIu32"\r\n", reqBody, itemPrefix, _imageVariation->n);
        OPENAI_ERROR_CHECK(reqBody != NULL, "asprintf failed!", NULL);
    }
    if (_imageVariation->user != NULL) {
        str_extend(reqBody, "%s%s\"user\"\r\n\r\n%s\r\n", reqBody, itemPrefix, _imageVariation->user);
        OPENAI_ERROR_CHECK(reqBody != NULL, "asprintf failed!", NULL);
    }
    str_extend(reqBody, "%s%s\"image\"; filename=\"image.png\"\r\nContent-Type: image/png\r\n\r\n", reqBody, itemPrefix);
    OPENAI_ERROR_CHECK(reqBody != NULL, "asprintf failed!", NULL);
    char *reqEndBody = NULL;
    asprintf(&reqEndBody, "\r\n--%s--\r\n", boundary);
    OPENAI_ERROR_CHECK(reqEndBody != NULL, "asprintf failed!", NULL);
    len = strlen(reqBody) + strlen(reqEndBody) + img_len;
    data = (uint8_t *)malloc(len + 1);
    OPENAI_ERROR_CHECK(data != NULL, "Failed to allocate request buffer!", NULL);
    uint8_t *d = data;
    memcpy(d, reqBody, strlen(reqBody));
    d += strlen(reqBody);
    memcpy(d, img_data, img_len);
    d += img_len;
    memcpy(d, reqEndBody, strlen(reqEndBody));
    d += strlen(reqEndBody);
    *d = 0;

    free(reqBody);
    free(reqEndBody);
    free(itemPrefix);
    char *res = _imageVariation->oai->upload(_imageVariation->oai->base_url, _imageVariation->oai->api_key, endpoint, boundary, data, len);
    free(data);
    OPENAI_ERROR_CHECK(res != NULL, "Empty result!", NULL);
    return OpenAI_ImageResponseCreate(res);
}

static OpenAI_ImageVariation_t *OpenAI_ImageVariationCreate(OpenAI_t *openai)
{
    _OpenAI_ImageVariation_t *_imageVariation = (_OpenAI_ImageVariation_t *)calloc(1, sizeof(_OpenAI_ImageVariation_t));
    OPENAI_ERROR_CHECK(_imageVariation != NULL, "imageVariation calloc failed!", NULL);
    _imageVariation->oai = __containerof(openai, _OpenAI_t, parent);;
    _imageVariation->size = OPENAI_IMAGE_SIZE_1024x1024;
    _imageVariation->response_format = OPENAI_IMAGE_RESPONSE_FORMAT_URL;
    _imageVariation->n = 1;

    _imageVariation->parent.setSize = &OpenAI_ImageVariationSetSize;
    _imageVariation->parent.setResponseFormat = &OpenAI_ImageVariationSetResponseFormat;
    _imageVariation->parent.setN = &OpenAI_ImageVariationSetN;
    _imageVariation->parent.setUser = &OpenAI_ImageVariationSetUser;
    _imageVariation->parent.image = &OpenAI_ImageVariationImage;

    return &_imageVariation->parent;
}

// images/edits { //Creates an edited or extended image given an original image and a prompt.
//   "image": "",//required string. The image to edit. Must be a valid PNG file, less than 4MB, and square. If mask is not provided, image must have transparency, which will be used as the mask.
//   "mask": "",//optional string. An additional image whose fully transparent areas (e.g. where alpha is zero) indicate where image should be edited. Must be a valid PNG file, less than 4MB, and have the same dimensions as image.
//   "prompt": "A cute baby sea otter",//required. A text description of the desired image(s). The maximum length is 1000 characters.
//   "n": 1,//integer. The number of images to generate. Must be between 1 and 10.
//   "size": "1024x1024",//string. The size of the generated images. Must be one of "256x256", "512x512", or "1024x1024"
//   "response_format": "url",//string. The format in which the generated images are returned. Must be one of "url" or "b64_json".
//   "user": null//string. A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
// }

/**
 * @brief Creates an edited or extended image given an original image and a prompt.
 *
 */
typedef struct {
    OpenAI_ImageEdit_t parent;                    /*!< Parent object */
    _OpenAI_t *oai;                               /*!< Pointer to the OpenAI object */
    char *prompt;                                 /*!< A text description of the desired image(s). The maximum length is 1000 characters. */
    OpenAI_Image_Size size;                       /*!< The size of the generated images. Must be one of 256x256, 512x512, or 1024x1024. */
    OpenAI_Image_Response_Format response_format; /*!< The format in which the generated images are returned. Must be one of url or b64_json. */
    uint32_t n;                                   /*!< The number of images to generate. Must be between 1 and 10. */
    char *user;                                   /*!< A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse. */
} _OpenAI_ImageEdit_t;

static void OpenAI_ImageEditDelete(OpenAI_ImageEdit_t *imageEdit)
{
    _OpenAI_ImageEdit_t *_imageEdit = __containerof(imageEdit, _OpenAI_ImageEdit_t, parent);
    if (_imageEdit != NULL) {
        if (_imageEdit->prompt != NULL) {
            free(_imageEdit->prompt);
            _imageEdit->prompt = NULL;
        }

        if (_imageEdit->user != NULL) {
            free(_imageEdit->user);
            _imageEdit->user = NULL;
        }

        free(_imageEdit);
        _imageEdit = NULL;
    }
}

static void OpenAI_ImageEditSetPrompt(OpenAI_ImageEdit_t *imageEdit, const char *p)
{
    _OpenAI_ImageEdit_t *_imageEdit = __containerof(imageEdit, _OpenAI_ImageEdit_t, parent);
    if (_imageEdit->prompt != NULL) {
        free(_imageEdit->prompt);
        _imageEdit->prompt = NULL;
    }

    _imageEdit->prompt = strdup(p);
}

static void OpenAI_ImageEditSetSize(OpenAI_ImageEdit_t *imageEdit, OpenAI_Image_Size s)
{
    _OpenAI_ImageEdit_t *_imageEdit = __containerof(imageEdit, _OpenAI_ImageEdit_t, parent);
    _imageEdit->size = s;
}

static void OpenAI_ImageEditSetResponseFormat(OpenAI_ImageEdit_t *imageEdit, OpenAI_Image_Response_Format rf)
{
    _OpenAI_ImageEdit_t *_imageEdit = __containerof(imageEdit, _OpenAI_ImageEdit_t, parent);
    if (rf >= OPENAI_IMAGE_RESPONSE_FORMAT_URL && rf <= OPENAI_IMAGE_RESPONSE_FORMAT_B64_JSON) {
        _imageEdit->response_format = rf;
    }
}

static void OpenAI_ImageEditSetN(OpenAI_ImageEdit_t *imageEdit, uint32_t n)
{
    _OpenAI_ImageEdit_t *_imageEdit = __containerof(imageEdit, _OpenAI_ImageEdit_t, parent);
    if (n > 0 && n <= 10) {
        _imageEdit->n = n;
    }
}

static void OpenAI_ImageEditSetUser(OpenAI_ImageEdit_t *imageEdit, const char *u)
{
    _OpenAI_ImageEdit_t *_imageEdit = __containerof(imageEdit, _OpenAI_ImageEdit_t, parent);
    if (_imageEdit->user != NULL) {
        free(_imageEdit->user);
        _imageEdit->user = NULL;
    }

    _imageEdit->user = strdup(u);
}

static OpenAI_ImageResponse_t *OpenAI_ImageEditImage(OpenAI_ImageEdit_t *imageEdit, uint8_t *img_data, size_t img_len, uint8_t *mask_data, size_t mask_len)
{
    const char *endpoint = "images/edits";
    const char *boundary = "----WebKitFormBoundaryb9v538xFWfzLzRO3";
    char *itemPrefix = NULL;
    asprintf(&itemPrefix, "--%s\r\nContent-Disposition: form-data; name=", boundary);
    OPENAI_ERROR_CHECK(itemPrefix != NULL, "Failed to allocate itemPrefix!", NULL);
    uint8_t *data = NULL;
    size_t len = 0;
    char *reqBody = NULL;
    _OpenAI_ImageEdit_t *_imageEdit = __containerof(imageEdit, _OpenAI_ImageEdit_t, parent);
    if (_imageEdit->prompt != NULL) {
        asprintf(&reqBody, "%s\"prompt\"\r\n\r\n%s\r\n", itemPrefix, _imageEdit->prompt);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    if (_imageEdit->size != OPENAI_IMAGE_SIZE_1024x1024) {
        str_extend(reqBody, "%s%s\"size\"\r\n\r\n%s\r\n", reqBody, itemPrefix, image_sizes[_imageEdit->size]);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    if (_imageEdit->response_format != OPENAI_IMAGE_RESPONSE_FORMAT_URL) {
        str_extend(reqBody, "%s%s\"response_format\"\r\n\r\n%s\r\n", reqBody, itemPrefix, image_response_formats[_imageEdit->response_format]);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    if (_imageEdit->n != 1) {
        str_extend(reqBody, "%s%s\"n\"\r\n\r\n%"PRIu32"\r\n", reqBody, itemPrefix, _imageEdit->n);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    if (_imageEdit->user != NULL) {
        str_extend(reqBody, "%s%s\"user\"\r\n\r\n%s\r\n", reqBody, itemPrefix, _imageEdit->user);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    str_extend(reqBody, "%s%s\"image\"; filename=\"image.png\"\r\nContent-Type: image/png\r\n\r\n", reqBody, itemPrefix);
    OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    char *reqEndBody = NULL;
    asprintf(&reqEndBody, "\r\n--%s--\r\n", boundary);
    OPENAI_ERROR_CHECK(reqEndBody != NULL, "Failed to allocate reqEndBody!", NULL);
    len = strlen(reqBody) + strlen(reqEndBody) + img_len;
    char *maskBody = NULL;
    if (mask_data != NULL && mask_len > 0) {
        asprintf(&maskBody, "\r\n%s\"mask\"; filename=\"mask.png\"\r\nContent-Type: image/png\r\n\r\n", itemPrefix);
        OPENAI_ERROR_CHECK(maskBody != NULL, "Failed to allocate maskBody!", NULL);
        len += strlen(maskBody) + mask_len;
    }

    data = (uint8_t *)malloc(len + 1);
    OPENAI_ERROR_CHECK(data != NULL, "Failed to allocate request buffer!", NULL);
    uint8_t *d = data;
    memcpy(d, reqBody, strlen(reqBody));
    d += strlen(reqBody);
    memcpy(d, img_data, img_len);
    d += img_len;
    if (mask_data != NULL && mask_len > 0) {
        memcpy(d, maskBody, strlen(maskBody));
        d += strlen(maskBody);
        memcpy(d, mask_data, mask_len);
        d += mask_len;
    }
    memcpy(d, reqEndBody, strlen(reqEndBody));
    d += strlen(reqEndBody);
    *d = 0;

    free(reqBody);
    free(reqEndBody);
    free(itemPrefix);
    if (maskBody != NULL) {
        free(maskBody);
    }
    char *res = _imageEdit->oai->upload(_imageEdit->oai->base_url, _imageEdit->oai->api_key, endpoint, boundary, data, len);
    free(data);
    OPENAI_ERROR_CHECK(res != NULL, "Empty result!", NULL);
    return OpenAI_ImageResponseCreate(res);
}

static OpenAI_ImageEdit_t *OpenAI_ImageEditCreate(OpenAI_t *openai)
{
    _OpenAI_ImageEdit_t *_imageEdit = (_OpenAI_ImageEdit_t *)calloc(1, sizeof(_OpenAI_ImageEdit_t));
    OPENAI_ERROR_CHECK(_imageEdit != NULL, "Failed to allocate _OpenAI_ImageEdit_t!", NULL);

    _imageEdit->oai = __containerof(openai, _OpenAI_t, parent);;
    _imageEdit->size = OPENAI_IMAGE_SIZE_1024x1024;
    _imageEdit->response_format = OPENAI_IMAGE_RESPONSE_FORMAT_URL;
    _imageEdit->n = 1;

    _imageEdit->parent.setPrompt = &OpenAI_ImageEditSetPrompt;
    _imageEdit->parent.setSize = &OpenAI_ImageEditSetSize;
    _imageEdit->parent.setResponseFormat = &OpenAI_ImageEditSetResponseFormat;
    _imageEdit->parent.setN = &OpenAI_ImageEditSetN;
    _imageEdit->parent.setUser = &OpenAI_ImageEditSetUser;
    _imageEdit->parent.image = &OpenAI_ImageEditImage;
    return &_imageEdit->parent;
}

// audio/transcriptions { //Transcribes audio into the input language.
//   "file": "audio.mp3",//required. The audio file to transcribe, in one of these formats: mp3, mp4, mpeg, mpga, m4a, wav, or webm.
//   "model": "whisper-1",//required. ID of the model to use. Only whisper-1 is currently available.
//   "prompt": "A cute baby sea otter",//An optional text to guide the model's style or continue a previous audio segment. The prompt should match the audio language.
//   "response_format": "json",//string. The format of the transcript output, in one of these options: "json", "text", "srt", "verbose_json", or "vtt".
//   "temperature": 1,//float between 0 and 2
//   "language": null//string. The language of the input audio. Supplying the input language in ISO-639-1 format will improve accuracy and latency.
// }

static const char *audio_input_formats[] = {
    "mp3",
    "mp4",
    "mpeg",
    "mpga",
    "m4a",
    "wav",
    "webm"
};

static const char *audio_input_mime[] = {
    "audio/mpeg",
    "audio/mp4",
    "audio/mpeg",
    "audio/mpeg",
    "audio/x-m4a",
    "audio/x-wav",
    "audio/webm"
};

static const char *audio_speech_formats[] = {"mp3", "opus", "aac", "flac"};

/**
 * @brief Gives audio from the input text.
 *
 */
typedef struct {
    OpenAI_AudioSpeech_t parent;              /*!< Base object */
    _OpenAI_t *oai;                                  /*!< Pointer to the OpenAI object */
    char *model;                                     /*!< ID of the model to use. */
    char *voice;
    float speed;
    OpenAI_Audio_Output_Format response_format;    /*!< The format of the output, in one of these options: "mp3", "opus", "aac", or "flac"*/
} _OpenAI_AudioSpeech_t;

static void OpenAI_AudioSpeechDelete(OpenAI_AudioSpeech_t *audioSpeech)
{
    _OpenAI_AudioSpeech_t *_audioSpeech = __containerof(audioSpeech, _OpenAI_AudioSpeech_t, parent);
    if (_audioSpeech != NULL) {
        if (_audioSpeech->model != NULL) {
            free(_audioSpeech->model);
            _audioSpeech->model = NULL;
        }
        if (_audioSpeech->voice != NULL) {
            free(_audioSpeech->voice);
            _audioSpeech->voice = NULL;
        }
        free(_audioSpeech);
        _audioSpeech = NULL;
    }
}

static void OpenAI_AudioSpeechSetModel(OpenAI_AudioSpeech_t *speech, const char *m)
{
    _OpenAI_AudioSpeech_t *_speech = __containerof(speech, _OpenAI_AudioSpeech_t, parent);
    if (_speech->model != NULL) {
        free(_speech->model);
        _speech->model = NULL;
    }
    _speech->model = strdup(m);
}

static void OpenAI_AudioSpeechSetVoice(OpenAI_AudioSpeech_t *speech, const char *m)
{
    _OpenAI_AudioSpeech_t *_speech = __containerof(speech, _OpenAI_AudioSpeech_t, parent);
    if (_speech->voice != NULL) {
        free(_speech->voice);
        _speech->voice = NULL;
    }
    _speech->voice = strdup(m);
}

static void OpenAI_AudioSpeechSetSpeed(OpenAI_AudioSpeech_t *speech, float t)
{
    _OpenAI_AudioSpeech_t *_speech = __containerof(speech, _OpenAI_AudioSpeech_t, parent);
    if (t >= 0.25 && t <= 4.0) {
        _speech->speed = t;
    }
}

static void OpenAI_AudioSpeechSetResponseFormat(OpenAI_AudioSpeech_t *audioCreateSpeech, OpenAI_Audio_Output_Format rf)
{
    _OpenAI_AudioSpeech_t *_audioCreateSpeech = __containerof(audioCreateSpeech, _OpenAI_AudioSpeech_t, parent);
    if (rf >= OPENAI_AUDIO_OUTPUT_FORMAT_MP3 && rf <= OPENAI_AUDIO_OUTPUT_FORMAT_FLAC) {
        _audioCreateSpeech->response_format = rf;
    }
}

/**
 * @brief Gives an audio from the input text.
 *
 */
typedef struct {
    OpenAI_SpeechResponse_t parent;
    uint32_t len;
    char *data;
} _OpenAI_SpeechResponse_t;

static void OpenAI_SpeechResponseDelete(OpenAI_SpeechResponse_t *audioSpeech)
{
    _OpenAI_SpeechResponse_t *_audioSpeech = __containerof(audioSpeech, _OpenAI_SpeechResponse_t, parent);

    if (_audioSpeech != NULL) {
        if (_audioSpeech->data) {
            free(_audioSpeech->data);
            _audioSpeech->data = NULL;
        }

        free(_audioSpeech);
        _audioSpeech = NULL;
    }
}

static uint32_t OpenAI_SpeechBufferGetLen(OpenAI_SpeechResponse_t *audioSpeech)
{
    _OpenAI_SpeechResponse_t *_audioSpeech = __containerof(audioSpeech, _OpenAI_SpeechResponse_t, parent);
    return _audioSpeech->len;
}

static char *OpenAI_SpeechGetDate(OpenAI_SpeechResponse_t *audioSpeech)
{
    _OpenAI_SpeechResponse_t *_audioSpeech = __containerof(audioSpeech, _OpenAI_SpeechResponse_t, parent);
    return (_audioSpeech->data);
}

static OpenAI_SpeechResponse_t *OpenAI_SpeechResponseCreate(char *payload, size_t dataLength)
{
    _OpenAI_SpeechResponse_t  *_audioSpeech = (_OpenAI_SpeechResponse_t *)calloc(1, sizeof(_OpenAI_SpeechResponse_t));
    OPENAI_ERROR_CHECK(NULL != _audioSpeech, "calloc failed!", NULL);
    if (payload == NULL || 0 == dataLength) {
        return &_audioSpeech->parent;
    }

    _audioSpeech->len = dataLength;
    _audioSpeech->data = (char *)malloc(dataLength * sizeof(char));
    OPENAI_ERROR_CHECK_GOTO(_audioSpeech->data != NULL, "malloc failed!", end);
    memcpy(_audioSpeech->data, payload, dataLength);

    free(payload);

    _audioSpeech->parent.getLen = &OpenAI_SpeechBufferGetLen;
    _audioSpeech->parent.getData = &OpenAI_SpeechGetDate;
    _audioSpeech->parent.deleteResponse = &OpenAI_SpeechResponseDelete;
    return &_audioSpeech->parent;
end:
    free(payload);
    OpenAI_SpeechResponseDelete(&_audioSpeech->parent);
    return NULL;
}

OpenAI_SpeechResponse_t *OpenAI_AudioSpeechMessage(OpenAI_AudioSpeech_t *audioSpeech, char *p)
{
    size_t dataLength = 0;
    const char *endpoint = "audio/speech";
    OpenAI_SpeechResponse_t *result = NULL;
    cJSON *req = cJSON_CreateObject();
    OPENAI_ERROR_CHECK(req != NULL, "cJSON_CreateObject failed!", NULL);
    _OpenAI_AudioSpeech_t *_audioSpeech = __containerof(audioSpeech, _OpenAI_AudioSpeech_t, parent);
    reqAddString("model", (_audioSpeech->model == NULL) ? "tts-1" : _audioSpeech->model);
    reqAddString("input", p);
    reqAddString("voice", (_audioSpeech->voice == NULL) ? "alloy" : _audioSpeech->voice);
    if (_audioSpeech->response_format != OPENAI_AUDIO_OUTPUT_FORMAT_MP3) {
        reqAddString("response_format", audio_speech_formats[_audioSpeech->response_format]);
    }
    if (_audioSpeech->speed != 1.0) {
        reqAddNumber("speed", _audioSpeech->speed);
    }
    char *jsonBody = cJSON_Print(req);
    ESP_LOGD(TAG, "json body for Speech Message %s", jsonBody);
    cJSON_Delete(req);
    char *res = _audioSpeech->oai->speechpost(_audioSpeech->oai->base_url, _audioSpeech->oai->api_key, endpoint, jsonBody, &dataLength);
    free(jsonBody);
    OPENAI_ERROR_CHECK(res != NULL, "Empty result!", result);
    return OpenAI_SpeechResponseCreate(res, dataLength);
}

static OpenAI_AudioSpeech_t *OpenAI_AudioSpeechCreate(OpenAI_t *openai)
{
    _OpenAI_AudioSpeech_t *_audioCreateSpeech = (_OpenAI_AudioSpeech_t *)calloc(1, sizeof(_OpenAI_AudioSpeech_t));
    OPENAI_ERROR_CHECK(_audioCreateSpeech != NULL, "Failed to allocate _audioCreateSpeech!", NULL);

    _audioCreateSpeech->oai = __containerof(openai, _OpenAI_t, parent);
    _audioCreateSpeech->response_format = OPENAI_AUDIO_OUTPUT_FORMAT_MP3;
    _audioCreateSpeech->parent.setModel = &OpenAI_AudioSpeechSetModel;
    _audioCreateSpeech->parent.setVoice = &OpenAI_AudioSpeechSetVoice;
    _audioCreateSpeech->parent.setSpeed = &OpenAI_AudioSpeechSetSpeed;
    _audioCreateSpeech->parent.setResponseFormat = &OpenAI_AudioSpeechSetResponseFormat;
    _audioCreateSpeech->parent.speech = &OpenAI_AudioSpeechMessage;

    return &_audioCreateSpeech->parent;
}

/**
 * @brief Transcribes audio into the input language.
 *
 */
typedef struct {
    OpenAI_AudioTranscription_t parent;              /*!< Base object */
    _OpenAI_t *oai;                                  /*!< Pointer to the OpenAI object */
    char *prompt;                                    /*!< An optional text to guide the model's style or continue a previous audio segment. The prompt should match the audio language. */
    OpenAI_Audio_Response_Format response_format;    /*!< The format of the transcript output, in one of these options: "json", "text", "srt", "verbose_json", or "vtt". */
    float temperature;                               /*!< float between 0 and 2. Higher value gives more random results. */
    char *language;                                  /*!< The language of the input audio. Supplying the input language in ISO-639-1 format will improve accuracy and latency. */
} _OpenAI_AudioTranscription_t;

static const char *audio_response_formats[] = {"json", "text", "srt", "verbose_json", "vtt"};

static void OpenAI_AudioTranscriptionDelete(OpenAI_AudioTranscription_t *audioTranscription)
{
    _OpenAI_AudioTranscription_t *_audioTranscription = __containerof(audioTranscription, _OpenAI_AudioTranscription_t, parent);
    if (_audioTranscription != NULL) {
        if (_audioTranscription->prompt != NULL) {
            free(_audioTranscription->prompt);
            _audioTranscription->prompt = NULL;
        }
        if (_audioTranscription->language != NULL) {
            free(_audioTranscription->language);
            _audioTranscription->language = NULL;
        }
        free(_audioTranscription);
        _audioTranscription = NULL;
    }
}

static void OpenAI_AudioTranscriptionSetPrompt(OpenAI_AudioTranscription_t *audioTranscription, const char *p)
{
    _OpenAI_AudioTranscription_t *_audioTranscription = __containerof(audioTranscription, _OpenAI_AudioTranscription_t, parent);
    if (_audioTranscription->prompt != NULL) {
        free(_audioTranscription->prompt);
        _audioTranscription->prompt = NULL;
    }
    if (p != NULL) {
        _audioTranscription->prompt = strdup(p);
    }
}

static void OpenAI_AudioTranscriptionSetResponseFormat(OpenAI_AudioTranscription_t *audioTranscription, OpenAI_Audio_Response_Format rf)
{
    _OpenAI_AudioTranscription_t *_audioTranscription = __containerof(audioTranscription, _OpenAI_AudioTranscription_t, parent);
    if (rf >= OPENAI_AUDIO_RESPONSE_FORMAT_JSON && rf <= OPENAI_AUDIO_RESPONSE_FORMAT_VTT) {
        _audioTranscription->response_format = rf;
    }
}

static void OpenAI_AudioTranscriptionSetTemperature(OpenAI_AudioTranscription_t *audioTranscription, float t)
{
    _OpenAI_AudioTranscription_t *_audioTranscription = __containerof(audioTranscription, _OpenAI_AudioTranscription_t, parent);
    if (t >= 0 && t <= 2.0) {
        _audioTranscription->temperature = t;
    }
}

static void OpenAI_AudioTranscriptionSetLanguage(OpenAI_AudioTranscription_t *audioTranscription, const char *l)
{
    _OpenAI_AudioTranscription_t *_audioTranscription = __containerof(audioTranscription, _OpenAI_AudioTranscription_t, parent);
    if (_audioTranscription->language != NULL) {
        free(_audioTranscription->language);
        _audioTranscription->language = NULL;
    }
    if (l != NULL) {
        _audioTranscription->language = strdup(l);
    }
}

static char *OpenAI_AudioTranscriptionFile(OpenAI_AudioTranscription_t *audioTranscription, uint8_t *audio_data, size_t audio_len, OpenAI_Audio_Input_Format f)
{
    const char *endpoint = "audio/transcriptions";
    const char *boundary = "----WebKitFormBoundary9HKFexBRLrf9dcpY";
    char *itemPrefix = NULL;
    asprintf(&itemPrefix, "--%s\r\nContent-Disposition: form-data; name=", boundary);
    OPENAI_ERROR_CHECK(itemPrefix != NULL, "Failed to allocate itemPrefix!", NULL);
    uint8_t *data = NULL;
    size_t len = 0;

    char *reqBody = NULL;
    asprintf(&reqBody, "%s\"model\"\r\n\r\nwhisper-1\r\n", itemPrefix);
    OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    _OpenAI_AudioTranscription_t *_audioTranscription = __containerof(audioTranscription, _OpenAI_AudioTranscription_t, parent);
    if (_audioTranscription->prompt != NULL) {
        str_extend(reqBody, "%s%s\"prompt\"\r\n\r\n%s\r\n", reqBody, itemPrefix, _audioTranscription->prompt);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    if (_audioTranscription->response_format != OPENAI_AUDIO_RESPONSE_FORMAT_JSON) {
        str_extend(reqBody, "%s%s\"response_format\"\r\n\r\n%s\r\n", reqBody, itemPrefix, audio_response_formats[_audioTranscription->response_format]);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    if (_audioTranscription->temperature != 0) {
        str_extend(reqBody, "%s%s\"temperature\"\r\n\r\n%f\r\n", reqBody, itemPrefix, _audioTranscription->temperature);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    if (_audioTranscription->language != NULL) {
        str_extend(reqBody, "%s%s\"language\"\r\n\r\n%s\r\n", reqBody, itemPrefix, _audioTranscription->language);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    }
    str_extend(reqBody, "%s%s\"file\"; filename=\"audio.%s\"\r\nContent-Type: %s\r\n\r\n", reqBody, itemPrefix, audio_input_formats[f], audio_input_mime[f]);
    OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate reqBody!", NULL);
    char *reqEndBody = NULL;
    asprintf(&reqEndBody, "\r\n--%s--\r\n", boundary);
    OPENAI_ERROR_CHECK(reqEndBody != NULL, "Failed to allocate reqEndBody!", NULL);
    len = strlen(reqBody) + strlen(reqEndBody) + audio_len;
    data = (uint8_t *)malloc(len + 1);
    OPENAI_ERROR_CHECK(data != NULL, "Failed to allocate request buffer!", NULL);
    uint8_t *d = data;
    memcpy(d, reqBody, strlen(reqBody));
    d += strlen(reqBody);
    memcpy(d, audio_data, audio_len);
    d += audio_len;
    memcpy(d, reqEndBody, strlen(reqEndBody));
    d += strlen(reqEndBody);
    *d = 0;
    free(reqBody);
    free(reqEndBody);
    free(itemPrefix);

    char *result = _audioTranscription->oai->upload(_audioTranscription->oai->base_url, _audioTranscription->oai->api_key, endpoint, boundary, data, len);
    free(data);
    OPENAI_ERROR_CHECK(result != NULL, "Empty result!", NULL);
    cJSON *json = cJSON_Parse(result);
    free(result);
    result = NULL;
    char *error = getJsonError(json);
    if (error != NULL) {
        if (strcmp(error, "cJSON_Parse failed!") == 0) {
            free(error);
            error = NULL;
        }
        result = error;
    } else {
        if (cJSON_HasObjectItem(json, "text")) {
            cJSON *text = cJSON_GetObjectItem(json, "text");
            result = strdup(cJSON_GetStringValue(text));
        }
    }

    cJSON_Delete(json);
    return result;
}

static OpenAI_AudioTranscription_t *OpenAI_AudioTranscriptionCreate(OpenAI_t *openai)
{
    _OpenAI_AudioTranscription_t *_audioTranscription = (_OpenAI_AudioTranscription_t *)calloc(1, sizeof(_OpenAI_AudioTranscription_t));
    OPENAI_ERROR_CHECK(_audioTranscription != NULL, "Failed to allocate _audioTranscription!", NULL);

    _audioTranscription->oai = __containerof(openai, _OpenAI_t, parent);
    _audioTranscription->response_format = OPENAI_AUDIO_RESPONSE_FORMAT_JSON;

    _audioTranscription->parent.setPrompt = &OpenAI_AudioTranscriptionSetPrompt;
    _audioTranscription->parent.setResponseFormat = &OpenAI_AudioTranscriptionSetResponseFormat;
    _audioTranscription->parent.setTemperature = &OpenAI_AudioTranscriptionSetTemperature;
    _audioTranscription->parent.setLanguage = &OpenAI_AudioTranscriptionSetLanguage;
    _audioTranscription->parent.file = &OpenAI_AudioTranscriptionFile;
    return &_audioTranscription->parent;
}

// audio/translations { //Translates audio into into English.
//   "file": "german.m4a",//required. The audio file to translate, in one of these formats: mp3, mp4, mpeg, mpga, m4a, wav, or webm.
//   "model": "whisper-1",//required. ID of the model to use. Only whisper-1 is currently available.
//   "prompt": "A cute baby sea otter",//An optional text to guide the model's style or continue a previous audio segment. The prompt should match the audio language.
//   "response_format": "json",//string. The format of the transcript output, in one of these options: "json", "text", "srt", "verbose_json", or "vtt".
//   "temperature": 1//float between 0 and 2
// }

/**
 * @brief Translates audio into into English.
 *
 */
typedef struct {
    OpenAI_AudioTranslation_t parent;               /*!< Parent object member */
    _OpenAI_t *oai;                                 /*!< Pointer to the OpenAI object */
    char *prompt;                                   /*!< An optional text to guide the model's style or continue a previous audio segment. The prompt should match the audio language. */
    OpenAI_Audio_Response_Format response_format;   /*!< The format of the transcript output, in one of these options: "json", "text", "srt", "verbose_json", or "vtt". */
    float temperature;                              /*!< float between 0 and 2. Higher value gives more random results. */
} _OpenAI_AudioTranslation_t;

static void OpenAI_AudioTranslationDelete(OpenAI_AudioTranslation_t *audioTranslation)
{
    _OpenAI_AudioTranslation_t *_audioTranslation = __containerof(audioTranslation, _OpenAI_AudioTranslation_t, parent);
    if (_audioTranslation != NULL) {
        if (_audioTranslation->prompt != NULL) {
            free(_audioTranslation->prompt);
            _audioTranslation->prompt = NULL;
        }
        free(_audioTranslation);
        _audioTranslation = NULL;
    }
}

static void OpenAI_AudioTranslationSetPrompt(OpenAI_AudioTranslation_t *audioTranslation, const char *p)
{
    _OpenAI_AudioTranslation_t *_audioTranslation = __containerof(audioTranslation, _OpenAI_AudioTranslation_t, parent);
    if (_audioTranslation->prompt != NULL) {
        free(_audioTranslation->prompt);
        _audioTranslation->prompt = NULL;
    }
    if (p != NULL) {
        _audioTranslation->prompt = strdup(p);
    }
}

static void OpenAI_AudioTranslationSetResponseFormat(OpenAI_AudioTranslation_t *audioTranslation, OpenAI_Audio_Response_Format rf)
{
    _OpenAI_AudioTranslation_t *_audioTranslation = __containerof(audioTranslation, _OpenAI_AudioTranslation_t, parent);
    if (rf >= OPENAI_AUDIO_RESPONSE_FORMAT_JSON && rf <= OPENAI_AUDIO_RESPONSE_FORMAT_VTT) {
        _audioTranslation->response_format = rf;
    }
}

static void OpenAI_AudioTranslationSetTemperature(OpenAI_AudioTranslation_t *audioTranslation, float t)
{
    _OpenAI_AudioTranslation_t *_audioTranslation = __containerof(audioTranslation, _OpenAI_AudioTranslation_t, parent);
    if (t >= 0 && t <= 2.0) {
        _audioTranslation->temperature = t;
    }
}

static char *OpenAI_AudioTranslationFile(OpenAI_AudioTranslation_t *audioTranslation, uint8_t *audio_data, size_t audio_len, OpenAI_Audio_Input_Format f)
{
    const char *endpoint = "audio/translations";
    const char *boundary = "----WebKitFormBoundary9HKFexBRLrf9dcpY";
    char *itemPrefix = NULL;
    asprintf(&itemPrefix, "--%s\r\nContent-Disposition: form-data; name=", boundary);
    OPENAI_ERROR_CHECK(itemPrefix != NULL, "Failed to allocate itemPrefix!", NULL);
    uint8_t *data = NULL;
    size_t len = 0;
    char *reqBody = NULL;
    asprintf(&reqBody, "%s\"model\"\r\n\r\nwhisper-1\r\n", itemPrefix);
    OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate prompt!", NULL);
    _OpenAI_AudioTranslation_t *_audioTranslation = __containerof(audioTranslation, _OpenAI_AudioTranslation_t, parent);
    if (_audioTranslation->prompt != NULL) {
        str_extend(reqBody, "%s%s\"prompt\"\r\n\r\n%s\r\n", reqBody, itemPrefix, _audioTranslation->prompt);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate prompt!", NULL);
    }
    if (_audioTranslation->response_format != OPENAI_AUDIO_RESPONSE_FORMAT_JSON) {
        str_extend(reqBody, "%s%s\"response_format\"\r\n\r\n%s\r\n", reqBody, itemPrefix, audio_response_formats[_audioTranslation->response_format]);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate prompt!", NULL);
    }
    if (_audioTranslation->temperature != 0) {
        str_extend(reqBody, "%s%s\"temperature\"\r\n\r\n%f\r\n", reqBody, itemPrefix, _audioTranslation->temperature);
        OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate prompt!", NULL);
    }
    str_extend(reqBody, "%s%s\"file\"; filename=\"audio.%s\"\r\nContent-Type: %s\r\n\r\n", reqBody, itemPrefix, audio_input_formats[f], audio_input_mime[f]);
    OPENAI_ERROR_CHECK(reqBody != NULL, "Failed to allocate prompt!", NULL);
    char *reqEndBody = NULL;
    asprintf(&reqEndBody, "\r\n--%s--\r\n", boundary);
    OPENAI_ERROR_CHECK(reqEndBody != NULL, "Failed to allocate reqEndBody!", NULL);
    len = strlen(reqBody) + strlen(reqEndBody) + audio_len;
    data = (uint8_t *)malloc(len + 1);
    OPENAI_ERROR_CHECK(data != NULL, "Failed to allocate request buffer!", NULL);
    uint8_t *d = data;
    memcpy(d, reqBody, strlen(reqBody));
    d += strlen(reqBody);
    memcpy(d, audio_data, audio_len);
    d += audio_len;
    memcpy(d, reqEndBody, strlen(reqEndBody));
    d += strlen(reqEndBody);
    *d = 0;
    free(itemPrefix);
    free(reqBody);
    free(reqEndBody);
    char *result = _audioTranslation->oai->upload(_audioTranslation->oai->base_url, _audioTranslation->oai->api_key, endpoint, boundary, data, len);
    free(data);
    OPENAI_ERROR_CHECK(result != NULL, "Empty result!", NULL);
    cJSON *json = cJSON_Parse(result);
    char *error = getJsonError(json);
    if (error != NULL) {
        ESP_LOGE(TAG, "%s", error);
    } else {
        free(result);
        if (cJSON_HasObjectItem(json, "text")) {
            cJSON *text = cJSON_GetObjectItem(json, "text");
            result = strdup(cJSON_GetStringValue(text));
        }
    }
    cJSON_Delete(json);
    return result;
}

static OpenAI_AudioTranslation_t *OpenAI_AudioTranslationCreate(OpenAI_t *openai)
{
    _OpenAI_AudioTranslation_t *_audioTranslation = (_OpenAI_AudioTranslation_t *)calloc(1, sizeof(_OpenAI_AudioTranslation_t));
    OPENAI_ERROR_CHECK(_audioTranslation != NULL, "Failed to allocate _audioTranslation!", NULL);
    _audioTranslation->oai = __containerof(openai, _OpenAI_t, parent);
    _audioTranslation->response_format = OPENAI_AUDIO_RESPONSE_FORMAT_JSON;

    _audioTranslation->parent.setPrompt = &OpenAI_AudioTranslationSetPrompt;
    _audioTranslation->parent.setResponseFormat = &OpenAI_AudioTranslationSetResponseFormat;
    _audioTranslation->parent.setTemperature = &OpenAI_AudioTranslationSetTemperature;
    _audioTranslation->parent.file = &OpenAI_AudioTranslationFile;
    return &_audioTranslation->parent;
}

// embeddings { //Creates an embedding vector representing the input text.
//   "model": "text-embedding-ada-002",//required
//   "input": "The food was delicious and the waiter...",//required string or array. Input text to get embeddings for, encoded as a string or array of tokens. To get embeddings for multiple inputs in a single request, pass an array of strings or array of token arrays. Each input must not exceed 8192 tokens in length.
//   "user": null//string. A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
// }

/**
 * @brief Get a vector representation of a given input that can be easily consumed by machine learning models and algorithms.
 *
 * @param openai Pointer to the OpenAI object
 * @param input Input text to embed, encoded as a string or array of tokens. To embed multiple inputs in a single request,
 *              pass an array of strings or array of token arrays. Each input must not exceed the max input tokens for the model (8191 tokens for text-embedding-ada-002).
 * @param model ID of the model to use.
 * @param user A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.
 * @return Pointer to the OpenAI_EmbeddingResponse_t.
 */
OpenAI_EmbeddingResponse_t *OpenAI_EmbeddingCreate(OpenAI_t *openai, char *input, const char *model, const char *user)
{
    const char *endpoint = "embeddings";
    OpenAI_EmbeddingResponse_t *result = NULL;
    cJSON *req = cJSON_CreateObject();

    OPENAI_ERROR_CHECK(req != NULL, "cJSON_CreateObject failed!", NULL);
    reqAddString("model", (model == NULL) ? "text-embedding-ada-002" : model);
    if (input[0] == '[') {
        cJSON *in = cJSON_Parse(input);
        OPENAI_ERROR_CHECK(in != NULL, "cJSON_Parse failed!", NULL);
        reqAddItem("input", in);
    } else {
        reqAddString("input", input);
    }
    if (user != NULL) {
        reqAddString("user", user);
    }
    char *jsonBody = cJSON_Print(req);
    cJSON_Delete(req);
    _OpenAI_t *_openai = __containerof(openai, _OpenAI_t, parent);
    char *response = _openai->post(_openai->base_url, _openai->api_key, endpoint, jsonBody);
    free(jsonBody);
    OPENAI_ERROR_CHECK(response != NULL, "Empty response!", NULL);
    return OpenAI_EmbeddingResponseCreate(response);
}

// moderations { //Classifies if text violates OpenAI's Content Policy
//   "input": "I want to kill them.",//required string or array
//   "model": "text-moderation-latest"//optional. Two content moderations models are available: text-moderation-stable and text-moderation-latest.
// }

/**
 * @brief Given a input text, outputs if the model classifies it as violating OpenAI's content policy.
 *
 * @param openai Pointer to the OpenAI object
 * @param input The input text to classify
 * @param model Two content moderations models are available: text-moderation-stable and text-moderation-latest.
 * @return Pointer to the OpenAI_ModerationResponse_t
 */
OpenAI_ModerationResponse_t *OpenAI_ModerationCreate(OpenAI_t *openai, char *input, const char *model)
{
    const char *endpoint = "moderations";
    OpenAI_ModerationResponse_t *result = NULL;
    char *res = NULL;
    cJSON *req = cJSON_CreateObject();
    OPENAI_ERROR_CHECK(req != NULL, "cJSON_CreateObject failed!", NULL);
    if (input[0] == '[') {
        cJSON *in = cJSON_Parse(input);
        OPENAI_ERROR_CHECK(in != NULL, "cJSON_Parse failed!", NULL);
        reqAddItem("input", in);
    } else {
        reqAddString("input", input);
    }
    if (model != NULL) {
        reqAddString("model", model);
    }
    char *jsonBody = cJSON_Print(req);
    cJSON_Delete(req);
    _OpenAI_t *_openai = __containerof(openai, _OpenAI_t, parent);
    res = _openai->post(_openai->base_url, _openai->api_key, endpoint, jsonBody);
    free(jsonBody);
    OPENAI_ERROR_CHECK(res != NULL, "Empty result!", NULL);
    return OpenAI_ModerationResponseCreate(res);
}

void OpenAIDelete(OpenAI_t *oai)
{
    _OpenAI_t *_oai = __containerof(oai, _OpenAI_t, parent);
    if (_oai != NULL) {
        if (_oai->api_key != NULL) {
            free(_oai->api_key);
            free(_oai->base_url);
            _oai->api_key = NULL;
        }
        free(_oai);
        _oai = NULL;
    }
}

void OpenAIChangeBaseURL(OpenAI_t *oai, const char *baseURL)
{
    _OpenAI_t *_oai = __containerof(oai, _OpenAI_t, parent);
    if (_oai->base_url != NULL) {
        free(_oai->base_url);
    }
    _oai->base_url = strdup(baseURL);
}

static char *OpenAI_Request(const char *base_url, const char *api_key, const char *endpoint, const char *content_type, esp_http_client_method_t method, const char *boundary, uint8_t *data, size_t len)
{
    ESP_LOGD(TAG, "\"%s\", len=%u", endpoint, len);
    char *url = NULL;
    char *result = NULL;
    asprintf(&url, "%s%s", base_url, endpoint);
    OPENAI_ERROR_CHECK(url != NULL, "Failed to allocate url!", NULL);
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = 60000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char *headers = NULL;
    if (boundary) {
        asprintf(&headers, "%s; boundary=%s", content_type, boundary);
    } else {
        asprintf(&headers, "%s", content_type);
    }
    OPENAI_ERROR_CHECK_GOTO(headers != NULL, "Failed to allocate headers!", end);
    esp_http_client_set_header(client, "Content-Type", headers);
    free(headers);

    asprintf(&headers, "Bearer %s", api_key);
    OPENAI_ERROR_CHECK_GOTO(headers != NULL, "Failed to allocate headers!", end);
    esp_http_client_set_header(client, "Authorization", headers);
    free(headers);

    esp_err_t err = esp_http_client_open(client, len);
    OPENAI_ERROR_CHECK_GOTO(err == ESP_OK, "Failed to open client!", end);
    if (len > 0) {
        int wlen = esp_http_client_write(client, (const char *)data, len);
        OPENAI_ERROR_CHECK_GOTO(wlen >= 0, "Failed to write client!", end);
    }
    int content_length = esp_http_client_fetch_headers(client);
    if (esp_http_client_is_chunked_response(client)) {
        esp_http_client_get_chunk_length(client, &content_length);
    }
    ESP_LOGD(TAG, "content_length=%d", content_length);
    OPENAI_ERROR_CHECK_GOTO(content_length >= 0, "HTTP client fetch headers failed!", end);
    result = (char *)malloc(content_length + 1);
    int read = esp_http_client_read_response(client, result, content_length);
    if (read != content_length) {
        ESP_LOGE(TAG, "HTTP_ERROR: read=%d, length=%d", read, content_length);
        free(result);
        result = NULL;
    } else {
        result[content_length] = 0;
        ESP_LOGD(TAG, "result: %s, size: %d", result, strlen(result));
    }

end:
    free(url);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result != NULL ? result : NULL;
}

static char *OpenAI_Speech_Request(const char *base_url, const char *api_key, const char *endpoint, const char *content_type, esp_http_client_method_t method, const char *boundary, uint8_t *data, size_t len, size_t *output_len)
{
    ESP_LOGD(TAG, "\"%s\", len=%u", endpoint, len);
    char *url = NULL;
    char *result = NULL;
    asprintf(&url, "%s%s", base_url, endpoint);
    OPENAI_ERROR_CHECK(url != NULL, "Failed to allocate url!", NULL);
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = 60000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    char *headers = NULL;
    if (boundary) {
        asprintf(&headers, "%s; boundary=%s", content_type, boundary);
    } else {
        asprintf(&headers, "%s", content_type);
    }
    OPENAI_ERROR_CHECK_GOTO(headers != NULL, "Failed to allocate headers!", end);
    esp_http_client_set_header(client, "Content-Type", headers);
    ESP_LOGD(TAG, "headers:\r\n%s", headers);
    free(headers);

    asprintf(&headers, "Bearer %s", api_key);
    OPENAI_ERROR_CHECK_GOTO(headers != NULL, "Failed to allocate headers!", end);
    esp_http_client_set_header(client, "Authorization", headers);
    free(headers);

    esp_err_t err = esp_http_client_open(client, len);
    ESP_LOGD(TAG, "data:\r\n%s", data);

    OPENAI_ERROR_CHECK_GOTO(err == ESP_OK, "Failed to open client!", end);
    if (len > 0) {
        int wlen = esp_http_client_write(client, (const char *)data, len);
        OPENAI_ERROR_CHECK_GOTO(wlen >= 0, "Failed to write client!", end);
    }
    int content_length = esp_http_client_fetch_headers(client);
    if (esp_http_client_is_chunked_response(client)) {
        esp_http_client_get_chunk_length(client, &content_length);
    }
    ESP_LOGD(TAG, "chunk_length=%d", content_length); //4096
    OPENAI_ERROR_CHECK_GOTO(content_length > 0, "HTTP client fetch headers failed!", end);

    int read_len = 0;
    *output_len = 0;
    do {
        result = (char *)realloc(result, *output_len + content_length + 1);
        OPENAI_ERROR_CHECK_GOTO(result != NULL, "Chunk Data reallocated Failed", end);
        read_len = esp_http_client_read_response(client, result + (int) * output_len, content_length);
        *output_len += read_len;
        ESP_LOGD(TAG, "HTTP_READ:=%d", read_len);
    } while (read_len > 0);
    ESP_LOGD(TAG, "output_len: %d\n", (int)*output_len);

end:
    free(url);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result != NULL ? result : NULL;
}

static char *OpenAI_Speech_Post(const char *base_url, const char *api_key, const char *endpoint, char *jsonBody, size_t *output_len)
{
    return OpenAI_Speech_Request(base_url, api_key, endpoint, "application/json", HTTP_METHOD_POST, NULL, (uint8_t *)jsonBody, strlen(jsonBody), output_len);
}

static char *OpenAI_Upload(const char *base_url, const char *api_key, const char *endpoint, const char *boundary, uint8_t *data, size_t len)
{
    return OpenAI_Request(base_url, api_key, endpoint, "multipart/form-data", HTTP_METHOD_POST, boundary, data, len);
}

static char *OpenAI_Post(const char *base_url, const char *api_key, const char *endpoint, char *jsonBody)
{
    return OpenAI_Request(base_url, api_key, endpoint, "application/json", HTTP_METHOD_POST, NULL, (uint8_t *)jsonBody, strlen(jsonBody));
}

static char *OpenAI_Get(const char *base_url, const char *api_key, const char *endpoint)
{
    return OpenAI_Request(base_url, api_key, endpoint, "application/json", HTTP_METHOD_GET, NULL, NULL, 0);
}

static char *OpenAI_Del(const char *base_url, const char *api_key, const char *endpoint)
{
    return OpenAI_Request(base_url, api_key, endpoint, "application/json", HTTP_METHOD_DELETE, NULL, NULL, 0);
}

OpenAI_t *OpenAICreate(const char *api_key)
{
    ESP_LOGI(TAG, "OpenAI create, version: %d.%d.%d", OPENAI_VER_MAJOR, OPENAI_VER_MINOR, OPENAI_VER_PATCH);
    _OpenAI_t *_oai = (_OpenAI_t *)calloc(1, sizeof(_OpenAI_t));
    OPENAI_ERROR_CHECK(_oai != NULL, "Failed to allocate _OpenAI!", NULL);
    _oai->api_key = strdup(api_key);
    _oai->base_url = strdup(OPENAI_DEFAULT_BASE_URL);

#if CONFIG_ENABLE_EMBEDDING
    _oai->parent.embeddingCreate = &OpenAI_EmbeddingCreate;
#endif

#if CONFIG_ENABLE_MODERATION
    _oai->parent.moderationCreate = &OpenAI_ModerationCreate;
#endif

#if CONFIG_ENABLE_COMPLETION
    _oai->parent.completionCreate = &OpenAI_CompletionCreate;
    _oai->parent.completionDelete = &OpenAI_CompletionDelete;
#endif

#if CONFIG_ENABLE_CHAT_COMPLETION
    _oai->parent.chatCreate = &OpenAI_ChatCompletionCreate;
    _oai->parent.chatDelete = &OpenAI_ChatCompletionDelete;
#endif

#if CONFIG_ENABLE_EDIT
    _oai->parent.editCreate = &OpenAI_EditCreate;
    _oai->parent.editDelete = &OpenAI_EditDelete;
#endif

#if CONFIG_ENABLE_IMAGE_GENERATION
    _oai->parent.imageGenerationCreate = &OpenAI_ImageGenerationCreate;
    _oai->parent.imageGenerationDelete = &OpenAI_ImageGenerationDelete;
#endif

#if CONFIG_ENABLE_IMAGE_VARIATION
    _oai->parent.imageVariationCreate = &OpenAI_ImageVariationCreate;
    _oai->parent.imageVariationDelete = &OpenAI_ImageVariationDelete;
#endif

#if CONFIG_ENABLE_IMAGE_EDIT
    _oai->parent.imageEditCreate = &OpenAI_ImageEditCreate;
    _oai->parent.imageEditDelete = &OpenAI_ImageEditDelete;
#endif

#if CONFIG_ENABLE_AUDIO_TRANSCRIPTION
    _oai->parent.audioTranscriptionCreate = &OpenAI_AudioTranscriptionCreate;
    _oai->parent.audioTranscriptionDelete = &OpenAI_AudioTranscriptionDelete;
#endif

#if CONFIG_ENABLE_AUDIO_TRANSLATION
    _oai->parent.audioTranslationCreate = &OpenAI_AudioTranslationCreate;
    _oai->parent.audioTranslationDelete = &OpenAI_AudioTranslationDelete;
#endif

#if CONFIG_ENABLE_AUDIO_SPEECH
    _oai->parent.audioSpeechCreate = &OpenAI_AudioSpeechCreate;
    _oai->parent.audioSpeechDelete = &OpenAI_AudioSpeechDelete;
#endif

    _oai->get = &OpenAI_Get;
    _oai->del = &OpenAI_Del;
    _oai->post = &OpenAI_Post;
    _oai->speechpost = &OpenAI_Speech_Post;
    _oai->upload = &OpenAI_Upload;
    return &_oai->parent;
}
