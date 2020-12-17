// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _SENSORS_EVENT_H_
#define _SENSORS_EVENT_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create sensors event loop
 *
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_NO_MEM: Cannot allocate memory for event loops list
 *  - ESP_FAIL: Failed to create task loop
 *  - Others: Fail
 */
esp_err_t sensors_event_loop_create(void);

/**
 * @brief Delete the sensors event loop
 *
 * @return
 *  - ESP_OK: Success
 *  - Others: Fail
 */
esp_err_t sensors_event_loop_delete(void);

/**
 * @brief Register an instance of event handler to the sensors loop.
 *
 * @param[in] event_base the base id of the event to register the handler for
 * @param[in] event_id the id of the event to register the handler for
 * @param[in] event_handler the handler function which gets called when the event is dispatched
 * @param[in] event_handler_arg data, aside from event data, that is passed to the handler when it is called
 * @param[out] instance An event handler instance object related to the registered event handler and data, can be NULL.
 *             This needs to be kept if the specific callback instance should be unregistered before deleting the whole
 *             event loop. Registering the same event handler multiple times is possible and yields distinct instance
 *             objects. The data can be the same for all registrations.
 *             If no unregistration is needed but the handler should be deleted when the event loop is deleted,
 *             instance can be NULL.
 *
 * @note the event loop library does not maintain a copy of event_handler_arg, therefore the user should
 * ensure that event_handler_arg still points to a valid location by the time the handler gets called
 *
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_NO_MEM: Cannot allocate memory for the handler
 *  - ESP_ERR_INVALID_ARG: Invalid combination of event base and event id or instance is NULL
 *  - Others: Fail
 */
esp_err_t sensors_event_handler_instance_register(esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_t event_handler,
        void *event_handler_arg,
        esp_event_handler_instance_t *context);

/**
 * @brief Unregister a handler from the sensors event loop.
 *
 * @param[in] event_base the base of the event with which to unregister the handler
 * @param[in] event_id the id of the event with which to unregister the handler
 * @param[in] instance the instance object of the registration to be unregistered
 *
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_INVALID_ARG: Invalid combination of event base and event id
 *  - Others: Fail
 */
esp_err_t sensors_event_handler_instance_unregister(esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_instance_t context);

/**
 * @brief Posts an event to the sensors event loop. The event loop library keeps a copy of event_data and manages
 * the copy's lifetime automatically (allocation + deletion); this ensures that the data the
 * handler recieves is always valid.
 *
 * @param[in] event_base the event base that identifies the event
 * @param[in] event_id the event id that identifies the event
 * @param[in] event_data the data, specific to the event occurence, that gets passed to the handler
 * @param[in] event_data_size the size of the event data
 * @param[in] ticks_to_wait number of ticks to block on a full event queue
 *
 * @return
 *  - ESP_OK: Success
 *  - ESP_ERR_TIMEOUT: Time to wait for event queue to unblock expired,
 *                      queue full when posting from ISR
 *  - ESP_ERR_INVALID_ARG: Invalid combination of event base and event id
 *  - Others: Fail
 */
esp_err_t sensors_event_post(esp_event_base_t event_base, int32_t event_id,
                             void *event_data, size_t event_data_size, TickType_t ticks_to_wait);

#if CONFIG_ESP_EVENT_POST_FROM_ISR
/**
 * @brief Special variant of sensors_event_post for posting events from interrupt handlers.
 *
 * @param[in] event_base the event base that identifies the event
 * @param[in] event_id the event id that identifies the event
 * @param[in] event_data the data, specific to the event occurence, that gets passed to the handler
 * @param[in] event_data_size the size of the event data; max is 4 bytes
 * @param[out] task_unblocked an optional parameter (can be NULL) which indicates that an event task with
 *                            higher priority than currently running task has been unblocked by the posted event;
 *                            a context switch should be requested before the interrupt is existed.
 *
 * @note this function is only available when CONFIG_ESP_EVENT_POST_FROM_ISR is enabled
 * @note when this function is called from an interrupt handler placed in IRAM, this function should
 *       be placed in IRAM as well by enabling CONFIG_ESP_EVENT_POST_FROM_IRAM_ISR
 *
 * @return
 *  - ESP_OK: Success
 *  - ESP_FAIL: Event queue for the default event loop full
 *  - ESP_ERR_INVALID_ARG: Invalid combination of event base and event id,
 *                          data size of more than 4 bytes
 *  - Others: Fail
 */
esp_err_t sensors_event_isr_post(esp_event_base_t event_base, int32_t event_id,
                                 void *event_data, size_t event_data_size, BaseType_t *task_unblocked);
#endif

#ifdef __cplusplus
}
#endif

#endif