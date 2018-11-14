/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lwip/sockets.h"

#include "alink_platform.h"
#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_info_store.h"

static const char *TAG = "alink_os";

typedef struct task_name_handler_content {
    const char *task_name;
    void *handler;
} task_infor_t;

static task_infor_t task_infor[] = {
    {"wsf_receive_worker", NULL},
    {"alcs_thread", NULL},
    {"work queue", NULL},
    {NULL, NULL}
};

void platform_printf(const char *fmt, ...)
{
    /* Clear sniffer process information */
    if (!strncmp(fmt, "[%d] ssid:%s, mac:", 18) || !strncmp(fmt, "channel %d", 10)) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}


/************************ memory manage ************************/
void *platform_malloc(_IN_ uint32_t size)
{
    return alink_malloc(size);
}

void platform_free(_IN_ void *ptr)
{
    alink_free(ptr);
}


/************************ mutex manage ************************/
void *platform_mutex_init(void)
{
    return xSemaphoreCreateMutex();
}

void platform_mutex_destroy(_IN_ void *mutex)
{
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

void platform_mutex_lock(_IN_ void *mutex)
{
    if (mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

void platform_mutex_unlock(_IN_ void *mutex)
{
    if (mutex) {
        xSemaphoreGive(mutex);
    }
}


/************************ semaphore manage ************************/
void *platform_semaphore_init(void)
{
    return xSemaphoreCreateCounting(255, 0);
}

void platform_semaphore_destroy(_IN_ void *sem)
{
    if (sem) {
        vSemaphoreDelete(sem);
    }
}

int platform_semaphore_wait(_IN_ void *sem, _IN_ uint32_t timeout_ms)
{
    ALINK_PARAM_CHECK(sem);

    return xSemaphoreTake(sem, timeout_ms / portTICK_RATE_MS) ? ALINK_OK : ALINK_ERR;
}

void platform_semaphore_post(_IN_ void *sem)
{
    if (sem) {
        xSemaphoreGive(sem);
    }
}

int platform_thread_get_stack_size(_IN_ const char *thread_name)
{
    ALINK_PARAM_CHECK(thread_name);

    if (0 == strcmp(thread_name, "work queue")) {
        ALINK_LOGD("get work queue");
        return 0xa00;
    }  else if (0 == strcmp(thread_name, "wsf_receive_worker")) {
        ALINK_LOGD("get wsf_receive_worker");
        return 0xa00;
    }  else if (0 == strcmp(thread_name, "alcs_thread")) {
        ALINK_LOGD("get alcs_thread");
        return 0xa00;
    } else {
        ALINK_LOGE("get othrer thread: %s", thread_name);
        return 0x800;
    }
}


/************************ task ************************/
static int get_task_name_location(_IN_ const char *name)
{
    uint32_t i = 0;
    uint32_t len = 0;

    for (i = 0; task_infor[i].task_name != NULL; i++) {
        len = (strlen(task_infor[i].task_name) >= configMAX_TASK_NAME_LEN ? configMAX_TASK_NAME_LEN : strlen(task_infor[i].task_name));

        if (!memcmp(task_infor[i].task_name, name, len)) {
            return i;
        }
    }

    return ALINK_ERR;
}

static bool set_task_name_handler(uint32_t pos, _IN_ void *handler)
{
    ALINK_PARAM_CHECK(handler);

    task_infor[pos].handler = handler;
    return ALINK_OK;
}

int platform_thread_create(_OUT_ void **thread,
                           _IN_ const char *name,
                           _IN_ void *(*start_routine)(void *),
                           _IN_ void *arg,
                           _IN_ void *stack,
                           _IN_ uint32_t stack_size,
                           _OUT_ int *stack_used)
{
    ALINK_PARAM_CHECK(name);
    ALINK_PARAM_CHECK(stack_size);

    alink_err_t ret;

    uint8_t task_priority = DEFAULU_TASK_PRIOTY;

    if (!strcmp(name, "work queue")) {
        task_priority++;
    }

    ret = xTaskCreate((TaskFunction_t)start_routine, name, (stack_size) * 2, arg, task_priority, thread);
    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "thread_create name: %s, stack_size: %d, ret: %d", name, stack_size, ret);
    ALINK_LOGD("thread_create name: %s, stack_size: %d, priority:%d, thread_handle: %p",
               name, stack_size * 2, task_priority, *thread);

    int pos = get_task_name_location(name);

    if (pos == ALINK_ERR) {
        ALINK_LOGE("get_task_name_location name: %s", name);
        vTaskDelete(*thread);
    }

    set_task_name_handler(pos, *thread);
    return ALINK_OK;
}

void platform_thread_exit(_IN_ void *thread)
{
    vTaskDelete(thread);
}


/************************ config ************************/
#define ALINK_CONFIG_KEY "alink_config"
int platform_config_read(_OUT_ char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(buffer);
    ALINK_PARAM_CHECK(length > 0);
    ALINK_LOGV("buffer: %p, length: %d", buffer, length);

    alink_err_t ret = esp_info_load(ALINK_CONFIG_KEY, buffer, length);

    return ret > 0 ? ALINK_OK : ALINK_ERR;
}

int platform_config_write(_IN_ const char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(buffer);
    ALINK_PARAM_CHECK(length > 0);
    ALINK_LOGV("buffer: %p, length: %d", buffer, length);

    alink_err_t ret = esp_info_save(ALINK_CONFIG_KEY, buffer, length);

    return ret > 0 ? ALINK_OK : ALINK_ERR;
}


/*********************** system info ***********************/
char *platform_get_chipid(_OUT_ char cid_str[PLATFORM_CID_LEN])
{
    return strncpy(cid_str, ALINK_CHIPID, PLATFORM_CID_LEN);
}

char *platform_get_os_version(_OUT_ char version_str[STR_SHORT_LEN])
{
    return strncpy(version_str, esp_get_idf_version(), STR_SHORT_LEN);
}

char *platform_get_module_name(_OUT_ char name_str[STR_SHORT_LEN])
{
    return strncpy(name_str, ALINK_MODULE_NAME, STR_SHORT_LEN);
}

void platform_sys_reboot(void)
{
    ALINK_LOGW("platform_sys_reboot");
    esp_restart();
}

void platform_msleep(_IN_ uint32_t ms)
{
    vTaskDelay(ms / portTICK_RATE_MS);
}

uint32_t platform_get_time_ms(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec * 1000000 + now.tv_usec) / 1000;
}
