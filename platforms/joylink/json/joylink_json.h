#ifndef __JOYLINK_JSON__
#define __JOYLINK_JSON__

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

#include <stdio.h>
#include "joylink.h"

int 
joylink_parse_scan(DevScan_t *scan, const char * pMsg);

char *
joylink_package_scan(const char *retMsg, const int retCode, 
        int scan_type, JLDevice_t *dv);

int 
joylink_parse_lan_write_key(DevEnable_t *de, const char * pMsg);

JLDevInfo_t *
joylink_parse_sub_add(const uint8_t* pMsg, int *out_num);

int 
joylink_packet_sub_add_rsp(const JLDevInfo_t *de, int num, char *out);

int
joylink_parse_sub_auth(const uint8_t* pMsg, JLDevInfo_t *dev);

int 
joylink_packet_sub_auth_rsp(const JLDevInfo_t *de, char *out);

char * 
joylink_package_subdev(JLDevInfo_t *sdev, int count);

int 
joylink_parse_json_ctrl(char *feedid, const char * pMsg);

int 
joylink_parse_server_ota_order_req(JLOtaOrder_t *otaOrder, const char * pMsg);

int
joylink_parse_server_ota_upload_req(JLOtaUploadRsp_t* otaUpload, const char* pMsg);

char *
joylink_package_ota_upload(JLOtaUpload_t *otaUpload);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
