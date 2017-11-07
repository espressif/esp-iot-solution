
/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */
#include "platform/platform.h"
#include "product/product.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "esp_system.h"

#include "alink_export.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_alink.h"

#define Method_PostData      "postDeviceData"
#define Method_PostRawData   "postDeviceRawData"
#define Method_GetAlinkTime  "getAlinkTime"

#if ALINK_PASSTHROUGH
#define ALINK_METHOD_POST     Method_PostRawData
#define ALINK_GET_DEVICE_DATA ALINK_GET_DEVICE_RAWDATA
#define ALINK_SET_DEVICE_DATA ALINK_SET_DEVICE_RAWDATA
#else
#define ALINK_METHOD_POST     Method_PostData
#define ALINK_GET_DEVICE_DATA ALINK_GET_DEVICE_STATUS
#define ALINK_SET_DEVICE_DATA ALINK_SET_DEVICE_STATUS
#endif

#define DOWN_CMD_QUEUE_NUM  5
#define UP_CMD_QUEUE_NUM    5
#define EVENT_QUEUE_NUM     3

static const char *TAG = "alink_main";
static alink_err_t post_data_enable        = ALINK_TRUE;
static xQueueHandle xQueueDownCmd          = NULL;
static xQueueHandle xQueueUpCmd            = NULL;
static SemaphoreHandle_t xSemWrite         = NULL;
static SemaphoreHandle_t xSemRead          = NULL;
static SemaphoreHandle_t xSemDownCmd       = NULL;
static xQueueHandle xQueueEvent            = NULL;
static alink_event_cb_t s_event_handler_cb = NULL;

static alink_err_t alink_event_post_to_user(alink_event_t event)
{
    if (s_event_handler_cb) {
        return (*s_event_handler_cb)(event);
    }
    return ALINK_OK;
}

static void alink_event_loop_task(void *pvParameters)
{
    alink_err_t ret = ALINK_OK;
    for (;;) {
        alink_event_t event;
        if (xQueueReceive(xQueueEvent, &event, portMAX_DELAY) == pdPASS) {
            ret = alink_event_post_to_user(event);
            if (ret != ALINK_OK) {
                ALINK_LOGE("post event to user fail!");
            }
        }
    }
    vTaskDelete(NULL);
}

alink_err_t alink_event_send(alink_event_t event)
{
    if (xQueueEvent == NULL)
        xQueueEvent = xQueueCreate(EVENT_QUEUE_NUM, sizeof(alink_event_t));
    if (xQueueSend(xQueueEvent, &event, 0) != pdTRUE) {
        ALINK_LOGE("xQueueSendToBack fail!");
        return ALINK_ERR;
    }
    return ALINK_OK;
}

static inline void alink_free(_IN_ void *arg)
{
    if (arg == NULL) return;
    free(arg);
    arg = NULL;
}

static alink_err_t cloud_get_device_data(_IN_ char *json_buffer)
{
    platform_mutex_lock(xSemDownCmd);
    ALINK_PARAM_CHECK(json_buffer == NULL);
    alink_event_send(ALINK_EVENT_GET_DEVICE_DATA);
    alink_err_t ret = ALINK_OK;
    int size = strlen(json_buffer) + 1;
    char *q_data = (char *)malloc(size);
    if (size > ALINK_DATA_LEN) ALINK_LOGW("json_buffer len:%d", size);
    memcpy(q_data, json_buffer, size);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        alink_free(q_data);
    }

    platform_mutex_unlock(xSemDownCmd);
    return ret;
}

static alink_err_t cloud_set_device_data(_IN_ char *json_buffer)
{
    platform_mutex_lock(xSemDownCmd);
    ALINK_PARAM_CHECK(json_buffer == NULL);
    alink_event_send(ALINK_EVENT_SET_DEVICE_DATA);
    alink_err_t ret = ALINK_ERR;
    int size = strlen(json_buffer) + 1;
    char *q_data = (char *)malloc(size);
    if (size > ALINK_DATA_LEN) ALINK_LOGW("json_buffer len:%d", size);
    memcpy(q_data, json_buffer, size);

    if (xQueueSend(xQueueDownCmd, &q_data, 0) != pdTRUE) {
        ALINK_LOGW("xQueueSend xQueueDownCmd is err");
        ret = ALINK_ERR;
        alink_free(q_data);
    }
    platform_mutex_unlock(xSemDownCmd);
    return ret;
}

static void alink_post_data(void *arg)
{
    alink_err_t ret;
    char *up_cmd = NULL;
    for (; post_data_enable;) {
        ret = xQueueReceive(xQueueUpCmd, &up_cmd, portMAX_DELAY);
        if (ret != pdTRUE) {
            ALINK_LOGD("There is no data to report");
            continue;
        }
        ret = alink_report(ALINK_METHOD_POST, up_cmd);

        if (ret != ALINK_OK) {
            ALINK_LOGW("post failed!");
            platform_msleep(2000);
        } else {
            alink_event_send(ALINK_EVENT_POST_CLOUD_DATA);
        }
        alink_free(up_cmd);
    }
    vTaskDelete(NULL);
}

static void cloud_connected(void)
{
    alink_event_send(ALINK_EVENT_CLOUD_CONNECTED);
}

static void cloud_disconnected(void)
{
    alink_event_send(ALINK_EVENT_CLOUD_DISCONNECTED);
}

static alink_err_t alink_trans_init()
{
    alink_err_t ret = ALINK_OK;
    post_data_enable = ALINK_TRUE;
    xSemWrite        = platform_mutex_init();
    xSemRead         = platform_mutex_init();
    xSemDownCmd      = platform_mutex_init();
    xQueueUpCmd      = xQueueCreate(DOWN_CMD_QUEUE_NUM, sizeof(char *));
    xQueueDownCmd    = xQueueCreate(UP_CMD_QUEUE_NUM, sizeof(char *));
    alink_set_loglevel(ALINK_LL_DEBUG);

    alink_register_callback(ALINK_CLOUD_CONNECTED, &cloud_connected);
    alink_register_callback(ALINK_CLOUD_DISCONNECTED, &cloud_disconnected);
    alink_register_callback(ALINK_GET_DEVICE_DATA, &cloud_get_device_data);
    alink_register_callback(ALINK_SET_DEVICE_DATA, &cloud_set_device_data);

    ret = alink_start();
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_start :%d", ret);
    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    ret = alink_wait_connect(ALINK_WAIT_FOREVER);
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_start :%d", ret);
    xTaskCreate(alink_post_data, "alink_post_data", 1024 * 4, NULL, ALINK_DEFAULU_TASK_PRIOTY, NULL);
    return ret;
}

void alink_trans_destroy()
{
    post_data_enable = ALINK_FALSE;
    alink_end();
    platform_mutex_destroy(xSemWrite);
    platform_mutex_destroy(xSemRead);
    platform_mutex_destroy(xSemDownCmd);
    vQueueDelete(xQueueUpCmd);
    vQueueDelete(xQueueDownCmd);
    vQueueDelete(xQueueEvent);
}

extern void factory_reset(void* arg);
extern alink_err_t alink_connect_ap();
int esp_alink_init(_IN_ const void *product_info)
{
    alink_err_t ret = ALINK_OK;
    xTaskCreate(factory_reset, "factory_reset", 1024 * 4, NULL, 10, NULL);
    ret = product_set(product_info);
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "product_set :%d", ret);

    ret = alink_connect_ap();
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_connect_ap :%d", ret);
    ret = alink_trans_init(NULL);
    if (ret != ALINK_OK) alink_trans_destroy();
    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "alink_trans_init :%d", ret);
    return ret;
}

int esp_alink_event_init(_IN_ alink_event_cb_t cb)
{
    ALINK_PARAM_CHECK(cb == NULL);
    s_event_handler_cb = cb;
    if (xQueueEvent == NULL)
        xQueueEvent = xQueueCreate(EVENT_QUEUE_NUM, sizeof(alink_event_t));
    xTaskCreate(alink_event_loop_task, "alink_event_task",
                1024 * 2, NULL, ALINK_DEFAULU_TASK_PRIOTY, NULL);
    return ALINK_OK;
}

#if ALINK_PASSTHROUGH
#define RawDataHeader   "{\"rawData\":\""
#define RawDataTail     "\", \"length\":\"%d\"}"
int esp_alink_write(_IN_ void *up_cmd, size_t len, int micro_seconds)
{
    platform_mutex_lock(xSemWrite);
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(len == 0 || len > ALINK_DATA_LEN);

    int i = 0;
    alink_err_t ret = ALINK_OK;
    char *q_data = (char *)malloc(ALINK_DATA_LEN);

    int size = strlen(RawDataHeader);
    strncpy((char *)q_data, RawDataHeader, ALINK_DATA_LEN);
    for (i = 0; i < len; i++) {
        size += snprintf((char *)q_data + size,
                         ALINK_DATA_LEN - size, "%02X", ((uint8_t *)up_cmd)[i]);
    }

    size += snprintf((char *)q_data + size,
                     ALINK_DATA_LEN - size, RawDataTail, len * 2);

    ret = xQueueSend(xQueueUpCmd, &q_data, micro_seconds / portTICK_PERIOD_MS);
    if (ret == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueUpCmd, wait_time: %d", micro_seconds);
        alink_free(q_data);
    } else {
        ret = size;
    }

    platform_mutex_unlock(xSemWrite);
    return ret;
}

static alink_err_t raw_data_unserialize(char *json_buffer, uint8_t *raw_data, int *raw_data_len)
{
    int attr_len = 0, i = 0;
    char *attr_str = NULL;
    assert(json_buffer && raw_data && raw_data_len);

    attr_str = json_get_value_by_name(json_buffer, strlen(json_buffer),
                                      "rawData", &attr_len, NULL);

    if (!attr_str || !attr_len || attr_len > *raw_data_len * 2)
        return ALINK_ERR;

    int raw_data_tmp = 0;
    for (i = 0; i < attr_len; i += 2) {
        sscanf(&attr_str[i], "%02x", &raw_data_tmp);
        raw_data[i / 2] = raw_data_tmp;
    }
    *raw_data_len = attr_len / 2;

    return ALINK_OK;
}

int esp_alink_read(_OUT_ void *down_cmd, size_t size, int micro_seconds)
{
    platform_mutex_lock(xSemRead);
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);

    alink_err_t ret = ALINK_OK;
    char *q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, micro_seconds / portTICK_PERIOD_MS);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, micro_seconds);
        ret = ALINK_ERR;
        goto EXIT;
    }
    if (strlen(q_data) + 1 > ALINK_DATA_LEN) {
        ALINK_LOGW("read len > ALINK_DATA_LEN, len: %d", strlen(q_data) + 1);
        ret = ALINK_DATA_LEN;
        goto EXIT;
    }

    ret = raw_data_unserialize(q_data, (uint8_t *)down_cmd, (int *)&size);
    if (ret != ALINK_OK) {
        ALINK_LOGW("raw_data_unserialize, ret:%d", ret);
    } else {
        ret = size;
    }

EXIT:
    alink_free(q_data);
    platform_mutex_unlock(xSemRead);
    return ret;
}

#else

int esp_alink_write(_IN_ void *up_cmd, size_t len, int micro_seconds)
{
    platform_mutex_lock(xSemWrite);
    ALINK_PARAM_CHECK(up_cmd == NULL);
    ALINK_PARAM_CHECK(len == 0 || len > ALINK_DATA_LEN);

    alink_err_t ret = ALINK_OK;
    char *q_data = (char *)malloc(ALINK_DATA_LEN);
    memcpy(q_data, up_cmd, len);
    ret = xQueueSend(xQueueUpCmd, &q_data, micro_seconds / portTICK_PERIOD_MS);
    if (ret == pdFALSE) {
        ALINK_LOGW("xQueueSend xQueueUpCmd, wait_time: %d", micro_seconds);
        alink_free(q_data);
    } else {
        ret = len;
    }
    platform_mutex_unlock(xSemWrite);
    return ret;
}

int esp_alink_read(_OUT_ void *down_cmd, size_t size, int micro_seconds)
{
    platform_mutex_lock(xSemRead);
    ALINK_PARAM_CHECK(down_cmd == NULL);
    ALINK_PARAM_CHECK(size == 0 || size > ALINK_DATA_LEN);
    alink_err_t ret = ALINK_OK;

    char *q_data = NULL;
    ret = xQueueReceive(xQueueDownCmd, &q_data, micro_seconds / portTICK_PERIOD_MS);
    if (ret == pdFALSE) {
        ALINK_LOGE("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, micro_seconds);
        ret = ALINK_ERR;
        goto EXIT;
    }

    size = strlen(q_data) + 1;
    if (size > ALINK_DATA_LEN) {
        ALINK_LOGW("read len > ALINK_DATA_LEN, len: %d", size);
        size = ALINK_DATA_LEN;
        q_data[size - 1] = '\0';
    }
    memcpy(down_cmd, q_data, size);

EXIT:
    alink_free(q_data);
    platform_mutex_unlock(xSemRead);
    return ret;
}
#endif
