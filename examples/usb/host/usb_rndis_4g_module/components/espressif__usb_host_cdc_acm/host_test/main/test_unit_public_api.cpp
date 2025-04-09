
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <catch2/catch_test_macros.hpp>

#include "usb/cdc_acm_host.h"

extern "C" {
#include "Mockusb_host.h"
#include "Mockqueue.h"
#include "Mocktask.h"
#include "Mockidf_additions.h"
#include "Mockportmacro.h"
#include "Mockevent_groups.h"
}

SCENARIO("CDC-ACM Host install")
{
    // CDC-ACM Host driver config set to nullptr
    GIVEN("NO CDC-ACM Host driver config, driver not installed") {
        TaskHandle_t task_handle;
        int sem;
        int event_group;

        // Call cdc_acm_host_install with cdc_acm_host_driver_config set to nullptr, fail to create EventGroup
        SECTION("Fail to create EventGroup") {
            // Create an EventGroup, return nullptr, so the EventGroup is not created successfully
            xEventGroupCreate_ExpectAndReturn(nullptr);
            // We should be calling xSemaphoreCreteMutex_ExpectAnyArgsAndRetrun instead of xQueueCreateMutex_ExpectAnyArgsAndReturn
            // Because of missing Freertos Mocks
            // Create a semaphore, return the semaphore handle (Queue Handle in this scenario), so the semaphore is created successfully
            xQueueCreateMutex_ExpectAnyArgsAndReturn(reinterpret_cast<QueueHandle_t>(&sem));
            // Create a task, return pdTRUE, so the task is created successfully
            xTaskCreatePinnedToCore_ExpectAnyArgsAndReturn(pdTRUE);
            // Return task handle by pointer
            xTaskCreatePinnedToCore_ReturnThruPtr_pxCreatedTask(&task_handle);

            // goto err: (xEventGroupCreate returned nullptr), delete the queue and the task
            vQueueDelete_Expect(reinterpret_cast<QueueHandle_t>(&sem));
            vTaskDelete_Expect(task_handle);

            // Call the DUT function, expect ESP_ERR_NO_MEM
            REQUIRE(ESP_ERR_NO_MEM == cdc_acm_host_install(nullptr));
        }

        // Call cdc_acm_host_install, expect to successfully install the CDC ACM host
        SECTION("Successfully install CDC ACM Host") {
            // Create an EventGroup, return event group handle, so the EventGroup is created successfully
            xEventGroupCreate_ExpectAndReturn(reinterpret_cast<EventGroupHandle_t>(&event_group));
            // We should be calling xSemaphoreCreteMutex_ExpectAnyArgsAndRetrun instead of xQueueCreateMutex_ExpectAnyArgsAndReturn
            // Because of missing Freertos Mocks
            // Create a semaphore, return the semaphore handle (Queue Handle in this scenario), so the semaphore is created successfully
            xQueueCreateMutex_ExpectAnyArgsAndReturn(reinterpret_cast<QueueHandle_t>(&sem));
            // Create a task, return pdTRUE, so the task is created successfully
            vPortEnterCritical_Expect();
            vPortExitCritical_Expect();
            xTaskCreatePinnedToCore_ExpectAnyArgsAndReturn(pdTRUE);
            // Return task handle by pointer
            xTaskCreatePinnedToCore_ReturnThruPtr_pxCreatedTask(&task_handle);

            // Call mocked function from USB Host
            // return ESP_OK, so the client si registered successfully
            usb_host_client_register_ExpectAnyArgsAndReturn(ESP_OK);

            // Resume the task
            xTaskGenericNotify_ExpectAnyArgsAndReturn(pdPASS);

            // Call the DUT Function, expect ESP_OK
            REQUIRE(ESP_OK == cdc_acm_host_install(nullptr));
        }
    }
}
