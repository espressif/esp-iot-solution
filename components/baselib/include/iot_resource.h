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

#ifndef _IOT_HW_RESOURCE_H_
#define _IOT_HW_RESOURCE_H_
#include "esp_err.h"
#include "esp_log.h"
#include "driver/pcnt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Install the driver's PCNT ISR handler service, which allows per-unit PCNT interrupt handlers.
  *
  * This function is incompatible with pcnt_isr_register() - if that function is used, a single global ISR is registered for all PCNT interrupts.
  * If this function is used, the ISR service provides a global PCNT ISR and individual pin handlers are registered via the pcnt_isr_handler_add() function.
  *
  * @param intr_alloc_flags Flags used to allocate the interrupt. One or multiple (ORred)
  *            ESP_INTR_FLAG_* values. See esp_intr_alloc.h for more info.
  *
  * @return
  *     - ESP_OK Success
  *     - ESP_FAIL Operation fail
  *     - ESP_ERR_NO_MEM No memory to install this service
  */
esp_err_t pcnt_install_isr_service(int intr_alloc_flags);

/**
  * @brief Uninstall the driver's PCNT ISR service, freeing related resources.
  */
void pcnt_uninstall_isr_service();

/**
  * @brief Add ISR handler for the corresponding PCNT unit.
  *
  * Call this function after using pcnt_install_isr_service() to
  * install the driver's PCNT ISR handler service.
  *
  * The pin ISR handlers no longer need to be declared with IRAM_ATTR,
  * unless you pass the ESP_INTR_FLAG_IRAM flag when allocating the
  * ISR in pcnt_install_isr_service().
  *
  * This ISR handler will be called from an ISR. So there is a stack
  * size limit (configurable as "ISR stack size" in menuconfig). This
  * limit is smaller compared to a global PCNT interrupt handler due
  * to the additional level of indirection.
  *
  * @param unit PCNT unit
  * @param isr_handler ISR handler function for the corresponding PCNT unit.
  * @param args parameter for ISR handler.
  *
  * @return
  *     - ESP_OK Success
  *     - ESP_ERR_INVALID_STATE Wrong state, the ISR service has not been initialized.
  *     - ESP_ERR_INVALID_ARG Parameter error
  */
esp_err_t pcnt_isr_handler_add(pcnt_unit_t unit, gpio_isr_t isr_handler, void* args);

/**
  * @brief Remove ISR handler for the corresponding PCNT unit.
  *
  * @param unit PCNT unit
  *
  * @return
  *     - ESP_OK Success
  *     - ESP_ERR_INVALID_STATE Wrong state, the ISR service has not been initialized.
  *     - ESP_ERR_INVALID_ARG Parameter error
  */
esp_err_t pcnt_isr_handler_remove(pcnt_unit_t unit);


#ifdef __cplusplus
}
#endif


#endif /* _IOT_HW_RESOURCE_H_ */
