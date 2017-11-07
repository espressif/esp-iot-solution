#ifndef __ESP_ALINK_H__
#define __ESP_ALINK_H__
#include "esp_log.h"
#include "alink_export.h"
#include "platform.h"
#include "assert.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "json_parser.h"
#include "alink_config.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t alink_err_t;
#ifndef ALINK_TRUE
#define ALINK_TRUE  1
#endif
#ifndef ALINK_FALSE
#define ALINK_FALSE 0
#endif
#ifndef ALINK_OK
#define ALINK_OK    0
#endif
#ifndef ALINK_ERR
#define ALINK_ERR   -1
#endif

#ifndef _IN_
#define _IN_            /**< indicate that this is a input parameter. */
#endif
#ifndef _OUT_
#define _OUT_           /**< indicate that this is a output parameter. */
#endif
#ifndef _INOUT_
#define _INOUT_         /**< indicate that this is a io parameter. */
#endif
#ifndef _IN_OPT_
#define _IN_OPT_        /**< indicate that this is a optional input parameter. */
#endif
#ifndef _OUT_OPT_
#define _OUT_OPT_       /**< indicate that this is a optional output parameter. */
#endif
#ifndef _INOUT_OPT_
#define _INOUT_OPT_     /**< indicate that this is a optional io parameter. */
#endif

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ALINK_LOG_LEVEL

#define ALINK_LOGE( format, ... ) ESP_LOGE(TAG, "[%s, %d]:" format, __func__, __LINE__, ##__VA_ARGS__)
#define ALINK_LOGW( format, ... ) ESP_LOGW(TAG, "[%s, %d]:" format, __func__, __LINE__, ##__VA_ARGS__)
#define ALINK_LOGI( format, ... ) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#define ALINK_LOGD( format, ... ) ESP_LOGD(TAG, "[%s, %d]:" format, __func__, __LINE__, ##__VA_ARGS__)
#define ALINK_LOGV( format, ... ) ESP_LOGV(TAG, format, ##__VA_ARGS__)

#define ALINK_ERROR_CHECK(con, err, format, ...) if(con) {ALINK_LOGE(format, ##__VA_ARGS__); perror(__func__); return err;}
#define ALINK_PARAM_CHECK(con) if(con) {ALINK_LOGE("Parameter error, "); perror(__func__); assert(0);}

/* alink_os */
#define ALINK_CHIPID "esp32"

typedef enum {
    ALINK_EVENT_CLOUD_CONNECTED = 0,
    ALINK_EVENT_CLOUD_DISCONNECTED,
    ALINK_EVENT_GET_DEVICE_DATA,
    ALINK_EVENT_SET_DEVICE_DATA,
    ALINK_EVENT_POST_CLOUD_DATA,
} alink_event_t;
typedef alink_err_t (*alink_event_cb_t)(alink_event_t event);
int esp_alink_event_init(_IN_ alink_event_cb_t cb);
int esp_alink_init(_IN_ const void *product_info);
int esp_alink_write(_IN_ void *up_cmd, size_t size, int micro_seconds);
int esp_alink_read(_OUT_ void *down_cmd, size_t size, int  micro_seconds);


#ifdef __cplusplus
}
#endif

#endif
