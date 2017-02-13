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
*	|	The file includes extern functions in basic_func_iot.								|
*	|																						|
*	----------------------------------------------------------------------------------------
*/

#ifndef __BASIC_FUNC_IOT__
#define __BASIC_FUNC_IOT__

#include <string.h>
#include "esp_system.h"
#include "user_config.h"
#include "esp_log.h"


/**
 * @brief For all the functions bellow 
 *
 * @return
 */
#define err_assert(param)      \
	if((param) != ESP_OK){     \
		IOT_LOG("ERROR!\n");   \
		return ESP_FAIL;       \
	}
	
#define pointer_assert(tag, param)	\
	if((param) == NULL){		\
		ESP_LOGE(tag, "%s:%d (%s) - the point is null\n", __FILE__, __LINE__, __FUNCTION__);	\
		return ESP_FAIL;	\
	}

#define res_assert(tag, res, ret) \
        if((res) == pdFALSE) { \
            ESP_LOGE(tag, "%s:%d (%s)Res pdFALSE",__FILE__, __LINE__, __FUNCTION__); \
            return ret; \
        }

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }


#endif /* __IOT_WIFI__ */
