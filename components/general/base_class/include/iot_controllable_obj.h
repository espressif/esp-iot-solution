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
#ifndef _IOT_CONTROLLABLE_OBJ_H_
#define _IOT_CONTROLLABLE_OBJ_H_
#include "esp_err.h"

#ifdef __cplusplus
class CControllable
{
public:
    virtual esp_err_t on() = 0;
    virtual esp_err_t off() = 0;
    virtual ~CControllable() = 0;
};
#endif

#endif
