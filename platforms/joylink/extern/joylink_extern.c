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

#include "joylink_utils.h"
#include "tcpip_adapter.h"
#include "stdio.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"

typedef struct __attr_{
    char name[32];
    E_JL_DEV_ATTR_GET_CB get;
    E_JL_DEV_ATTR_SET_CB set;
}Attr_t;

typedef struct _attr_manage_{
    Attr_t power;
    Attr_t subdev;
    Attr_t uuid;
    Attr_t feedid;
    Attr_t accesskey;
    Attr_t localkey;
    Attr_t server_st;
	Attr_t macaddr;	
	Attr_t server_info;	
    
    Attr_t version;	

}WIFIManage_t;

WIFIManage_t _g_am, *_g_pam = &_g_am;

#define JOYLINK_NVS_NAMESPACE       "joylink"
#define JOYLINK_CTRL_DEV_TIMEOUT  (500) //ms


int joylink_get_dev_json_status(char *out_data, int32_t max_len);

/**
 * brief:
 * Check dev net is ok. 
 * @Param: st
 *
 * @Returns: 
 *  E_RET_TRUE
 *  E_RET_FAIL
 */
E_JLRetCode_t
joylink_dev_is_net_ok()
{
    /**
     *FIXME:must to do
     */
    int ret = E_RET_TRUE;
	tcpip_adapter_ip_info_t ip_config;

	if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA,&ip_config) != ESP_OK) {
        return E_RET_FAIL;
    }
	
	if(ip_config.ip.addr == 0){
		ret = E_RET_FAIL;
	} else {
		ret = E_RET_TRUE;
	}
	
    return ret;
}

/**
 * brief:
 * When connecting server st changed,
 * this fun will be called.
 *
 * @Param: st
 * JL_SERVER_ST_INIT      (0)
 * JL_SERVER_ST_AUTH      (1)
 * JL_SERVER_ST_WORK      (2)
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_set_connect_st(int st)
{
    /**
     *FIXME:must to do
     */
    static uint8_t server_st = 0;
    if (st == JL_SERVER_ST_WORK) {
		if (server_st == 0) {
			server_st = 1;
    		joylink_event_send(JOYLINK_EVENT_CLOUD_CONNECTED);
		}
    } else {
    	if (server_st == 1) {
			server_st = 0;
			joylink_event_send(JOYLINK_EVENT_CLOUD_DISCONNECTED);
    	}
	}
    int ret = E_RET_OK;
    return ret;
}

/**
 * brief:
 * Save joylink protocol info in flash.
 *
 * @Param:jlp 
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_set_attr_jlp(JLPInfo_t *jlp)
{
    nvs_handle out_handle;
    if(NULL == jlp){
        return E_RET_ERROR;
    }
    /**
     *FIXME:must to do
     */
    log_debug("--joylink_dev_set_attr_jlp");
    if (nvs_open(JOYLINK_NVS_NAMESPACE, NVS_READWRITE, &out_handle) != ESP_OK) {
        return E_RET_ERROR;
    }
    if(strlen(jlp->feedid)){
        if (nvs_set_str(out_handle,"feedid",jlp->feedid) != ESP_OK) {
            log_debug("--set feedid fail");
            return E_RET_ERROR;
        }
    }

    if(strlen(jlp->accesskey)){
        if (nvs_set_str(out_handle,"accesskey",jlp->accesskey) != ESP_OK) {
            log_debug("--set accesskey fail");
            return E_RET_ERROR;
        }
    }

    if(strlen(jlp->localkey)){
        if (nvs_set_str(out_handle,"localkey",jlp->localkey) != ESP_OK) {
            log_debug("--set localkey fail");
            return E_RET_ERROR;
        }
    }

    if(strlen(jlp->joylink_server)){
        if (nvs_set_str(out_handle,"joylink_server",jlp->joylink_server) != ESP_OK) {
            log_debug("--set joylink_server fail");
            return E_RET_ERROR;
        }

        if (nvs_set_u16(out_handle,"server_port",jlp->server_port) != ESP_OK) {
            log_debug("--set server_port fail");
            return E_RET_ERROR;
        }
    }

    nvs_close(out_handle);
    return E_RET_OK;
}


/**
 * brief:
 * get joylink protocol info.
 *
 * @Param:jlp 
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_get_jlp_info(JLPInfo_t *jlp)
{
    nvs_handle out_handle;
    if(NULL == jlp){
        return E_RET_ERROR;
    }
    /**
     *FIXME:must to do
     */
    int ret = E_RET_ERROR;
    size_t size = 0;
    log_debug("--joylink_dev_get_jlp_info");
    if (nvs_open(JOYLINK_NVS_NAMESPACE, NVS_READONLY, &out_handle) != ESP_OK) {
        return E_RET_ERROR;
    }

    size = sizeof(jlp->feedid);
    if (nvs_get_str(out_handle,"feedid",jlp->feedid,&size) != ESP_OK) {
        log_debug("--get feedid fail");
        return E_RET_ERROR;
    }

    size = sizeof(jlp->accesskey);
    if (nvs_get_str(out_handle,"accesskey",jlp->accesskey,&size) != ESP_OK) {
        log_debug("--get accesskey fail");
        return E_RET_ERROR;
    }

    size = sizeof(jlp->localkey);
    if (nvs_get_str(out_handle,"localkey",jlp->localkey,&size) != ESP_OK) {
        log_debug("--get localkey fail");
        return E_RET_ERROR;
    }

    size = sizeof(jlp->joylink_server);
    if (nvs_get_str(out_handle,"joylink_server",jlp->joylink_server,&size) != ESP_OK) {
        log_debug("--get joylink_server fail");
        return E_RET_ERROR;
    }

    if (nvs_get_u16(out_handle,"server_port", (uint16_t*) &jlp->server_port) != ESP_OK) {
        log_debug("--get server_port fail");
        return E_RET_ERROR;
    }

    nvs_close(out_handle);
    
    return ret;
}

void joylink_dev_clear_jlp_info(void)
{
    nvs_handle out_handle;
    if (nvs_open(JOYLINK_NVS_NAMESPACE, NVS_READWRITE, &out_handle) == ESP_OK) {
        nvs_erase_all(out_handle);
        nvs_close(out_handle);
    }
}

/**
 * brief:
 * Get dev snap shot.
 *
 * @Param:out_snap
 *
 * @Returns: snap shot len.
 */
int
joylink_dev_get_snap_shot(char *out_snap, int32_t out_max)
{
    if(NULL == out_snap || out_max < 0){
        return 0;
    }
    /**
     *FIXME:must to do
     */
    log_debug("Lan upload Script : to server");
    int len = 0;

    return len;
}

/**
 * brief:
 * Get dev snap shot.
 *
 * @Param:out_snap
 *
 * @Returns: snap shot len.
 */
int
joylink_dev_get_json_snap_shot(char *out_snap, int32_t out_max, int code, char *feedid)
{
    /**
     *FIXME:must to do
     */
    log_debug("Lan upload JSON : to lan");
	//joylink_get_staus_buff();
//    sprintf(out_snap, "{\"code\":%d, \"feedid\":\"%s\"}", code, feedid);
	char *data_buf = NULL;
	data_buf = (char *)malloc(JL_MAX_PACKET_LEN);
	if (data_buf == NULL) {
		log_error("malloc err");
		goto ERR;
	}
	memset(data_buf, 0, JL_MAX_PACKET_LEN);
	joylink_get_dev_json_status(data_buf, out_max);
	sprintf(out_snap, "%s", (char*)data_buf);
	int32_t size = strlen(out_snap);
	if (size > out_max) {
		log_error(" ERROR: len > out_max, len %d", size);
		goto ERR;
	}
    return size;
ERR:
	if (strlen(feedid)){
		sprintf(out_snap, "{\"code\":%d, \"feedid\":\"%s\"}", 0, feedid);
	} else {
		sprintf(out_snap, "{\"code\":%d, \"msg\":\"%d\"}", 0, JOYLINK_ERROR_RAM_LACK);
	}
	return strlen(out_snap);
}

/**
 * brief:
 * json ctrl.
 *
 * @Param:json_cmd
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t 
joylink_dev_lan_json_ctrl(const char *json_cmd)
{
    /**
     *FIXME:must to do
     */
    int ret = E_RET_ERROR;
	if(json_cmd == NULL){
        log_error("--->:ERROR: json_cmd is NULL\n");
        return ret;
    }
	
    log_debug("json ctrl:%s", json_cmd);
	int size = strlen(json_cmd) + 1;
	char * pdata = (char*)malloc(size);
	memset(pdata, 0, size);
	if (size > JL_MAX_PACKET_LEN) {
		log_error("--->:ERROR: size overflaw\n");
	}

	memcpy(pdata, json_cmd, size);
	if (xQueueSend(xQueueJoyDown, &pdata, 0) != pdTRUE) {
		log_error("--->:ERROR: xQueueSend err\n");
		free(pdata);
		return ret;
	}
	
	//JOYLINK_CTRL_DEV_TIMEOUT / portTICK_PERIOD_MS
	//ctrl dev OK, wait data redly
	ret = xSemaphoreTake(xSemJoyReply, JOYLINK_CTRL_DEV_TIMEOUT / portTICK_PERIOD_MS);
	if (ret == pdTRUE) {
		ret = E_RET_OK;
	}
    return ret;
}

/**
 * brief:
 * script control.
 * @Param: 
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t 
joylink_dev_script_ctrl(const char *cmd, JLContrl_t *ctr, int from_server)
{
    if(NULL == cmd|| NULL == ctr){
        return -1;
    }
    /**
     *FIXME:must to do
     */
    log_debug("script ctrl : form lan or server");
    int ret = E_RET_ERROR;
    int offset = 0;
    int time_tmp;
    memcpy(&time_tmp, cmd + offset, 4);
    offset +=4;
    memcpy(&ctr->biz_code, cmd + offset, 4);
    offset +=4;
    memcpy(&ctr->serial, cmd + offset, 4);
    offset +=4;

    if(ctr->biz_code == JL_BZCODE_GET_SNAPSHOT){
        /*
         *Nothing to do!
         */
        ret = 0;
    }else if(ctr->biz_code == JL_BZCODE_CTRL){
        /**
         *Must to do!
         */
        log_debug("script ctrl:%s", cmd+offset);
    }
    
    return ret;
}

/**
 * brief:
 * dev ota update
 * @Param: JLOtaOrder_t *otaOrder
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_ota(JLOtaOrder_t *otaOrder)
{
    if(NULL == otaOrder){
        return -1;
    }
    /**
     *FIXME:must to do
     */
    int ret = E_RET_OK;
    log_debug("serial:%d | feedid:%s | productuuid:%s | version:%d | versionname:%s | crc32:%d | url:%s\n",
        otaOrder->serial, otaOrder->feedid, otaOrder->productuuid, otaOrder->version,
        otaOrder->versionname, otaOrder->crc32, otaOrder->url);
    {
        extern void esp_ota_task_start(char* url);
        esp_ota_task_start(otaOrder->url);
    }
    return ret;
}

/**
 * brief:
 * dev ota status upload
 * @Param: 
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
void
joylink_dev_ota_status_upload()
{
    JLOtaUpload_t otaUpload;
    strcpy(otaUpload.feedid, _g_pdev->jlp.feedid);
    strcpy(otaUpload.productuuid, _g_pdev->jlp.uuid);

    /**
     *FIXME:must to do
     *status,status_desc, progress
     */
    joylink_server_ota_status_upload_req(&otaUpload);
}


