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

// pcnt interrupt service
static isr_func_t* pcnt_isr_func = NULL;
static intr_handler_t pcnt_isr_handle;
void IRAM_ATTR pcnt_intr_service(void* arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    for (int unit = 0; unit < PCNT_UNIT_MAX; unit++) {
        if (intr_status & (BIT(unit))) {
            if (pcnt_isr_func[unit].fn != NULL) {
                pcnt_isr_func[unit].fn(pcnt_isr_func[unit].args);
            }
            PCNT.int_clr.val = BIT(unit);
        }
    }
}

esp_err_t pcnt_isr_handler_add(pcnt_unit_t unit, gpio_isr_t isr_handler, void* args)
{
    ISR_CHECK(pcnt_isr_func != NULL, "ISR service is not installed, call xxx_install_isr_service() first", ESP_ERR_INVALID_STATE);
    ISR_CHECK(unit < PCNT_UNIT_MAX, "PCNT unit error", ESP_ERR_INVALID_ARG);
    portHwENTER_CRITICAL();
    pcnt_intr_disable(unit);
    if (pcnt_isr_func) {
        pcnt_isr_func[unit].fn = isr_handler;
        pcnt_isr_func[unit].args = args;
    }
    pcnt_intr_enable(unit);
    portHwEXIT_CRITICAL();
    return ESP_OK;
}

esp_err_t pcnt_isr_handler_remove(pcnt_unit_t unit)
{
    ISR_CHECK(pcnt_isr_func != NULL, "ISR service is not installed, call xxx_install_isr_service() first", ESP_ERR_INVALID_STATE);
    ISR_CHECK(unit < PCNT_UNIT_MAX, "PCNT unit error", ESP_ERR_INVALID_ARG);
    portHwENTER_CRITICAL();
    pcnt_intr_disable(unit);
    if (pcnt_isr_func) {
        pcnt_isr_func[unit].fn = NULL;
        pcnt_isr_func[unit].args = NULL;
    }
    portHwEXIT_CRITICAL();
    return ESP_OK;
}

esp_err_t pcnt_install_isr_service(int intr_alloc_flags)
{
    if (pcnt_isr_func != NULL) {
        ESP_LOGI(ISRM_TAG, "ISR service already installed.");
        return ESP_OK;
    }
    esp_err_t ret;
    portHwENTER_CRITICAL();
    pcnt_isr_func = (isr_func_t*) calloc(PCNT_UNIT_MAX, sizeof(isr_func_t));
    if (pcnt_isr_func == NULL) {
        ret = ESP_ERR_NO_MEM;
    } else {
        ret = pcnt_isr_register(pcnt_intr_service, NULL, intr_alloc_flags, &pcnt_isr_handle);
    }
    portHwEXIT_CRITICAL();
    return ret;
}

void pcnt_uninstall_isr_service()
{
    if (pcnt_isr_func == NULL) {
        return;
    }
    portHwENTER_CRITICAL();
    esp_intr_free(pcnt_isr_handle);
    free(pcnt_isr_func);
    pcnt_isr_func = NULL;
    portHwEXIT_CRITICAL();
    return;
}

#ifdef __cplusplus
}
#endif

