/* --------------------------------------------------
 * @file: joylink_extern.h
 *
 * @brief: 
 *
 * @version: 1.0
 *
 * @date: 10/26/2015 02:02:09 PM
 *
 * @author: yangzhongxuan , yangzhongxuan@gmail.com
 * --------------------------------------------------
 */

#ifndef __JOYLINK_EXTERN_H__
#define __JOYLINK_EXTERN_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "joylink_dev.h"
#include "joylink.h"
#include "joylink_ret_code.h"

#define JL_ATTR_WLAN24G      			"wlan24g"
#define JL_ATTR_SUBDEVS      	        "subdeviceslist"
#define JL_ATTR_WAN_SPEED      		    "wanspeed"
#define JL_ATTR_UUID    				"uuid"
#define JL_ATTR_FEEDID  				"feedid"
#define JL_ATTR_ACCESSKEY              "accesskey"
#define JL_ATTR_LOCALKEY  				"localkey"
#define JL_ATTR_CONN_STATUS            "connstatus"
#define JL_ATTR_MACADDR                "macaddr"
#define JL_ATTR_VERSION                "version"

typedef enum {
    TGW_ATTR_J = 1, 
    TGW_ATTR_A    
}E_JL_DEV_ATTR_TYPE;

typedef enum {
    TGW_CLOUD_INITAL = 0,  
    TGW_CLOUD_LOGGED      
}TGW_CLOUD_CONN_STATUS_E;

typedef enum {
    TGW_NET_UNCONNECT = 0,
    TGW_NET_CONNECTING,	
    TGW_NET_CONNECTED  
}TGW_NET_CONN_STATUS_E;

typedef enum {
    TGW_STATUS_UNBIND = 0, 
    TGW_STATUS_BINDED  	
}TGW_BIND_STATUS_E;

TGW_NET_CONN_STATUS_E TGW_NET_STATUS(void);

typedef int (*E_JL_DEV_ATTR_GET_CB)(char *buf, \
                                        unsigned int buf_sz);

typedef int (*E_JL_DEV_ATTR_SET_CB)(const char *json_in);
//---------- ---------------

E_JLRetCode_t
joylink_dev_is_net_ok();

E_JLRetCode_t
joylink_dev_set_connect_st(int st);

E_JLRetCode_t
joylink_dev_set_attr_jlp(JLPInfo_t *jlp);

E_JLRetCode_t
joylink_dev_get_jlp_info(JLPInfo_t *jlp);

E_JLRetCode_t
joylink_dev_set_attr(WIFICtrl_t *wi);

int
joylink_dev_get_snap_shot(char *out_snap, int32_t out_max);

int
joylink_dev_get_json_snap_shot(char *out_snap, int32_t out_max, int code, char *feedid);

E_JLRetCode_t 
joylink_dev_lan_json_ctrl(const char *json_cmd);

E_JLRetCode_t 
joylink_dev_script_ctrl(const char *recPainText, JLContrl_t *ctr, int from_server);

E_JLRetCode_t
joylink_dev_sub_add(JLDevInfo_t *dev, int num);

E_JLRetCode_t
joylink_sub_dev_del(JLDevInfo_t *dev, int num);

E_JLRetCode_t
joylink_dev_sub_get_by_feedid(char *feedid, JLDevInfo_t *dev);

E_JLRetCode_t
joylink_sub_dev_get_by_uuid_mac(char *uuid, char *mac, JLDevInfo_t *dev);

E_JLRetCode_t
joylink_dev_sub_update_keys_by_uuid_mac(char *uuid, char *mac, JLDevInfo_t *dev);

JLDevInfo_t *
joylink_dev_sub_devs_get(int *count, int scan_type);

E_JLRetCode_t
joylink_dev_sub_ctrl(const char* cmd, int cmd_len, char* feedid);

char *
joylink_dev_sub_get_snap_shot(char *feedid, int *out_len);

E_JLRetCode_t
joylink_dev_sub_unbind(const char *feedid);

E_JLRetCode_t
joylink_dev_ota(JLOtaOrder_t *otaOrder);

void
joylink_dev_ota_status_upload();

int 
joylink_dev_register_attr_cb(
        const char *name,
        E_JL_DEV_ATTR_TYPE type,
        E_JL_DEV_ATTR_GET_CB attr_get_cb,
        E_JL_DEV_ATTR_SET_CB attr_set_cb);
#ifdef __cplusplus
}
#endif

#endif /* __LOGGING_EXTERN__ */
