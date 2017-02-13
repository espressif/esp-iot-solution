/* 
*
* Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

/*	--------------------------------------------------------------------------------------- 
*	|																						|
*	|	The file includes every single parameter to configure the whole project.			|
*	|																						|
*	----------------------------------------------------------------------------------------
*/


#ifndef __USER_CONFIG__
#define __USER_CONFIG__

#include <string.h>
#include <stdio.h>
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "basic_func_iot.h"


/* LOG setting */
#define IOT_LOG(fmt, ...)   printf("%s[%d]"fmt, __func__, __LINE__, ##__VA_ARGS__)


/* Wi-Fi setting */
#define EXAMPLE_WIFI_SSID "IOT_DEMO_TEST" 
#define EXAMPLE_WIFI_PASS "123456789"
//#define EXAMPLE_WIFI_SSID "SMART_LIGHT"
//#define EXAMPLE_WIFI_PASS "qwertyuiop"

/* Server info */
#define WEB_SERVER "iot.espressif.com"
#define WEB_PORT "80"
#define WEB_URL "http://iot.espressif.com/"




#endif


