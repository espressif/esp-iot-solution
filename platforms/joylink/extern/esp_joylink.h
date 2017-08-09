/*
 **** example : the format of light device ***********
 **** from server or lan *****************************
 *{
 *   "cmd": 5,
 *   "data": {
 *       "streams": [
 *           {
 *               "current_value": "1",
 *               "stream_id": "power"
 *           }
 *       ],
 *       "snapshot": [
 *           {
 *               "current_value": "",
 *               "stream_id": "brightness"
 *           },
 *           {
 *               "current_value": "",
 *               "stream_id": "colortemp"
 *           },
 *           {
 *               "current_value": "",
 *               "stream_id": "color"
 *           },
 *           {
 *               "current_value": "0",
 *               "stream_id": "power"
 *           },
 *           {
 *               "current_value": "",
 *               "stream_id": "mode"
 *           }
 *       ]
 *   }
 *}
 **** send server or lan ***************************** 
 *{
 *   "code":0,
 *   "data":[
 *           {
 *               "current_value": "",
 *               "stream_id": "brightness"
 *           },
 *           {
 *               "current_value": "",
 *               "stream_id": "colortemp"
 *           },
 *           {
 *               "current_value": "",
 *               "stream_id": "color"
 *           },
 *           {
 *               "current_value": "0",
 *               "stream_id": "power"
 *           },
 *           {
 *               "current_value": "",
 *               "stream_id": "mode"
 *           }
 *       ]
 *}
*/

#ifndef __ESP_JOYLINK_H__
#define __ESP_JOYLINK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "joylink.h"
#include "joylink_log.h"
#include "joylink_ret_code.h"

#define JOYLINK_DATA_LEN (JL_MAX_PACKET_LEN) //JL_MAX_PACKET_LEN
#define JOYLINK_TASK_PRIOTY 5

typedef struct {
    JLPInfo_t jlp;
    char innet_aes_key[64];
} joylink_info_t;

typedef enum {
	JD_NONE = 0,
	JD_JSON_CMD_GET  = 4,
	JD_JSON_CMD_CTRL = 5
} joylink_type_t;

typedef enum {
	JOYLINK_ERROR_PKG_SAME = -1001,
	JOYLINK_ERROR_PKG_NUM_BREAK_MAX = -1002,
    JOYLINK_ERROR_PKG_BREAK_ARRAY = -1003,
    JOYLINK_ERROR_PARAM_INVAID = -3,
    JOYLINK_ERROR_NO_EXIST = -2,	
    JOYLINK_ERROR_RAM_LACK = -8001,
    JOYLINK_ERROR_DEV_INVAID = -8002,
    JOYLINK_RET_JSON_OK = 0,
    JOYLINK_RET_JSON_ERR = 1,
    JOYLINK_OK = (0),
    JOYLINK_ERR = (-1)
} joylink_ret_t;

typedef enum {
	JOYLINK_EVENT_NONE = 0,
	JOYLINK_EVENT_WIFI_START_SMARTCONFIG,
	JOYLINK_EVENT_WIFI_GOT_IP,
	JOYLINK_EVENT_WIFI_DISCONNECTED,
    JOYLINK_EVENT_CLOUD_CONNECTED,
    JOYLINK_EVENT_CLOUD_DISCONNECTED,
    JOYLINK_EVENT_GET_DEVICE_DATA,
    JOYLINK_EVENT_SET_DEVICE_DATA,
//    JOYLINK_EVENT_POST_CLOUD_DATA,
//    JOYLINK_EVENT_OTA,
} joylink_event_t;

typedef int (*joylink_event_cb_t)(joylink_event_t event);

/**
  * @brief register joylink SDK event callback.
  *
  * @param cb  joylink_event_cb_t.
  *
  * @return
  *    - JOYLINK_OK:  ok
  *    - JOYLINK_ERR:  err
  */
int esp_joylink_event_init(joylink_event_cb_t cb);

/**
  * @brief register function, reset device.
  *
  * @param arg  NULL.
  *
  */
void joylink_button_smnt_tap_cb(void* arg);

/**
  * @brief register function, reset device.
  *
  * @param arg  NULL.
  *
  */
void joylink_button_reset_tap_cb(void* arg);

/**
  * @brief upload device info to joylink SDK.
  *
  * @param down_cmd  [out]get the cmd from server or lan.
  * @param len  [in]the lenth of down_cmd, should less than JOYLINK_DATA_LEN.
  * @param micro_seconds  timeout.
  * 
  * @return
  *    - >0 : real cmd lenth
  *    - <= : read err
  */
int esp_joylink_read(void *down_cmd, size_t size, int micro_seconds);

/**
  * @brief upload device info to joylink SDK.
  *
  * @param up_cmd  [in]must be upload cmd with the format.
  * @param len  [in]the lenth of up_cmd, should include '\0',should less than JOYLINK_DATA_LEN.
  * @param micro_seconds  [in]timeout.
  * 
  * @return
  *    - >0 : real cmd lenth
  *    - <= : read err
  */
int esp_joylink_write(void *up_cmd, size_t len, int micro_seconds);

/**
  * @brief joylink SDK init.
  *
  * @param arg  [in] get info from https://devsmart.jd.com.
  *
  * @return
  *    - JOYLINK_OK: set ok
  *    - JOYLINK_ERR: set err
  */
int esp_joylink_init(joylink_info_t * arg);

/**
  * @brief get the system jlp info, e.g. "feedid".
  *
  * @attention 1. This API can only be called after activate the device.
  *
  * @param jlp  memory must be ready.
  *
  * @return
  *    - JOYLINK_OK: get ok
  *    - JOYLINK_ERR: get err
  */
int joylink_get_jlp_info(joylink_info_t *jlp);

/**
  * @brief get the json cmd type.
  *
  * @param down_cmd  [in] json format from server or lan.
  * @param feedid_out  [out] return the dev's feedid.
  *
  * @return
  *    - JD_NONE : none
  *    - JD_JSON_CMD_GET : this cmd is to get device snapshot
  *    - JD_JSON_CMD_CTRL : this is ctrl cmd
  */
joylink_type_t joylink_get_down_cmd_type(char *down_cmd, char *feedid_out);

/**
* @brief Get the value by a specified key from a JOYLINK json string
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
 * e.g. (JOYLINK json, StrLen, "power", ValueLen)
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
char* joylink_json_get_value_by_id(char* p_cJsonStr, int iStrLen, const char* p_cName, int* p_iValueLen);

#ifdef __cplusplus
}
#endif
#endif
