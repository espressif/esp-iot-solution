/* --------------------------------------------------
 * @brief: 
 *
 * @version: 1.0
 *
 * @date: 10/08/2015 09:28:27 AM
 *
 * --------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "joylink.h"
#include "joylink_packets.h"
#include "joylink_extern.h"
#include "joylink_json.h"
#include "joylink_extern_json.h"
#include "joylink_ret_code.h"
#include "esp_joylink.h"
#include "jd_innet.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "joylink_utils.h"
#include "tcpip_adapter.h"
#include "stdio.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "jd_innet.h"

#define JOYLINK_JSON_FORMAT_TRANS

#define JOYLINK_DOWN_Q_NUM 5
#define JOYLINK_UP_Q_NUM 5
#define JOYLINK_EVENT_QUEUE_NUM    3

#define JOYLINK_PARAM_CHECK(con) if(con) {log_error("Parameter error, "); perror(__func__); assert(0);}

static SemaphoreHandle_t xSemJoyRead = NULL;
static SemaphoreHandle_t xSemJoyWrite = NULL;
static SemaphoreHandle_t xSemJoyData = NULL;
static xQueueHandle xQueueJoyUp = NULL;
SemaphoreHandle_t xSemJoyReply = NULL;
xQueueHandle xQueueJoyDown = NULL;
xQueueHandle xQueueJoyEvent = NULL;
static joylink_event_cb_t s_joylink_event_cb = NULL;

void joylink_dev_clear_jlp_info(void);

static portMUX_TYPE reasonSpinlock = portMUX_INITIALIZER_UNLOCKED;
//{
//"streams":[
//    { “stream_id”: “switch”, “current_value”: “1” }
//],
//"snapshot": [ //控制时下发全量快照
//    {"current_value":"0","stream_id":"switch"}]
//}
char g_joylink_up_buf[JL_MAX_PACKET_LEN] = {0};

int joylink_get_dev_json_status(char *out_data, int32_t max_len)
{
	xSemaphoreTake(xSemJoyData, portMAX_DELAY);
	JOYLINK_PARAM_CHECK(out_data == NULL);
	
	int ret = 0;
	int32_t size = strlen(g_joylink_up_buf) + 1;
	if (size > max_len) {
		memcpy(out_data, g_joylink_up_buf, max_len);
		out_data[max_len] = '\0'; //make sure return str add '\0'
		ret = max_len;
	} else {
		memcpy(out_data, g_joylink_up_buf, size);
		
		ret = size;
	}
	
	xSemaphoreGive(xSemJoyData);
	return ret;
}

int joylink_set_dev_json_status(char *in_data, int32_t in_len)
{
	xSemaphoreTake(xSemJoyData, portMAX_DELAY);
	JOYLINK_PARAM_CHECK(in_data == NULL);
 	log_debug();
	E_JLRetCode_t ret = E_RET_OK;
	if (in_len > JL_MAX_PACKET_LEN) {
		log_error("len err\r\n");
		goto EXIT;
	} else if (0 == in_len){
		goto EXIT;
	}
	memset(g_joylink_up_buf, 0, sizeof(g_joylink_up_buf));
	memcpy(g_joylink_up_buf, in_data, in_len);
	ret = in_len;
EXIT:	
	xSemaphoreGive(xSemJoyData);
	return ret;
}

#ifdef JOYLINK_JSON_FORMAT_TRANS
/*
 * return the real len
*/
int esp_joylink_read(void *down_cmd, size_t size, int micro_seconds)
{
	xSemaphoreTake(xSemJoyRead, portMAX_DELAY);
	JOYLINK_PARAM_CHECK(down_cmd == NULL);
	JOYLINK_PARAM_CHECK(size == 0 || size > JL_MAX_PACKET_LEN);

	int ret = E_RET_OK;
	char *pdata = NULL;
	ret = xQueueReceive(xQueueJoyDown, &pdata, micro_seconds / portTICK_PERIOD_MS);
	if (ret == pdFALSE) {
		log_error("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, micro_seconds);
		ret = E_RET_ERROR;
		goto EXIT;
	}
	if (strlen(pdata)+1 > JL_MAX_PACKET_LEN) {
		log_error("read len > ALINK_DATA_LEN, len: %d", strlen(pdata) + 1);
		ret = E_RET_ERROR;
		goto EXIT;
	}
	memcpy(down_cmd, pdata, strlen(pdata)+1);
	ret = strlen(pdata)+1;
	//wait Queue
EXIT:
	if(!pdata)
		free(pdata);
	pdata = NULL;
	xSemaphoreGive(xSemJoyRead);
	return ret;
}

int esp_joylink_write(void *up_cmd, size_t len, int micro_seconds)
{
	xSemaphoreTake(xSemJoyWrite, portMAX_DELAY);
	JOYLINK_PARAM_CHECK(up_cmd == NULL);
	JOYLINK_PARAM_CHECK(len == 0 || len > JL_MAX_PACKET_LEN);
	log_debug("up cmd %s\r\n",(char*)up_cmd);
	E_JLRetCode_t ret = E_RET_OK;
	ret = joylink_set_dev_json_status(up_cmd, len);
	if (ret < 0) {
		log_error("len err\r\n");
		ret = -1;
		goto EXIT;
	}
EXIT:
	xSemaphoreGive(xSemJoyReply); //ctrl dev OK, data redly
//	char *pdata = NULL;
//	pdata = (char *)malloc(len + 1);
//	memset(pdata, 0, len + 1);
//	memcpy(pdata, up_cmd, len);
//	ret = xQueueSend(xQueueJoyUp, &pdata, 0);
//	if (ret == pdFALSE) {
//		log_error("xQueueReceive xQueueDownCmd, ret:%d, wait_time: %d", ret, micro_seconds);
//	}
//	joylink_server_upload_req();//send to server, but no LOCK no safe
	xSemaphoreGive(xSemJoyWrite);
	return ret;
}
#else 
int esp_joylink_read(void *down_cmd, size_t size, int micro_seconds)
{

}
int esp_joylink_write(void *up_cmd, size_t len, int micro_seconds)
{
	
}
#endif

int joylink_trans_init()
{
	log_debug();
	if(NULL == xSemJoyRead)
    	xSemJoyRead = xSemaphoreCreateMutex();
    JOYLINK_PARAM_CHECK(xSemJoyRead == NULL);
	if(NULL == xSemJoyWrite)
    	xSemJoyWrite = xSemaphoreCreateMutex();
    JOYLINK_PARAM_CHECK(xSemJoyWrite == NULL);
	if(NULL == xSemJoyData)
    	xSemJoyData = xSemaphoreCreateMutex();
    JOYLINK_PARAM_CHECK(xSemJoyData == NULL);
	if(NULL == xSemJoyReply)
    	xSemJoyReply = xSemaphoreCreateMutex();
    JOYLINK_PARAM_CHECK(xSemJoyReply == NULL);
	if(NULL == xQueueJoyDown)
    	xQueueJoyDown = xQueueCreate(JOYLINK_DOWN_Q_NUM, sizeof(char*));
    JOYLINK_PARAM_CHECK(xQueueJoyDown == NULL);	
	if(NULL == xQueueJoyUp)
    	xQueueJoyUp = xQueueCreate(JOYLINK_UP_Q_NUM, sizeof(char*));
    JOYLINK_PARAM_CHECK(xQueueJoyUp == NULL);
	return JOYLINK_OK;
	
}

int joylink_event_send(joylink_event_t event)
{
    if (xQueueJoyEvent == NULL)
        xQueueJoyEvent = xQueueCreate(JOYLINK_EVENT_QUEUE_NUM, sizeof(joylink_event_t));
    if (xQueueSend(xQueueJoyEvent, &event, 0) != pdTRUE) {
        log_error("xQueueSendToBack fail!");
        return -1;
    }
    return JOYLINK_OK;
}

int joylink_event_post_to_user(joylink_event_t event) 
{
	int ret = JOYLINK_OK;
	if (s_joylink_event_cb) {
		return (*s_joylink_event_cb)(event);
	}
	return JOYLINK_OK;
}

void joylink_event_ctrl_task(void *p)
{
	int ret = JOYLINK_OK;
	//loop to recv event, and callback
	joylink_event_t event;
	for(;;) {
		if (xQueueReceive(xQueueJoyEvent, &event, portMAX_DELAY) == pdPASS) {
			ret = joylink_event_post_to_user(event);
            if (ret != JOYLINK_OK) {
                log_error("post event to user fail!");
            }
		}
	}
	vTaskDelete(NULL);
}

int esp_joylink_event_init(joylink_event_cb_t cb)
{
	if (cb != NULL)
		s_joylink_event_cb = cb;
	if (xQueueJoyEvent == NULL)
		xQueueJoyEvent = xQueueCreate(JOYLINK_EVENT_QUEUE_NUM, sizeof(joylink_event_t));
	xTaskCreate(joylink_event_ctrl_task, "joy_eve", 1024*2, NULL, JOYLINK_TASK_PRIOTY, NULL);
	return JOYLINK_OK;
}


static void joylink_wifi_clear_info(void)
{
    nvs_handle out_handle;
    if (nvs_open("joylink_wifi", NVS_READWRITE, &out_handle) == ESP_OK) {
        nvs_erase_all(out_handle);
        nvs_close(out_handle);
    }
}

void joylink_button_smnt_tap_cb(void* arg)
{
    joylink_wifi_clear_info();
    jd_innet_start_task();
	joylink_event_send(JOYLINK_EVENT_WIFI_START_SMARTCONFIG);
}

void joylink_button_reset_tap_cb(void* arg)
{
    joylink_wifi_clear_info();
    joylink_dev_clear_jlp_info();
    esp_restart();
}

static void joylink_task(void *pvParameters)
{
    nvs_handle out_handle;
    wifi_config_t config;
    size_t size = 0;
    bool flag = false;
    if (nvs_open("joylink_wifi", NVS_READONLY, &out_handle) == ESP_OK) {
        memset(&config,0x0,sizeof(config));
        size = sizeof(config.sta.ssid);
        if (nvs_get_str(out_handle,"ssid",(char*)config.sta.ssid,&size) == ESP_OK) {
            if (size > 0) {
                size = sizeof(config.sta.password);
                if (nvs_get_str(out_handle,"password",(char*)config.sta.password,&size) == ESP_OK) {
                    flag = true;
                } else {
                    printf("--get password fail");
                }
            }
        } else {
            printf("--get ssid fail");
        }

        nvs_close(out_handle);
    }

    esp_wifi_set_mode(WIFI_MODE_STA);
	if (flag) {
	   esp_wifi_set_config(ESP_IF_WIFI_STA,&config);
	   esp_wifi_connect();
	}
   	extern int joylink_main_start();
   	joylink_main_start();
	vTaskDelete(NULL);
}

int esp_joylink_init(joylink_info_t * arg)
{
	int ret = JOYLINK_OK;

	//init arg of product
	if (arg != NULL) 
		memcpy(&(_g_pdev->jlp), &(arg->jlp), sizeof(JLPInfo_t));

	//innet
	jd_innet_set_aes_key((char*)(arg->innet_aes_key));

	//init Queue & sem
	joylink_trans_init();

	//init event task
	ret = xTaskCreate(joylink_task, "jl_task", 1024*10, NULL, 2, NULL);
	
	return (ret == pdPASS ? JOYLINK_OK : JOYLINK_ERR);
}

int joylink_get_jlp_info(joylink_info_t *jlp)
{
	if (jlp != NULL){
		memcpy(&jlp->jlp, &_g_pdev->jlp, sizeof(joylink_info_t));
		memcpy(jlp->innet_aes_key, jd_innet_get_aes_key(), strlen(jd_innet_get_aes_key())+1);
		return JOYLINK_OK;
	} else {
		return JOYLINK_ERR;
	}
}

joylink_type_t joylink_get_down_cmd_type(char *down_cmd, char *feedid_out)
{
	joylink_type_t ret = 0;
	joylink_info_t jlp_info;
	
	cJSON * pJson = cJSON_Parse(down_cmd);

    if(NULL == pJson){
        log_error("--->:cJSON_Parse is error:%s", down_cmd);
    }

	if (NULL != feedid_out) {
		cJSON * pFeedid = cJSON_GetObjectItem(pJson, "feedid");
		if(NULL != pFeedid){
			strcpy(feedid_out, pFeedid->valuestring);
	    } else {
			joylink_get_jlp_info(&jlp_info);
			strcpy(feedid_out, jlp_info.jlp.feedid);
		}
	}

    cJSON * pSub = cJSON_GetObjectItem(pJson, "cmd");
    if(NULL != pSub){
        log_info("--->:cmd is %d",pSub->valueint);
    }

	if (JD_JSON_CMD_GET == pSub->valueint) {
		ret = JD_JSON_CMD_GET;
		joylink_event_send(JOYLINK_EVENT_GET_DEVICE_DATA);
	} else if (JD_JSON_CMD_CTRL == pSub->valueint) {
		ret = JD_JSON_CMD_CTRL;
		joylink_event_send(JOYLINK_EVENT_SET_DEVICE_DATA);
	} else {
		ret = JD_NONE;
	}
	cJSON_Delete(pJson);
	return ret;
}

/**
* @brief Get the value by a specified key from a json string
*
* @param[in]  p_cJsonStr   @n the JSON string
* @param[in]  iStrLen      @n the JSON string length
* @param[in]  p_cName      @n the specified key string
* @param[out] p_iValueLen  @n the value length
* @return A pointer to the value
* @see None.
* @note None.
**/
/*
 *{
 *   "cmd": 5,
 *   "data": {
 *       "streams": [
 *           {
 *               "current_value": "1",
 *               "stream_id": "power"
 *           }
 *       ]
 *   }
 *}
*/
char * joylink_json_get_value_by_id(char* p_cJsonStr, int iStrLen, const char* p_cName, int* p_iValueLen)
{
	JOYLINK_PARAM_CHECK((p_cJsonStr == NULL) || (p_cName == NULL));
	int i;
	char *pTarget = NULL;
	char *str1 = malloc(iStrLen+1);
	memset(str1, 0, iStrLen+1);
	memcpy(str1, p_cJsonStr, iStrLen);
	
	cJSON *pJson = cJSON_Parse(str1);
	if (pJson != NULL) {
		cJSON *pData = cJSON_GetObjectItem(pJson, "data");
		if (pData != NULL) {
			cJSON *pStm = cJSON_GetObjectItem(pData, "streams");
			if (NULL != pStm) {
				int stmNum = cJSON_GetArraySize(pStm);
				for (i = 0; i < stmNum; i++) {
					cJSON *pItem = cJSON_GetArrayItem(pStm, i);
					if (pItem != NULL) {
						cJSON *pTar = cJSON_GetObjectItem(pItem, "stream_id");
						if(pTar != NULL) {
							if(0 == strcmp(p_cName, pTar->valuestring)) {
								cJSON *pTar = cJSON_GetObjectItem(pItem, "current_value");
								if(pTar != NULL) {
									pTarget = pTar->valuestring;
								}
							}
						}
					}
				}
			}
		}
	}
	
	if(str1)
		free(str1);
	cJSON_Delete(pJson);

	if (pTarget != NULL) {
		*p_iValueLen = strlen(pTarget);
	} else {
		*p_iValueLen = 0;
	}

	return pTarget;
}