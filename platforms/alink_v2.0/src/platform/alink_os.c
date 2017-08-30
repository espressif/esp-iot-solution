#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/portmacro.h"
#include "esp_log.h"

#include "string.h"
#include "esp_system.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "platform.h"
#include "esp_alink.h"

#define US_PER_SECOND   1000000
#define PLATFORM_TABLE_CONTENT_CNT(table) (sizeof(table)/sizeof(table[0]))

static const char *TAG = "alink_os";

typedef struct task_name_handler_content {
    const char* task_name;
    void * handler;
} task_infor_t;

typedef enum {
    RESULT_ERROR = -1,
    RESULT_OK = 0,
} esp_platform_result;

task_infor_t task_infor[] = {
    {"wsf_receive_worker", NULL},
    {"wsf_send_worker", NULL},
    {"wsf_callback_worker", NULL},
    {"wsf_worker_thread", NULL},
    {"fota_thread", NULL},
    {"cota_thread", NULL},
    {"alcs_thread", NULL},
    {"alink_main_thread", NULL},
    {"send_worker", NULL},
    {"callback_thread", NULL},
    {"firmware_upgrade_pthread", NULL},
    {"work queue", NULL},
    {NULL, NULL}
};

// int platform_printf(const char *fmt, ...) __attribute__((alias("printf")));
void platform_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

/************************ memory manage ************************/
void *platform_malloc(_IN_ uint32_t size)
{
    void * c = malloc(size);
    ALINK_ERROR_CHECK(c == NULL, NULL, "malloc size : %d", size);
    return c;
}

void platform_free(_IN_ void *ptr)
{
    if (ptr == NULL) return;
    free(ptr);
    ptr = NULL;
}


/************************ mutex manage ************************/

void *platform_mutex_init(void)
{
    xSemaphoreHandle mux_sem = NULL;
    mux_sem = xSemaphoreCreateMutex();
    ALINK_ERROR_CHECK(mux_sem == NULL, NULL, "xSemaphoreCreateMutex");
    return mux_sem;
}

void platform_mutex_destroy(_IN_ void *mutex)
{
    ALINK_PARAM_CHECK(mutex == NULL);
    vSemaphoreDelete(mutex);
}

void platform_mutex_lock(_IN_ void *mutex)
{
    //if can not get the mux,it will wait all the time
    ALINK_PARAM_CHECK(mutex == NULL);
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void platform_mutex_unlock(_IN_ void *mutex)
{
    ALINK_PARAM_CHECK(mutex == NULL);
    xSemaphoreGive(mutex);
}


/************************ semaphore manage ************************/
void *platform_semaphore_init(void)
{
    xSemaphoreHandle count_handler = NULL;
    count_handler = xSemaphoreCreateCounting(255, 0);
    ALINK_ERROR_CHECK(count_handler == NULL, NULL, "xSemaphoreCreateCounting");
    return count_handler;
}

void platform_semaphore_destroy(_IN_ void *sem)
{
    ALINK_PARAM_CHECK(sem == NULL);
    vSemaphoreDelete(sem);
}

int platform_semaphore_wait(_IN_ void *sem, _IN_ uint32_t timeout_ms)
{
    ALINK_PARAM_CHECK(sem == NULL);
    //Take the Semaphore
    if (pdTRUE == xSemaphoreTake(sem, timeout_ms / portTICK_RATE_MS)) {
        return 0;
    }
    return -1;
}

void platform_semaphore_post(_IN_ void *sem)
{
    ALINK_PARAM_CHECK(sem == NULL);
    xSemaphoreGive(sem);
}

void platform_msleep(_IN_ uint32_t ms)
{
    vTaskDelay(ms / portTICK_RATE_MS);
}

uint32_t platform_get_time_ms(void)
{
    struct timeval tm;
    gettimeofday(&tm, NULL);
    return (tm.tv_sec * US_PER_SECOND + tm.tv_usec) / 1000;
}

int platform_thread_get_stack_size(_IN_ const char *thread_name)
{
    ALINK_PARAM_CHECK(thread_name == NULL);
    if (0 == strcmp(thread_name, "alink_main_thread")) {
        ALINK_LOGD("get alink_main_thread");
        return 0xc00;
    } else if (0 == strcmp(thread_name, "wsf_worker_thread")) {
        ALINK_LOGD("get wsf_worker_thread");
        return 0x2100;
    } else if (0 == strcmp(thread_name, "firmware_upgrade_pthread")) {
        ALINK_LOGD("get firmware_upgrade_pthread");
        return 0xc00;
    } else if (0 == strcmp(thread_name, "send_worker")) {
        ALINK_LOGD("get send_worker");
        return 0x800;
    } else if (0 == strcmp(thread_name, "callback_thread")) {
        ALINK_LOGD("get callback_thread");
        return 0x800;
    } else if (0 == strcmp(thread_name, "work queue")) {
        ALINK_LOGD("get work queue");
        return 0x800;
    }  else if (0 == strcmp(thread_name, "wsf_receive_worker")) {
        ALINK_LOGD("get wsf_receive_worker");
        return 0x800;
    } else {
        ALINK_LOGE("get othrer thread: %s", thread_name);
        return 0x800;
    }
    esp_restart();;
}

/************************ task ************************/

/*
    return -1: not found the name from the list
          !-1: found the pos in the list
*/
static int get_task_name_location(_IN_ const char * name)
{
    uint32_t i = 0;
    uint32_t len = 0;
    for (i = 0; task_infor[i].task_name != NULL; i++) {
        len = (strlen(task_infor[i].task_name) >= configMAX_TASK_NAME_LEN ? configMAX_TASK_NAME_LEN : strlen(task_infor[i].task_name));
        if (0 == memcmp(task_infor[i].task_name, name, len)) {
            return i;
        }
    }
    return ALINK_ERR;
}

static bool set_task_name_handler(uint32_t pos, _IN_ void * handler)
{
    ALINK_PARAM_CHECK(handler == NULL);
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
    ALINK_PARAM_CHECK(name == NULL);
    ALINK_PARAM_CHECK(stack_size == 0);
    alink_err_t ret;

    // if (pdTRUE == xTaskCreatePinnedToCore((TaskFunction_t)start_routine, name, stack_size * 2, arg, DEFAULU_TASK_PRIOTY, thread, 0)) {
    ALINK_LOGD("thread_create name: %s, stack_size: %d, priority:%d",
               name, stack_size * 2, ALINK_DEFAULU_TASK_PRIOTY);
    ret = xTaskCreate((TaskFunction_t)start_routine, name, stack_size * 2, arg, ALINK_DEFAULU_TASK_PRIOTY, thread);
    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "thread_create name: %s, stack_size: %d, ret: %d", name, stack_size * 2, ret);

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
int platform_config_read(_OUT_ char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(buffer == NULL);
    ALINK_PARAM_CHECK(length < 0);

    alink_err_t ret     = -1;
    nvs_handle config_handle = 0;
    memset(buffer, 0, length);

    ret = nvs_open("ALINK", NVS_READWRITE, &config_handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_open ret:%x", ret);
    ret = nvs_get_blob(config_handle, "os_config", buffer, (size_t *)&length);
    nvs_close(config_handle);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ALINK_LOGD("nvs_get_blob ret:%x,No data storage,the read data is empty", ret);
        return ALINK_ERR;
    }
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_get_blob ret:%x", ret);
    ALINK_LOGD("platform_config_read: %02x %02x %02x length: %d",
               buffer[0], buffer[1], buffer[2], length);
    return 0;
}

int platform_config_write(_IN_ const char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(buffer == NULL);
    ALINK_PARAM_CHECK(length < 0);
    ALINK_LOGD("platform_config_write: %02x %02x %02x length: %d",
               buffer[0], buffer[1], buffer[2], length);

    alink_err_t ret     = -1;
    nvs_handle config_handle = 0;
    ret = nvs_open("ALINK", NVS_READWRITE, &config_handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_open ret:%x", ret);
    ret = nvs_set_blob(config_handle, "os_config", buffer, length);
    nvs_commit(config_handle);
    nvs_close(config_handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_set_blob ret:%x", ret);
    return 0;
}


char *platform_get_chipid(_OUT_ char cid_str[PLATFORM_CID_LEN])
{
    ALINK_PARAM_CHECK(cid_str == NULL);
    memcpy(cid_str, ALINK_CHIPID, PLATFORM_CID_LEN);
    return cid_str;
}

char *platform_get_os_version(_OUT_ char version_str[PLATFORM_OS_VERSION_LEN])
{
    ALINK_PARAM_CHECK(version_str == NULL);
    const char *idf_version = esp_get_idf_version();
    memcpy(version_str, idf_version, PLATFORM_OS_VERSION_LEN);
    return version_str;
}

char *platform_get_module_name(_OUT_ char name_str[PLATFORM_MODULE_NAME_LEN])
{
    ALINK_PARAM_CHECK(name_str == NULL);
    memcpy(name_str, ALINK_MODULE_NAME, PLATFORM_MODULE_NAME_LEN);
    return name_str;
}

void platform_sys_reboot(void)
{
    esp_restart();
}


/**
 * @brief release the specified thread resource.
 *
 * @param[in] thread: the specified thread handle.
 * @return None.
 * @see None.
 * @note Called outside of the thread. The resource that must be kept until thread exit completely
 *  can be released here, such as thread stack.
 */
void platform_thread_release_resources(void *thread)
{

}
