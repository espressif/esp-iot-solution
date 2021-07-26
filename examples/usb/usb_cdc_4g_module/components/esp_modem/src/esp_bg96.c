// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_modem_internal.h"

static const char *TAG = "bg96";

esp_err_t esp_modem_bg96_specific_init(esp_modem_dce_t *dce)
{
    ESP_MODEM_ERR_CHECK(dce, "failed to specific init with zero dce", err_params);
    // BG96 specifics is the same as the default DCE, as of now
    return ESP_OK;
err_params:
    return ESP_ERR_INVALID_ARG;
}
