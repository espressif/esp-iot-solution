/* --------------------------------------------------
 * @brief: 
 *
 * @version: 1.0
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

/**
 * brief:
 * Sub dev add. 
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_sub_add(JLDevInfo_t *dev, int num)
{
    /**
     *FIXME: todo
     */
    int ret = E_RET_OK;

    return ret;
}

/**
 * brief:
 * Sub dev del
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_sub_dev_del(JLDevInfo_t *dev, int num)
{
    /**
     *FIXME: todo
     */
    int ret = E_RET_OK;

    return ret;
}

/**
 * brief:
 * get by feedid 
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_sub_get_by_feedid(char *feedid, JLDevInfo_t *dev)
{
    /**
     *FIXME: todo
     */
    int ret = E_RET_ERROR;

    return ret;
}

/**
 * brief:
 * Get by uuid mac.
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_sub_dev_get_by_uuid_mac(char *uuid, char *mac, JLDevInfo_t *dev)
{
    /**
     *FIXME: todo
     */
    int ret = E_RET_OK;

    return ret;
}

/**
 * brief:
 * update feedid, localkey, accesskey.
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_sub_update_keys_by_uuid_mac(char *uuid, char *mac, JLDevInfo_t *dev)
{
    /**
     *FIXME: todo
     */
    int ret = E_RET_OK;

    return ret;
}

/**
 * brief:
 * Get devs info!!!! MUST TO FREE IT !!!.
 *
 * @Returns: 
 * JLDevInfo_t * , 
 */
JLDevInfo_t *
joylink_dev_sub_devs_get(int *count, int scan_type)
{
    /**
     *FIXME: todo must lock
     */
    JLDevInfo_t *devs = (JLDevInfo_t*)malloc(sizeof(JLDevInfo_t) * 10);
    
    switch(scan_type){
        case E_SCAN_TYPE_ALL: //All devs
            break;
        case E_SCAN_TYPE_WAITCONF: //devs no conf
            break;
        case E_SCAN_TYPE_CONFIGED: //devs confd
            break;
        default:
            break;
    }

    return devs;
}

/**
 * brief:
 * Sub dev ctrl
 *
 * @Returns: 
 *  E_RET_OK
 *  E_RET_ERROR
 */
E_JLRetCode_t
joylink_dev_sub_ctrl(const char* cmd, int cmd_len, char* feedid)
{
    log_debug("cmd:%s:feedid:%s\n", cmd, feedid);
    int ret = E_RET_OK;

    return ret;
}

/**
 * brief:
 * Get sub dev snapshot !!!! MUST TO FREE IT !!!.
 *
 * @Returns: 
 * JLDevInfo_t * , 
 */
char *
joylink_dev_sub_get_snap_shot(char *feedid, int *out_len)
{
    log_debug("");
    char *snap_shot = (char*)malloc(10);

    return snap_shot;
}

/**
 * brief:
 * sub dev unbind 
 *
 * @Returns: 
 * JLDevInfo_t * , 
 */
E_JLRetCode_t
joylink_dev_sub_unbind(const char *feedid)
{
    int ret = E_RET_OK;
    log_debug("feedid:%s\n", feedid);

    return ret;
}
