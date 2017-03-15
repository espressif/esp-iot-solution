/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include "esp_system.h"
#include "virtual_device.h"
#include "cloud.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "button.h"
#ifdef CLOUD_ALINK
#include "product.h"
#include "esp_alink.h"
#endif
#ifdef CLOUD_JOYLINK
#include "jd_innet.h"
#include "esp_joylink.h"
#endif

typedef esp_err_t (*cloud_event_cb_t)(cloud_event_t);
static const char *TAG = "cloud";

#ifdef CLOUD_ALINK
const char* device_attr[5] = { "OnOff_Power", "Color_Temperature", "Light_Brightness",
                                "TimeDelay_PowerOff", "WorkMode_MasterLight"
                              };

const char *main_dev_params =
    "{\"OnOff_Power\": { \"value\": \"%d\" }, \"Color_Temperature\": { \"value\": \"%d\" }, \"Light_Brightness\": { \"value\": \"%d\" }, \"TimeDelay_PowerOff\": { \"value\": \"%d\"}, \"WorkMode_MasterLight\": { \"value\": \"%d\"}}";
#endif

#ifdef CLOUD_JOYLINK
static char *device_attr[5] = { "power", "brightness", "colortemp", "mode", "color"};
/*
 * brightness  |  int     |  0 ~ 100
 * color       |  string  |  
 * colortemp   |  int     |  3000 ~ 6500
 * mode        |  int     |  0/1/2/3/4
 * power       |  int     |  0/1
*/
const char *joylink_json_upload_str = \
"{\
\"code\":%d,\
\"data\":[\
{\"current_value\": \"%d\",\"stream_id\": \"power\"},\
{\"current_value\": \"%d\",\"stream_id\": \"brightness\"},\
{\"current_value\": \"%d\",\"stream_id\": \"colortemp\"},\
{\"current_value\": \"%d\",\"stream_id\": \"mode\"},\
{\"current_value\": \"%s\",\"stream_id\": \"color\"}\
]}";
#endif

#ifdef CLOUD_JOYLINK
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        // esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    //ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_loop_init(NULL, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void initialise_key(void)
{
    button_handle_t btn_handle = button_dev_init(CONFIG_JOYLINK_SMNT_BUTTON_NUM, 0, BUTTON_ACTIVE_LOW);
    button_dev_add_tap_cb(BUTTON_PUSH_CB, joylink_button_smnt_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS, btn_handle);

    btn_handle = button_dev_init(CONFIG_JOYLINK_RESET_BUTTON_NUM, 0, BUTTON_ACTIVE_LOW);
    button_dev_add_tap_cb(BUTTON_PUSH_CB, joylink_button_reset_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS, btn_handle);
}
#endif


esp_err_t cloud_init(cloud_event_cb_t cb)
{
#ifdef CLOUD_ALINK
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    alink_product_t product_info = {
        .sn             = "12345678",
        .name           = "ALINKTEST",
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_SMARTLED",
        .key            = "ljB6vqoLzmP8fGkE6pon",
        .secret         = "YJJZjytOCXDhtQqip4EjWbhR95zTgI92RVjzjyZF",
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
        // .type           = "LIGHT",
        // .category       = "LIVING",
        // .manufacturer   = "ALINKTEST",
        // .cid            = "2D0044000F47333139373038",
    };

    ALINK_LOGI("*********************************");
    ALINK_LOGI("*         PRODUCT INFO          *");
    ALINK_LOGI("*********************************");
    ALINK_LOGI("name   : %s", product_info.name);
    ALINK_LOGI("type   : %s", product_info.type);
    ALINK_LOGI("version: %s", product_info.version);
    ALINK_LOGI("model  : %s", product_info.model);
    ESP_ERROR_CHECK(esp_alink_event_init(cb));
    esp_alink_init(&product_info);
#endif

#ifdef CLOUD_JOYLINK
    initialise_wifi();
    initialise_key();
    printf("mode: json, free_heap: %u\n", esp_get_free_heap_size());
    joylink_info_t product_info = {
			.innet_aes_key = "JBJS5PB8CN47I8FB",
		    .jlp.version = 1,
		    .jlp.accesskey = "",//NDJY9396Z9P4KNDU
		    .jlp.localkey = "",
		    .jlp.feedid = "",
		    .jlp.devtype = E_JLDEV_TYPE_NORMAL,
		    .jlp.joylink_server = "live.smart.jd.com",
		    .jlp.server_port = 2002, 
		    .jlp.CID= "011c022b",
		    .jlp.firmwareVersion = "001",
		    .jlp.modelCode = "a1",
		    .jlp.uuid = "PAAXAS",
		    .jlp.lancon = E_LAN_CTRL_ENABLE,
		    .jlp.cmd_tran_type = E_CMD_TYPE_JSON
    };
	uint8_t dev_mac[6];
	char dev_mac_str[20];
	memset(dev_mac_str, 0, 20);
	esp_wifi_get_mac(ESP_IF_WIFI_STA, dev_mac);
	sprintf(dev_mac_str, ""MACSTR"", MAC2STR(dev_mac));
	strcpy(product_info.jlp.mac, dev_mac_str);
    printf("*********************************\r\n");
    printf("*         PRODUCT INFO          *\r\n");
    printf("*********************************\r\n");
	printf("UUID     : %s\r\n", product_info.jlp.uuid);
    printf("mac      : %s\r\n", product_info.jlp.mac);
    printf("type     : %d\r\n", product_info.jlp.devtype);
    printf("version  : %d\r\n", product_info.jlp.version);
	printf("fw_ver   : %s\r\n", product_info.jlp.firmwareVersion);
    printf("model    : %s\r\n", product_info.jlp.modelCode);
	printf("tran type: %d\r\n", product_info.jlp.cmd_tran_type);
    ESP_ERROR_CHECK( esp_joylink_event_init((joylink_event_cb_t)cb) );
    esp_joylink_init(&product_info);
#endif
    return ESP_OK;
}

esp_err_t cloud_read(virtual_device_t* virtual_dev,int ms_wait)
{
#ifdef CLOUD_ALINK
    char *down_cmd = (char *) malloc(ALINK_DATA_LEN);
    esp_alink_read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);

    int attrLen = 0, valueLen = 0, value = 0, i = 0;
    char *valueStr = NULL, *attrStr = NULL;
    for (i = 0; i < 5; i++) {
        attrStr = json_get_value_by_name(down_cmd, strlen(down_cmd), device_attr[i], &attrLen, 0);
        valueStr = json_get_value_by_name(attrStr, attrLen, "value", &valueLen, 0);
        if (valueStr && valueLen > 0) {
            char lastChar = *(valueStr + valueLen);
            *(valueStr + valueLen) = 0;
            value = atoi(valueStr);
            *(valueStr + valueLen) = lastChar;

            switch (i) {
                case 0:
                    virtual_dev->power = value;
                    break;
                case 1:
                    virtual_dev->temp_value = value;
                    break;
                case 2:
                    virtual_dev->light_value = value;
                    break;
                case 3:
                    virtual_dev->time_delay = value;
                    break;
                case 4:
                    virtual_dev->work_mode = value;
                    break;
                default:
                    break;
            }
        }
    }
    free(down_cmd);
#endif
    
#ifdef CLOUD_JOYLINK
    char *down_cmd = (char *)malloc(JOYLINK_DATA_LEN);
    esp_joylink_read(down_cmd, JOYLINK_DATA_LEN, portMAX_DELAY);
    int i, is_crtl = 0;
	int valueLen = 0;
    char *valueStr = NULL;
	char data_buff[30];

	char *pCmd = down_cmd;

	for (i = 0; i < 5; i++) {
		valueStr = joylink_json_get_value_by_id(pCmd, strlen(pCmd), device_attr[i], &valueLen);
	    if (valueStr && valueLen > 0) {
			memset(data_buff, 0, 30);
			memcpy(data_buff, valueStr, valueLen);
			ESP_LOGI(TAG, "valueStr:%s, attrLen:%d",data_buff, valueLen);
			is_crtl ++;
	        switch (i) {
	        case 0:
	            virtual_dev->power = atoi(data_buff);
	            break;
	        case 1:
	            virtual_dev->light_value= atoi(data_buff);
	            break;
	        case 2:
	            virtual_dev->temp_value = atoi(data_buff) / 100;
	            break;
	        case 3:
	            virtual_dev->work_mode= atoi(data_buff);
	            break;
	        case 4:
                virtual_dev->time_delay = 0;
				//strcpy(virtual_device.color, data_buff);
	            break;
	        default:
	            break;
	        }
	    }
	}
    free(down_cmd);
#endif
    return ESP_OK;
}

esp_err_t cloud_write(virtual_device_t virtual_device, int ms_wait)
{
#ifdef CLOUD_ALINK
    char *up_cmd = (char *) malloc(ALINK_DATA_LEN);
    memset(up_cmd, 0, ALINK_DATA_LEN);
    sprintf(up_cmd, main_dev_params, virtual_device.power, virtual_device.temp_value, virtual_device.light_value,
            virtual_device.time_delay, virtual_device.work_mode);
    free(up_cmd);
    return esp_alink_write(up_cmd, ALINK_DATA_LEN, ms_wait);
#endif
#ifdef CLOUD_JOYLINK
    char *up_cmd = (char *)malloc(JOYLINK_DATA_LEN);
    memset(up_cmd, 0, JOYLINK_DATA_LEN);
    sprintf(up_cmd, joylink_json_upload_str, JOYLINK_RET_JSON_OK, virtual_device.power, virtual_device.light_value,
			virtual_device.temp_value, virtual_device.work_mode, "red");
    return esp_joylink_write(up_cmd, strlen(up_cmd)+1, 500 / portTICK_PERIOD_MS);
#endif
    return ESP_FAIL;
}

int cloud_sys_net_is_ready(void)
{
#ifdef CLOUD_ALINK
    return platform_sys_net_is_ready();
#endif
    return -1;
}