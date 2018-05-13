// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifdef __cplusplus
extern "C" {
#endif
#include <esp_types.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/pcnt.h"
#include "soc/pcnt_struct.h"
#include "esp_intr_alloc.h"

static const char* ISRM_TAG = "ISRM";
#define ISR_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(ISRM_TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }

typedef struct {
    intr_handler_t fn;   /*!< isr function */
    void* args;          /*!< isr function args */
} isr_func_t;

static portMUX_TYPE hw_res_spinlock = portMUX_INITIALIZER_UNLOCKED;
#define portHwENTER_CRITICAL() portENTER_CRITICAL(&hw_res_spinlock);
#define portHwEXIT_CRITICAL()  portEXIT_CRITICAL(&hw_res_spinlock);


#ifdef __cplusplus
}
#endif

