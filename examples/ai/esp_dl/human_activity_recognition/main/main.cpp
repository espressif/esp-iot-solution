/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <iostream>
#include <iomanip>
#include "esp_log.h"
#include "test_data.h"
#include "dl_model_base.hpp"
#include "human_activity_recognition.h"

static const char *TAG = "HAR";

extern "C" void app_main(void)
{
    HumanActivityRecognition har("model", mean_array, std_array, 561);

    for (size_t i = 0; i < sizeof(test_inputs) / sizeof(test_inputs[0]); ++i) {
        ESP_LOGI(TAG, "Test case %d: Predict result: %s", i, index_to_category[har.predict(test_inputs[i])].c_str());
    }
}
